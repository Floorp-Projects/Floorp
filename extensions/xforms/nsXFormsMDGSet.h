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
 * The Original Code is Mozilla XForms support.
 *
 * The Initial Developer of the Original Code is
 * Novell, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Allan Beaufour <abeaufour@novell.com>
 *  David Landwehr <dlandwehr@novell.com>
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

#ifndef __NSXFORMSMDGSET_H__
#define __NSXFORMSMDGSET_H__

#include "nsVoidArray.h"
#include "nsIDOMNode.h"

/**
 * A simple "set" implementation. For now it is just a wrapper for nsVoidArray,
 * but nsVoidArray can be changed if a more suitable structure is found.
 * 
 * It is not a set, in that no two same nodes can exist. You need to call
 * MakeUnique() manually for that to be true.
 * 
 * The class owns the nsIDOMNodes that it stores, and manages the reference
 * counters automatically.
 */
class nsXFormsMDGSet {
private:
  /** The data structure */
  nsVoidArray array;
  
  /** The sorting function used by MakeUnique() */
  static int sortFunc(const void* aElement1, const void* aElement2, void* aData);
  
public:
  /** Constructor */
  nsXFormsMDGSet();
  
  /** Destructor */
  ~nsXFormsMDGSet();
  
  /** Clears the struture (removes all stored data) */
  void Clear();
  
  /**
   * Delete any duplicates (ie. make the set a set...).
   * 
   * As a side effect, it also sorts the data structure by the data pointer in
   * ascending order.
   */
  void MakeUnique();
  
  /** Returns the number of members */
  PRInt32 Count() const;
  
  /**
   * Adds a node to the set.
   * 
   * @param aDomNode         The node to add.
   * @return                 Did the storage succeed?
   */
  PRBool AddNode(nsIDOMNode* aDomNode);
  
  /**
   * Adds all nodes from another set to this set
   * 
   * @param aSet             The set to add nodes from
   * @return                 Did the operation succeed?
   */
  PRBool AddSet(nsXFormsMDGSet& aSet);
  
  /**
   * Get a specific node from the set
   * 
   * @param aIndex           The position of the node to get
   * @return                 The node
   */
  nsIDOMNode* GetNode(const PRInt32 aIndex) const;
};

#endif
