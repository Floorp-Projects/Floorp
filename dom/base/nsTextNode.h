/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTextNode_h
#define nsTextNode_h

/*
 * Implementation of DOM Core's Text node.
 */

#include "mozilla/Attributes.h"
#include "mozilla/dom/Text.h"
#include "nsDebug.h"

class nsNodeInfoManager;

/**
 * Class used to implement DOM text nodes
 */
class nsTextNode : public mozilla::dom::Text {
 private:
  void Init() {
    MOZ_ASSERT(mNodeInfo->NodeType() == TEXT_NODE, "Bad NodeType in aNodeInfo");
  }

 public:
  explicit nsTextNode(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : mozilla::dom::Text(std::move(aNodeInfo)) {
    Init();
  }

  explicit nsTextNode(nsNodeInfoManager* aNodeInfoManager)
      : mozilla::dom::Text(aNodeInfoManager->GetTextNodeInfo()) {
    Init();
  }

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsINode
  already_AddRefed<CharacterData> CloneDataNode(
      mozilla::dom::NodeInfo* aNodeInfo, bool aCloneText) const override;

  nsresult BindToTree(BindContext&, nsINode& aParent) override;
  void UnbindFromTree(bool aNullParent = true) override;

  nsresult AppendTextForNormalize(const char16_t* aBuffer, uint32_t aLength,
                                  bool aNotify, nsIContent* aNextSibling);

#ifdef MOZ_DOM_LIST
  virtual void List(FILE* out, int32_t aIndent) const override;
  virtual void DumpContent(FILE* out, int32_t aIndent,
                           bool aDumpAll) const override;
#endif

 protected:
  virtual ~nsTextNode();

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;
};

#endif  // nsTextNode_h
