/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *   L. David Baron <dbaron@fas.harvard.edu>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsILink_h___
#define nsILink_h___

#include "nsISupports.h"
#include "nsILinkHandler.h" // definition of nsLinkState

// IID for the nsILink interface
#define NS_ILINK_IID    \
{ 0xa904ac22, 0x28fa, 0x4812,  \
  { 0xbe, 0xf3, 0x2, 0x2f, 0xf3, 0x3b, 0x8e, 0xf5 } }

/**
 * This interface allows SelectorMatches to get the canonical
 * URL pointed to by an element representing a link and allows
 * it to store the visited state of a link element in the link.
 * It is needed for performance reasons (to prevent copying of
 * strings and excessive calls to history).
 */
class nsILink : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ILINK_IID)

  /**
   * Get the cached state of the link.  If the state is unknown, 
   * return eLinkState_Unknown.
   *
   * @param aState [out] The cached link state of the link.
   * @return NS_OK
   */
  NS_IMETHOD GetLinkState(nsLinkState &aState) = 0;

  /**
   * Set the cached state of the link.
   *
   * @param aState The cached link state of the link.
   * @return NS_OK
   */
  NS_IMETHOD SetLinkState(nsLinkState aState) = 0;

  /**
    * Get a pointer to the canonical URL (i.e., fully resolved to the
    * base URL) that this linkable element points to.  The buffer
    * returned has been allocated for and should be deleted by the
    * caller.
    *
    * @param aBuf [out] A pointer to be filled in with a pointer to a
    *             null-terminated string containing the canonical URL.
    *             If the element has no HREF attribute, it is set to
    *             nsnull.
    * @return NS_OK if the out pointer is filled in
    */
  NS_IMETHOD GetHrefCString(char* &aBuf) = 0;

};

#endif /* nsILink_h___ */
