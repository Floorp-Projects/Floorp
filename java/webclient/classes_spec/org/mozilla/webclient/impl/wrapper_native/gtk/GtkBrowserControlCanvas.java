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
package org.mozilla.webclient.impl.wrapper_native.gtk;

// GtkBrowserControlCanvas.java

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;

import org.mozilla.webclient.BrowserControlCanvas;

import java.awt.Graphics;
import java.awt.Toolkit;
import java.awt.Dimension;

/**

 * GtkBrowserControlCanvas provides a concrete realization
 * of the RaptorCanvas for Gtk.

 * <B>Lifetime And Scope</B> <P>

 * There is one instance of GtkBrowserControlCanvas per top level awt Frame.

 * @version $Id: GtkBrowserControlCanvas.java,v 1.1 2003/09/28 06:29:09 edburns%acm.org Exp $
 * 
 * @see	org.mozilla.webclient.BrowserControlCanvasFactory
 * 

 */


public class GtkBrowserControlCanvas extends BrowserControlCanvas /* implements ActionListener*/  {

    private boolean firstTime = true;
    private int gtkWinID;
    private int gtkTopWindow;
    private int canvasWinID;
    private int gtkWinPtr;
    // We dont need this, now that we use the JAWT Native Interface
    //    private MDrawingSurfaceInfo drawingSurfaceInfo;

    native int createTopLevelWindow();
    native int createContainerWindow(int parent, int width, int height);
    native int getGTKWinID(int gtkWinPtr);
    native void reparentWindow(int child, int parent);
    native void processEvents();
    native void setGTKWindowSize(int gtkWinPtr, int width, int height);
    //New method for obtaining access to the Native Peer handle
    native int getHandleToPeer();

    public GtkBrowserControlCanvas() {
        super();
        
        this.gtkWinID = 0;
        this.canvasWinID = 0;
        this.gtkWinPtr = 0;
        // We dont need this, now that we use the JAWT Native Interface
        //        this.drawingSurfaceInfo = null;

    }

    public void paint(Graphics g) {
        super.paint(g);
        
        if (firstTime) {
            synchronized(getTreeLock()) {
                //Use the AWT Native Peer interface to get the handle
                //of this Canvas's native peer
                canvasWinID = this.getHandleToPeer();
                //Set our canvas as a parent of the top-level gtk widget
                //which contains Mozilla.
                this.reparentWindow(this.gtkWinID, this.canvasWinID);
                firstTime = false;
            }
        }
    }

    public void setBounds(int x, int y, int width, int height) {
        super.setBounds(x, y, width, height);

        synchronized(getTreeLock()) {
            this.setGTKWindowSize(this.gtkTopWindow, width, height);
        }
    }

    /**
     * Needed for the hashtable look up of gtkwinid <-> WebShellInitContexts
     */
    public int getGTKWinPtr() {
        return this.gtkWinPtr;
    }

	/**
	 * Create the top-level gtk window to be embedded in our AWT
     * Window and return a handle to this window
	 *
	 * @returns The native window handle. 
	 */
    
	protected int getWindow() {
        synchronized(getTreeLock()) {
            this.gtkTopWindow = this.createTopLevelWindow();
            Dimension screenSize = Toolkit.getDefaultToolkit().getScreenSize();
            
            this.gtkWinPtr = 
                this.createContainerWindow(this.gtkTopWindow, screenSize.width,
                                           screenSize.height);
            
            this.gtkWinID = this.getGTKWinID(gtkWinPtr);

        }

		return this.gtkWinPtr;
	}

    // The test for this class is TestGtkBrowserControlCanvas
    
}

