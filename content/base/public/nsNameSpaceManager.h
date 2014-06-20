/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNameSpaceManager_h___
#define nsNameSpaceManager_h___

#include "nsDataHashtable.h"
#include "nsTArray.h"

#include "mozilla/StaticPtr.h"

class nsIAtom;
class nsAString;

class nsNameSpaceKey : public PLDHashEntryHdr
{
public:
  typedef const nsAString* KeyType;
  typedef const nsAString* KeyTypePointer;

  nsNameSpaceKey(KeyTypePointer aKey) : mKey(aKey)
  {
  }
  nsNameSpaceKey(const nsNameSpaceKey& toCopy) : mKey(toCopy.mKey)
  {
  }

  KeyType GetKey() const
  {
    return mKey;
  }
  bool KeyEquals(KeyType aKey) const
  {
    return mKey->Equals(*aKey);
  }

  static KeyTypePointer KeyToPointer(KeyType aKey)
  {
    return aKey;
  }
  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    return mozilla::HashString(*aKey);
  }

  enum {
    ALLOW_MEMMOVE = true
  };

private:
  const nsAString* mKey;
};
 
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

class nsNameSpaceManager
{
public:
  virtual ~nsNameSpaceManager() {}

  virtual nsresult RegisterNameSpace(const nsAString& aURI,
                                     int32_t& aNameSpaceID);

  virtual nsresult GetNameSpaceURI(int32_t aNameSpaceID, nsAString& aURI);
  virtual int32_t GetNameSpaceID(const nsAString& aURI);

  virtual bool HasElementCreator(int32_t aNameSpaceID);

  static nsNameSpaceManager* GetInstance();
private:
  bool Init();
  nsresult AddNameSpace(const nsAString& aURI, const int32_t aNameSpaceID);

  nsDataHashtable<nsNameSpaceKey,int32_t> mURIToIDTable;
  nsTArray< nsAutoPtr<nsString> > mURIArray;

  static mozilla::StaticAutoPtr<nsNameSpaceManager> sInstance;
};
 
#endif // nsNameSpaceManager_h___
