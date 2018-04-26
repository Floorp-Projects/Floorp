/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(XMLStylesheetProcessingInstruction,
                                             ProcessingInstruction,
                                             nsIDOMNode,
                                             nsIStyleSheetLinkingElement)

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
XMLStylesheetProcessingInstruction::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return XMLStylesheetProcessingInstructionBinding::Wrap(aCx, this, aGivenProto);
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
  nsContentUtils::AddScriptRunner(NewRunnableMethod(
    "dom::XMLStylesheetProcessingInstruction::BindToTree", this, update));

  return rv;
}

void
XMLStylesheetProcessingInstruction::UnbindFromTree(bool aDeep, bool aNullParent)
{
  nsCOMPtr<nsIDocument> oldDoc = GetUncomposedDoc();

  ProcessingInstruction::UnbindFromTree(aDeep, aNullParent);
  Unused << UpdateStyleSheetInternal(oldDoc, nullptr);
}

// nsIDOMNode

void
XMLStylesheetProcessingInstruction::SetNodeValueInternal(const nsAString& aNodeValue,
                                                         ErrorResult& aError)
{
  CharacterData::SetNodeValueInternal(aNodeValue, aError);
  if (!aError.Failed()) {
    Unused << UpdateStyleSheetInternal(nullptr, nullptr, ForceUpdate::Yes);
  }
}

// nsStyleLinkElement

void
XMLStylesheetProcessingInstruction::GetCharset(nsAString& aCharset)
{
  if (!GetAttrValue(nsGkAtoms::charset, aCharset)) {
    aCharset.Truncate();
  }
}

/* virtual */ void
XMLStylesheetProcessingInstruction::OverrideBaseURI(nsIURI* aNewBaseURI)
{
  mOverriddenBaseURI = aNewBaseURI;
}

already_AddRefed<nsIURI>
XMLStylesheetProcessingInstruction::GetStyleSheetURL(bool* aIsInline, nsIPrincipal** aTriggeringPrincipal)
{
  *aIsInline = false;
  *aTriggeringPrincipal = nullptr;

  nsAutoString href;
  if (!GetAttrValue(nsGkAtoms::href, href)) {
    return nullptr;
  }

  nsIURI *baseURL;
  nsIDocument *document = OwnerDoc();
  baseURL = mOverriddenBaseURI ?
            mOverriddenBaseURI.get() :
            document->GetDocBaseURI();
  auto encoding = document->GetDocumentCharacterSet();

  nsCOMPtr<nsIURI> aURI;
  NS_NewURI(getter_AddRefs(aURI), href, encoding, baseURL);
  return aURI.forget();
}

void
XMLStylesheetProcessingInstruction::GetStyleSheetInfo(nsAString& aTitle,
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

  // Make sure the type handling here matches
  // nsXMLContentSink::HandleProcessingInstruction
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
}

already_AddRefed<CharacterData>
XMLStylesheetProcessingInstruction::CloneDataNode(mozilla::dom::NodeInfo *aNodeInfo,
                                                  bool aCloneText) const
{
  nsAutoString data;
  GetData(data);
  RefPtr<mozilla::dom::NodeInfo> ni = aNodeInfo;
  return do_AddRef(new XMLStylesheetProcessingInstruction(ni.forget(), data));
}

} // namespace dom
} // namespace mozilla
