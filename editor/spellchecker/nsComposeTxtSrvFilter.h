/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsComposeTxtSrvFilter_h__
#define nsComposeTxtSrvFilter_h__

#include "mozilla/UniquePtr.h"

class nsINode;

/**
 * This class enables those using it to skip over certain nodes when
 * traversing content.
 *
 * This filter is used to skip over various form control nodes and
 * mail's cite nodes
 */
class nsComposeTxtSrvFilter final {
 public:
  static mozilla::UniquePtr<nsComposeTxtSrvFilter> CreateNormalFilter() {
    return CreateHelper(false);
  }
  static mozilla::UniquePtr<nsComposeTxtSrvFilter> CreateMailFilter() {
    return CreateHelper(true);
  }

  /**
   * Indicates whether the content node should be skipped by the iterator
   *  @param aNode - node to skip
   */
  bool Skip(nsINode* aNode) const;

 private:
  // Helper - Intializer
  void Init(bool aIsForMail) { mIsForMail = aIsForMail; }

  static mozilla::UniquePtr<nsComposeTxtSrvFilter> CreateHelper(
      bool aIsForMail);

  bool mIsForMail = false;
};

#endif
