/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CDATASection_h
#define mozilla_dom_CDATASection_h

#include "mozilla/Attributes.h"
#include "nsIDOMCDATASection.h"
#include "mozilla/dom/Text.h"

namespace mozilla {
namespace dom {

class CDATASection final : public Text,
                           public nsIDOMCDATASection
{
private:
  void Init()
  {
    MOZ_ASSERT(mNodeInfo->NodeType() == nsIDOMNode::CDATA_SECTION_NODE,
               "Bad NodeType in aNodeInfo");
  }

  virtual ~CDATASection();

public:
  explicit CDATASection(already_AddRefed<mozilla::dom::NodeInfo> aNodeInfo)
    : Text(aNodeInfo)
  {
    Init();
  }

  explicit CDATASection(nsNodeInfoManager* aNodeInfoManager)
    : Text(aNodeInfoManager->GetNodeInfo(nsGkAtoms::cdataTagName,
                                         nullptr, kNameSpaceID_None,
                                         nsIDOMNode::CDATA_SECTION_NODE))
  {
    Init();
  }

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMCharacterData
  NS_FORWARD_NSIDOMCHARACTERDATA(nsGenericDOMDataNode::)
  using nsGenericDOMDataNode::SetData; // Prevent hiding overloaded virtual function.

  // nsIDOMText
  NS_FORWARD_NSIDOMTEXT(nsGenericDOMDataNode::)

  // nsIDOMCDATASection
  // Empty interface

  // nsINode
  virtual bool IsNodeOfType(uint32_t aFlags) const override;

  virtual nsGenericDOMDataNode* CloneDataNode(mozilla::dom::NodeInfo *aNodeInfo,
                                              bool aCloneText) const override;

  virtual nsIDOMNode* AsDOMNode() override { return this; }
#ifdef DEBUG
  virtual void List(FILE* out, int32_t aIndent) const override;
  virtual void DumpContent(FILE* out, int32_t aIndent,bool aDumpAll) const override;
#endif

protected:
  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CDATASection_h
