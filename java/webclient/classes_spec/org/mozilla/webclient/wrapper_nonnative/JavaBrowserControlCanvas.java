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

package org.mozilla.webclient.wrapper_nonnative;

// JavaBrowserControlCanvas.java

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;

import org.mozilla.webclient.BrowserControlCanvas;
import org.mozilla.webclient.BrowserControl;

/**

 * JavaBrowserControlCanvas provides a concrete realization of the
 * BrowserControlCanvas.  This is a no-op class in the java case.

 * <B>Lifetime And Scope</B> <P>

 * There is one instance of the BrowserControlCanvas per top level awt Frame.

 * @version $Id: JavaBrowserControlCanvas.java,v 1.1 2001/07/27 21:01:11 ashuk%eng.sun.com Exp $
 * 
 * @see	org.mozilla.webclient.BrowserControlCanvasFactory
 * 

 */


/**
 * JavaBrowserControlCanvas provides a concrete realization
 * of the BrowserControlCanvas.
 */

// PENDING(ashuk) remove JavaBrowserControlCanvas.

public class JavaBrowserControlCanvas extends BrowserControlCanvas {
    
protected int getWindow() 
{
    return 1;
}

protected void initialize(BrowserControl controlImpl)
{
    return;
}

}
