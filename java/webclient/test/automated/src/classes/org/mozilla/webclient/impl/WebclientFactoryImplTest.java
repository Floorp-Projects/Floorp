/*
 * $Id: WebclientFactoryImplTest.java,v 1.1 2003/09/28 06:29:19 edburns%acm.org Exp $
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

package org.mozilla.webclient.impl;

import junit.framework.TestSuite;
import junit.framework.Test;

import org.mozilla.webclient.WebclientTestCase;

// WebclientFactoryImplTest.java

public class WebclientFactoryImplTest extends WebclientTestCase {

    public WebclientFactoryImplTest(String name) {
	super(name);
    }

    public static Test suite() {
	return (new TestSuite(WebclientFactoryImplTest.class));
    }

    //
    // Testcases
    // 

    public void testWrapperFactoryLocator() throws Exception {
	WebclientFactoryImpl impl = new WebclientFactoryImpl();
	WrapperFactory wrapper = impl.getWrapperFactory();
	assertNotNull(wrapper);
    }

}
