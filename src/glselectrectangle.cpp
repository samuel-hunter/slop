/* glrectangle.hpp: Handles creating hardware accelerated rectangles on the screen using X11 and OpenGL.
 *
 * Copyright (C) 2014: Dalton Nell, Slop Contributors (https://github.com/naelstrof/slop/graphs/contributors).
 *
 * This file is part of Slop.
 *
 * Slop is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Slop is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Slop.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "glselectrectangle.hpp"

static Bool isDestroyNotify( Display* dpy, XEvent* ev, XPointer win ) {
    return ev->type == DestroyNotify && ev->xdestroywindow.window == *((Window*)win);
}

slop::GLSelectRectangle::~GLSelectRectangle() {
    if ( m_window == None ) {
        return;
    }
    // Try to erase the window before destroying it.
    glClearColor( 0, 0, 0, 0 );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    glXSwapBuffers( xengine->m_display, m_glxWindow );
    // Sleep for 0.1 seconds in hope that the rectangle was erased.
    usleep( 10000 );
    XDestroyWindow( xengine->m_display, m_window );
    XEvent event;
    // Block until the window is actually completely removed.
    XIfEvent( xengine->m_display, &event, &isDestroyNotify, (XPointer)&m_window );
    // Sleep for 0.1 seconds in hope that the screen actually cleared the window.
    usleep( 10000 );
}

void slop::GLSelectRectangle::constrainWithinMonitor( int* x, int* y, int* w, int* h ) {
    m_offsetx = 0;
    m_offsety = 0;
    m_offsetw = 0;
    m_offseth = 0;
    for ( unsigned int i=0;i<m_monitors.size();i++ ) {
        XRRCrtcInfo* monitor = m_monitors[ i ];
        if ( !((int)xengine->m_mousex >= (int)monitor->x && (int)xengine->m_mousey >= (int)monitor->y &&
               (int)xengine->m_mousex <= (int)(monitor->x+monitor->width) && (int)xengine->m_mousey <= (int)(monitor->y+monitor->height) ) ) {
            continue;
        }
        if ( (int)*x < (int)monitor->x ) {
            m_offsetx = monitor->x-*x;
            *w += *x-monitor->x;
            *x = monitor->x;
        }
        if ( (int)(*x+*w) >= (int)(monitor->x+monitor->width) ) {
            m_offsetw = (monitor->width-1-(*x-monitor->x+*w));
            *w = monitor->width-1-(*x-monitor->x);
        }
        if ( (int)*y < (int)monitor->y ) {
            m_offsety = monitor->y-*y;
            *h += *y-monitor->y;
            *y = monitor->y;
        }
        if ( (int)(*y+*h) >= (int)(monitor->y+monitor->height) ) {
            m_offseth = (monitor->height-1-(*y-monitor->y+*h));
            *h = monitor->height-1-(*y-monitor->y);
        }
        break;
    }
    m_offsetx *= m_glassSize;
    m_offsety *= m_glassSize;
    m_offsetw *= m_glassSize;
    m_offseth *= m_glassSize;
}

void slop::GLSelectRectangle::setMagnifySettings( bool on, float magstrength, unsigned int pixels ) {
    m_glassSize = magstrength;
    m_glassPixels = pixels;
    m_glassEnabled = on;
    m_glassx = xengine->m_mousex;
    m_glassy = xengine->m_mousey;
    m_realglassx = xengine->m_mousex;
    m_realglassy = xengine->m_mousey;
}

void slop::GLSelectRectangle::pushOut( int* x, int* y, int w, int h, int rx, int ry, int rw, int rh ) {
    // AABB to test for collision
    if (!(
          *x < rx + rw &&
          *x + w > rx &&
          *y < ry + rh &&
          h + *y > ry
       )) {
        // No collision, so we do nothing.
        return;
    }
    // Otherwise we find an optimal angle to push ourselves out at.
    int centerx = rx+rw/2;
    int centery = ry+rh/2;
    float ang = -atan2( (float)(*y+(float)h/2.f)-(float)centery, (float)(*x+(float)w/2.f)-(float)centerx );
    float pi = 3.1415926535897;
    float upright = pi/2 - atan( (2.f*float(rx+rw)-2.f*(float)centerx)/(float)rh );
    float upleft = pi/2 - atan( (2.f*(float)rx-2.f*(float)centerx)/(float)rh );
    float downright = -upright;
    float downleft = -upleft;
    if ( ang >= upright && ang <= upleft ) {
        *x = centerx + ((rh*cos(ang))/(2*sin(ang))) - w/2;
        *y = centery - rh/2 - h;
    } else if ( ang <= downright && ang >= downleft) {
        *x = centerx - ((rh*cos(ang))/(2*sin(ang))) - w/2;
        *y = centery + rh/2;
    } else if ( ang < downleft || ang > upleft ) {
        *x = centerx - rw/2 - w;
        *y = centery + (rw*sin(ang))/(2*cos(ang)) - h/2;
    } else {
        *x = centerx + rw/2;
        *y = centery - (rw*sin(ang))/(2*cos(ang)) - h/2;
    }
}

void slop::GLSelectRectangle::pushIn( int* x, int* y, int w, int h, int rx, int ry, int rw, int rh ) {
    if ( *x > rx && *y > ry &&
         *x+w < rx+rw && *y+h < ry+rh ) {
        // We're already fully contained...
        return;
    }
    // Otherwise we find an optimal angle to push ourselves in at.
    int centerx = rx+rw/2;
    int centery = ry+rh/2;
    float ang = -atan2( (float)(*y+(float)h/2.f)-(float)centery, (float)(*x+(float)w/2.f)-(float)centerx );
    float pi = 3.1415926535897;
    float upright = pi/2 - atan( (2.f*float(rx+rw)-2.f*(float)centerx)/(float)rh );
    float upleft = pi/2 - atan( (2.f*(float)rx-2.f*(float)centerx)/(float)rh );
    float downright = -upright;
    float downleft = -upleft;
    if ( ang >= upright && ang <= upleft ) {
        *x = centerx + ((rh*cos(ang))/(2*sin(ang))) - w/2;
        *y = centery - rh/2;
    } else if ( ang <= downright && ang >= downleft) {
        *x = centerx - ((rh*cos(ang))/(2*sin(ang))) - w/2;
        *y = centery + rh/2 - h;
    } else if ( ang < downleft || ang > upleft ) {
        *x = centerx - rw/2;
        *y = centery + (rw*sin(ang))/(2*cos(ang)) - h/2;
    } else {
        *x = centerx + rw/2 - w;
        *y = centery - (rw*sin(ang))/(2*cos(ang)) - h/2;
    }
    if ( !(*x > rx && *y > ry &&
         *x+w < rx+rw && *y+h < ry+rh) ) {
        if ( *x+w > rx+rw ) {
            *x -= w/2;
        }
        if ( *x < rx ) {
            *x += w/2;
        }
        if ( *y+h > ry+rh ) {
            *y -= h/2;
        }
        if ( *y < ry ) {
            *y += h/2;
        }
    }
}

void slop::GLSelectRectangle::findOptimalGlassPosition() {
    // Try to move the glass next to the mouse.
    m_glassx = xengine->m_mousex+m_glassPixels/2+5-m_glassBorder;
    m_glassy = xengine->m_mousey+m_glassPixels/2+5-m_glassBorder;
    XRectangle view, selection, combined;
    view.x = xengine->m_mousex-(m_glassPixels+1+m_glassBorder)/2;
    view.y = xengine->m_mousey-(m_glassPixels+1+m_glassBorder)/2;
    view.width = m_glassPixels+1;
    view.height = m_glassPixels+1;
    selection.x = m_x-m_border;
    selection.y = m_y-m_border;
    selection.width = m_width+m_border*2;
    selection.height = m_height+m_border*2;
    combined.x = std::min( selection.x, view.x );
    combined.y = std::min( selection.y, view.y );
    combined.width = selection.width + std::max( selection.x-view.x, (view.x+view.width)-(selection.x+selection.width) );
    combined.height = selection.height + std::max( selection.y-view.y, (view.y+view.height)-(selection.y+selection.height) );
    for ( unsigned int i=0;i<m_monitors.size();i++ ) {
        XRRCrtcInfo* monitor = m_monitors[ i ];
        // Push the glass inside the monitor the mouse is on.
        if ( (int)xengine->m_mousex >= (int)monitor->x && (int)xengine->m_mousex <= (int)(monitor->x + monitor->width) &&
             (int)xengine->m_mousey >= (int)monitor->y && (int)xengine->m_mousey <= (int)(monitor->y + monitor->height) ) {
            pushIn( &m_glassx, &m_glassy, m_glassPixels*m_glassSize+m_glassBorder*2, m_glassPixels*m_glassSize+m_glassBorder*2, monitor->x, monitor->y, monitor->width, monitor->height );
            break;
        }
    }
    // Push the glass outside of the selection, but only if we are left clicking, and always keep it out of the "shot"
    if ( xengine->getCursor() != slop::Left ) {
        pushOut( &m_glassx, &m_glassy, m_glassPixels*m_glassSize+m_glassBorder*2, m_glassPixels*m_glassSize+m_glassBorder*2, combined.x, combined.y, combined.width, combined.height );
    } else {
        pushOut( &m_glassx, &m_glassy, m_glassPixels*m_glassSize+m_glassBorder*2, m_glassPixels*m_glassSize+m_glassBorder*2, view.x, view.y, view.width, view.height );
    }
    m_glassx += m_glassBorder;
    m_glassy += m_glassBorder;
}

void slop::GLSelectRectangle::generateMagnifyingGlass() {
    int x = xengine->m_mousex-m_glassPixels/2;
    int y = xengine->m_mousey-m_glassPixels/2;
    int w = m_glassPixels;
    int h = m_glassPixels;
    constrainWithinMonitor( &x, &y, &w, &h );
    XImage* image = XGetImage( xengine->m_display, xengine->m_root, x, y, w, h, 0xffffffff, ZPixmap );
    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &m_texid);
    glBindTexture(GL_TEXTURE_2D, m_texid);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, (void*)(&(image->data[0])));
    XDestroyImage( image );
    glDisable(GL_TEXTURE_2D);
}

void slop::GLSelectRectangle::setTheme( bool on, std::string name ) {
    if ( !on ) {
        return;
    }
    std::string root = resource->getRealPath( name );
    std::string tl = root + "/corner_tl.png";
    std::string bl = root + "/corner_bl.png";
    std::string tr = root + "/corner_tr.png";
    std::string br = root + "/corner_br.png";
    std::string straight = root + "/straight.png";
    // One of the textures didn't exist, so we cancel the theme.
    if (!resource->validatePath( tl ) ||
        !resource->validatePath( bl ) ||
        !resource->validatePath( tr ) ||
        !resource->validatePath( br ) ||
        !resource->validatePath( straight ) ) {
        fprintf( stderr, "One of the textures was missing in the theme... disabling.\n" );
        return;
    }
    // Otherwise we load each one :)
    ilInit();
    ilutInit();
    ilutRenderer(ILUT_OPENGL);
    ILuint corner;
    ilGenImages( 1, &corner );
    ilBindImage( corner );
    ilLoadImage( tl.c_str() );
    m_cornerids[0] = ilutGLBindMipmaps();
    ilLoadImage( tr.c_str() );
    m_cornerids[1] = ilutGLBindMipmaps();
    ilLoadImage( bl.c_str() );
    m_cornerids[2] = ilutGLBindMipmaps();
    ilLoadImage( br.c_str() );
    m_cornerids[3] = ilutGLBindMipmaps();
    ilLoadImage( straight.c_str() );
    m_straightwidth = ilGetInteger( IL_IMAGE_WIDTH );
    m_straightheight = ilGetInteger( IL_IMAGE_HEIGHT );
    m_straightid = ilutGLBindMipmaps();
    // And clean up after.
    ilDeleteImages( 1, &corner );
    m_themed = on;
}

slop::GLSelectRectangle::GLSelectRectangle( int sx, int sy, int ex, int ey, int border, bool highlight, float r, float g, float b, float a ) {
    m_x = std::min( sx, ex );
    m_y = std::min( sy, ey );
    m_width = std::max( sx, ex ) - m_x;
    m_height = std::max( sy, ey ) - m_y;
    m_r = r;
    m_g = g;
    m_b = b;
    m_a = a;
    m_border = border;
    m_window = None;
    m_highlight = highlight;
    m_glassPixels = 64;
    m_glassx = xengine->m_mousex;
    m_glassy = xengine->m_mousey;
    m_realglassx = xengine->m_mousex;
    m_realglassy = xengine->m_mousey;
    m_glassSize = 4;
    m_glassBorder = 1;
    m_monitors = xengine->getCRTCS();
    m_themed = false;

    // If we don't have a border, we don't exist, so just die.
    if ( m_border == 0 ) {
        return;
    }

    if ( m_highlight ) {
        m_border = 0;
    }

    static int visdata[] = {
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_DOUBLEBUFFER, True,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        GLX_DEPTH_SIZE, 16,
        None
    };

    int numfbconfigs = 0;
    GLXFBConfig* fbconfigs = glXChooseFBConfig( xengine->m_display,  DefaultScreen( xengine->m_display ), visdata, &numfbconfigs );
    m_fbconfig = 0;
    for ( int i=0; i<numfbconfigs; i++ ) {
        m_visual = (XVisualInfo*)glXGetVisualFromFBConfig( xengine->m_display, fbconfigs[i] );
        if ( !m_visual ) {
            continue;
        }
        m_pictFormat = XRenderFindVisualFormat( xengine->m_display, m_visual->visual );
        if ( !m_pictFormat ) {
            continue;
        }
        m_fbconfig = fbconfigs[i];
        if ( m_pictFormat->direct.alphaMask > 0 ) {
            break;
        }
    }

    if ( !m_fbconfig ) {
        fprintf( stderr, "Couldn't find a matching FB config for a transparent OpenGL window!\n");
    }

    m_cmap = XCreateColormap( xengine->m_display, xengine->m_root, m_visual->visual, AllocNone );

    XSetWindowAttributes attributes;
    attributes.colormap = m_cmap;
    attributes.background_pixmap = None;
    attributes.border_pixmap = None;
    attributes.border_pixel = 0;
    // Disable window decorations.
    attributes.override_redirect = True;
    // Make sure we know when we've been successfully destroyed later!
    attributes.event_mask = StructureNotifyMask;
    unsigned long valueMask = CWOverrideRedirect | CWEventMask | CWBackPixmap | CWColormap | CWBorderPixel;


    // Create the window
    m_window = XCreateWindow( xengine->m_display, xengine->m_root, 0, 0, xengine->getWidth(), xengine->getHeight(),
                              0, m_visual->depth, InputOutput,
                              m_visual->visual, valueMask, &attributes );

    if ( !m_window ) {
        fprintf( stderr, "Couldn't create a GL window!\n");
    }

    m_glxWindow = m_window;

    static char title[] = "OpenGL Slop";
    XWMHints* startup_state = XAllocWMHints();
    startup_state->initial_state = NormalState;
    startup_state->flags = StateHint;
    XTextProperty textprop;
    textprop.value = (unsigned char*)title;
    textprop.encoding = XA_STRING;
    textprop.format = 8;
    textprop.nitems = strlen( title );
    XSizeHints sizehints;
    sizehints.x = 0;
    sizehints.y = 0;
    sizehints.width = xengine->getWidth();
    sizehints.height = xengine->getHeight();
    sizehints.flags = USPosition | USSize;
    XClassHint classhints;
    char name[] = "slop";
    classhints.res_name = name;
    classhints.res_class = name;
    XSetClassHint( xengine->m_display, m_window, &classhints );
    XSetWMProperties( xengine->m_display, m_window, &textprop, &textprop, NULL, 0, &sizehints, startup_state, NULL );
    XFree( startup_state );

    // Make it so all input falls through
    XRectangle rect;
    rect.x = rect.y = rect.width = rect.height = 0;
    XShapeCombineRectangles( xengine->m_display, m_window, ShapeInput, 0, 0, &rect, 1, ShapeSet, 0);

    XMapWindow( xengine->m_display, m_window );

    int dummy;
    if ( !glXQueryExtension( xengine->m_display, &dummy, &dummy ) ) {
        fprintf( stderr, "OpenGL is not supported!\n" );
    }
    m_renderContext = glXCreateNewContext( xengine->m_display, m_fbconfig, GLX_RGBA_TYPE, 0, True );
    if ( !m_renderContext ) {
        fprintf( stderr, "Failed to create a GL context.\n" );
    }
    if ( !glXMakeContextCurrent( xengine->m_display, m_glxWindow, m_glxWindow, m_renderContext ) ) {
        fprintf( stderr, "Failed to attach GL context to window!\n" );
    }
    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    glClearColor( 0, 0, 0, 0 );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    glXSwapBuffers( xengine->m_display, m_glxWindow );
}

void slop::GLSelectRectangle::setGeo( int sx, int sy, int ex, int ey ) {
    int x = std::min( sx, ex );
    int y = std::min( sy, ey );
    int w = std::max( sx, ex ) - x;
    int h = std::max( sy, ey ) - y;

    m_x = x;
    m_y = y;
    m_width = w;
    m_height = h;
}

void slop::GLSelectRectangle::update( double dt ) {
    glViewport( 0, 0, xengine->getWidth(), xengine->getHeight() );

    glClearColor( 0, 0, 0, 0 );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
    glOrtho( 0, xengine->getWidth(), xengine->getHeight(), 0, 1, -1 );

    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();

    if ( !m_themed ) {
        glColor4f( m_r, m_g, m_b, m_a );
        glRecti( m_x-m_border, m_y, m_x+m_width+m_border, m_y-m_border );
        glRecti( m_x-m_border, m_y+m_height, m_x+m_width+m_border, m_y+m_height+m_border );
        glRecti( m_x-m_border, m_y, m_x, m_y+m_height );
        glRecti( m_x+m_width, m_y, m_x+m_width+m_border, m_y+m_height );
    } else {
        glColor4f( m_r, m_g, m_b, m_a );
        glEnable( GL_TEXTURE_2D );
        glBindTexture( GL_TEXTURE_2D, m_straightid );
        float something = (float)(m_border)/(float)m_straightheight;
        float txoffset = (((float)m_width+m_border)/(float)m_straightwidth)/something;
        float tyoffset = (((float)m_height+m_border)/(float)m_straightwidth)/something;
        //float ratio = ((float)m_straightwidth/(float)m_straightheight);
        glBegin( GL_QUADS );
        // straight top
        glTexCoord2f(0.0, 1.0); glVertex2f( m_x-m_border/2, m_y-m_border );
        glTexCoord2f(txoffset, 1.0); glVertex2f( m_x+m_width+m_border/2, m_y-m_border );
        glTexCoord2f(txoffset, 0.0); glVertex2f( m_x+m_width+m_border/2, m_y );
        glTexCoord2f(0.0, 0.0); glVertex2f( m_x-m_border/2, m_y );
        // straight bot
        glTexCoord2f(0.0, 1.0); glVertex2f( m_x-m_border/2, m_y+m_height );
        glTexCoord2f(txoffset, 1.0); glVertex2f( m_x+m_width+m_border/2, m_y+m_height );
        glTexCoord2f(txoffset, 0.0); glVertex2f( m_x+m_width+m_border/2, m_y+m_height+m_border );
        glTexCoord2f(0.0, 0.0); glVertex2f( m_x-m_border/2, m_y+m_height+m_border );
        // straight left
        glTexCoord2f(0.0, 0.0); glVertex2f( m_x-m_border, m_y-m_border/2 );
        glTexCoord2f(0.0, 1.0); glVertex2f( m_x, m_y-m_border/2 );
        glTexCoord2f(tyoffset, 1.0); glVertex2f( m_x, m_y+m_height+m_border/2 );
        glTexCoord2f(tyoffset, 0.0); glVertex2f( m_x-m_border, m_y+m_height+m_border/2 );
        // straight right
        glTexCoord2f(0.0, 0.0); glVertex2f( m_x+m_width, m_y-m_border/2 );
        glTexCoord2f(0.0, 1.0); glVertex2f( m_x+m_width+m_border, m_y-m_border/2 );
        glTexCoord2f(tyoffset, 1.0); glVertex2f( m_x+m_width+m_border, m_y+m_height+m_border/2 );
        glTexCoord2f(tyoffset, 0.0); glVertex2f( m_x+m_width, m_y+m_height+m_border/2 );
        glEnd();
        // top left corner
        glBindTexture( GL_TEXTURE_2D, m_cornerids[0] );
        glBegin( GL_QUADS );
        glTexCoord2f(0.0, 1.0); glVertex2f( m_x-m_border, m_y-m_border );
        glTexCoord2f(1.0, 1.0); glVertex2f( m_x, m_y-m_border );
        glTexCoord2f(1.0, 0.0); glVertex2f( m_x, m_y );
        glTexCoord2f(0.0, 0.0); glVertex2f( m_x-m_border, m_y );
        glEnd();
        // top right
        glBindTexture( GL_TEXTURE_2D, m_cornerids[1] );
        glBegin( GL_QUADS );
        glTexCoord2f(0.0, 1.0); glVertex2f( m_x+m_width, m_y-m_border );
        glTexCoord2f(1.0, 1.0); glVertex2f( m_x+m_width+m_border, m_y-m_border );
        glTexCoord2f(1.0, 0.0); glVertex2f( m_x+m_width+m_border, m_y );
        glTexCoord2f(0.0, 0.0); glVertex2f( m_x+m_width, m_y );
        glEnd();
        // bottom left
        glBindTexture( GL_TEXTURE_2D, m_cornerids[2] );
        glBegin( GL_QUADS );
        glTexCoord2f(0.0, 1.0); glVertex2f( m_x-m_border, m_y+m_height );
        glTexCoord2f(1.0, 1.0); glVertex2f( m_x, m_y+m_height );
        glTexCoord2f(1.0, 0.0); glVertex2f( m_x, m_y+m_height+m_border );
        glTexCoord2f(0.0, 0.0); glVertex2f( m_x-m_border, m_y+m_height+m_border );
        glEnd();
        // bottom right
        glBindTexture( GL_TEXTURE_2D, m_cornerids[2] );
        glBegin( GL_QUADS );
        glTexCoord2f(0.0, 1.0); glVertex2f( m_x+m_width, m_y+m_height );
        glTexCoord2f(1.0, 1.0); glVertex2f( m_x+m_width+m_border, m_y+m_height );
        glTexCoord2f(1.0, 0.0); glVertex2f( m_x+m_width+m_border, m_y+m_height+m_border );
        glTexCoord2f(0.0, 0.0); glVertex2f( m_x+m_width, m_y+m_height+m_border );
        glEnd();
    }

    if ( m_glassEnabled ) {
        generateMagnifyingGlass();
        findOptimalGlassPosition();

        // Takes .1 second to reach the real position. Used for easing.
        m_realglassx -= float(m_realglassx - (float)m_glassx) * dt * 10;
        m_realglassy -= float(m_realglassy - (float)m_glassy) * dt * 10;

        // Black outline...

        glColor4f( 0, 0, 0, 1 );
        glBegin( GL_QUADS );
        glTexCoord2f(0.0, 1.0); glVertex3f( m_realglassx+m_offsetx-m_glassBorder,                               m_realglassy+(m_glassSize*m_glassPixels)+m_offseth+m_glassBorder, 0.0);
        glTexCoord2f(1.0, 1.0); glVertex3f( m_realglassx+(m_glassSize*m_glassPixels)+m_offsetw+m_glassBorder,   m_realglassy+(m_glassSize*m_glassPixels)+m_offseth+1, 0.0);
        glTexCoord2f(1.0, 0.0); glVertex3f( m_realglassx+(m_glassSize*m_glassPixels)+m_offsetw+m_glassBorder,   m_realglassy+m_offsety-m_glassBorder, 0.0);
        glTexCoord2f(0.0, 0.0); glVertex3f( m_realglassx+m_offsetx-m_glassBorder,                               m_realglassy+m_offsety-m_glassBorder, 0.0);
        glEnd();

        glEnable( GL_TEXTURE_2D );
        glBindTexture( GL_TEXTURE_2D, m_texid );
        glColor4f( 1, 1, 1, 1 );
        glBegin( GL_QUADS );
        glTexCoord2f(0.0, 1.0); glVertex3f( m_realglassx+m_offsetx,                             m_realglassy+(m_glassSize*m_glassPixels)+m_offseth, 0.0);
        glTexCoord2f(1.0, 1.0); glVertex3f( m_realglassx+(m_glassSize*m_glassPixels)+m_offsetw, m_realglassy+(m_glassSize*m_glassPixels)+m_offseth, 0.0);
        glTexCoord2f(1.0, 0.0); glVertex3f( m_realglassx+(m_glassSize*m_glassPixels)+m_offsetw, m_realglassy+m_offsety, 0.0);
        glTexCoord2f(0.0, 0.0); glVertex3f( m_realglassx+m_offsetx,                             m_realglassy+m_offsety, 0.0);
        glEnd();
        glDisable( GL_TEXTURE_2D );

        glLogicOp(GL_INVERT);
        glEnable(GL_COLOR_LOGIC_OP);
        glLineWidth( 2 );
        glColor4f( 0, 0, 0, 1 );
        glBegin( GL_LINES );
        float cx = m_realglassx+(m_glassSize*m_glassPixels)/2;
        float cy = m_realglassy+(m_glassSize*m_glassPixels)/2;
        glVertex3f( cx-5, cy, 0 );
        glVertex3f( cx+5, cy, 0 );
        glVertex3f( cx, cy-5, 0 );
        glVertex3f( cx, cy+5, 0 );
        glEnd();
        glLogicOp(GL_NOOP);
        glDisable(GL_COLOR_LOGIC_OP);
    }

    glXSwapBuffers( xengine->m_display, m_glxWindow );
}
