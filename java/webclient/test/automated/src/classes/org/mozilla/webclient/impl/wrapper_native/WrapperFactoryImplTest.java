/*
 * $Id: WrapperFactoryImplTest.java,v 1.1 2003/09/28 06:29:20 edburns%acm.org Exp $
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
 * Contributor(s): Ed Burns &lt;edburns@acm.org&gt;
 */

package org.mozilla.webclient.impl.wrapper_native;

import junit.framework.TestSuite;
import junit.framework.Test;

import org.mozilla.webclient.BrowserControl;
import org.mozilla.webclient.WebclientTestCase;

// WrapperFactoryImplTest.java

public class WrapperFactoryImplTest extends WebclientTestCase {

    public WrapperFactoryImplTest(String name) {
	super(name);
    }

    public static Test suite() {
	return (new TestSuite(WrapperFactoryImplTest.class));
    }

    //
    // Testcases
    // 

    protected String [] expectedInterfaces = {
	BrowserControl.WINDOW_CONTROL_NAME,
	BrowserControl.NAVIGATION_NAME,
	BrowserControl.HISTORY_NAME,
	BrowserControl.EVENT_REGISTRATION_NAME,
	BrowserControl.BOOKMARKS_NAME
    };
	

    public void testNativeDoesImplement() throws Exception {
	WrapperFactoryImpl wrapper = new WrapperFactoryImpl();
	wrapper.initialize(getBrowserBinDir());
	for (int i = 0, len = expectedInterfaces.length; i < len; i++) {
	    assertTrue(wrapper.nativeDoesImplement(expectedInterfaces[i]));
	}
    }

}
