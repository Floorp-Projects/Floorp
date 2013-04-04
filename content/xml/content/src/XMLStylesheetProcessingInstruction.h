/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XMLStylesheetProcessingInstruction_h
#define mozilla_dom_XMLStylesheetProcessingInstruction_h

#include "mozilla/dom/ProcessingInstruction.h"
#include "nsStyleLinkElement.h"

namespace mozilla {
namespace dom {

class XMLStylesheetProcessingInstruction : public ProcessingInstruction,
                                           public nsStyleLinkElement
{
public:
  XMLStylesheetProcessingInstruction(already_AddRefed<nsINodeInfo> aNodeInfo,
                                     const nsAString& aData)
    : ProcessingInstruction(aNodeInfo, aData)
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

  virtual JSObject* WrapNode(JSContext *aCx, JSObject *aScope) MOZ_OVERRIDE;

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // CC
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XMLStylesheetProcessingInstruction,
                                           ProcessingInstruction)

  // nsIDOMNode
  virtual void SetNodeValueInternal(const nsAString& aNodeValue,
                                    mozilla::ErrorResult& aError);

  // nsIContent
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);

  // nsIStyleSheetLinkingElement
  virtual void OverrideBaseURI(nsIURI* aNewBaseURI);

  // nsStyleLinkElement
  NS_IMETHOD GetCharset(nsAString& aCharset);

protected:
  nsCOMPtr<nsIURI> mOverriddenBaseURI;

  already_AddRefed<nsIURI> GetStyleSheetURL(bool* aIsInline);
  void GetStyleSheetInfo(nsAString& aTitle,
                         nsAString& aType,
                         nsAString& aMedia,
                         bool* aIsScoped,
                         bool* aIsAlternate);
  virtual nsGenericDOMDataNode* CloneDataNode(nsINodeInfo *aNodeInfo,
                                              bool aCloneText) const;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_XMLStylesheetProcessingInstruction_h
