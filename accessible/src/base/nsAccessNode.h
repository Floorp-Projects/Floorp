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
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Original Author: Aaron Leventhal (aaronl@netscape.com)
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

/* For documentation of the accessibility architecture, 
 * see http://lxr.mozilla.org/seamonkey/source/accessible/accessible-docs.html
 */

#ifndef _nsAccessNode_H_
#define _nsAccessNode_H_

#include "nsCOMPtr.h"
#include "nsIAccessNode.h"
#include "nsIDOMNode.h"
#include "nsIStringBundle.h"

class nsSupportsHashtable;
class nsHashKey;

static PRBool gIsAccessibilityActive;

#define ACCESSIBLE_BUNDLE_URL "chrome://global-platform/locale/accessible.properties"
#define PLATFORM_KEYS_BUNDLE_URL "chrome://global-platform/locale/platformKeys.properties"

// Used in 16 bit sibling index field as flags
enum { eSiblingsUninitialized = 0xffff, eSiblingsWalkNormalDOM = 0xfffe};

class nsAccessNode: public nsIAccessNode
{
  public: // construction, destruction
    nsAccessNode(nsIDOMNode *);
    virtual ~nsAccessNode();

    NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);
    NS_IMETHOD_(nsrefcnt) AddRef(void);
    NS_IMETHOD_(nsrefcnt) Release(void);
    NS_DECL_OWNINGTHREAD  // Debug - ensure AddRef happens on original thread

    NS_DECL_NSIACCESSNODE

    static void InitAccessibility();
    static void ShutdownAccessibility();

  protected:
    nsCOMPtr<nsIDOMNode> mDOMNode;

    // Cache where we are in list of kids that we got from nsIBindingManager::GetContentList(parentContent)
    // Don't need it until nsAccessible, but it packs together with mRefCnt for 4 bytes total instead of 8
    PRUint16 mSiblingIndex; 

    PRUint16 mRefCnt;

    // XXX aaronl: we should be able to remove this once we have hash table
    nsIAccessibleDocument *mRootAccessibleDoc; 

    // Static data, we do our own refcounting for our static data
    static nsIStringBundle *gStringBundle;
    static nsIStringBundle *gKeyStringBundle;
    static nsIDOMNode * gLastFocusedNode;
    static nsSupportsHashtable *gGlobalAccessibleDocCache;

    // Static methods for handling cache
    static void PutCacheEntry(nsSupportsHashtable *aCache, 
                              void* aUniqueID, nsIAccessNode *aAccessNode);
    static void GetCacheEntry(nsSupportsHashtable *aCache, void* aUniqueID, 
                              nsIAccessNode **aAccessNode);
    static void ClearCache(nsSupportsHashtable *aCache);

    static PRIntn PR_CALLBACK ClearCacheEntry(nsHashKey *aKey, void *aData, 
                                              void* aClosure);

    // Static cache methods for global document cache
    static void GetDocumentAccessible(nsIDOMNode *aDocNode,
                                      nsIAccessible **aDocAccessible);
};


#endif

