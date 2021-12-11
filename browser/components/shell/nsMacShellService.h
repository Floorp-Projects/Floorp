/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsmacshellservice_h____
#define nsmacshellservice_h____

#include "nsToolkitShellService.h"
#include "nsIMacShellService.h"
#include "nsIWebProgressListener.h"
#include "nsIFile.h"
#include "nsCOMPtr.h"

class nsMacShellService : public nsIMacShellService,
                          public nsToolkitShellService,
                          public nsIWebProgressListener {
 public:
  nsMacShellService(){};

  NS_DECL_ISUPPORTS
  NS_DECL_NSISHELLSERVICE
  NS_DECL_NSIMACSHELLSERVICE
  NS_DECL_NSIWEBPROGRESSLISTENER

 protected:
  virtual ~nsMacShellService(){};

 private:
  nsCOMPtr<nsIFile> mBackgroundFile;
};

#endif  // nsmacshellservice_h____
