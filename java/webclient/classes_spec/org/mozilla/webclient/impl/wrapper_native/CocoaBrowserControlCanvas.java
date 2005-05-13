/*
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
 * Contributor(s): edburns &lt;edburns@acm.org&gt;
 */
/*
 * CocoaBrowserControlCanvas.java
 *
 * Created on May 10, 2005, 8:59 PM
 */

package org.mozilla.webclient.impl.wrapper_native;

import org.mozilla.webclient.BrowserControlCanvas;

import org.mozilla.webclient.impl.wrapper_native.WCRunnable;
import org.mozilla.webclient.impl.wrapper_native.NativeEventThread;

import java.awt.Graphics;

/**
 *
 * @author edburns
 */
public class CocoaBrowserControlCanvas extends BrowserControlCanvas {
    
    /** Creates a new instance of CocoaBrowserControlCanvas */
    public CocoaBrowserControlCanvas() {
    }
    
    //New method for obtaining access to the Native Peer handle
    private native int getHandleToPeer(Graphics graphics);
    
    private int nativeWindow = -1;


	/**
	 * Obtain the native window handle for this
	 * component's peer.
	 *
	 * @returns The native window handle. 
	 */
    protected int getWindow() {
        WCRunnable runner = new WCRunnable() {
            public Object run() {
                Integer result =
                        new Integer(CocoaBrowserControlCanvas.this.getHandleToPeer(null));
                return result;
            }
        };
        Integer result = null;
             
        this.setVisible(true);
        repaint();
        while (-1 == nativeWindow) {
            result = (Integer) NativeEventThread.instance.pushBlockingWCRunnable(runner);
            nativeWindow = result.intValue();
        }
        return nativeWindow;
    }
    
    public void paint(Graphics graphics) {
        nativeWindow = getHandleToPeer(graphics);
    }
    
    
}
