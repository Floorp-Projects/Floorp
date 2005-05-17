/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): edburns <edburns@acm.org>
 */


/*
 * CocoaBrowserControlCanvas.cpp
 */

#include <jawt_md.h>

#include <assert.h>

#import <Cocoa/Cocoa.h>

#include "CocoaBrowserControlCanvas.h"

#include "jni_util.h" //for throwing Exceptions to Java

jint CocoaBrowserControlCanvas::cocoaGetHandleToPeer(JNIEnv *env, jobject canvas) {
    JAWT awt;
    JAWT_DrawingSurface* ds = NULL;
    JAWT_DrawingSurfaceInfo* dsi = NULL;
    JAWT_MacOSXDrawingSurfaceInfo* dsi_mac = NULL;
    jboolean result = JNI_FALSE;
    jint lock = 0;
    NSView *view = NULL;
    NSWindow *win = NULL;
    void * windowPtr = NULL;
    
    // get the AWT
    awt.version = JAWT_VERSION_1_4;

    result = JAWT_GetAWT(env, &awt);

    // Get the drawing surface.  This can be safely cached.
    // Anything below the DS (DSI, contexts, etc) 
    // can possibly change/go away and should not be cached.
    ds = awt.GetDrawingSurface(env, canvas);

    if (NULL == ds) {
        util_ThrowExceptionToJava(env, "CocoaBrowserControlCanvas: can't get drawing surface");
    }
    
    // Lock the drawing surface
    // You must lock EACH TIME before drawing
    lock = ds->Lock(ds); 
    
    if ((lock & JAWT_LOCK_ERROR) != 0) {
        util_ThrowExceptionToJava(env, "CocoaBrowserControlCanvas: can't lock drawing surface");
    }
    
    // Get the drawing surface info
    dsi = ds->GetDrawingSurfaceInfo(ds);
    
    // Check DrawingSurfaceInfo.  This can be NULL on Mac OS X
    // if the windowing system is not ready
    if (dsi != NULL) {

        // Get the platform-specific drawing info
        // We will use this to get at Cocoa and CoreGraphics
        // See <JavaVM/jawt_md.h>
        dsi_mac = (JAWT_MacOSXDrawingSurfaceInfo*)dsi->platformInfo;
        if (NULL == dsi_mac) {
            util_ThrowExceptionToJava(env, "CocoaBrowserControlCanvas: can't get DrawingSurfaceInfo");
        }
        
        // Get the corresponding peer from the caller canvas
        view = dsi_mac->cocoaViewRef;
        win = [view window];
        windowPtr = [win windowRef];
        // Free the DrawingSurfaceInfo
        ds->FreeDrawingSurfaceInfo(dsi);
    }
  
    // Unlock the drawing surface
    // You must unlock EACH TIME when done drawing
    ds->Unlock(ds); 
    
    // Free the drawing surface (if not caching it)
    awt.FreeDrawingSurface(ds);

    return (jint) windowPtr;
}
