/*
 * $Id: ProfileManagerTest.java,v 1.1 2003/09/28 06:29:18 edburns%acm.org Exp $
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

// ProfileManagerTest.java

public class ProfileManagerTest extends WebclientTestCase {

    public ProfileManagerTest(String name) {
	super(name);
    }

    public static Test suite() {
	return (new TestSuite(ProfileManagerTest.class));
    }

    //
    // Testcases
    // 

    public void testProfile() throws Exception {
	int 
	    i = 0,
	    len = 0;
	final String 
	    name = "testProfile",
	    newName = "testProfile2";
	BrowserControl firstBrowserControl = null;
	BrowserControlFactory.setAppData(getBrowserBinDir());
	firstBrowserControl = BrowserControlFactory.newBrowserControl();
	assertNotNull(firstBrowserControl);

	// test we can get the profileManager
	ProfileManager profileManager = (ProfileManager)
	    firstBrowserControl.queryInterface(BrowserControl.PROFILE_MANAGER_NAME);
	assertNotNull(profileManager);
	
	// create a new profile
	profileManager.createNewProfile(name, null, null, false);
	
	// test that we have at least one profile
	int profileCount = profileManager.getProfileCount();
	assertTrue(1 <= profileCount);
	
	// test that the new profile exists
	assertTrue(profileManager.profileExists(name));
	
	// test that we can set the current profile to the new profile
	profileManager.setCurrentProfile(name);
	
	// test that the current profile is the new profile
	String currentProfile = profileManager.getCurrentProfile();
	assertTrue(currentProfile.equals(name));
	
	// test that the list of profiles contains the new profile
	String [] profiles = profileManager.getProfileList();
	assertNotNull(profiles);
	boolean hasExpectedProfile = false;
	for (i = 0, len = profiles.length; i < len; i++) {
	    if (profiles[i].equals(name)) {
		hasExpectedProfile = true;
	    }
	}
	assertTrue(hasExpectedProfile);

	// test that you can rename the newly created profile
	profileManager.renameProfile(name, newName);

	// test that the new profile exists
	assertTrue(profileManager.profileExists(newName));
	profiles = profileManager.getProfileList();
	assertNotNull(profiles);
	hasExpectedProfile = false;
	for (i = 0, len = profiles.length; i < len; i++) {
	    if (profiles[i].equals(newName)) {
		hasExpectedProfile = true;
	    }
	}
	assertTrue(hasExpectedProfile);

	// test that the old name doesn't exist
	assertTrue(!profileManager.profileExists(name));
	profiles = profileManager.getProfileList();
	assertNotNull(profiles);
	hasExpectedProfile = false;
	for (i = 0, len = profiles.length; i < len; i++) {
	    if (profiles[i].equals(name)) {
		hasExpectedProfile = true;
	    }
	}
	assertTrue(!hasExpectedProfile);

	// test that we can delete the new profile
	profileManager.deleteProfile(newName, true);

	// test that the new name doesn't exist
	assertTrue(!profileManager.profileExists(newName));
	profiles = profileManager.getProfileList();
	assertNotNull(profiles);
	hasExpectedProfile = false;
	for (i = 0, len = profiles.length; i < len; i++) {
	    if (profiles[i].equals(newName)) {
		hasExpectedProfile = true;
	    }
	}
	assertTrue(!hasExpectedProfile);
	

	BrowserControlFactory.deleteBrowserControl(firstBrowserControl);
	BrowserControlFactory.appTerminate();

    }

}
