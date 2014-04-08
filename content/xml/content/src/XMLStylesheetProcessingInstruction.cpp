/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XMLStylesheetProcessingInstruction.h"
#include "mozilla/dom/XMLStylesheetProcessingInstructionBinding.h"
#include "nsContentUtils.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace dom {

// nsISupports implementation

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(XMLStylesheetProcessingInstruction)
  NS_INTERFACE_TABLE_INHERITED3(XMLStylesheetProcessingInstruction, nsIDOMNode,
                                nsIDOMProcessingInstruction,
                                nsIStyleSheetLinkingElement)
NS_INTERFACE_TABLE_TAIL_INHERITING(ProcessingInstruction)

NS_IMPL_ADDREF_INHERITED(XMLStylesheetProcessingInstruction,
                         ProcessingInstruction)
NS_IMPL_RELEASE_INHERITED(XMLStylesheetProcessingInstruction,
                          ProcessingInstruction)

NS_IMPL_CYCLE_COLLECTION_CLASS(XMLStylesheetProcessingInstruction)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(XMLStylesheetProcessingInstruction,
                                                  ProcessingInstruction)
  tmp->nsStyleLinkElement::Traverse(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(XMLStylesheetProcessingInstruction,
                                                ProcessingInstruction)
  tmp->nsStyleLinkElement::Unlink();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END


XMLStylesheetProcessingInstruction::~XMLStylesheetProcessingInstruction()
{
}

JSObject*
XMLStylesheetProcessingInstruction::WrapNode(JSContext *aCx)
{
  return XMLStylesheetProcessingInstructionBinding::Wrap(aCx, this);
}

// nsIContent

nsresult
XMLStylesheetProcessingInstruction::BindToTree(nsIDocument* aDocument,
                                               nsIContent* aParent,
                                               nsIContent* aBindingParent,
                                               bool aCompileEventHandlers)
{
  nsresult rv = ProcessingInstruction::BindToTree(aDocument, aParent,
                                                  aBindingParent,
                                                  aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  void (XMLStylesheetProcessingInstruction::*update)() =
    &XMLStylesheetProcessingInstruction::UpdateStyleSheetInternal;
  nsContentUtils::AddScriptRunner(NS_NewRunnableMethod(this, update));

  return rv;  
}

void
XMLStylesheetProcessingInstruction::UnbindFromTree(bool aDeep, bool aNullParent)
{
  nsCOMPtr<nsIDocument> oldDoc = GetCurrentDoc();

  ProcessingInstruction::UnbindFromTree(aDeep, aNullParent);
  UpdateStyleSheetInternal(oldDoc, nullptr);
}

// nsIDOMNode

void
XMLStylesheetProcessingInstruction::SetNodeValueInternal(const nsAString& aNodeValue,
                                                         ErrorResult& aError)
{
  nsGenericDOMDataNode::SetNodeValueInternal(aNodeValue, aError);
  if (!aError.Failed()) {
    UpdateStyleSheetInternal(nullptr, nullptr, true);
  }
}

// nsStyleLinkElement

NS_IMETHODIMP
XMLStylesheetProcessingInstruction::GetCharset(nsAString& aCharset)
{
  return GetAttrValue(nsGkAtoms::charset, aCharset) ? NS_OK : NS_ERROR_FAILURE;
}

/* virtual */ void
XMLStylesheetProcessingInstruction::OverrideBaseURI(nsIURI* aNewBaseURI)
{
  mOverriddenBaseURI = aNewBaseURI;
}

already_AddRefed<nsIURI>
XMLStylesheetProcessingInstruction::GetStyleSheetURL(bool* aIsInline)
{
  *aIsInline = false;

  nsAutoString href;
  if (!GetAttrValue(nsGkAtoms::href, href)) {
    return nullptr;
  }

  nsIURI *baseURL;
  nsAutoCString charset;
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
XMLStylesheetProcessingInstruction::GetStyleSheetInfo(nsAString& aTitle,
                                                      nsAString& aType,
                                                      nsAString& aMedia,
                                                      bool* aIsScoped,
                                                      bool* aIsAlternate)
{
  aTitle.Truncate();
  aType.Truncate();
  aMedia.Truncate();
  *aIsScoped = false;
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
XMLStylesheetProcessingInstruction::CloneDataNode(nsINodeInfo *aNodeInfo,
                                                  bool aCloneText) const
{
  nsAutoString data;
  nsGenericDOMDataNode::GetData(data);
  nsCOMPtr<nsINodeInfo> ni = aNodeInfo;
  return new XMLStylesheetProcessingInstruction(ni.forget(), data);
}

} // namespace dom
} // namespace mozilla
