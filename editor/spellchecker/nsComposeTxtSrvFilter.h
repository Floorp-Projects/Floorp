/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsComposeTxtSrvFilter_h__
#define nsComposeTxtSrvFilter_h__

#include "nsISupportsImpl.h"            // for NS_DECL_ISUPPORTS
#include "mozilla/AlreadyAddRefed.h"

/**
 * This class implements a filter interface, that enables
 * those using it to skip over certain nodes when traversing content
 *
 * This filter is used to skip over various form control nodes and
 * mail's cite nodes
 */
class nsComposeTxtSrvFilter final : public nsISupports
{
public:
  // nsISupports interface...
  NS_DECL_ISUPPORTS

  static already_AddRefed<nsComposeTxtSrvFilter> CreateNormalFilter()
  {
    return CreateHelper(false);
  }
  static already_AddRefed<nsComposeTxtSrvFilter> CreateMailFilter()
  {
    return CreateHelper(true);
  }

  /**
   * Indicates whether the content node should be skipped by the iterator
   *  @param aNode - node to skip
   */
  bool Skip(nsINode* aNode) const;

private:
  nsComposeTxtSrvFilter();
  ~nsComposeTxtSrvFilter() {}

  // Helper - Intializer
  void Init(bool aIsForMail) { mIsForMail = aIsForMail; }

  static already_AddRefed<nsComposeTxtSrvFilter> CreateHelper(bool aIsForMail);

  bool              mIsForMail;
};

#endif
