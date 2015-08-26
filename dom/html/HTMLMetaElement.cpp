/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/dom/HTMLMetaElement.h"
#include "mozilla/dom/HTMLMetaElementBinding.h"
#include "nsContentUtils.h"
#include "nsStyleConsts.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Meta)

namespace mozilla {
namespace dom {

HTMLMetaElement::HTMLMetaElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

HTMLMetaElement::~HTMLMetaElement()
{
}


NS_IMPL_ISUPPORTS_INHERITED(HTMLMetaElement, nsGenericHTMLElement,
                            nsIDOMHTMLMetaElement)

NS_IMPL_ELEMENT_CLONE(HTMLMetaElement)


NS_IMPL_STRING_ATTR(HTMLMetaElement, Content, content)
NS_IMPL_STRING_ATTR(HTMLMetaElement, HttpEquiv, httpEquiv)
NS_IMPL_STRING_ATTR(HTMLMetaElement, Name, name)
NS_IMPL_STRING_ATTR(HTMLMetaElement, Scheme, scheme)

void
HTMLMetaElement::GetItemValueText(DOMString& aValue)
{
  GetContent(aValue);
}

void
HTMLMetaElement::SetItemValueText(const nsAString& aValue)
{
  SetContent(aValue);
}


nsresult
HTMLMetaElement::AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                              const nsAttrValue* aValue, bool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::content) {
      nsIDocument *document = GetUncomposedDoc();
      if (document && AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                                  nsGkAtoms::viewport, eIgnoreCase)) {
        nsAutoString content;
        nsresult rv = GetContent(content);
        NS_ENSURE_SUCCESS(rv, rv);
        nsContentUtils::ProcessViewportInfo(document, content);
      }
      CreateAndDispatchEvent(document, NS_LITERAL_STRING("DOMMetaChanged"));
    }
  }

  return nsGenericHTMLElement::AfterSetAttr(aNameSpaceID, aName, aValue,
                                            aNotify);
}

nsresult
HTMLMetaElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                            nsIContent* aBindingParent,
                            bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aDocument &&
      AttrValueIs(kNameSpaceID_None, nsGkAtoms::name, nsGkAtoms::viewport, eIgnoreCase)) {
    nsAutoString content;
    rv = GetContent(content);
    NS_ENSURE_SUCCESS(rv, rv);
    nsContentUtils::ProcessViewportInfo(aDocument, content);
  }
  if (aDocument &&
      AttrValueIs(kNameSpaceID_None, nsGkAtoms::name, nsGkAtoms::referrer, eIgnoreCase)) {
    nsAutoString content;
    rv = GetContent(content);
    NS_ENSURE_SUCCESS(rv, rv);

    // Referrer Policy spec requires a <meta name="referrer" tag to be in the
    // <head> element.
    Element* headElt = aDocument->GetHeadElement();
    if (headElt && nsContentUtils::ContentIsDescendantOf(this, headElt)) {
      content = nsContentUtils::TrimWhitespace<nsContentUtils::IsHTMLWhitespace>(content);
      aDocument->SetHeaderData(nsGkAtoms::referrer, content);
    }
  }
  CreateAndDispatchEvent(aDocument, NS_LITERAL_STRING("DOMMetaAdded"));
  return rv;
}

void
HTMLMetaElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  nsCOMPtr<nsIDocument> oldDoc = GetUncomposedDoc();
  CreateAndDispatchEvent(oldDoc, NS_LITERAL_STRING("DOMMetaRemoved"));
  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
}

void
HTMLMetaElement::CreateAndDispatchEvent(nsIDocument* aDoc,
                                        const nsAString& aEventName)
{
  if (!aDoc)
    return;

  nsRefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(this, aEventName, true, true);
  asyncDispatcher->RunDOMEventWhenSafe();
}

JSObject*
HTMLMetaElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLMetaElementBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
