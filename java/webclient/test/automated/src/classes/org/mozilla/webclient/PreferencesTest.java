/*
 * $Id: PreferencesTest.java,v 1.1 2003/09/28 06:29:18 edburns%acm.org Exp $
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

package org.mozilla.webclient;

import junit.framework.TestSuite;
import junit.framework.Test;

import java.util.Properties;
import java.util.Iterator;

// PreferencesTest.java

public class PreferencesTest extends WebclientTestCase {

    public PreferencesTest(String name) {
	super(name);
    }

    public static Test suite() {
	return (new TestSuite(PreferencesTest.class));
    }

    //
    // Constants
    // 

    public static final String CLOSURE = "closure";
    public static final String PREFNAME = "webclient.pref";
    public static final String PREFVALUE = "webclient.value";

    //
    // Testcases
    // 

    public void testPreferences() throws Exception {
	Properties prefProperties = null;
	BrowserControl firstBrowserControl = null;
	PrefChangedCallback callback = new TestPrefChangedCallback();
	BrowserControlFactory.setAppData(getBrowserBinDir());
	firstBrowserControl = BrowserControlFactory.newBrowserControl();
	assertNotNull(firstBrowserControl);

	// test we can get the prefs
	Preferences prefs = (Preferences)
	    firstBrowserControl.queryInterface(BrowserControl.PREFERENCES_NAME);
	assertNotNull(prefs);

	// make sure we don't have the pref already
	prefProperties = prefs.getPrefs();
	assertNotNull(prefProperties);
	assertTrue(null == prefProperties.getProperty(PREFNAME));
	
	// test that we can set a preference
	prefs.setPref(PREFNAME, PREFVALUE);
	
	// test that the set value is actually there
	prefProperties = prefs.getPrefs();
	assertNotNull(prefProperties);
	
	// test that the set value is as expected
	assertTrue(prefProperties.getProperty(PREFNAME).equals(PREFVALUE));
	
	// test that we can register a prefChangedCallback
	System.setProperty(PREFNAME, "");
	prefs.registerPrefChangedCallback(callback, PREFNAME, CLOSURE);
	
	// change the value of the preference
	prefs.setPref(PREFNAME, "newValue");

	// test that our pref-change callback has been called.
	assertTrue(System.getProperty(PREFNAME).equals(CLOSURE));

	// test that we can successfully unregister our callback
	prefs.unregisterPrefChangedCallback(callback, PREFNAME, CLOSURE);

	// verify that it is no longer called
	System.setProperty(PREFNAME, "");
	prefs.setPref(PREFNAME, "newValue");

	// test that our pref-change callback has not been called.
	assertFalse(System.getProperty(PREFNAME).equals(CLOSURE));
	

	BrowserControlFactory.deleteBrowserControl(firstBrowserControl);
	BrowserControlFactory.appTerminate();
    }

    class TestPrefChangedCallback extends Object implements PrefChangedCallback {
	public int prefChanged(String prefName, Object closure) {
	    System.setProperty(prefName, closure.toString());
	    return 0;
	}
    }

}
