/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XMLStylesheetProcessingInstruction_h
#define mozilla_dom_XMLStylesheetProcessingInstruction_h

#include "mozilla/Attributes.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/ProcessingInstruction.h"
#include "nsIURI.h"
#include "nsStyleLinkElement.h"

namespace mozilla {
namespace dom {

class XMLStylesheetProcessingInstruction final
  : public ProcessingInstruction
{
public:
  XMLStylesheetProcessingInstruction(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                                     const nsAString& aData)
    : ProcessingInstruction(Move(aNodeInfo), aData)
  {
  }

  XMLStylesheetProcessingInstruction(nsNodeInfoManager* aNodeInfoManager,
                                     const nsAString& aData)
    : ProcessingInstruction(aNodeInfoManager->GetNodeInfo(
                                       nsGkAtoms::processingInstructionTagName,
                                       nullptr, kNameSpaceID_None,
                                       PROCESSING_INSTRUCTION_NODE,
                                       nsGkAtoms::xml_stylesheet), aData)
  {
  }

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // CC
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XMLStylesheetProcessingInstruction,
                                           ProcessingInstruction)

  // nsIDOMNode
  virtual void SetNodeValueInternal(const nsAString& aNodeValue,
                                    mozilla::ErrorResult& aError) override;

  // nsIContent
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) override;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) override;

  // nsIStyleSheetLinkingElement
  virtual void OverrideBaseURI(nsIURI* aNewBaseURI) override;

  // nsStyleLinkElement
  void GetCharset(nsAString& aCharset) override;

  virtual void SetData(const nsAString& aData, mozilla::ErrorResult& rv) override
  {
    CharacterData::SetData(aData, rv);
    if (rv.Failed()) {
      return;
    }
    Unused << UpdateStyleSheetInternal(nullptr, nullptr, ForceUpdate::Yes);
  }

protected:
  virtual ~XMLStylesheetProcessingInstruction();

  nsCOMPtr<nsIURI> mOverriddenBaseURI;

  Maybe<SheetInfo> GetStyleSheetInfo() final;

  already_AddRefed<CharacterData>
    CloneDataNode(mozilla::dom::NodeInfo* aNodeInfo,
                  bool aCloneText) const final;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_XMLStylesheetProcessingInstruction_h
