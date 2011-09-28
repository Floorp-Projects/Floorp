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

#include "nsIAccessNode.h"
#include "nsIAccessibleTypes.h"

#include "a11yGeneric.h"

#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsINameSpaceManager.h"
#include "nsIStringBundle.h"
#include "nsWeakReference.h"

class nsAccessNode;
class nsApplicationAccessible;
class nsDocAccessible;
class nsIAccessibleDocument;
class nsRootAccessible;

class nsIPresShell;
class nsPresContext;
class nsIFrame;
class nsIDocShellTreeItem;

#define ACCESSIBLE_BUNDLE_URL "chrome://global-platform/locale/accessible.properties"
#define PLATFORM_KEYS_BUNDLE_URL "chrome://global-platform/locale/platformKeys.properties"

#define NS_ACCESSNODE_IMPL_CID                          \
{  /* 2b07e3d7-00b3-4379-aa0b-ea22e2c8ffda */           \
  0x2b07e3d7,                                           \
  0x00b3,                                               \
  0x4379,                                               \
  { 0xaa, 0x0b, 0xea, 0x22, 0xe2, 0xc8, 0xff, 0xda }    \
}

class nsAccessNode: public nsIAccessNode
{
public:

  nsAccessNode(nsIContent *aContent, nsIWeakReference *aShell);
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
  static nsApplicationAccessible* GetApplicationAccessible();

  /**
   * Return the document accessible for this access node.
   */
  nsDocAccessible *GetDocAccessible() const;

  /**
   * Return the root document accessible for this accessnode.
   */
  nsRootAccessible* RootAccessible() const;

  /**
   * Return focused node within accessible window.
   *
   * XXX: it shouldn't break us if we return focused node not depending on
   * window so that we can turn this method into util method.
   */
  already_AddRefed<nsINode> GetCurrentFocus();

  /**
   * Initialize the access node object, add it to the cache.
   */
  virtual PRBool Init();

  /**
   * Shutdown the access node object.
   */
  virtual void Shutdown();

  /**
   * Returns true when the accessible is defunct.
   */
  virtual bool IsDefunct() const;

  /**
   * Return frame for the given access node object.
   */
  virtual nsIFrame* GetFrame() const;

  /**
   * Return DOM node associated with this accessible.
   */
  already_AddRefed<nsIDOMNode> GetDOMNode() const
  {
    nsIDOMNode *DOMNode = nsnull;
    if (GetNode())
      CallQueryInterface(GetNode(), &DOMNode);
    return DOMNode;
  }

  /**
   * Return DOM node associated with the accessible.
   */
  virtual nsINode* GetNode() const { return mContent; }
  nsIContent* GetContent() const { return mContent; }
  virtual nsIDocument* GetDocumentNode() const
    { return mContent ? mContent->GetOwnerDoc() : nsnull; }

  /**
   * Return node type information of DOM node associated with the accessible.
   */
  PRBool IsContent() const
  {
    return GetNode() && GetNode()->IsNodeOfType(nsINode::eCONTENT);
  }
  bool IsElement() const
  {
    nsINode* node = GetNode();
    return node && node->IsElement();
  }
  bool IsDocumentNode() const
  {
    return GetNode() && GetNode()->IsNodeOfType(nsINode::eDOCUMENT);
  }

  /**
   * Return the corresponding press shell for this accessible.
   */
  already_AddRefed<nsIPresShell> GetPresShell();

  /**
   * Return presentation shell for the accessible.
   */
  nsIWeakReference* GetWeakShell() const { return mWeakShell; }

  /**
   * Return the unique identifier of the accessible.
   */
  void* UniqueID() { return static_cast<void*>(this); }

  /**
   * Return true if the accessible is primary accessible for the given DOM node.
   *
   * Accessible hierarchy may be complex for single DOM node, in this case
   * these accessibles share the same DOM node. The primary accessible "owns"
   * that DOM node in terms it gets stored in the accessible to node map.
   */
  virtual bool IsPrimaryForNode() const;

protected:
    nsPresContext* GetPresContext();

    void LastRelease();

  nsCOMPtr<nsIContent> mContent;
  nsCOMPtr<nsIWeakReference> mWeakShell;

    /**
     * Notify global nsIObserver's that a11y is getting init'd or shutdown
     */
    static void NotifyA11yInitOrShutdown(PRBool aIsInit);

    // Static data, we do our own refcounting for our static data
    static nsIStringBundle *gStringBundle;

    static PRBool gIsFormFillEnabled;

private:
  static nsApplicationAccessible *gApplicationAccessible;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsAccessNode,
                              NS_ACCESSNODE_IMPL_CID)

#endif

