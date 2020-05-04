/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XMLStylesheetProcessingInstruction.h"
#include "nsContentUtils.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace dom {

// nsISupports implementation

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(XMLStylesheetProcessingInstruction,
                                             ProcessingInstruction,
                                             nsIStyleSheetLinkingElement)

NS_IMPL_CYCLE_COLLECTION_CLASS(XMLStylesheetProcessingInstruction)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(
    XMLStylesheetProcessingInstruction, ProcessingInstruction)
  tmp->nsStyleLinkElement::Traverse(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(
    XMLStylesheetProcessingInstruction, ProcessingInstruction)
  tmp->nsStyleLinkElement::Unlink();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

XMLStylesheetProcessingInstruction::~XMLStylesheetProcessingInstruction() =
    default;

// nsIContent

nsresult XMLStylesheetProcessingInstruction::BindToTree(BindContext& aContext,
                                                        nsINode& aParent) {
  nsresult rv = ProcessingInstruction::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);

  void (XMLStylesheetProcessingInstruction::*update)() =
      &XMLStylesheetProcessingInstruction::UpdateStyleSheetInternal;
  nsContentUtils::AddScriptRunner(NewRunnableMethod(
      "dom::XMLStylesheetProcessingInstruction::BindToTree", this, update));

  return rv;
}

void XMLStylesheetProcessingInstruction::UnbindFromTree(bool aNullParent) {
  nsCOMPtr<Document> oldDoc = GetUncomposedDoc();

  ProcessingInstruction::UnbindFromTree(aNullParent);
  Unused << UpdateStyleSheetInternal(oldDoc, nullptr);
}

// nsINode

void XMLStylesheetProcessingInstruction::SetNodeValueInternal(
    const nsAString& aNodeValue, ErrorResult& aError) {
  CharacterData::SetNodeValueInternal(aNodeValue, aError);
  if (!aError.Failed()) {
    Unused << UpdateStyleSheetInternal(nullptr, nullptr, ForceUpdate::Yes);
  }
}

// nsStyleLinkElement

void XMLStylesheetProcessingInstruction::GetCharset(nsAString& aCharset) {
  if (!GetAttrValue(nsGkAtoms::charset, aCharset)) {
    aCharset.Truncate();
  }
}

/* virtual */
void XMLStylesheetProcessingInstruction::OverrideBaseURI(nsIURI* aNewBaseURI) {
  mOverriddenBaseURI = aNewBaseURI;
}

Maybe<nsStyleLinkElement::SheetInfo>
XMLStylesheetProcessingInstruction::GetStyleSheetInfo() {
  // xml-stylesheet PI is special only in prolog
  if (!nsContentUtils::InProlog(this)) {
    return Nothing();
  }

  nsAutoString href;
  if (!GetAttrValue(nsGkAtoms::href, href)) {
    return Nothing();
  }

  nsAutoString data;
  GetData(data);

  nsAutoString title;
  nsContentUtils::GetPseudoAttributeValue(data, nsGkAtoms::title, title);

  nsAutoString alternateAttr;
  nsContentUtils::GetPseudoAttributeValue(data, nsGkAtoms::alternate,
                                          alternateAttr);

  bool alternate = alternateAttr.EqualsLiteral("yes");
  if (alternate && title.IsEmpty()) {
    // alternates must have title
    return Nothing();
  }

  nsAutoString media;
  nsContentUtils::GetPseudoAttributeValue(data, nsGkAtoms::media, media);

  // Make sure the type handling here matches
  // nsXMLContentSink::HandleProcessingInstruction
  nsAutoString type;
  nsContentUtils::GetPseudoAttributeValue(data, nsGkAtoms::type, type);

  nsAutoString mimeType, notUsed;
  nsContentUtils::SplitMimeType(type, mimeType, notUsed);
  if (!mimeType.IsEmpty() && !mimeType.LowerCaseEqualsLiteral("text/css")) {
    return Nothing();
  }

  Document* doc = OwnerDoc();
  nsIURI* baseURL =
      mOverriddenBaseURI ? mOverriddenBaseURI.get() : doc->GetDocBaseURI();
  auto encoding = doc->GetDocumentCharacterSet();
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), href, encoding, baseURL);

  return Some(SheetInfo{
      *doc,
      this,
      uri.forget(),
      nullptr,
      MakeAndAddRef<ReferrerInfo>(*doc),
      CORS_NONE,
      title,
      media,
      /* integrity = */ EmptyString(),
      /* nonce = */ EmptyString(),
      alternate ? HasAlternateRel::Yes : HasAlternateRel::No,
      IsInline::No,
      IsExplicitlyEnabled::No,
  });
}

already_AddRefed<CharacterData>
XMLStylesheetProcessingInstruction::CloneDataNode(
    mozilla::dom::NodeInfo* aNodeInfo, bool aCloneText) const {
  nsAutoString data;
  GetData(data);
  RefPtr<mozilla::dom::NodeInfo> ni = aNodeInfo;
  auto* nim = ni->NodeInfoManager();
  return do_AddRef(new (nim)
                       XMLStylesheetProcessingInstruction(ni.forget(), data));
}

}  // namespace dom
}  // namespace mozilla
