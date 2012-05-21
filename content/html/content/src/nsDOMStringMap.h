/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMStringMap_h
#define nsDOMStringMap_h

#include "nsIDOMDOMStringMap.h"

#include "nsCycleCollectionParticipant.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "nsString.h"

class nsGenericHTMLElement;

class nsDOMStringMap : public nsIDOMDOMStringMap
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIDOMDOMSTRINGMAP
  NS_DECL_CYCLE_COLLECTION_CLASS(nsDOMStringMap)

  nsDOMStringMap(nsGenericHTMLElement* aElement);

  // GetDataPropList is not defined in IDL due to difficulty
  // of returning arrays in IDL. Instead, we cast to this
  // class if this method needs to be called.
  nsresult GetDataPropList(nsTArray<nsString>& aResult);

  nsresult RemovePropInternal(nsIAtom* aAttr);
  nsGenericHTMLElement* GetElement();

private:
  virtual ~nsDOMStringMap();

protected:
  nsRefPtr<nsGenericHTMLElement> mElement;
  // Flag to guard against infinite recursion.
  bool mRemovingProp;
  bool DataPropToAttr(const nsAString& aProp, nsAString& aResult);
  bool AttrToDataProp(const nsAString& aAttr, nsAString& aResult);
};

#endif
