/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of nsIDOMDOMStringList, used by various DOM stuff.
 */

#ifndef nsDOMLists_h___
#define nsDOMLists_h___

#include "nsIDOMDOMStringList.h"
#include "nsTArray.h"
#include "nsString.h"

class nsDOMStringList : public nsIDOMDOMStringList
{
public:
  nsDOMStringList();
  virtual ~nsDOMStringList();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMDOMSTRINGLIST

  bool Add(const nsAString& aName)
  {
    return mNames.AppendElement(aName) != nullptr;
  }

  void Clear()
  {
    mNames.Clear();
  }

  void CopyList(nsTArray<nsString>& aNames)
  {
    aNames = mNames;
  }

private:
  nsTArray<nsString> mNames;
};

#endif /* nsDOMLists_h___ */
