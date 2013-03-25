/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsIMenuBuilder.h"
#include "nsIXULContextMenuBuilder.h"
#include "nsCycleCollectionParticipant.h"

class nsIAtom;
class nsIContent;
class nsIDocument;
class nsIDOMHTMLElement;

class nsXULContextMenuBuilder : public nsIMenuBuilder,
                                public nsIXULContextMenuBuilder
{
public:
  nsXULContextMenuBuilder();
  virtual ~nsXULContextMenuBuilder();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsXULContextMenuBuilder,
                                           nsIMenuBuilder)
  NS_DECL_NSIMENUBUILDER

  NS_DECL_NSIXULCONTEXTMENUBUILDER

protected:
  nsresult CreateElement(nsIAtom* aTag,
                         nsIDOMHTMLElement* aHTMLElement,
                         nsIContent** aResult);

  nsCOMPtr<nsIContent>          mFragment;
  nsCOMPtr<nsIDocument>         mDocument;
  nsCOMPtr<nsIAtom>             mGeneratedItemIdAttr;

  nsCOMPtr<nsIContent>          mCurrentNode;
  int32_t                       mCurrentGeneratedItemId;

  nsCOMArray<nsIDOMHTMLElement> mElements;
};
