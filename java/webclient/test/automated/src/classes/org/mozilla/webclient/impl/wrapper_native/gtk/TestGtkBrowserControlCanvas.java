/*
 * $Id: TestGtkBrowserControlCanvas.java,v 1.1 2003/09/28 06:29:21 edburns%acm.org Exp $
 */

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

// TestGtkBrowserControlCanvas.java

import org.mozilla.util.Assert;
import org.mozilla.util.ParameterCheck;

import org.mozilla.webclient.WebclientTestCase;

/**
 *
 *  <B>TestGtkBrowserControlCanvas</B> is a class ...
 *
 * <B>Lifetime And Scope</B> <P>
 *
 * @version $Id: TestGtkBrowserControlCanvas.java,v 1.1 2003/09/28 06:29:21 edburns%acm.org Exp $
 * 
 * @see	Blah
 * @see	Bloo
 *
 */

public class TestGtkBrowserControlCanvas extends WebclientTestCase
{
//
// Protected Constants
//

    public static final int WIDTH = 400;
    public static final int HEIGHT = WIDTH;

//
// Class Variables
//

//
// Instance Variables
//

// Attribute Instance Variables

// Relationship Instance Variables

//
// Constructors and Initializers    
//

public TestGtkBrowserControlCanvas()
{
    super("TestGtkBrowserControlCanvas");
}

public TestGtkBrowserControlCanvas(String name)
{
    super(name);
}

//
// Class methods
//

// 
// Methods from WebclientTestCase
//

public String getExpectedOutputFilename() { return "TestGtkBrowserControlCanvas_correct"; }

public boolean sendOutputToFile() { return true; }

//
// General Methods
//

public void testLoadMainDll() {
    GtkBrowserControlCanvas gtkCanvas = new GtkBrowserControlCanvas();
    assertTrue(verifyExpectedOutput());
}

} // end of class TestGtkBrowserControlCanvas
