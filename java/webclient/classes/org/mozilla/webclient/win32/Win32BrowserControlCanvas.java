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

package org.mozilla.webclient.win32;

// Win32BrowserControlCanvas.java

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;

/**

 * Win32RaptorCanvas provides a concrete realization
 * of the RaptorCanvas.

 * <B>Lifetime And Scope</B> <P>

 * There is one instance of the BrowserControlCanvas per top level awt Frame.

 * @version $Id: Win32BrowserControlCanvas.java,v 1.1 1999/08/10 18:55:01 mark.lin%eng.sun.com Exp $
 * 
 * @see	org.mozilla.webclient.BrowserControlCanvasFactory
 * 

 */

import sun.awt.*;
import sun.awt.windows.*;

import org.mozilla.webclient.*;

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
