/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CDATASection_h
#define mozilla_dom_CDATASection_h

#include "nsIDOMCDATASection.h"
#include "mozilla/dom/Text.h"

namespace mozilla {
namespace dom {

class CDATASection : public Text,
                     public nsIDOMCDATASection
{
private:
  void Init()
  {
    NS_ABORT_IF_FALSE(mNodeInfo->NodeType() == nsIDOMNode::CDATA_SECTION_NODE,
                      "Bad NodeType in aNodeInfo");
    SetIsDOMBinding();
  }

public:
  CDATASection(already_AddRefed<nsINodeInfo> aNodeInfo)
    : Text(aNodeInfo)
  {
    Init();
  }

  CDATASection(nsNodeInfoManager* aNodeInfoManager)
    : Text(aNodeInfoManager->GetNodeInfo(nsGkAtoms::cdataTagName,
                                         nullptr, kNameSpaceID_None,
                                         nsIDOMNode::CDATA_SECTION_NODE))
  {
    Init();
  }

  virtual ~CDATASection();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMCharacterData
  NS_FORWARD_NSIDOMCHARACTERDATA(nsGenericDOMDataNode::)

  // nsIDOMText
  NS_FORWARD_NSIDOMTEXT(nsGenericDOMDataNode::)

  // nsIDOMCDATASection
  // Empty interface

  // nsINode
  virtual bool IsNodeOfType(uint32_t aFlags) const;

  virtual nsGenericDOMDataNode* CloneDataNode(nsINodeInfo *aNodeInfo,
                                              bool aCloneText) const;

  virtual nsIDOMNode* AsDOMNode() { return this; }
#ifdef DEBUG
  virtual void List(FILE* out, int32_t aIndent) const;
  virtual void DumpContent(FILE* out, int32_t aIndent,bool aDumpAll) const;
#endif

protected:
  virtual JSObject* WrapNode(JSContext *aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CDATASection_h
