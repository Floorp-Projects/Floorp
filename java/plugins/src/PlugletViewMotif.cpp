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

#include "PlugletViewMotif.h"
#include "PlugletEngine.h"


jclass   PlugletViewMotif::clazz = NULL;
jmethodID  PlugletViewMotif::initMID = NULL;

PlugletViewMotif::PlugletViewMotif() {
    frame = NULL;
    WindowID = 0;
}
void PlugletViewMotif::Initialize() {
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
}

#define AWT_LOCK()      (env)->MonitorEnter(awt_lock)

#define AWT_UNLOCK()	(env)->MonitorExit(awt_lock)



extern jobject awt_lock;
extern Display *awt_display;

PRBool PlugletViewMotif::SetWindow(nsPluginWindow* win) {
    printf("--PlugletViewMotif::SetWindow \n");
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
            printf("--PlugletViewMotif win->window = NULL. We have a bug in plugin module \n");
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
    GtkWidget * containerWidget = (GtkWidget *) win->window;
    Window parentWindowID;
    Window rootWindowID;
    Window * childrenWindowIDs;
    unsigned int numberOfChildren;
    int containerWindowID = GDK_WINDOW_XWINDOW(containerWidget->window);
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
    AWT_LOCK();
    XSync(awt_display, FALSE);
    Arg args[40];
    int argc = 0;
    XtSetArg(args[argc], XmNsaveUnder, False); argc++;
    XtSetArg(args[argc], XmNallowShellResize, False); argc++;
    XtSetArg(args[argc], XmNwidth, win->width); argc++;
    XtSetArg(args[argc], XmNheight, win->height); argc++;
    XtSetArg(args[argc], XmNx, 0); argc++;
    XtSetArg(args[argc], XmNy, 0); argc++;
    XtSetArg(args[argc],XmNmappedWhenManaged,False); argc++;
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
    printf("--before new frame\n");
    frame = env->NewObject(clazz,initMID,(jlong)w);
    printf("--after new frame\n");
    if(frame) {
        frame = env->NewGlobalRef(frame);
        if (env->ExceptionOccurred()) {
            env->ExceptionDescribe();
            return PR_FALSE;
        }
        
    }
    AWT_UNLOCK();
    return PR_TRUE;
}

jobject PlugletViewMotif::GetJObject() {
    return frame;
}







