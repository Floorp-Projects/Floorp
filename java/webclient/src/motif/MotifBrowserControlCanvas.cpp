/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
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

#include "nsIDOMDocument.h"
#include <dlfcn.h>

extern "C" void NS_SetupRegistry();

extern "C" {

/*
 * Class:     org_mozilla_webclient_motif_MotifBrowserControlCanvas
 * Method:    createTopLevelWindow
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_webclient_motif_MotifBrowserControlCanvas_createTopLevelWindow
(JNIEnv * env, jobject obj) {
    static GtkWidget *window = NULL;
    NS_SetupRegistry();
    
    /* Initialise GTK */
    gtk_set_locale ();
    
    gtk_init (0, NULL);
    
    gdk_rgb_init();
    
    window = gtk_window_new (GTK_WINDOW_POPUP);
    gtk_window_set_default_size(GTK_WINDOW(window), 300, 300);
    gtk_window_set_title(GTK_WINDOW(window), "Simple browser");

    return (jint) window;
}

/*
 * Class:     org_mozilla_webclient_motif_MotifBrowserControlCanvas
 * Method:    createContainerWindow
 * Signature: (III)I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_webclient_motif_MotifBrowserControlCanvas_createContainerWindow
    (JNIEnv * env, jobject obj, jint parent, jint screenWidth, jint screenHeight) {
    GtkWidget * window = (GtkWidget *) parent;
    GtkWidget * vbox;
    GtkWidget * mozilla;
    
    vbox = gtk_vbox_new (FALSE, 5);
    gtk_container_add (GTK_CONTAINER (window), vbox);
    gtk_widget_show(vbox);
    
    mozilla = gtk_mozilla_new();
    gtk_box_pack_start (GTK_BOX (vbox), mozilla, TRUE, TRUE, 0);
    
    // HACK: javaMake sure this window doesn't appear onscreen!!!!
    gtk_widget_set_uposition(window, screenWidth + 20, screenHeight + 20);
    gtk_widget_show(mozilla);
    
    gtk_widget_show(window);
    
    //gtk_main();

    return (jint) mozilla;
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
JNIEXPORT jint JNICALL Java_org_mozilla_webclient_motif_MotifBrowserControlCanvas_getGTKWinID
(JNIEnv * env, jobject obj, jint gtkWinPtr) {
    GtkWidget * gtkWidgetPtr = (GtkWidget *) gtkWinPtr;

    return getWinID(gtkWidgetPtr);
}


/*
 * Class:     org_mozilla_webclient_motif_MotifBrowserControlCanvas
 * Method:    reparentWindow
 * Signature: (II)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_motif_MotifBrowserControlCanvas_reparentWindow (JNIEnv * env, jobject obj, jint childID, jint parentID) {
    XReparentWindow(GDK_DISPLAY(), childID, parentID, 0, 0);
}

/*
 * Class:     org_mozilla_webclient_motif_MotifBrowserControlCanvas
 * Method:    processEvents
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_motif_MotifBrowserControlCanvas_processEvents
    (JNIEnv * env, jobject obj) {
    //printf("process events....\n");
    //processEventLoopIntelligently();
}

/*
 * Class:     org_mozilla_webclient_motif_MotifBrowserControlCanvas
 * Method:    setGTKWindowSize
 * Signature: (III)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_motif_MotifBrowserControlCanvas_setGTKWindowSize
    (JNIEnv * env, jobject obj, jint gtkWinPtr, jint width, jint height) {
#ifdef DEBUG_RAPTOR_CANVAS
    printf("set gtk window size....width=%i, height=%i\n", width, height);
#endif
    if (gtkWinPtr != 0) {
        GtkWidget * gtkWidgetPtr = (GtkWidget *) gtkWinPtr;

        if (gtkWidgetPtr) {
            printf("size is being set...\n");
            gtk_widget_set_usize(gtkWidgetPtr, width, height);
        }
    }
}


} // End extern "C"




