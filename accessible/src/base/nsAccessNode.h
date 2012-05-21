/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* For documentation of the accessibility architecture, 
 * see http://lxr.mozilla.org/seamonkey/source/accessible/accessible-docs.html
 */

#ifndef _nsAccessNode_H_
#define _nsAccessNode_H_

#include "nsIAccessibleTypes.h"

#include "a11yGeneric.h"

#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsINameSpaceManager.h"
#include "nsIStringBundle.h"
#include "nsWeakReference.h"

class nsAccessNode;
class nsDocAccessible;
class nsIAccessibleDocument;

namespace mozilla {
namespace a11y {
class ApplicationAccessible;
class RootAccessible;
}
}

class nsIPresShell;
class nsPresContext;
class nsIFrame;
class nsIDocShellTreeItem;

class nsAccessNode: public nsISupports
{
public:

  nsAccessNode(nsIContent* aContent, nsDocAccessible* aDoc);
  virtual ~nsAccessNode();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsAccessNode)

  static void ShutdownXPAccessibility();

  /**
   * Return an application accessible.
   */
  static mozilla::a11y::ApplicationAccessible* GetApplicationAccessible();

  /**
   * Return the document accessible for this access node.
   */
  nsDocAccessible* Document() const { return mDoc; }

  /**
   * Return the root document accessible for this accessnode.
   */
  mozilla::a11y::RootAccessible* RootAccessible() const;

  /**
   * Initialize the access node object, add it to the cache.
   */
  virtual bool Init();

  /**
   * Shutdown the access node object.
   */
  virtual void Shutdown();

  /**
   * Return frame for the given access node object.
   */
  virtual nsIFrame* GetFrame() const;
  /**
   * Return DOM node associated with the accessible.
   */
  virtual nsINode* GetNode() const { return mContent; }
  nsIContent* GetContent() const { return mContent; }
  virtual nsIDocument* GetDocumentNode() const
    { return mContent ? mContent->OwnerDoc() : nsnull; }

  /**
   * Return node type information of DOM node associated with the accessible.
   */
  bool IsContent() const
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

  /**
   * Interface methods on nsIAccessible shared with ISimpleDOM.
   */
  void Language(nsAString& aLocale);

protected:
  void LastRelease();

  nsCOMPtr<nsIContent> mContent;
  nsDocAccessible* mDoc;

private:
  nsAccessNode() MOZ_DELETE;
  nsAccessNode(const nsAccessNode&) MOZ_DELETE;
  nsAccessNode& operator =(const nsAccessNode&) MOZ_DELETE;
  
  static mozilla::a11y::ApplicationAccessible* gApplicationAccessible;
};

#endif

