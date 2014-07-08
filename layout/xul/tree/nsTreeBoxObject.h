/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTreeBoxObject_h___
#define nsTreeBoxObject_h___

#include "mozilla/Attributes.h"
#include "nsBoxObject.h"
#include "nsITreeView.h"
#include "nsITreeBoxObject.h"

class nsTreeBodyFrame;

class nsTreeBoxObject : public nsITreeBoxObject, public nsBoxObject
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsTreeBoxObject, nsBoxObject)
  NS_DECL_NSITREEBOXOBJECT

  nsTreeBoxObject();

  nsTreeBodyFrame* GetTreeBody(bool aFlushLayout = false);
  nsTreeBodyFrame* GetCachedTreeBody() { return mTreeBody; }

  //NS_PIBOXOBJECT interfaces
  virtual void Clear() MOZ_OVERRIDE;
  virtual void ClearCachedValues() MOZ_OVERRIDE;

protected:
  ~nsTreeBoxObject();
  nsTreeBodyFrame* mTreeBody;
  nsCOMPtr<nsITreeView> mView;
};

#endif
