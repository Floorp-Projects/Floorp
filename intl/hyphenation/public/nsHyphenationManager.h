/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHyphenationManager_h__
#define nsHyphenationManager_h__

#include "nsInterfaceHashtable.h"
#include "nsRefPtrHashtable.h"
#include "nsHashKeys.h"
#include "nsIObserver.h"
#include "mozilla/Omnijar.h"

class nsHyphenator;
class nsIAtom;
class nsIURI;

class nsHyphenationManager
{
public:
  nsHyphenationManager();

  already_AddRefed<nsHyphenator> GetHyphenator(nsIAtom *aLocale);

  static nsHyphenationManager *Instance();

  static void Shutdown();

private:
  ~nsHyphenationManager();

protected:
  class MemoryPressureObserver MOZ_FINAL : public nsIObserver
  {
  public:
      NS_DECL_ISUPPORTS
      NS_DECL_NSIOBSERVER
  };

  void LoadPatternList();
  void LoadPatternListFromOmnijar(mozilla::Omnijar::Type aType);
  void LoadPatternListFromDir(nsIFile *aDir);
  void LoadAliases();

  nsInterfaceHashtable<nsISupportsHashKey,nsIAtom> mHyphAliases;
  nsInterfaceHashtable<nsISupportsHashKey,nsIURI> mPatternFiles;
  nsRefPtrHashtable<nsISupportsHashKey,nsHyphenator> mHyphenators;

  static nsHyphenationManager *sInstance;
};

#endif // nsHyphenationManager_h__
