/*  -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <Xm/VendorS.h>

#include <Xm/Xm.h>
#include <Xm/PushB.h>
#include <Xm/DrawingA.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <gdksuperwin.h>
#include <gtkmozbox.h>

#include "PlugletViewMotif.h"
#include "PlugletEngine.h"

#include "PlugletLog.h"

jclass   PlugletViewMotif::clazz = NULL;
jmethodID  PlugletViewMotif::initMID = NULL;

PlugletViewMotif::PlugletViewMotif() {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	            ("PlugletViewMotif.Constructor this=%p\n",this));
    frame = NULL;
    WindowID = 0;
}

extern "C" void getAwtData(int          *awt_depth,
                Colormap     *awt_cmap,
                Visual       **awt_visual,
                int          *awt_num_colors,
                void         *pReserved);

extern "C" Display *getAwtDisplay(void);

extern "C" void getAwtLockFunctions(void (**AwtLock)(JNIEnv *),
                                    void (**AwtUnlock)(JNIEnv *),
                                    void (**AwtNoFlushUnlock)(JNIEnv *),
                                    void *pReserved);

static int awt_depth;
static Colormap awt_cmap;
static Visual * awt_visual;
static int awt_num_colors;

void PlugletViewMotif::Initialize() {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	   ("PlugletViewMotif.Initialize\n"));
    JNIEnv *env = PlugletEngine::GetJNIEnv();
    clazz = env->FindClass("sun/awt/motif/MEmbeddedFrame");
    if (!clazz) {
        env->ExceptionDescribe(); 
        return;
    }
    initMID = env->GetMethodID(clazz, "<init>", "(J)V");
    if (!initMID) {
        env->ExceptionDescribe();
        clazz = NULL;
        return;
    }
    getAwtData(&awt_depth, &awt_cmap, &awt_visual, &awt_num_colors, NULL);
}

PRBool PlugletViewMotif::SetWindow(nsPluginWindow* win) {
    
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	              ("PlugletViewMotif.SetWindow this=%p\n",this));
    JNIEnv *env = PlugletEngine::GetJNIEnv();
    if(!clazz) {
        Initialize();
        if(!clazz) {
            return PR_FALSE;
        }
    }
    if (!win 
        || !win->window) {
        if (win && !win->window) {
	    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	              ("PlugletViewMotif.SetWindow  win->window = NULL. We have a bug in plugin module. this=%p\n",this));
        }
        if (frame) {
            env->DeleteGlobalRef(frame);
            if (env->ExceptionOccurred()) {
                env->ExceptionDescribe();
            }
            frame = NULL;
            return PR_TRUE;
        }
        return PR_FALSE;
    }
    GdkSuperWin * superWin = (GdkSuperWin *) win->window;
    Window parentWindowID;
    Window rootWindowID;
    Window * childrenWindowIDs;
    unsigned int numberOfChildren;
    int containerWindowID = GDK_WINDOW_XWINDOW(superWin->shell_window);

    Status status = XQueryTree(GDK_DISPLAY(), containerWindowID,
                               &rootWindowID, &parentWindowID,
                               &childrenWindowIDs, & numberOfChildren);
    if (numberOfChildren >= 1) {
        containerWindowID = childrenWindowIDs[0];
    }
    if (WindowID == containerWindowID) {
        return PR_FALSE;
    }


    WindowID = containerWindowID;
    void (*AwtLock)(JNIEnv *);
    void (*AwtUnLock)(JNIEnv *);
    void (*AwtNoFlushUnLock)(JNIEnv *);
    getAwtLockFunctions(&AwtLock, &AwtUnLock, &AwtNoFlushUnLock,NULL);
    AwtLock(env);
    Display *awt_display = getAwtDisplay();
    XSync(awt_display, FALSE);
    Arg args[40];
    int argc = 0;
    XtSetArg(args[argc], XmNsaveUnder, False); argc++;
    XtSetArg(args[argc], XmNallowShellResize, False); argc++;
    XtSetArg(args[argc], XmNwidth, win->width); argc++;
    XtSetArg(args[argc], XmNheight, win->height); argc++;
    XtSetArg(args[argc], XmNx, 0); argc++;
    XtSetArg(args[argc], XmNy, 0); argc++;
    XtSetArg(args[argc], XmNmappedWhenManaged,False); argc++;
    XtSetArg(args[argc], XmNvisual, awt_visual); argc++;
    XtSetArg(args[argc], XmNdepth, awt_depth); argc++;
    XtSetArg(args[argc], XmNcolormap, awt_cmap); argc++;
    Widget w = XtAppCreateShell("AWTapp", "XApplication", 
                                vendorShellWidgetClass,
                                awt_display,
                                args, argc);
    XtRealizeWidget(w);
    XFlush(awt_display);
    XSync(awt_display, True);
    Window child, parent;
    parent = (Window) containerWindowID;
    child = XtWindow(w);
    XReparentWindow(awt_display, child, parent, 0, 0);
    XFlush(awt_display);
    XSync(awt_display, True);
    
    if (frame) {
        env->DeleteGlobalRef(frame);
        if (env->ExceptionOccurred()) {
            env->ExceptionDescribe();
            return PR_FALSE;
        }
        
    }
    frame = env->NewObject(clazz,initMID,(jlong)w);
    if(frame) {
        frame = env->NewGlobalRef(frame);
        if (env->ExceptionOccurred()) {
            env->ExceptionDescribe();
            return PR_FALSE;
        }
        
    }
    AwtUnLock(env);
    return PR_TRUE;
}

jobject PlugletViewMotif::GetJObject() {
    return frame;
}







