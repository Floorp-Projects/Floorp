/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef NSFRAMETRAVERSAL_H
#define NSFRAMETRAVERSAL_H

#include "mozilla/Attributes.h"
#include "nsIFrameTraversal.h"

class nsIFrame;

nsresult NS_NewFrameTraversal(nsIFrameEnumerator **aEnumerator,
                              nsPresContext* aPresContext,
                              nsIFrame *aStart,
                              nsIteratorType aType,
                              bool aVisual,
                              bool aLockInScrollView,
                              bool aFollowOOFs);

nsresult NS_CreateFrameTraversal(nsIFrameTraversal** aResult);

class nsFrameTraversal : public nsIFrameTraversal
{
public:
  nsFrameTraversal();
  virtual ~nsFrameTraversal();

  NS_DECL_ISUPPORTS

  NS_IMETHOD NewFrameTraversal(nsIFrameEnumerator **aEnumerator,
                               nsPresContext* aPresContext,
                               nsIFrame *aStart,
                               int32_t aType,
                               bool aVisual,
                               bool aLockInScrollView,
                               bool aFollowOOFs) MOZ_OVERRIDE;
};

#endif //NSFRAMETRAVERSAL_H
