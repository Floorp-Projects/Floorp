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
 * Contributor(s):  Ashu Kulkarni <ashuk@eng.sun.com>
 */

package org.mozilla.webclient.wrapper_native;

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;

import org.mozilla.webclient.BrowserControl;
import org.mozilla.webclient.ProfileManager;
import org.mozilla.webclient.WindowControl;
import org.mozilla.webclient.WrapperFactory;

import org.mozilla.webclient.UnimplementedException; 

import org.mozilla.xpcom.*;
import org.mozilla.webclient.stubs.*;

public class ProfileManagerImpl extends ImplObjectNative implements ProfileManager
{

// local variables
private nsIXPIDLServiceManager serviceMgr;
private nsIProfile profileStub;
private CID nsProfileCID = new CID("02b0625b-e7f3-11d2-9f5a-006008a6efe9");


public ProfileManagerImpl(WrapperFactory yourFactory, 
		      BrowserControl yourBrowserControl)
{
    super(yourFactory, yourBrowserControl);
    
    // Make whatever native calls are required to initialize the
    // BlackConnect Profile module here
    InterfaceRegistry.register(nsIProfile.class);
    serviceMgr = org.mozilla.xpcom.Components.getServiceManager();
    nsISupports profile =  serviceMgr.getService(nsProfileCID, nsIProfile.IID);
    profileStub = (nsIProfile) profile.queryInterface(nsIProfile.IID);
}


public int GetProfileCount()
    {
        myFactory.throwExceptionIfNotInitialized();
    
        int count;
        synchronized(myBrowserControl) {
            count = profileStub.getProfileCount();
        }
        return count;
    }

public String [] GetProfileList(int [] length)
    {
        myFactory.throwExceptionIfNotInitialized();
        
        String [] list = null;
        synchronized(myBrowserControl) {
            list = profileStub.getProfileList(length);
        }
        return list;
    }

public boolean ProfileExists(String profileName)
    {
        myFactory.throwExceptionIfNotInitialized();
        
        boolean exists = false;
        synchronized(myBrowserControl) {
            exists = profileStub.profileExists(profileName);
        }
        return exists;
    }

public String GetCurrentProfile()
    {
        myFactory.throwExceptionIfNotInitialized();

        String currProfile = null;
        synchronized(myBrowserControl) {
            currProfile = profileStub.getCurrentProfile();
        }
        return currProfile;
    }

public void SetCurrentProfile(String profileName)
    {
        myFactory.throwExceptionIfNotInitialized();

        synchronized(myBrowserControl) {
            profileStub.setCurrentProfile(profileName);
        }
    }

public void CreateNewProfile(String profileName, String nativeProfileDir, String langcode, boolean useExistingDir)
    {
        myFactory.throwExceptionIfNotInitialized();

        System.out.println("\nIn ProfileManager CreateNewProfile\n");
        synchronized(myBrowserControl) {
            profileStub.createNewProfile(profileName, nativeProfileDir, langcode, useExistingDir);
        }
    }

public void RenameProfile(String currName, String newName)
    {
        myFactory.throwExceptionIfNotInitialized();
 
        synchronized(myBrowserControl) {
            profileStub.renameProfile(currName, newName);
        }
    }

public void DeleteProfile(String profileName, boolean canDeleteFiles)
    {
        myFactory.throwExceptionIfNotInitialized();

        synchronized(myBrowserControl) {
            profileStub.deleteProfile(profileName, canDeleteFiles);
        }   
    }


public void CloneProfile(String currName)
    {
        myFactory.throwExceptionIfNotInitialized();
 
        synchronized(myBrowserControl) {
            profileStub.cloneProfile(currName);
        }   
    }

protected void finalize()
    {
        if (profileStub != null) {
            //Release any service that we may be holding on to      
            nsISupports obj = (nsISupports) profileStub.queryInterface(nsISupports.IID);
            serviceMgr.releaseService(nsProfileCID, obj);
        }
    }

}
