/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIDOMLinkStyle.h"
#include "nsIDOMStyleSheet.h"
#include "nsIDocument.h"
#include "nsIStyleSheet.h"
#include "nsIURI.h"
#include "nsStyleLinkElement.h"
#include "nsNetUtil.h"
#include "nsXMLProcessingInstruction.h"
#include "nsUnicharUtils.h"
#include "nsGkAtoms.h"
#include "nsThreadUtils.h"
#include "nsContentUtils.h"

class nsXMLStylesheetPI : public nsXMLProcessingInstruction,
                          public nsStyleLinkElement
{
public:
  nsXMLStylesheetPI(already_AddRefed<nsINodeInfo> aNodeInfo, const nsAString& aData);
  virtual ~nsXMLStylesheetPI();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_IMETHOD SetNodeValue(const nsAString& aData);

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

  virtual nsXPCClassInfo* GetClassInfo();
protected:
  nsCOMPtr<nsIURI> mOverriddenBaseURI;

  already_AddRefed<nsIURI> GetStyleSheetURL(bool* aIsInline);
  void GetStyleSheetInfo(nsAString& aTitle,
                         nsAString& aType,
                         nsAString& aMedia,
                         bool* aIsAlternate);
  virtual nsGenericDOMDataNode* CloneDataNode(nsINodeInfo *aNodeInfo,
                                              bool aCloneText) const;
};

// nsISupports implementation

DOMCI_NODE_DATA(XMLStylesheetProcessingInstruction, nsXMLStylesheetPI)

NS_INTERFACE_TABLE_HEAD(nsXMLStylesheetPI)
  NS_NODE_INTERFACE_TABLE4(nsXMLStylesheetPI, nsIDOMNode,
                           nsIDOMProcessingInstruction, nsIDOMLinkStyle,
                           nsIStyleSheetLinkingElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(XMLStylesheetProcessingInstruction)
NS_INTERFACE_MAP_END_INHERITING(nsXMLProcessingInstruction)

NS_IMPL_ADDREF_INHERITED(nsXMLStylesheetPI, nsXMLProcessingInstruction)
NS_IMPL_RELEASE_INHERITED(nsXMLStylesheetPI, nsXMLProcessingInstruction)


nsXMLStylesheetPI::nsXMLStylesheetPI(already_AddRefed<nsINodeInfo> aNodeInfo,
                                     const nsAString& aData)
  : nsXMLProcessingInstruction(aNodeInfo, aData)
{
}

nsXMLStylesheetPI::~nsXMLStylesheetPI()
{
}

// nsIContent

nsresult
nsXMLStylesheetPI::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers)
{
  nsresult rv = nsXMLProcessingInstruction::BindToTree(aDocument, aParent,
                                                       aBindingParent,
                                                       aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  void (nsXMLStylesheetPI::*update)() = &nsXMLStylesheetPI::UpdateStyleSheetInternal;
  nsContentUtils::AddScriptRunner(NS_NewRunnableMethod(this, update));

  return rv;  
}

void
nsXMLStylesheetPI::UnbindFromTree(bool aDeep, bool aNullParent)
{
  nsCOMPtr<nsIDocument> oldDoc = GetCurrentDoc();

  nsXMLProcessingInstruction::UnbindFromTree(aDeep, aNullParent);
  UpdateStyleSheetInternal(oldDoc);
}

// nsIDOMNode

NS_IMETHODIMP
nsXMLStylesheetPI::SetNodeValue(const nsAString& aNodeValue)
{
  nsresult rv = nsGenericDOMDataNode::SetNodeValue(aNodeValue);
  if (NS_SUCCEEDED(rv)) {
    UpdateStyleSheetInternal(nullptr, true);
  }
  return rv;
}

// nsStyleLinkElement

NS_IMETHODIMP
nsXMLStylesheetPI::GetCharset(nsAString& aCharset)
{
  return GetAttrValue(nsGkAtoms::charset, aCharset) ? NS_OK : NS_ERROR_FAILURE;
}

/* virtual */ void
nsXMLStylesheetPI::OverrideBaseURI(nsIURI* aNewBaseURI)
{
  mOverriddenBaseURI = aNewBaseURI;
}

already_AddRefed<nsIURI>
nsXMLStylesheetPI::GetStyleSheetURL(bool* aIsInline)
{
  *aIsInline = false;

  nsAutoString href;
  if (!GetAttrValue(nsGkAtoms::href, href)) {
    return nullptr;
  }

  nsIURI *baseURL;
  nsCAutoString charset;
  nsIDocument *document = OwnerDoc();
  baseURL = mOverriddenBaseURI ?
            mOverriddenBaseURI.get() :
            document->GetDocBaseURI();
  charset = document->GetDocumentCharacterSet();

  nsCOMPtr<nsIURI> aURI;
  NS_NewURI(getter_AddRefs(aURI), href, charset.get(), baseURL);
  return aURI.forget();
}

void
nsXMLStylesheetPI::GetStyleSheetInfo(nsAString& aTitle,
                                     nsAString& aType,
                                     nsAString& aMedia,
                                     bool* aIsAlternate)
{
  aTitle.Truncate();
  aType.Truncate();
  aMedia.Truncate();
  *aIsAlternate = false;

  // xml-stylesheet PI is special only in prolog
  if (!nsContentUtils::InProlog(this)) {
    return;
  }

  nsAutoString data;
  GetData(data);

  nsContentUtils::GetPseudoAttributeValue(data, nsGkAtoms::title, aTitle);

  nsAutoString alternate;
  nsContentUtils::GetPseudoAttributeValue(data,
                                          nsGkAtoms::alternate,
                                          alternate);

  // if alternate, does it have title?
  if (alternate.EqualsLiteral("yes")) {
    if (aTitle.IsEmpty()) { // alternates must have title
      return;
    }

    *aIsAlternate = true;
  }

  nsContentUtils::GetPseudoAttributeValue(data, nsGkAtoms::media, aMedia);

  nsAutoString type;
  nsContentUtils::GetPseudoAttributeValue(data, nsGkAtoms::type, type);

  nsAutoString mimeType, notUsed;
  nsContentUtils::SplitMimeType(type, mimeType, notUsed);
  if (!mimeType.IsEmpty() && !mimeType.LowerCaseEqualsLiteral("text/css")) {
    aType.Assign(type);
    return;
  }

  // If we get here we assume that we're loading a css file, so set the
  // type to 'text/css'
  aType.AssignLiteral("text/css");

  return;
}

nsGenericDOMDataNode*
nsXMLStylesheetPI::CloneDataNode(nsINodeInfo *aNodeInfo, bool aCloneText) const
{
  nsAutoString data;
  nsGenericDOMDataNode::GetData(data);
  nsCOMPtr<nsINodeInfo> ni = aNodeInfo;
  return new nsXMLStylesheetPI(ni.forget(), data);
}

nsresult
NS_NewXMLStylesheetProcessingInstruction(nsIContent** aInstancePtrResult,
                                         nsNodeInfoManager *aNodeInfoManager,
                                         const nsAString& aData)
{
  NS_PRECONDITION(aNodeInfoManager, "Missing nodeinfo manager");

  *aInstancePtrResult = nullptr;
  
  nsCOMPtr<nsINodeInfo> ni;
  ni = aNodeInfoManager->GetNodeInfo(nsGkAtoms::processingInstructionTagName,
                                     nullptr, kNameSpaceID_None,
                                     nsIDOMNode::PROCESSING_INSTRUCTION_NODE,
                                     nsGkAtoms::xml_stylesheet);
  NS_ENSURE_TRUE(ni, NS_ERROR_OUT_OF_MEMORY);

  nsXMLStylesheetPI *instance = new nsXMLStylesheetPI(ni.forget(), aData);
  if (!instance) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aInstancePtrResult = instance);

  return NS_OK;
}
