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

package org.mozilla.webclient.wrapper_native.win32;

// Win32BrowserControlCanvas.java

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;

/**

 * Win32RaptorCanvas provides a concrete realization
 * of the RaptorCanvas.

 * <B>Lifetime And Scope</B> <P>

 * There is one instance of the BrowserControlCanvas per top level awt Frame.

 * @version $Id: Win32BrowserControlCanvas.java,v 1.1 2000/03/04 01:10:58 edburns%acm.org Exp $
 * 
 * @see	org.mozilla.webclient.BrowserControlCanvasFactory
 * 

 */

import sun.awt.*;
import sun.awt.windows.*;

import org.mozilla.webclient.BrowserControlCanvas;

/**
 * Win32BrowserControlCanvas provides a concrete realization
 * of the RaptorCanvas.
 */
public class Win32BrowserControlCanvas extends BrowserControlCanvas {

    static {
        System.loadLibrary("webclient");
    }


	/**
	 * Obtain the native window handle for this
	 * component's peer.
	 *
	 * @returns The native window handle. 
	 */
	protected int getWindow(DrawingSurfaceInfo dsi) {
		WDrawingSurfaceInfo ds = (WDrawingSurfaceInfo)dsi;
		return ds.getHWnd();
	}
}
