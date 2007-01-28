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
 *   Original Author: Aaron Leventhal (aaronl@netscape.com)
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

/* For documentation of the accessibility architecture, 
 * see http://lxr.mozilla.org/seamonkey/source/accessible/accessible-docs.html
 */

#ifndef _nsAccessNode_H_
#define _nsAccessNode_H_

#include "nsCOMPtr.h"
#include "nsAccessibilityAtoms.h"
#include "nsIAccessNode.h"
#include "nsIContent.h"
#include "nsPIAccessNode.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDOMNode.h"
#include "nsINameSpaceManager.h"
#include "nsIStringBundle.h"
#include "nsWeakReference.h"
#include "nsInterfaceHashtable.h"
#include "nsIAccessibilityService.h"

class nsIPresShell;
class nsPresContext;
class nsIAccessibleDocument;
class nsIFrame;
class nsIDOMNodeList;
class nsITimer;
class nsRootAccessible;

#define ACCESSIBLE_BUNDLE_URL "chrome://global-platform/locale/accessible.properties"
#define PLATFORM_KEYS_BUNDLE_URL "chrome://global-platform/locale/platformKeys.properties"

/* hashkey wrapper using void* KeyType
 *
 * @see nsTHashtable::EntryType for specification
 */
class nsVoidHashKey : public PLDHashEntryHdr
{
public:
  typedef const void* KeyType;
  typedef const void* KeyTypePointer;
  
  nsVoidHashKey(KeyTypePointer aKey) : mValue(aKey) { }
  nsVoidHashKey(const nsVoidHashKey& toCopy) : mValue(toCopy.mValue) { }
  ~nsVoidHashKey() { }

  KeyType GetKey() const { return mValue; }
  KeyTypePointer GetKeyPointer() const { return mValue; }
  PRBool KeyEquals(KeyTypePointer aKey) const { return aKey == mValue; }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey) { return NS_PTR_TO_INT32(aKey) >> 2; }
  enum { ALLOW_MEMMOVE = PR_TRUE };

private:
  const void* mValue;
};

class nsAccessNode: public nsIAccessNode, public nsPIAccessNode
{
  public: // construction, destruction
    nsAccessNode(nsIDOMNode *, nsIWeakReference* aShell);
    virtual ~nsAccessNode();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIACCESSNODE
    NS_DECL_NSPIACCESSNODE

    static void InitXPAccessibility();
    static void ShutdownXPAccessibility();

    // Static methods for handling per-document cache
    static void PutCacheEntry(nsInterfaceHashtable<nsVoidHashKey, nsIAccessNode>& aCache, 
                              void* aUniqueID, nsIAccessNode *aAccessNode);
    static void GetCacheEntry(nsInterfaceHashtable<nsVoidHashKey, nsIAccessNode>& aCache, void* aUniqueID, 
                              nsIAccessNode **aAccessNode);
    static void ClearCache(nsInterfaceHashtable<nsVoidHashKey, nsIAccessNode>& aCache);

    static PLDHashOperator PR_CALLBACK ClearCacheEntry(const void* aKey, nsCOMPtr<nsIAccessNode>& aAccessNode, void* aUserArg);

    // Static cache methods for global document cache
    static already_AddRefed<nsIAccessibleDocument> GetDocAccessibleFor(nsIWeakReference *aPresShell);
    static already_AddRefed<nsIAccessibleDocument> GetDocAccessibleFor(nsISupports *aContainer, PRBool aCanCreate = PR_FALSE);
    static already_AddRefed<nsIAccessibleDocument> GetDocAccessibleFor(nsIDOMNode *aNode);

    static already_AddRefed<nsIDocShellTreeItem> GetDocShellTreeItemFor(nsIDOMNode *aStartNode);
    static already_AddRefed<nsIDOMNode> GetDOMNodeForContainer(nsISupports *aContainer);
    static already_AddRefed<nsIPresShell> GetPresShellFor(nsIDOMNode *aStartNode);
    
    // Return PR_TRUE if there is a role attribute
    static PRBool HasRoleAttribute(nsIContent *aContent)
    {
      return (aContent->IsNodeOfType(nsINode::eHTML) && aContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::role)) ||
              aContent->HasAttr(kNameSpaceID_XHTML, nsAccessibilityAtoms::role) ||
              aContent->HasAttr(kNameSpaceID_XHTML2_Unofficial, nsAccessibilityAtoms::role);
    }

    // Return PR_TRUE if there is a role attribute, and fill it into aRole
    static PRBool GetRoleAttribute(nsIContent *aContent, nsAString& aRole)
    {
      aRole.Truncate();
      return (aContent->IsNodeOfType(nsINode::eHTML) && aContent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::role, aRole)) ||
              aContent->GetAttr(kNameSpaceID_XHTML, nsAccessibilityAtoms::role, aRole) ||
              aContent->GetAttr(kNameSpaceID_XHTML2_Unofficial, nsAccessibilityAtoms::role, aRole);
    }

    static void GetComputedStyleDeclaration(const nsAString& aPseudoElt,
                                            nsIDOMElement *aElement,
                                            nsIDOMCSSStyleDeclaration **aCssDecl);

    already_AddRefed<nsRootAccessible> GetRootAccessible();

    static nsIDOMNode *gLastFocusedNode;
    static nsIAccessibilityService* GetAccService();

protected:
    nsresult MakeAccessNode(nsIDOMNode *aNode, nsIAccessNode **aAccessNode);
    already_AddRefed<nsIPresShell> GetPresShell();
    nsPresContext* GetPresContext();
    already_AddRefed<nsIAccessibleDocument> GetDocAccessible();

    nsCOMPtr<nsIDOMNode> mDOMNode;
    nsCOMPtr<nsIWeakReference> mWeakShell;

#ifdef DEBUG_A11Y
    PRBool mIsInitialized;
#endif

    // Static data, we do our own refcounting for our static data
    static nsIStringBundle *gStringBundle;
    static nsIStringBundle *gKeyStringBundle;
    static nsITimer *gDoCommandTimer;
    static PRBool gIsAccessibilityActive;
    static PRBool gIsCacheDisabled;
    static PRBool gIsFormFillEnabled;

    static nsInterfaceHashtable<nsVoidHashKey, nsIAccessNode> gGlobalDocAccessibleCache;

private:
  static nsIAccessibilityService *sAccService;
};

#endif

