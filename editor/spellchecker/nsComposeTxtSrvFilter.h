/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsComposeTxtSrvFilter_h__
#define nsComposeTxtSrvFilter_h__

#include "nsISupportsImpl.h"            // for NS_DECL_ISUPPORTS
#include "nsITextServicesFilter.h"

/**
 * This class implements a filter interface, that enables
 * those using it to skip over certain nodes when traversing content
 *
 * This filter is used to skip over various form control nodes and
 * mail's cite nodes
 */
class nsComposeTxtSrvFilter final : public nsITextServicesFilter
{
public:
  nsComposeTxtSrvFilter();

  // nsISupports interface...
  NS_DECL_ISUPPORTS

  // nsITextServicesFilter
  NS_DECL_NSITEXTSERVICESFILTER

  // Helper - Intializer
  void Init(bool aIsForMail) { mIsForMail = aIsForMail; }

  /**
   * Indicates whether the content node should be skipped by the iterator
   *  @param aNode - node to skip
   */
  bool Skip(nsINode* aNode) const;

private:
  ~nsComposeTxtSrvFilter() {}

  bool              mIsForMail;
};

#define NS_COMPOSERTXTSRVFILTER_CID \
{/* {171E72DB-0F8A-412a-8461-E4C927A3A2AC}*/ \
0x171e72db, 0xf8a, 0x412a, \
{ 0x84, 0x61, 0xe4, 0xc9, 0x27, 0xa3, 0xa2, 0xac} }

#define NS_COMPOSERTXTSRVFILTERMAIL_CID \
{/* {7FBD2146-5FF4-4674-B069-A7BBCE66E773}*/ \
0x7fbd2146, 0x5ff4, 0x4674, \
{ 0xb0, 0x69, 0xa7, 0xbb, 0xce, 0x66, 0xe7, 0x73} }

// Generic for the editor
#define COMPOSER_TXTSRVFILTER_CONTRACTID     "@mozilla.org/editor/txtsrvfilter;1"

// This is the same but includes "cite" typed blocked quotes
#define COMPOSER_TXTSRVFILTERMAIL_CONTRACTID "@mozilla.org/editor/txtsrvfiltermail;1"

#endif
