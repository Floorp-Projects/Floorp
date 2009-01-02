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
 * The Original Code is Mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla.com.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *         Boris Zbarsky <bzbarsky@mit.edu> (Original Author)
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

#ifndef nsINodeList_h___
#define nsINodeList_h___

#include "nsIDOMNodeList.h"

class nsINode;

// IID for the nsINodeList interface
#define NS_INODELIST_IID \
{ 0x943420c4, 0x8774, 0x43ea, \
 { 0xb3, 0x53, 0x62, 0xa1, 0x26, 0x1c, 0x9b, 0x55 } }

/**
 * An internal interface that allows QI-less getting of nodes from node lists
 */
class nsINodeList : public nsIDOMNodeList
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_INODELIST_IID)

  /**
   * Get the node at the index.  Returns null if the index is out of bounds
   */
  virtual nsINode* GetNodeAt(PRUint32 aIndex) = 0;
};

#define NS_NODELIST_OFFSET_AND_INTERFACE_TABLE_BEGIN(_class)                  \
  NS_OFFSET_AND_INTERFACE_TABLE_BEGIN_AMBIGUOUS(_class, nsINodeList)

NS_DEFINE_STATIC_IID_ACCESSOR(nsINodeList, NS_INODELIST_IID)

#endif /* nsINodeList_h___ */
