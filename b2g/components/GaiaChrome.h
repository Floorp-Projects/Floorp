/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __GAIACHROME_H__
#define __GAIACHROME_H__

#include "nsIGaiaChrome.h"

#include "nsIFile.h"

#include "nsCOMPtr.h"
#include "nsString.h"

using namespace mozilla;

class GaiaChrome final : public nsIGaiaChrome
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGAIACHROME

  static already_AddRefed<GaiaChrome>
  FactoryCreate();

private:
  nsCString mPackageName;

  nsAutoString mAppsDir;
  nsAutoString mDataRoot;
  nsAutoString mSystemRoot;

  nsCOMPtr<nsIFile> mProfDir;

  GaiaChrome();
  ~GaiaChrome();

  nsresult ComputeAppsPath(nsIFile*);
  bool EnsureIsDirectory(nsIFile*);
  nsresult EnsureValidPath(nsIFile*);
  nsresult GetProfileDir();
};

#endif  // __GAIACHROME_H__
