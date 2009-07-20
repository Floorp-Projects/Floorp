/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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
