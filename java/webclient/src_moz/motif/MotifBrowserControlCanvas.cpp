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
 * Contributor(s): 
 */


/*
 * MotifBrowserControlCanvas.cpp
 */

#include <jni.h>
#include "MotifBrowserControlCanvas.h"

#include <X11/Xlib.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include "gtkmozilla.h"
#include "gtkmozarea.h"

#include "nsIDOMDocument.h"
#include "nsGtkEventHandler.h"

#include <dlfcn.h>

extern "C" void NS_SetupRegistry();

extern "C" {

/*
 * Class:     org_mozilla_webclient_motif_MotifBrowserControlCanvas
 * Method:    createTopLevelWindow
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_webclient_wrapper_1native_motif_MotifBrowserControlCanvas_createTopLevelWindow
(JNIEnv * env, jobject obj) {
    static GtkWidget *mShell = NULL;

    /* Initialise GTK */
    gtk_set_locale ();
    
    gtk_init (0, NULL);

    gdk_event_handler_set (handle_gdk_event, NULL, NULL);

    gdk_rgb_init();
    
    mShell = gtk_window_new (GTK_WINDOW_POPUP);
    gtk_window_set_default_size(GTK_WINDOW(mShell), 300, 300);
    gtk_window_set_title(GTK_WINDOW(mShell), "Simple browser");
    
    return (jint) mShell;
}

/*
 * Class:     org_mozilla_webclient_motif_MotifBrowserControlCanvas
 * Method:    createContainerWindow
 * Signature: (III)I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_webclient_wrapper_1native_motif_MotifBrowserControlCanvas_createContainerWindow
    (JNIEnv * env, jobject obj, jint parent, jint screenWidth, jint screenHeight) {
	
   GtkWidget * window = (GtkWidget *) parent;

   gtk_widget_realize(GTK_WIDGET(window));
   gtk_widget_set_uposition(window, screenWidth + 20, screenHeight + 20);
   gtk_widget_show(window);

   return (jint) window;

}

int getWinID(GtkWidget * gtkWidgetPtr) {
    //GdkWindow * gdkWindow = gtk_widget_get_parent_window(gtkWidgetPtr);
    GdkWindow * gdkWindow = gtkWidgetPtr->window;
    int gtkwinid = GDK_WINDOW_XWINDOW(gdkWindow);
    
    return gtkwinid;
}

/*
 * Class:     org_mozilla_webclient_motif_MotifBrowserControlCanvas
 * Method:    getGTKWinID
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_webclient_wrapper_1native_motif_MotifBrowserControlCanvas_getGTKWinID
(JNIEnv * env, jobject obj, jint gtkWinPtr) {
    GtkWidget * gtkWidgetPtr = (GtkWidget *) gtkWinPtr;

    return getWinID(gtkWidgetPtr);
}


/*
 * Class:     org_mozilla_webclient_motif_MotifBrowserControlCanvas
 * Method:    reparentWindow
 * Signature: (II)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_motif_MotifBrowserControlCanvas_reparentWindow (JNIEnv * env, jobject obj, jint childID, jint parentID) {
    XReparentWindow(GDK_DISPLAY(), childID, parentID, 0, 0);
}

/*
 * Class:     org_mozilla_webclient_motif_MotifBrowserControlCanvas
 * Method:    processEvents
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_motif_MotifBrowserControlCanvas_processEvents
    (JNIEnv * env, jobject obj) {
    //printf("process events....\n");
    //processEventLoopIntelligently();
}

/*
 * Class:     org_mozilla_webclient_motif_MotifBrowserControlCanvas
 * Method:    setGTKWindowSize
 * Signature: (III)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_motif_MotifBrowserControlCanvas_setGTKWindowSize
    (JNIEnv * env, jobject obj, jint gtkWinPtr, jint width, jint height) {
    if (gtkWinPtr != 0) {
        GtkWidget * gtkWidgetPtr = (GtkWidget *) gtkWinPtr;

        if (gtkWidgetPtr) {
            gtk_widget_set_usize(gtkWidgetPtr, width, height);
        }
    }
}

/*
 * Class:     org_mozilla_webclient_motif_MotifBrowserControlCanvas
 * Method:    loadMainDll
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_motif_MotifBrowserControlCanvas_loadMainDll
  (JNIEnv *, jclass)
{
    printf("incorrect loadMainDll called\n\n");
    fflush(stdout);
}



} // End extern "C"

