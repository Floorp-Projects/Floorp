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
#include "nsIAccessibleTypes.h"
#include "nsIAccessNode.h"
#include "nsIContent.h"
#include "nsPIAccessNode.h"
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
class nsApplicationAccessibleWrap;
class nsIDocShellTreeItem;

#define ACCESSIBLE_BUNDLE_URL "chrome://global-platform/locale/accessible.properties"
#define PLATFORM_KEYS_BUNDLE_URL "chrome://global-platform/locale/platformKeys.properties"

typedef nsInterfaceHashtable<nsVoidPtrHashKey, nsIAccessNode>
        nsAccessNodeHashtable;

/**
 * Does the current content have this ARIA role? 
 * Implemented as a compiler macro so that length can be computed at compile time.
 * @param aContent  Node to get role string from
 * @param aRoleName Role string to compare with -- literal const char*
 * @return PR_TRUE if there is a match
 */
#define ARIARoleEquals(aContent, aRoleName) \
  nsAccessNode::ARIARoleEqualsImpl(aContent, aRoleName, NS_ARRAY_LENGTH(aRoleName) - 1)

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

    /**
     * Return an application accessible.
     */
    static already_AddRefed<nsApplicationAccessibleWrap> GetApplicationAccessible();

    // Static methods for handling per-document cache
    static void PutCacheEntry(nsAccessNodeHashtable& aCache,
                              void* aUniqueID, nsIAccessNode *aAccessNode);
    static void GetCacheEntry(nsAccessNodeHashtable& aCache,
                              void* aUniqueID, nsIAccessNode **aAccessNode);
    static void ClearCache(nsAccessNodeHashtable& aCache);

    static PLDHashOperator PR_CALLBACK ClearCacheEntry(const void* aKey, nsCOMPtr<nsIAccessNode>& aAccessNode, void* aUserArg);

    // Static cache methods for global document cache
    static already_AddRefed<nsIAccessibleDocument> GetDocAccessibleFor(nsIDocument *aDocument);
    static already_AddRefed<nsIAccessibleDocument> GetDocAccessibleFor(nsIWeakReference *aWeakShell);
    static already_AddRefed<nsIAccessibleDocument> GetDocAccessibleFor(nsIDocShellTreeItem *aContainer, PRBool aCanCreate = PR_FALSE);
    static already_AddRefed<nsIAccessibleDocument> GetDocAccessibleFor(nsIDOMNode *aNode);

    static already_AddRefed<nsIDOMNode> GetDOMNodeForContainer(nsISupports *aContainer);
    static already_AddRefed<nsIPresShell> GetPresShellFor(nsIDOMNode *aStartNode);
    
    // Return PR_TRUE if there is a role attribute
    static PRBool HasRoleAttribute(nsIContent *aContent)
    {
      return (aContent->IsNodeOfType(nsINode::eHTML) && aContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::role)) ||
              aContent->HasAttr(kNameSpaceID_XHTML, nsAccessibilityAtoms::role) ||
              aContent->HasAttr(kNameSpaceID_XHTML2_Unofficial, nsAccessibilityAtoms::role);
    }

    /**
     * Provide the role string if there is one
     * @param aContent Node to get role string from
     * @param aRole String to fill role into
     * @return PR_TRUE if there is a role attribute, and fill it into aRole
     */
    static PRBool GetARIARole(nsIContent *aContent, nsString& aRole);

    static PRBool ARIARoleEqualsImpl(nsIContent* aContent, const char* aRoleName, PRUint32 aLen)
      { nsAutoString role; return GetARIARole(aContent, role) && role.EqualsASCII(aRoleName, aLen); }

    static void GetComputedStyleDeclaration(const nsAString& aPseudoElt,
                                            nsIDOMElement *aElement,
                                            nsIDOMCSSStyleDeclaration **aCssDecl);

    already_AddRefed<nsRootAccessible> GetRootAccessible();

    static nsIDOMNode *gLastFocusedNode;
    static nsIAccessibilityService* GetAccService();
    already_AddRefed<nsIDOMNode> GetCurrentFocus();

protected:
    nsresult MakeAccessNode(nsIDOMNode *aNode, nsIAccessNode **aAccessNode);
    already_AddRefed<nsIPresShell> GetPresShell();
    nsPresContext* GetPresContext();
    already_AddRefed<nsIAccessibleDocument> GetDocAccessible();
    void LastRelease();

    nsCOMPtr<nsIDOMNode> mDOMNode;
    nsCOMPtr<nsIWeakReference> mWeakShell;

#ifdef DEBUG_A11Y
    PRBool mIsInitialized;
#endif

    /**
     * Notify global nsIObserver's that a11y is getting init'd or shutdown
     */
    static void NotifyA11yInitOrShutdown();

    // Static data, we do our own refcounting for our static data
    static nsIStringBundle *gStringBundle;
    static nsIStringBundle *gKeyStringBundle;
    static nsITimer *gDoCommandTimer;
    static PRBool gIsAccessibilityActive;
    static PRBool gIsCacheDisabled;
    static PRBool gIsFormFillEnabled;

    static nsAccessNodeHashtable gGlobalDocAccessibleCache;

private:
  static nsIAccessibilityService *sAccService;
  static nsApplicationAccessibleWrap *gApplicationAccessible;
};

#endif

