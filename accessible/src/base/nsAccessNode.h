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
#include "nsCoreUtils.h"
#include "nsAccUtils.h"

#include "nsIAccessibleTypes.h"
#include "nsIAccessNode.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsINameSpaceManager.h"
#include "nsIStringBundle.h"
#include "nsWeakReference.h"
#include "nsInterfaceHashtable.h"
#include "nsAccessibilityService.h"

class nsIPresShell;
class nsPresContext;
class nsIAccessibleDocument;
class nsIFrame;
class nsIDOMNodeList;
class nsRootAccessible;
class nsApplicationAccessibleWrap;
class nsIDocShellTreeItem;

#define ACCESSIBLE_BUNDLE_URL "chrome://global-platform/locale/accessible.properties"
#define PLATFORM_KEYS_BUNDLE_URL "chrome://global-platform/locale/platformKeys.properties"

typedef nsInterfaceHashtable<nsVoidPtrHashKey, nsAccessNode>
        nsAccessNodeHashtable;

// What we want is: NS_INTERFACE_MAP_ENTRY(self) for static IID accessors,
// but some of our classes have an ambiguous base class of nsISupports which
// prevents this from working (the default macro converts it to nsISupports,
// then addrefs it, then returns it). Therefore, we expand the macro here and
// change it so that it works. Yuck.
#define NS_INTERFACE_MAP_STATIC_AMBIGUOUS(_class) \
  if (aIID.Equals(NS_GET_IID(_class))) { \
  NS_ADDREF(this); \
  *aInstancePtr = this; \
  return NS_OK; \
  } else

#define NS_OK_DEFUNCT_OBJECT \
NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_GENERAL, 0x22)

#define NS_ENSURE_A11Y_SUCCESS(res, ret)                                  \
  PR_BEGIN_MACRO                                                          \
    nsresult __rv = res; /* Don't evaluate |res| more than once */        \
    if (NS_FAILED(__rv)) {                                                \
      NS_ENSURE_SUCCESS_BODY(res, ret)                                    \
      return ret;                                                         \
    }                                                                     \
    if (__rv == NS_OK_DEFUNCT_OBJECT)                                     \
      return ret;                                                         \
  PR_END_MACRO

#define NS_ACCESSNODE_IMPL_CID                          \
{  /* 2b07e3d7-00b3-4379-aa0b-ea22e2c8ffda */           \
  0x2b07e3d7,                                           \
  0x00b3,                                               \
  0x4379,                                               \
  { 0xaa, 0x0b, 0xea, 0x22, 0xe2, 0xc8, 0xff, 0xda }    \
}

class nsAccessNode: public nsIAccessNode
{
  public: // construction, destruction
    nsAccessNode(nsIDOMNode *, nsIWeakReference* aShell);
    virtual ~nsAccessNode();

    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsAccessNode, nsIAccessNode)

    NS_DECL_NSIACCESSNODE
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_ACCESSNODE_IMPL_CID)

    static void InitXPAccessibility();
    static void ShutdownXPAccessibility();

    /**
     * Return an application accessible.
     */
    static already_AddRefed<nsApplicationAccessibleWrap> GetApplicationAccessible();

  /**
   * Clear the cache and shutdown the access nodes.
   */
  static void ClearCache(nsAccessNodeHashtable& aCache);

    // Static cache methods for global document cache
    static already_AddRefed<nsIAccessibleDocument> GetDocAccessibleFor(nsIDocument *aDocument);
    static already_AddRefed<nsIAccessibleDocument> GetDocAccessibleFor(nsIWeakReference *aWeakShell);
    static already_AddRefed<nsIAccessibleDocument> GetDocAccessibleFor(nsIDocShellTreeItem *aContainer, PRBool aCanCreate = PR_FALSE);
    static already_AddRefed<nsIAccessibleDocument> GetDocAccessibleFor(nsIDOMNode *aNode);

    already_AddRefed<nsRootAccessible> GetRootAccessible();

    static nsIDOMNode *gLastFocusedNode;

    already_AddRefed<nsIDOMNode> GetCurrentFocus();

    /**
     * Returns true when the accessible is defunct.
     */
    virtual PRBool IsDefunct();

    /**
     * Initialize the access node object, add it to the cache.
     */
    virtual nsresult Init();

    /**
     * Shutdown the access node object.
     */
    virtual nsresult Shutdown();

    /**
     * Return frame for the given access node object.
     */
    virtual nsIFrame* GetFrame();

  /**
   * Return the corresponding press shell for this accessible.
   */
  already_AddRefed<nsIPresShell> GetPresShell();

  /**
   * Return true if the accessible still has presentation shell. Light-weight
   * version of IsDefunct() method.
   */
  PRBool HasWeakShell() const { return !!mWeakShell; }

protected:
    nsresult MakeAccessNode(nsIDOMNode *aNode, nsIAccessNode **aAccessNode);

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
    static void NotifyA11yInitOrShutdown(PRBool aIsInit);

    // Static data, we do our own refcounting for our static data
    static nsIStringBundle *gStringBundle;
    static nsIStringBundle *gKeyStringBundle;

    static PRBool gIsCacheDisabled;
    static PRBool gIsFormFillEnabled;

    static nsAccessNodeHashtable gGlobalDocAccessibleCache;

private:
  static nsApplicationAccessibleWrap *gApplicationAccessible;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsAccessNode,
                              NS_ACCESSNODE_IMPL_CID)

#endif

