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
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s):  Ed Burns <edburns@acm.org>
 */

package org.mozilla.webclient.wrapper_native;

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;

import org.mozilla.webclient.BrowserControl;
import org.mozilla.webclient.WrapperFactory;
import org.mozilla.webclient.Preferences;
import org.mozilla.webclient.PrefChangedCallback;

import java.util.Properties;

import org.mozilla.webclient.UnimplementedException; 

public class PreferencesImpl extends ImplObjectNative implements Preferences
{
//
// Constants
//

//
// Class Variables
//

//
// Instance Variables
//

// Attribute Instance Variables

// Relationship Instance Variables

private Properties props;

//
// Constructors and Initializers    
//

public PreferencesImpl(WrapperFactory yourFactory, 
                     BrowserControl yourBrowserControl)
{
    super(yourFactory, yourBrowserControl);
    props = null;
}

/**

 * Since this class is a singleton, we don't expect this method to be
 * called until the app is done with bookmarks for a considerable amount
 * of time.

// PENDING(): Write test case to see that a cycle of Preferences
// allocation/destruction/new instance allocation works correctly.
 
 */

public void delete()
{
}

//
// Class methods
//

//
// General Methods
//

//
// Methods from Preferences    
//

public void setPref(String prefName, String prefValue)
{
    if (null == prefName) {
        return;
    }
    if (null == prefValue) {
        return;
    }
    // determine the type of pref value: String, boolean, integer
    try {
        Integer intVal = Integer.valueOf(prefValue);
        nativeSetIntPref(nativeWebShell, prefName, intVal.intValue());
    }
    catch (NumberFormatException e) {
        // it's not an integer
        if (prefValue.equals("true") || prefValue.equals("false")) {
            Boolean boolVal = Boolean.valueOf(prefValue);
            nativeSetBoolPref(nativeWebShell, prefName, 
                              boolVal.booleanValue());
        }
        else {
            // it must be a string
            nativeSetUnicharPref(nativeWebShell, prefName, prefValue);
        }
    }
}
 
public Properties getPrefs()
{
    props = nativeGetPrefs(nativeWebShell, props);
    return props;
}

public void registerPrefChangedCallback(PrefChangedCallback cb,
                                       String prefName, Object closure)
{
    ParameterCheck.nonNull(cb);
    ParameterCheck.nonNull(prefName);
    
    nativeRegisterPrefChangedCallback(nativeWebShell, cb, prefName, closure);
}


//
// native methods
//

public native void nativeSetUnicharPref(int nativeWebShell, 
                                        String prefName, String prefValue);
public native void nativeSetIntPref(int nativeWebShell, 
                                    String prefName, int prefValue);
public native void nativeSetBoolPref(int nativeWebShell, 
                                     String prefName, boolean prefValue);
public native Properties nativeGetPrefs(int nativeWebShell, Properties props);
public native void nativeRegisterPrefChangedCallback(int nativeWebShell, 
                                                     PrefChangedCallback cb,
                                                     String prefName,
                                                     Object closure);

// ----VERTIGO_TEST_START

//
// Test methods
//

public static void main(String [] args)
{
    Assert.setEnabled(true);

    Log.setApplicationName("PreferencesImpl");
    Log.setApplicationVersion("0.0");
    Log.setApplicationVersionDate("$Id: PreferencesImpl.java,v 1.2 2001/04/02 21:13:59 ashuk%eng.sun.com Exp $");

    try {
        org.mozilla.webclient.BrowserControlFactory.setAppData(args[0]);
	org.mozilla.webclient.BrowserControl control = 
	    org.mozilla.webclient.BrowserControlFactory.newBrowserControl();
        Assert.assert(control != null);
	
	Preferences wc = (Preferences)
	    control.queryInterface(org.mozilla.webclient.BrowserControl.WINDOW_CONTROL_NAME);
	Assert.assert(wc != null);
    }
    catch (Exception e) {
	System.out.println("got exception: " + e.getMessage());
    }
}

// ----VERTIGO_TEST_END

} // end of class PreferencesImpl
