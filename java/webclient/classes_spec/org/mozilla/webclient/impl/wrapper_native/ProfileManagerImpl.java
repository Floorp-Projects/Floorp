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
 *                  Ed Burns <edburns@acm.org>
 */

package org.mozilla.webclient.impl.wrapper_native;

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;

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
    nativeStartup(getWrapperFactory().getNativeWrapperFactory(), null, null);
}

public void shutdown() {
    nativeShutdown(getWrapperFactory().getNativeWrapperFactory());
}

public int getProfileCount()
{
    return nativeGetProfileCount(getWrapperFactory().getNativeWrapperFactory());
}

public String [] getProfileList()
{
    String [] list = null;
    list = nativeGetProfileList(getWrapperFactory().getNativeWrapperFactory());
    return list;
}

public boolean profileExists(String profileName)
{
    boolean exists = false;
    exists = nativeProfileExists(getWrapperFactory().getNativeWrapperFactory(),
                                 profileName);
    return exists;
}

public String getCurrentProfile()
{
    String currProfile = null;
    currProfile = 
        nativeGetCurrentProfile(getWrapperFactory().getNativeWrapperFactory());
    return currProfile;
}

public void setCurrentProfile(String profileName)
{
    nativeSetCurrentProfile(getWrapperFactory().getNativeWrapperFactory(),
                            profileName);
}

public void createNewProfile(String profileName, 
                             String nativeProfileDir, 
                             String langcode, 
                             boolean useExistingDir)
{
    nativeCreateNewProfile(getWrapperFactory().getNativeWrapperFactory(),
                           profileName, nativeProfileDir, langcode, 
                           useExistingDir);
}

public void renameProfile(String currName, String newName)
{
    nativeRenameProfile(getWrapperFactory().getNativeWrapperFactory(),
                        currName, newName);
}

public void deleteProfile(String profileName, boolean canDeleteFiles)
{
    nativeDeleteProfile(getWrapperFactory().getNativeWrapperFactory(),
                        profileName, canDeleteFiles);
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
