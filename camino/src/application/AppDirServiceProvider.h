/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001, 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Conrad Carlen <ccarlen@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __AppDirServiceProvider_h__
#define __AppDirServiceProvider_h__

#include "nsIDirectoryService.h"
#include "nsILocalFile.h"
#include "nsString.h"

#include <Carbon/Carbon.h>

class nsIFile;

//*****************************************************************************
// class AppDirServiceProvider
//*****************************************************************************   

class AppDirServiceProvider : public nsIDirectoryServiceProvider
{
public:
                            // If |isCustomProfile| is true, we use the passed in string as a path to the custom 
                            // profile.   If it is false, the string is the product name, which will be used for the 
                            // Application Support/<name> folder, as well as the Caches/<name> system folder.
                            AppDirServiceProvider(const char *inProductName, PRBool isCustomProfile);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIDIRECTORYSERVICEPROVIDER

protected:
    virtual                 ~AppDirServiceProvider();

    nsresult                GetProfileDirectory(nsILocalFile **outFolder);
    nsresult                GetParentCacheDirectory(nsILocalFile** outFolder);
      
    nsresult                GetSystemDirectory(OSType inFolderType, nsILocalFile** outFolder);
    static nsresult 				AppDirServiceProvider::EnsureExists(nsILocalFile* inFolder);
  
protected:
    nsCOMPtr<nsILocalFile>  mProfileDir;
    PRBool                  mIsCustomProfile;
    
    // this is either the product name (e.g., "Camino") or a path, depending on mIsCustomPath
    nsCString               mName; 
};

#endif // __AppDirServiceProvider_h__

