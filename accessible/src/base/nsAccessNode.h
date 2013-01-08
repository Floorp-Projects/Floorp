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
#include "nsINode.h"

class nsIContent;
class nsIDocShellTreeItem;
class nsIFrame;
class nsIPresShell;
class nsPresContext;

namespace mozilla {
namespace a11y {

class DocAccessible;
class RootAccessible;

class nsAccessNode : public nsISupports
{
public:

  nsAccessNode(nsIContent* aContent, DocAccessible* aDoc);
  virtual ~nsAccessNode();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsAccessNode)

  /**
   * Return the document accessible for this access node.
   */
  DocAccessible* Document() const { return mDoc; }

  /**
   * Return the root document accessible for this accessnode.
   */
  a11y::RootAccessible* RootAccessible() const;

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
  virtual nsINode* GetNode() const;
  nsIContent* GetContent() const { return mContent; }

  /**
   * Return node type information of DOM node associated with the accessible.
   */
  bool IsContent() const
  {
    return GetNode() && GetNode()->IsNodeOfType(nsINode::eCONTENT);
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
   * Interface methods on nsIAccessible shared with ISimpleDOM.
   */
  void Language(nsAString& aLocale);

protected:
  void LastRelease();

  nsCOMPtr<nsIContent> mContent;
  DocAccessible* mDoc;

private:
  nsAccessNode() MOZ_DELETE;
  nsAccessNode(const nsAccessNode&) MOZ_DELETE;
  nsAccessNode& operator =(const nsAccessNode&) MOZ_DELETE;
};

} // namespace a11y
} // namespace mozilla

#endif

