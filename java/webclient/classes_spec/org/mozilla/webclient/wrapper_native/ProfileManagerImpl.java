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


public class ProfileManagerImpl extends ImplObjectNative implements ProfileManager
{

// local variables

public ProfileManagerImpl(WrapperFactory yourFactory, 
		      BrowserControl yourBrowserControl)
{
    super(yourFactory, yourBrowserControl);
    
}


public int GetProfileCount()
    {
        return -1;
    }

public String [] GetProfileList(int [] length)
    {
        String [] list = null;
        return list;
    }

public boolean ProfileExists(String profileName)
    {
        boolean exists = false;
        return exists;
    }

public String GetCurrentProfile()
    {
        String currProfile = null;
        return currProfile;
    }

public void SetCurrentProfile(String profileName)
    {
    }

public void CreateNewProfile(String profileName, String nativeProfileDir, String langcode, boolean useExistingDir)
    {
    }

public void RenameProfile(String currName, String newName)
    {
    }

public void DeleteProfile(String profileName, boolean canDeleteFiles)
    {
    }


public void CloneProfile(String currName)
    {
    }

protected void finalize()
    {
    }

}
