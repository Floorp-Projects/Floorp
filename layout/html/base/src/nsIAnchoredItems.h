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
#ifndef nsIAnchoredItems_h___
#define nsIAnchoredItems_h___

#include "nsIFrame.h"

// 7327a0a0-bc8d-11d1-8539-00a02468fab6
#define NS_IANCHORED_ITEMS_IID \
 { 0x7327a0a0, 0xbc8d, 0x11d1, {0x85, 0x39, 0x0, 0xa0, 0x24, 0x68, 0xfa, 0xb6}}

/**
 * An interface for managing anchored items. Note that this interface is not
 * an nsISupports interface, and therefore you cannot QueryInterface() back
 */
class nsIAnchoredItems
{
public:
  enum AnchoringPosition {anHTMLFloater};

  /**
   * Request to add an anchored item to this geometric parent. This
   * is for handling anchored items that are not displayed inline (at
   * the current point).
   */
  virtual void  AddAnchoredItem(nsIFrame*         aAnchoredItem,
                                AnchoringPosition aPosition,
                                nsIFrame*         aContainer) = 0;

 /**
  * Called to remove an anchored item, most likely because the associated
  * piece of content has been removed.
  */
  virtual void RemoveAnchoredItem(nsIFrame* aAnchoredItem) = 0;
};

#endif /* nsIAnchoredItems_h___ */
