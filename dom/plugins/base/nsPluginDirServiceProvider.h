/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPluginDirServiceProvider_h_
#define nsPluginDirServiceProvider_h_

#include "nsIDirectoryService.h"

#if defined (XP_WIN)
#include "nsCOMArray.h"
#endif

class nsISimpleEnumerator;

// Note: Our directory service provider scan keys are prefs which are check
//       for minimum versions compatibility
#define NS_WIN_ACROBAT_SCAN_KEY        "plugin.scan.Acrobat"
#define NS_WIN_QUICKTIME_SCAN_KEY      "plugin.scan.Quicktime"
#define NS_WIN_WMP_SCAN_KEY            "plugin.scan.WindowsMediaPlayer"

//*****************************************************************************
// class nsPluginDirServiceProvider
//*****************************************************************************   

class nsPluginDirServiceProvider : public nsIDirectoryServiceProvider
{
public:
   nsPluginDirServiceProvider();
   
   NS_DECL_ISUPPORTS
   NS_DECL_NSIDIRECTORYSERVICEPROVIDER

#ifdef XP_WIN
   static nsresult GetPLIDDirectories(nsISimpleEnumerator **aEnumerator);
private:
   static nsresult GetPLIDDirectoriesWithRootKey(uint32_t aKey,
     nsCOMArray<nsIFile> &aDirs);
#endif

protected:
   virtual ~nsPluginDirServiceProvider();
};

#endif // nsPluginDirServiceProvider_h_
