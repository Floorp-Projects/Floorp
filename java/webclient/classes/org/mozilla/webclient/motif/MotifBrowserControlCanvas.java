/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 */

package org.mozilla.webclient.motif;

// MotifBrowserControlCanvas.java

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;

/**

 * MotifBrowserControlCanvas provides a concrete realization
 * of the RaptorCanvas for Motif.

 * <B>Lifetime And Scope</B> <P>

 * There is one instance of the WebShellCanvas per top level awt Frame.

 * @version $Id: MotifBrowserControlCanvas.java,v 1.1 1999/08/10 18:55:00 mark.lin%eng.sun.com Exp $
 * 
 * @see	org.mozilla.webclient.BrowserControlCanvasFactory
 * 

 */

import sun.awt.*;
import sun.awt.motif.*;
import java.awt.*;

import org.mozilla.webclient.*;

/**
 * MotifBrowserControlCanvas provides a concrete realization
 * of the RaptorCanvas.
 */
public class MotifBrowserControlCanvas extends BrowserControlCanvas /* implements ActionListener*/  {

    static {
        System.loadLibrary("webclientstub");
    }
    
    static private boolean firstTime = true;
    private int gtkWinID;
    private int gtkTopWindow;
    private int canvasWinID;
    private int gtkWinPtr;
    private MDrawingSurfaceInfo drawingSurfaceInfo;

    private native int createTopLevelWindow();
    private native int createContainerWindow(int parent, int width, int height);
    private native int getGTKWinID(int gtkWinPtr);
    private native void reparentWindow(int child, int parent);
    private native void processEvents();
    private native void setGTKWindowSize(int gtkWinPtr, int width, int height);

    public MotifBrowserControlCanvas() {
        super();
        
        this.gtkWinID = 0;
        this.canvasWinID = 0;
        this.gtkWinPtr = 0;
        this.drawingSurfaceInfo = null;
    }

    /*
    public void actionPerformed(ActionEvent e) {
        System.out.println("process events....\n");
        this.processEvents();
    }
    */

    public void paint(Graphics g) {
        super.paint(g);
        
        if (firstTime) {
            synchronized(getTreeLock()) {
                canvasWinID = this.drawingSurfaceInfo.getDrawable();
                this.reparentWindow(this.gtkWinID, this.canvasWinID);

                //Timer timer = new Timer(1, this);
                //timer.start();

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
	 * Obtain the native window handle for this
	 * component's peer.
	 *
	 * @returns The native window handle. 
	 */
	protected int getWindow(DrawingSurfaceInfo dsi) {
        synchronized(getTreeLock()) {
            this.drawingSurfaceInfo = (MDrawingSurfaceInfo) dsi;

            this.gtkTopWindow = this.createTopLevelWindow();
            Dimension screenSize = Toolkit.getDefaultToolkit().getScreenSize();
            
            this.gtkWinPtr = 
                this.createContainerWindow(this.gtkTopWindow, screenSize.width,
                                           screenSize.height);
            
            this.gtkWinID = this.getGTKWinID(gtkWinPtr);
        }

		return this.gtkWinPtr;
	}
}

