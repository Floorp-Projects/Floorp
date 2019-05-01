/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPluginDirServiceProvider_h_
#define nsPluginDirServiceProvider_h_

#include "nsIDirectoryService.h"

#if defined(XP_WIN)
#  include "nsCOMArray.h"
#endif

class nsISimpleEnumerator;

//*****************************************************************************
// class nsPluginDirServiceProvider
//*****************************************************************************

class nsPluginDirServiceProvider : public nsIDirectoryServiceProvider {
 public:
  nsPluginDirServiceProvider();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDIRECTORYSERVICEPROVIDER

#ifdef XP_WIN
  static nsresult GetPLIDDirectories(nsISimpleEnumerator** aEnumerator);

 private:
  static nsresult GetPLIDDirectoriesWithRootKey(uint32_t aKey,
                                                nsCOMArray<nsIFile>& aDirs);
#endif

 protected:
  virtual ~nsPluginDirServiceProvider();
};

#endif  // nsPluginDirServiceProvider_h_
