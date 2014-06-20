/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XMLStylesheetProcessingInstruction_h
#define mozilla_dom_XMLStylesheetProcessingInstruction_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/ProcessingInstruction.h"
#include "nsIURI.h"
#include "nsStyleLinkElement.h"

namespace mozilla {
namespace dom {

class XMLStylesheetProcessingInstruction MOZ_FINAL
: public ProcessingInstruction
, public nsStyleLinkElement
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
                                       nsIDOMNode::PROCESSING_INSTRUCTION_NODE,
                                       nsGkAtoms::xml_stylesheet), aData)
  {
  }

  virtual ~XMLStylesheetProcessingInstruction();

  virtual JSObject* WrapNode(JSContext *aCx) MOZ_OVERRIDE;

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // CC
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XMLStylesheetProcessingInstruction,
                                           ProcessingInstruction)

  // nsIDOMNode
  virtual void SetNodeValueInternal(const nsAString& aNodeValue,
                                    mozilla::ErrorResult& aError) MOZ_OVERRIDE;

  // nsIContent
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) MOZ_OVERRIDE;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) MOZ_OVERRIDE;

  // nsIStyleSheetLinkingElement
  virtual void OverrideBaseURI(nsIURI* aNewBaseURI) MOZ_OVERRIDE;

  // nsStyleLinkElement
  NS_IMETHOD GetCharset(nsAString& aCharset) MOZ_OVERRIDE;

  virtual void SetData(const nsAString& aData, mozilla::ErrorResult& rv) MOZ_OVERRIDE
  {
    nsGenericDOMDataNode::SetData(aData, rv);
    if (rv.Failed()) {
      return;
    }
    UpdateStyleSheetInternal(nullptr, nullptr, true);
  }
  using ProcessingInstruction::SetData; // Prevent hiding overloaded virtual function.

protected:
  nsCOMPtr<nsIURI> mOverriddenBaseURI;

  already_AddRefed<nsIURI> GetStyleSheetURL(bool* aIsInline) MOZ_OVERRIDE;
  void GetStyleSheetInfo(nsAString& aTitle,
                         nsAString& aType,
                         nsAString& aMedia,
                         bool* aIsScoped,
                         bool* aIsAlternate) MOZ_OVERRIDE;
  virtual nsGenericDOMDataNode* CloneDataNode(mozilla::dom::NodeInfo *aNodeInfo,
                                              bool aCloneText) const MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_XMLStylesheetProcessingInstruction_h
