/* 
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
 * Contributor(s):  Ashu Kulkarni <ashuk@eng.sun.com>
 *                  Ed Burns <edburns@acm.org>
 */

package org.mozilla.webclient.impl.wrapper_native;

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;
import org.mozilla.util.ReturnRunnable;

import org.mozilla.webclient.ProfileManager;
import org.mozilla.webclient.impl.WrapperFactory;
import org.mozilla.webclient.impl.Service;

import org.mozilla.webclient.UnimplementedException; 


public class ProfileManagerImpl extends ImplObjectNative implements ProfileManager, Service
{

// local variables

public ProfileManagerImpl(WrapperFactory yourFactory)
{
    super(yourFactory, null);
    
}

public void startup() {
    Assert.assert_it(isNativeEventThread());
    nativeStartup(getWrapperFactory().getNativeWrapperFactory(), null, 
		  getWrapperFactory().getProfile());
}

public void shutdown() {
    Assert.assert_it(isNativeEventThread());
    nativeShutdown(getWrapperFactory().getNativeWrapperFactory());
}

public int getProfileCount()
{
    Integer result = (Integer)
	NativeEventThread.instance.pushBlockingReturnRunnable(new ReturnRunnable() {
		public Object run() {
		    Integer count = new Integer(nativeGetProfileCount(getWrapperFactory().getNativeWrapperFactory()));
		    return count;
		}
	    });
    return result.intValue();
}

public String [] getProfileList()
{
    String [] list = 
	(String []) NativeEventThread.instance.pushBlockingReturnRunnable(new ReturnRunnable() {
		public Object run() {
		    Object result = nativeGetProfileList(getWrapperFactory().getNativeWrapperFactory());
		    return result;
		}
	    });
    return list;
}

public boolean profileExists(String profileName)
{
    ParameterCheck.nonNull(profileName);
    final String finalStr = new String(profileName);
    Boolean exists = (Boolean) NativeEventThread.instance.pushBlockingReturnRunnable(new ReturnRunnable() {
	    public Object run() {
		Boolean result = new Boolean(nativeProfileExists(getWrapperFactory().getNativeWrapperFactory(),
								 finalStr));
		return result;
	    }
	});
    return exists.booleanValue();
}

public String getCurrentProfile()
{
    String currProfile = (String)
	NativeEventThread.instance.pushBlockingReturnRunnable(new ReturnRunnable() {
	    public Object run() {
		Object result = nativeGetCurrentProfile(getWrapperFactory().getNativeWrapperFactory());
		return result;
	    }
	});
    return currProfile;
}

public void setCurrentProfile(String profileName)
{
    ParameterCheck.nonNull(profileName);
    final String finalStr = new String(profileName);
    NativeEventThread.instance.pushBlockingReturnRunnable(new ReturnRunnable() {
	    public Object run() {
		nativeSetCurrentProfile(getWrapperFactory().getNativeWrapperFactory(),
					finalStr);
		return null;
	    }
	});
}

public void createNewProfile(String profileName, 
                             String nativeProfileDir, 
                             String langcode, 
                             boolean useExistingDir)
{
    ParameterCheck.nonNull(profileName);
    final String finalProfileName = new String(profileName);
    final String finalProfileDir = (null != nativeProfileDir) ?
	new String(nativeProfileDir) : null;
    final String finalLangcode = (null != langcode) ? 
	new String(langcode) : null;
    final boolean finalExistingDir = useExistingDir;
    NativeEventThread.instance.pushBlockingReturnRunnable(new ReturnRunnable() {
	    public Object run() {
		nativeCreateNewProfile(getWrapperFactory().getNativeWrapperFactory(),
				       finalProfileName, finalProfileDir, 
				       finalLangcode, finalExistingDir);
		return null;
	    }
	});
}

public void renameProfile(String currName, String newName)
{
    ParameterCheck.nonNull(currName);
    ParameterCheck.nonNull(newName);
    final String finalCurrName = new String(currName);
    final String finalNewName = new String(newName);
    NativeEventThread.instance.pushBlockingReturnRunnable(new ReturnRunnable() {
	    public Object run() {
		nativeRenameProfile(getWrapperFactory().getNativeWrapperFactory(),
				    finalCurrName, finalNewName);
		return null;
	    }
	});
}

public void deleteProfile(String profileName, boolean canDeleteFiles)
{
    ParameterCheck.nonNull(profileName);
    final String finalProfileName = new String(profileName);
    final boolean finalCanDeleteFiles = canDeleteFiles;
    NativeEventThread.instance.pushBlockingReturnRunnable(new ReturnRunnable() {
	    public Object run() {
		nativeDeleteProfile(getWrapperFactory().getNativeWrapperFactory(),
				    finalProfileName, finalCanDeleteFiles);
		return null;
	    }
	});
}

public void cloneProfile(String currName)
{
}

protected void finalize()
{
}

//
// Native methods
//

native void nativeStartup(int nativeContext, 
                          String profileDir, String profileName);

native void nativeShutdown(int nativeContext);

native int nativeGetProfileCount(int nativeContext);

native boolean nativeProfileExists(int nativeContext, String profileName); 

native void nativeSetCurrentProfile(int nativeContext, String profileName);

native String nativeGetCurrentProfile(int nativeContext);

native String [] nativeGetProfileList(int nativeContext);

native void nativeCreateNewProfile(int nativeContext,
                                   String profileName, 
                                   String nativeProfileDir, 
                                   String langcode, 
                                   boolean useExistingDir);

native void nativeRenameProfile(int nativeContext,
                                String currName, String newName);

native void nativeDeleteProfile(int nativeContext,
                                String profileName, boolean canDeleteFiles);



}
