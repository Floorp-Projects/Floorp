/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun
 * Microsystems, Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
package org.mozilla.webclient.wrapper_native.motif;


/**
 * This is NativeEventThread. It's job is to process any pending mozilla events
 * which have been queued up.
 * 
 * It also contains a private static hashtable mapping of GTK window to 
 * WebShellInitContexts (see BrowserControllMozillaShim.cpp). It needs this
 * because in order to process any GTK / Mozilla events, it needs a pointer
 * to the native event queue, the native web shell, as well as native pointers
 * to other things which it can't readily access.
 * So what we do is when the WebShellInitContext for a GTK Window first gets 
 * created, it gets stored in the webShellContextMapping hashtable below. Then 
 * we can now easily lookup the appropriate WebShellInitContext when it is needed.
 * 
 * <B>Lifetime And Scope</B> <P>
 * There will be one of these per BrowserControlCanvas (but hasn't been tested yet)
 *
 * @version $Id: NativeEventThread.java,v 1.1 2000/03/04 01:10:57 edburns%acm.org Exp $
 * @see	org.mozilla.webclient.motif.NativeEventThread
 * 
 */

import java.util.*;

public class NativeEventThread extends Thread {

    static private int eventQueueCount = 0;
    // A mapping of gtkWindowID <-> WebShellInitContext structs
    static private Hashtable webShellContextMapping = null;

    protected MotifBrowserControlCanvas browserCanvas;

    // Calling this will process any GTK / Mozilla events which have been pending
    public native void processNativeEventQueue(int nativeWebShellContext);
    
    public static void storeContext(int gtkWinID, int nativeWebShellContext) {
        if (webShellContextMapping == null) {
            webShellContextMapping = new Hashtable();
        }

        webShellContextMapping.put(new Integer(gtkWinID), new Integer(nativeWebShellContext));
    }

    public static int getContext(int gtkWinID) {
        Integer result = (Integer) webShellContextMapping.get(new Integer(gtkWinID));

        if (result != null) {
            return result.intValue();
        }
 
        System.out.println("Requested invalid NativeWebShellInitContext!\n");
        return 0;
    }

    public NativeEventThread(MotifBrowserControlCanvas canvas) {
        super("Mozilla-Event-Thread-" + eventQueueCount);

        this.browserCanvas = canvas;
        eventQueueCount++;
        this.setPriority(Thread.MAX_PRIORITY);
    }

    public void run() {
        // Infinite loop to eternity.
	    while (true) {
            // PENDING(mark): Do we need to yield?
            // Thread.currentThread().yield();

            synchronized(this.browserCanvas.getTreeLock()) {
                this.processNativeEventQueue(getContext(this.browserCanvas.getGTKWinPtr()));
            }
        }
    }

}
