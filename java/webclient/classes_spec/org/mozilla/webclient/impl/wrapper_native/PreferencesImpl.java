/* 
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

package org.mozilla.webclient.impl.wrapper_native;

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;
import org.mozilla.util.ReturnRunnable;

import org.mozilla.webclient.impl.WrapperFactory;
import org.mozilla.webclient.impl.Service;
import org.mozilla.webclient.Preferences;
import org.mozilla.webclient.PrefChangedCallback;

import java.util.Properties;
import java.util.Iterator;

import org.mozilla.webclient.UnimplementedException; 

public class PreferencesImpl extends ImplObjectNative implements Preferences, Service
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

public PreferencesImpl(WrapperFactory yourFactory)
{
    super(yourFactory, null);
    props = null;
}

public void startup() {
    nativeStartup(getWrapperFactory().getNativeWrapperFactory());
}

public void shutdown() {
    nativeShutdown(getWrapperFactory().getNativeWrapperFactory());
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
    final String finalName = new String(prefName);
    // determine the type of pref value: String, boolean, integer
    try {
        final Integer intVal = Integer.valueOf(prefValue);
        NativeEventThread.instance.pushBlockingReturnRunnable(new ReturnRunnable() {
		public Object run() {
		    nativeSetIntPref(getWrapperFactory().getNativeWrapperFactory(), finalName, intVal.intValue());
		    return null;
		}
                public String toString() {
                    return "WCRunnable.nativeSetIntPref";
                }
	    });
    }
    catch (NumberFormatException e) {
        // it's not an integer
        if (null != prefValue &&
            (prefValue.equals("true") || prefValue.equals("false"))) {
            final Boolean boolVal = Boolean.valueOf(prefValue);
	    NativeEventThread.instance.pushBlockingReturnRunnable(new ReturnRunnable(){
		    public Object run() {
			nativeSetBoolPref(getWrapperFactory().getNativeWrapperFactory(), 
					  finalName, boolVal.booleanValue());
			return null;
		    }
                public String toString() {
                    return "WCRunnable.nativeSetBoolPref";
                }
                    
		});
        }
        else {
            // it must be a string
	    final String finalValue = (null != prefValue) ? 
		new String(prefValue) : null;
	    NativeEventThread.instance.pushBlockingReturnRunnable(new ReturnRunnable(){
		    public Object run() {
			nativeSetUnicharPref(getWrapperFactory().getNativeWrapperFactory(), finalName, finalValue);
			return null;
		    }
                    public String toString() {
                        return "WCRunnable.nativeSetUnicharPref";
                    }

		});
	}
    }
}
 
public Properties getPrefs()
{
    props = (Properties)
	NativeEventThread.instance.pushBlockingReturnRunnable(new ReturnRunnable() {
		public Object run() {
		    Properties result = 
			nativeGetPrefs(getWrapperFactory().getNativeWrapperFactory(), 
				       PreferencesImpl.this.props);
		    return result;
		}
                public String toString() {
                    return "WCRunnable.nativeGetPrefs";
                }
                
	    });
    
    return props;
}

public void registerPrefChangedCallback(PrefChangedCallback cb,
                                       String prefName, Object closure)
{
    ParameterCheck.nonNull(cb);
    ParameterCheck.nonNull(prefName);
    
    nativeRegisterPrefChangedCallback(getWrapperFactory().getNativeWrapperFactory(), cb, prefName, closure);
}

public void unregisterPrefChangedCallback(PrefChangedCallback cb,
                                          String prefName, Object closure)
{
    ParameterCheck.nonNull(cb);
    ParameterCheck.nonNull(prefName);
    
    nativeUnregisterPrefChangedCallback(getWrapperFactory().getNativeWrapperFactory(), cb, prefName, closure);
}



//
// native methods
//

native void nativeStartup(int nativeContext);

native void nativeShutdown(int nativeContext);

native void nativeSetUnicharPref(int nativeContext, 
                                 String prefName, String prefValue);
native void nativeSetIntPref(int nativeContext, 
                             String prefName, int prefValue);
native void nativeSetBoolPref(int nativeContext, 
                              String prefName, boolean prefValue);
native Properties nativeGetPrefs(int nativeContext, Properties props);
native void nativeRegisterPrefChangedCallback(int nativeContext, 
                                              PrefChangedCallback cb,
                                              String prefName,
                                              Object closure);
native void nativeUnregisterPrefChangedCallback(int nativeContext, 
                                                PrefChangedCallback cb,
                                                String prefName,
                                                Object closure);

} // end of class PreferencesImpl
