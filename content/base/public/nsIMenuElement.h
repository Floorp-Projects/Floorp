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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/*
 * nsIMenuElement is an interface to be implemented on the
 * parent nodes of elements which can be in the :-moz-menuactive
 * state (this corresponds to hovering over the item or going to
 * it with the arrow keys).  It is used by the CSS pseudoclass
 * matching code to decide if a menu item matches :-moz-menuactive.
 */

#ifndef _nsIMenuElement_h_
#define _nsIMenuElement_h_

#include "nsISupports.h"

// IID for the nsIMenuElement interface
#define NS_IMENUELEMENT_IID \
 { 0xbfede654, 0x1dd1, 0x11b2, \
   { 0x89, 0x35, 0x87, 0x92, 0xfc, 0xb7, 0xc9, 0xa1 } }

class nsIMenuElement : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IMENUELEMENT_IID);

  /**
    * Get the current active item for this menu.
    */
  virtual const nsIContent* GetActiveItem() const = 0;

  /**
    * Set the active item for this menu.
    * @ param aItem The new active item
    */
  virtual void SetActiveItem(const nsIContent* aItem) = 0;
};

#endif
