/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/dom/HTMLStyleElement.h"
#include "mozilla/dom/HTMLStyleElementBinding.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsIDOMStyleSheet.h"
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

NS_IMPL_ADDREF_INHERITED(HTMLStyleElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLStyleElement, Element)


// QueryInterface implementation for HTMLStyleElement
NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(HTMLStyleElement)
  NS_INTERFACE_TABLE_INHERITED(HTMLStyleElement,
                               nsIDOMHTMLStyleElement,
                               nsIStyleSheetLinkingElement,
                               nsIMutationObserver)
NS_INTERFACE_TABLE_TAIL_INHERITING(nsGenericHTMLElement)

NS_IMPL_ELEMENT_CLONE(HTMLStyleElement)


NS_IMETHODIMP
HTMLStyleElement::GetMozDisabled(bool* aDisabled)
{
  NS_ENSURE_ARG_POINTER(aDisabled);

  *aDisabled = Disabled();
  return NS_OK;
}

bool
HTMLStyleElement::Disabled()
{
  StyleSheet* ss = GetSheet();
  return ss && ss->Disabled();
}

NS_IMETHODIMP
HTMLStyleElement::SetMozDisabled(bool aDisabled)
{
  SetDisabled(aDisabled);
  return NS_OK;
}

void
HTMLStyleElement::SetDisabled(bool aDisabled)
{
  if (StyleSheet* ss = GetSheet()) {
    ss->SetDisabled(aDisabled);
  }
}

NS_IMPL_STRING_ATTR(HTMLStyleElement, Media, media)
NS_IMPL_BOOL_ATTR(HTMLStyleElement, Scoped, scoped)
NS_IMPL_STRING_ATTR(HTMLStyleElement, Type, type)

void
HTMLStyleElement::CharacterDataChanged(nsIDocument* aDocument,
                                       nsIContent* aContent,
                                       CharacterDataChangeInfo* aInfo)
{
  ContentChanged(aContent);
}

void
HTMLStyleElement::ContentAppended(nsIDocument* aDocument,
                                  nsIContent* aContainer,
                                  nsIContent* aFirstNewContent,
                                  int32_t aNewIndexInContainer)
{
  ContentChanged(aContainer);
}

void
HTMLStyleElement::ContentInserted(nsIDocument* aDocument,
                                  nsIContent* aContainer,
                                  nsIContent* aChild,
                                  int32_t aIndexInContainer)
{
  ContentChanged(aChild);
}

void
HTMLStyleElement::ContentRemoved(nsIDocument* aDocument,
                                 nsIContent* aContainer,
                                 nsIContent* aChild,
                                 int32_t aIndexInContainer,
                                 nsIContent* aPreviousSibling)
{
  ContentChanged(aChild);
}

void
HTMLStyleElement::ContentChanged(nsIContent* aContent)
{
  if (nsContentUtils::IsInSameAnonymousTree(this, aContent)) {
    UpdateStyleSheetInternal(nullptr, nullptr);
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
  nsContentUtils::AddScriptRunner(NewRunnableMethod(this, update));

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

  UpdateStyleSheetInternal(oldDoc, oldShadow);
}

nsresult
HTMLStyleElement::AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                               const nsAttrValue* aValue,
                               const nsAttrValue* aOldValue, bool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::title ||
        aName == nsGkAtoms::media ||
        aName == nsGkAtoms::type) {
      UpdateStyleSheetInternal(nullptr, nullptr, true);
    } else if (aName == nsGkAtoms::scoped &&
               nsContentUtils::IsScopedStyleEnabled(OwnerDoc())) {
      bool isScoped = aValue;
      UpdateStyleSheetScopedness(isScoped);
    }
  }

  return nsGenericHTMLElement::AfterSetAttr(aNameSpaceID, aName, aValue,
                                            aOldValue, aNotify);
}

NS_IMETHODIMP
HTMLStyleElement::GetInnerHTML(nsAString& aInnerHTML)
{
  if (!nsContentUtils::GetNodeTextContent(this, false, aInnerHTML, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

void
HTMLStyleElement::SetInnerHTML(const nsAString& aInnerHTML,
                               ErrorResult& aError)
{
  SetEnableUpdates(false);

  aError = nsContentUtils::SetNodeTextContent(this, aInnerHTML, true);

  SetEnableUpdates(true);

  UpdateStyleSheetInternal(nullptr, nullptr);
}

already_AddRefed<nsIURI>
HTMLStyleElement::GetStyleSheetURL(bool* aIsInline)
{
  *aIsInline = true;
  return nullptr;
}

void
HTMLStyleElement::GetStyleSheetInfo(nsAString& aTitle,
                                    nsAString& aType,
                                    nsAString& aMedia,
                                    bool* aIsScoped,
                                    bool* aIsAlternate)
{
  aTitle.Truncate();
  aType.Truncate();
  aMedia.Truncate();
  *aIsAlternate = false;

  nsAutoString title;
  GetAttr(kNameSpaceID_None, nsGkAtoms::title, title);
  title.CompressWhitespace();
  aTitle.Assign(title);

  GetAttr(kNameSpaceID_None, nsGkAtoms::media, aMedia);
  // The HTML5 spec is formulated in terms of the CSSOM spec, which specifies
  // that media queries should be ASCII lowercased during serialization.
  nsContentUtils::ASCIIToLower(aMedia);

  GetAttr(kNameSpaceID_None, nsGkAtoms::type, aType);

  *aIsScoped = HasAttr(kNameSpaceID_None, nsGkAtoms::scoped) &&
               nsContentUtils::IsScopedStyleEnabled(OwnerDoc());

  nsAutoString mimeType;
  nsAutoString notUsed;
  nsContentUtils::SplitMimeType(aType, mimeType, notUsed);
  if (!mimeType.IsEmpty() && !mimeType.LowerCaseEqualsLiteral("text/css")) {
    return;
  }

  // If we get here we assume that we're loading a css file, so set the
  // type to 'text/css'
  aType.AssignLiteral("text/css");
}

JSObject*
HTMLStyleElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLStyleElementBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla

