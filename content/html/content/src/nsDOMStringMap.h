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
#include "nsWrapperCache.h"
#include "nsGenericHTMLElement.h"

class nsDOMStringMap : public nsIDOMDOMStringMap,
                       public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIDOMDOMSTRINGMAP
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsDOMStringMap)

  nsINode* GetParentObject()
  {
    return mElement;
  }

  static nsDOMStringMap* FromSupports(nsISupports* aSupports)
  {
    nsIDOMDOMStringMap* map =
      static_cast<nsDOMStringMap*>(aSupports);
#ifdef DEBUG
    {
      nsCOMPtr<nsIDOMDOMStringMap> map_qi =
        do_QueryInterface(aSupports);

      // If this assertion fires the QI implementation for the object in
      // question doesn't use the nsIDOMDOMStringMap pointer as the
      // nsISupports pointer. That must be fixed, or we'll crash...
      NS_ASSERTION(map_qi == map, "Uh, fix QI!");
    }
#endif

    return static_cast<nsDOMStringMap*>(map);
  }

  
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
