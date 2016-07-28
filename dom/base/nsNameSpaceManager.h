/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNameSpaceManager_h___
#define nsNameSpaceManager_h___

#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsIAtom.h"
#include "nsTArray.h"

#include "mozilla/StaticPtr.h"

class nsAString;

/**
 * The Name Space Manager tracks the association between a NameSpace
 * URI and the int32_t runtime id. Mappings between NameSpaces and 
 * NameSpace prefixes are managed by nsINameSpaces.
 *
 * All NameSpace URIs are stored in a global table so that IDs are
 * consistent accross the app. NameSpace IDs are only consistent at runtime
 * ie: they are not guaranteed to be consistent accross app sessions.
 *
 * The nsNameSpaceManager needs to have a live reference for as long as
 * the NameSpace IDs are needed.
 *
 */

class nsNameSpaceManager final
{
public:
  ~nsNameSpaceManager() {}

  nsresult RegisterNameSpace(const nsAString& aURI, int32_t& aNameSpaceID);

  nsresult GetNameSpaceURI(int32_t aNameSpaceID, nsAString& aURI);

  nsIAtom* NameSpaceURIAtom(int32_t aNameSpaceID) {
    MOZ_ASSERT(aNameSpaceID > 0 && (int64_t) aNameSpaceID <= (int64_t) mURIArray.Length());
    return mURIArray.ElementAt(aNameSpaceID - 1); // id is index + 1
  }

  int32_t GetNameSpaceID(const nsAString& aURI);
  int32_t GetNameSpaceID(nsIAtom* aURI);

  bool HasElementCreator(int32_t aNameSpaceID);

  static nsNameSpaceManager* GetInstance();
private:
  bool Init();
  nsresult AddNameSpace(already_AddRefed<nsIAtom> aURI, const int32_t aNameSpaceID);

  nsDataHashtable<nsISupportsHashKey, int32_t> mURIToIDTable;
  nsTArray<nsCOMPtr<nsIAtom>> mURIArray;

  static mozilla::StaticAutoPtr<nsNameSpaceManager> sInstance;
};
 
#endif // nsNameSpaceManager_h___
