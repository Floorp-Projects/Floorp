/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XMLStylesheetProcessingInstruction_h
#define mozilla_dom_XMLStylesheetProcessingInstruction_h

#include "mozilla/Attributes.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/LinkStyle.h"
#include "mozilla/dom/ProcessingInstruction.h"
#include "nsIURI.h"

namespace mozilla::dom {

class XMLStylesheetProcessingInstruction final : public ProcessingInstruction,
                                                 public LinkStyle {
 public:
  XMLStylesheetProcessingInstruction(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
      const nsAString& aData)
      : ProcessingInstruction(std::move(aNodeInfo), aData) {}

  XMLStylesheetProcessingInstruction(nsNodeInfoManager* aNodeInfoManager,
                                     const nsAString& aData)
      : ProcessingInstruction(
            aNodeInfoManager->GetNodeInfo(
                nsGkAtoms::processingInstructionTagName, nullptr,
                kNameSpaceID_None, PROCESSING_INSTRUCTION_NODE,
                nsGkAtoms::xml_stylesheet),
            aData) {}

  // CC
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XMLStylesheetProcessingInstruction,
                                           ProcessingInstruction)

  // nsINode
  virtual void SetNodeValueInternal(const nsAString& aNodeValue,
                                    mozilla::ErrorResult& aError) override;

  // nsIContent
  virtual nsresult BindToTree(BindContext&, nsINode& aParent) override;
  virtual void UnbindFromTree(bool aNullParent = true) override;

  /**
   * Tells this processing instruction to use a different base URI. This is used
   * for proper loading of xml-stylesheet processing instructions in XUL
   * overlays and is only currently used by nsXMLStylesheetPI.
   *
   * @param aNewBaseURI the new base URI, nullptr to use the default base URI.
   */
  void OverrideBaseURI(nsIURI* aNewBaseURI);

  // LinkStyle
  void GetCharset(nsAString& aCharset) override;

  virtual void SetData(const nsAString& aData,
                       mozilla::ErrorResult& rv) override {
    CharacterData::SetData(aData, rv);
    if (rv.Failed()) {
      return;
    }
    Unused << UpdateStyleSheetInternal(nullptr, nullptr, ForceUpdate::Yes);
  }

 protected:
  virtual ~XMLStylesheetProcessingInstruction();

  nsCOMPtr<nsIURI> mOverriddenBaseURI;

  nsIContent& AsContent() final { return *this; }
  const LinkStyle* AsLinkStyle() const final { return this; }
  Maybe<SheetInfo> GetStyleSheetInfo() final;

  already_AddRefed<CharacterData> CloneDataNode(
      mozilla::dom::NodeInfo* aNodeInfo, bool aCloneText) const final;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_XMLStylesheetProcessingInstruction_h
