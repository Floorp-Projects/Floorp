/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsBoxObject_h_
#define nsBoxObject_h_

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsIBoxObject.h"
#include "nsPIBoxObject.h"
#include "nsPoint.h"
#include "nsAutoPtr.h"
#include "nsHashKeys.h"
#include "nsInterfaceHashtable.h"
#include "nsCycleCollectionParticipant.h"

class nsIFrame;
class nsIDocShell;
struct nsIntRect;
class nsIPresShell;

class nsBoxObject : public nsPIBoxObject
{
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsBoxObject)
  NS_DECL_NSIBOXOBJECT

public:
  nsBoxObject();

  // nsPIBoxObject
  virtual nsresult Init(nsIContent* aContent) MOZ_OVERRIDE;
  virtual void Clear() MOZ_OVERRIDE;
  virtual void ClearCachedValues() MOZ_OVERRIDE;

  nsIFrame* GetFrame(bool aFlushLayout);
  nsIPresShell* GetPresShell(bool aFlushLayout);
  nsresult GetOffsetRect(nsIntRect& aRect);
  nsresult GetScreenPosition(nsIntPoint& aPoint);

  // Given a parent frame and a child frame, find the frame whose
  // next sibling is the given child frame and return its element
  static nsresult GetPreviousSibling(nsIFrame* aParentFrame, nsIFrame* aFrame,
                                     nsIDOMElement** aResult);

protected:
  virtual ~nsBoxObject();

  nsAutoPtr<nsInterfaceHashtable<nsStringHashKey,nsISupports> > mPropertyTable; //[OWNER]

  nsIContent* mContent; // [WEAK]
};

#endif
