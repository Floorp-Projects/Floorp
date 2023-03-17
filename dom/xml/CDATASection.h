/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CDATASection_h
#define mozilla_dom_CDATASection_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/Text.h"

namespace mozilla::dom {

class CDATASection final : public Text {
 private:
  void Init() {
    MOZ_ASSERT(mNodeInfo->NodeType() == CDATA_SECTION_NODE,
               "Bad NodeType in aNodeInfo");
  }

  virtual ~CDATASection();

 public:
  explicit CDATASection(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : Text(std::move(aNodeInfo)) {
    Init();
  }

  explicit CDATASection(nsNodeInfoManager* aNodeInfoManager)
      : Text(aNodeInfoManager->GetNodeInfo(nsGkAtoms::cdataTagName, nullptr,
                                           kNameSpaceID_None,
                                           CDATA_SECTION_NODE)) {
    Init();
  }

  // nsISupports
  NS_INLINE_DECL_REFCOUNTING_INHERITED(CDATASection, Text)

  // nsINode
  already_AddRefed<CharacterData> CloneDataNode(
      mozilla::dom::NodeInfo* aNodeInfo, bool aCloneText) const override;

#ifdef MOZ_DOM_LIST
  void List(FILE* out, int32_t aIndent) const override;
  void DumpContent(FILE* out, int32_t aIndent, bool aDumpAll) const override;
#endif

 protected:
  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_CDATASection_h
