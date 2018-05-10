/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/dom/HTMLStyleElement.h"
#include "mozilla/dom/HTMLStyleElementBinding.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsIDocument.h"
#include "nsUnicharUtils.h"
#include "nsThreadUtils.h"
#include "nsContentUtils.h"
#include "nsStubMutationObserver.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Style)

namespace mozilla {
namespace dom {

HTMLStyleElement::HTMLStyleElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
  AddMutationObserver(this);
}

HTMLStyleElement::~HTMLStyleElement()
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLStyleElement)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLStyleElement,
                                                  nsGenericHTMLElement)
  tmp->nsStyleLinkElement::Traverse(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(HTMLStyleElement,
                                                nsGenericHTMLElement)
  tmp->nsStyleLinkElement::Unlink();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(HTMLStyleElement,
                                             nsGenericHTMLElement,
                                             nsIStyleSheetLinkingElement,
                                             nsIMutationObserver)

NS_IMPL_ELEMENT_CLONE(HTMLStyleElement)


bool
HTMLStyleElement::Disabled()
{
  StyleSheet* ss = GetSheet();
  return ss && ss->Disabled();
}

void
HTMLStyleElement::SetDisabled(bool aDisabled)
{
  if (StyleSheet* ss = GetSheet()) {
    ss->SetDisabled(aDisabled);
  }
}

void
HTMLStyleElement::CharacterDataChanged(nsIContent* aContent,
                                       const CharacterDataChangeInfo&)
{
  ContentChanged(aContent);
}

void
HTMLStyleElement::ContentAppended(nsIContent* aFirstNewContent)
{
  ContentChanged(aFirstNewContent->GetParent());
}

void
HTMLStyleElement::ContentInserted(nsIContent* aChild)
{
  ContentChanged(aChild);
}

void
HTMLStyleElement::ContentRemoved(nsIContent* aChild,
                                 nsIContent* aPreviousSibling)
{
  ContentChanged(aChild);
}

void
HTMLStyleElement::ContentChanged(nsIContent* aContent)
{
  mTriggeringPrincipal = nullptr;
  if (nsContentUtils::IsInSameAnonymousTree(this, aContent)) {
    Unused << UpdateStyleSheetInternal(nullptr, nullptr);
  }
}

nsresult
HTMLStyleElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                             nsIContent* aBindingParent,
                             bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  void (HTMLStyleElement::*update)() = &HTMLStyleElement::UpdateStyleSheetInternal;
  nsContentUtils::AddScriptRunner(
    NewRunnableMethod("dom::HTMLStyleElement::BindToTree", this, update));

  return rv;
}

void
HTMLStyleElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  nsCOMPtr<nsIDocument> oldDoc = GetUncomposedDoc();
  ShadowRoot* oldShadow = GetContainingShadow();

  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);

  if (oldShadow && GetContainingShadow()) {
    // The style is in a shadow tree and is still in the
    // shadow tree. Thus the sheets in the shadow DOM
    // do not need to be updated.
    return;
  }

  Unused << UpdateStyleSheetInternal(oldDoc, oldShadow);
}

nsresult
HTMLStyleElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                               const nsAttrValue* aValue,
                               const nsAttrValue* aOldValue,
                               nsIPrincipal* aSubjectPrincipal,
                               bool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::title ||
        aName == nsGkAtoms::media ||
        aName == nsGkAtoms::type) {
      Unused << UpdateStyleSheetInternal(nullptr, nullptr, ForceUpdate::Yes);
    }
  }

  return nsGenericHTMLElement::AfterSetAttr(aNameSpaceID, aName, aValue,
                                            aOldValue, aSubjectPrincipal, aNotify);
}

void
HTMLStyleElement::GetInnerHTML(nsAString& aInnerHTML, OOMReporter& aError)
{
  if (!nsContentUtils::GetNodeTextContent(this, false, aInnerHTML, fallible)) {
    aError.ReportOOM();
  }
}

void
HTMLStyleElement::SetInnerHTML(const nsAString& aInnerHTML,
                               nsIPrincipal* aScriptedPrincipal,
                               ErrorResult& aError)
{
  SetTextContentInternal(aInnerHTML, aScriptedPrincipal, aError);
}

void
HTMLStyleElement::SetTextContentInternal(const nsAString& aTextContent,
                                         nsIPrincipal* aScriptedPrincipal,
                                         ErrorResult& aError)
{
  // Per spec, if we're setting text content to an empty string and don't
  // already have any children, we should not trigger any mutation observers, or
  // re-parse the stylesheet.
  if (aTextContent.IsEmpty() && !GetFirstChild()) {
    nsIPrincipal* principal = mTriggeringPrincipal ? mTriggeringPrincipal.get() : NodePrincipal();
    if (principal == aScriptedPrincipal) {
      return;
    }
  }

  SetEnableUpdates(false);

  aError = nsContentUtils::SetNodeTextContent(this, aTextContent, true);

  SetEnableUpdates(true);

  mTriggeringPrincipal = aScriptedPrincipal;

  Unused << UpdateStyleSheetInternal(nullptr, nullptr);
}

Maybe<nsStyleLinkElement::SheetInfo>
HTMLStyleElement::GetStyleSheetInfo()
{
  if (!IsCSSMimeTypeAttribute(*this)) {
    return Nothing();
  }

  nsAutoString title;
  nsAutoString media;
  GetTitleAndMediaForElement(*this, title, media);

  nsCOMPtr<nsIPrincipal> prin = mTriggeringPrincipal;
  return Some(SheetInfo {
    *OwnerDoc(),
    this,
    nullptr,
    prin.forget(),
    net::ReferrerPolicy::RP_Unset,
    CORS_NONE,
    title,
    media,
    HasAlternateRel::No,
    IsInline::Yes,
  });
}

JSObject*
HTMLStyleElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLStyleElementBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla

