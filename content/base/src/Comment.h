/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Comment_h
#define mozilla_dom_Comment_h

#include "nsIDOMComment.h"
#include "nsGenericDOMDataNode.h"

namespace mozilla {
namespace dom {

class Comment : public nsGenericDOMDataNode,
                public nsIDOMComment
{
private:
  void Init()
  {
    NS_ABORT_IF_FALSE(mNodeInfo->NodeType() == nsIDOMNode::COMMENT_NODE,
                      "Bad NodeType in aNodeInfo");
    SetIsDOMBinding();
  }

public:
  Comment(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsGenericDOMDataNode(aNodeInfo)
  {
    Init();
  }

  Comment(nsNodeInfoManager* aNodeInfoManager)
    : nsGenericDOMDataNode(aNodeInfoManager->GetCommentNodeInfo())
  {
    Init();
  }

  virtual ~Comment();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMCharacterData
  NS_FORWARD_NSIDOMCHARACTERDATA(nsGenericDOMDataNode::)

  // nsIDOMComment
  // Empty interface

  // nsINode
  virtual bool IsNodeOfType(uint32_t aFlags) const;

  virtual nsGenericDOMDataNode* CloneDataNode(nsINodeInfo *aNodeInfo,
                                              bool aCloneText) const;

  virtual nsIDOMNode* AsDOMNode() { return this; }
#ifdef DEBUG
  virtual void List(FILE* out, int32_t aIndent) const;
  virtual void DumpContent(FILE* out = stdout, int32_t aIndent = 0,
                           bool aDumpAll = true) const
  {
    return;
  }
#endif

protected:
  virtual JSObject* WrapNode(JSContext *aCx, JSObject *aScope) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Comment_h
