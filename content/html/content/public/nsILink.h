/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsILink_h___
#define nsILink_h___

#include "nsISupports.h"
#include "nsILinkHandler.h" // definition of nsLinkState

class nsIURI;

// IID for the nsILink interface
#define NS_ILINK_IID    \
{ 0x6f374a11, 0x212d, 0x47d6, \
  { 0x94, 0xd1, 0xe6, 0x7c, 0x23, 0x4d, 0x34, 0x99 } }

/**
 * This interface allows SelectorMatches to get the canonical
 * URL pointed to by an element representing a link and allows
 * it to store the visited state of a link element in the link.
 * It is needed for performance reasons (to prevent copying of
 * strings and excessive calls to history).
 */
class nsILink : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ILINK_IID)

  /**
   * GetLinkState/SetLinkState/GetHrefURI were moved to nsIContent.
   * @see nsIContent
   */

  /**
   * Dispatch a LinkAdded event to the chrome event handler for this document.
   * This is used to notify the chrome listeners when restoring a page
   * presentation.  Currently, this only applies to HTML <link> elements.
   */
  NS_IMETHOD LinkAdded() = 0;

  /**
   * Dispatch a LinkRemoved event to the chrome event handler for this
   * document.  This is used to notify the chrome listeners when saving a page
   * presentation (since the document is not torn down).  Currently, this only
   * applies to HTML <link> elements.
   */
  NS_IMETHOD LinkRemoved() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsILink, NS_ILINK_IID)

#endif /* nsILink_h___ */
