/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *  
 * The Original Code is The Waterfall Java Plugin Module
 *  
 * The Initial Developer of the Original Code is Sun Microsystems Inc
 * Portions created by Sun Microsystems Inc are Copyright (C) 2001
 * All Rights Reserved.
 * 
 * $Id: jvmp_drawing.h,v 1.2 2001/07/12 19:58:07 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */

#ifndef _JVMP_DRAWING_H
#define _JVMP_DRAWING_H
#ifdef __cplusplus
extern "C" {
#endif

/* 
 * Incomplete structure describing platform dependent window handle.
 * HWND on Win32, X window handler on X
 */
struct JPluginWindow;

/*
 * The type of a JPluginWindow - it specifies the type of the data structure
 * returned in the window field.
 */
enum JPluginWindowType {
    JPluginWindowType_Window = 1,
    JPluginWindowType_Drawable
};

struct JPluginRect {
    uint16_t            top;
    uint16_t            left;
    uint16_t            bottom;
    uint16_t            right;
};

struct JVMP_DrawingSurfaceInfo {
    struct JPluginWindow*  window;       /* Platform specific window handle */
    uint32_t      x;            /* Position of top left corner relative */
    uint32_t      y;            /*      to a netscape page.             */
    uint32_t      width;        /* Maximum window size */
    uint32_t      height;
    void*         ws_info;      /* Platform-dependent additonal data */
    enum JPluginWindowType type;      /* Is this a window or a drawable? */
    
};
typedef struct JVMP_DrawingSurfaceInfo JVMP_DrawingSurfaceInfo;


#ifdef __cplusplus
};
#endif
#endif










