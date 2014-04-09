/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Element.h"
#include "mozilla/dom/SVGStyleElement.h"
#include "nsContentUtils.h"
#include "mozilla/dom/SVGStyleElementBinding.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Style)

namespace mozilla {
namespace dom {

JSObject*
SVGStyleElement::WrapNode(JSContext *aCx)
{
  return SVGStyleElementBinding::Wrap(aCx, this);
}

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(SVGStyleElement, SVGStyleElementBase)
NS_IMPL_RELEASE_INHERITED(SVGStyleElement, SVGStyleElementBase)

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(SVGStyleElement)
  NS_INTERFACE_TABLE_INHERITED2(SVGStyleElement,
                                nsIStyleSheetLinkingElement,
                                nsIMutationObserver)
NS_INTERFACE_TABLE_TAIL_INHERITING(SVGStyleElementBase)

NS_IMPL_CYCLE_COLLECTION_CLASS(SVGStyleElement)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(SVGStyleElement,
                                                  SVGStyleElementBase)
  tmp->nsStyleLinkElement::Traverse(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(SVGStyleElement,
                                                SVGStyleElementBase)
  tmp->nsStyleLinkElement::Unlink();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

//----------------------------------------------------------------------
// Implementation

SVGStyleElement::SVGStyleElement(already_AddRefed<nsINodeInfo>& aNodeInfo)
  : SVGStyleElementBase(aNodeInfo)
{
  AddMutationObserver(this);
}


//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGStyleElement)


//----------------------------------------------------------------------
// nsIContent methods

nsresult
SVGStyleElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                            nsIContent* aBindingParent,
                            bool aCompileEventHandlers)
{
  nsresult rv = SVGStyleElementBase::BindToTree(aDocument, aParent,
                                                aBindingParent,
                                                aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  void (SVGStyleElement::*update)() = &SVGStyleElement::UpdateStyleSheetInternal;
  nsContentUtils::AddScriptRunner(NS_NewRunnableMethod(this, update));

  return rv;
}

void
SVGStyleElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  nsCOMPtr<nsIDocument> oldDoc = GetCurrentDoc();
  ShadowRoot* oldShadow = GetShadowRoot();
  SVGStyleElementBase::UnbindFromTree(aDeep, aNullParent);
  UpdateStyleSheetInternal(oldDoc, oldShadow);
}

nsresult
SVGStyleElement::SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                         nsIAtom* aPrefix, const nsAString& aValue,
                         bool aNotify)
{
  nsresult rv = SVGStyleElementBase::SetAttr(aNameSpaceID, aName, aPrefix,
                                             aValue, aNotify);
  if (NS_SUCCEEDED(rv) && aNameSpaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::title ||
        aName == nsGkAtoms::media ||
        aName == nsGkAtoms::type) {
      UpdateStyleSheetInternal(nullptr, nullptr, true);
    } else if (aName == nsGkAtoms::scoped) {
      UpdateStyleSheetScopedness(true);
    }
  }

  return rv;
}

nsresult
SVGStyleElement::UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                           bool aNotify)
{
  nsresult rv = SVGStyleElementBase::UnsetAttr(aNameSpaceID, aAttribute,
                                               aNotify);
  if (NS_SUCCEEDED(rv) && aNameSpaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::title ||
        aAttribute == nsGkAtoms::media ||
        aAttribute == nsGkAtoms::type) {
      UpdateStyleSheetInternal(nullptr, nullptr, true);
    } else if (aAttribute == nsGkAtoms::scoped) {
      UpdateStyleSheetScopedness(false);
    }
  }

  return rv;
}

bool
SVGStyleElement::ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None &&
      aAttribute == nsGkAtoms::crossorigin) {
    ParseCORSValue(aValue, aResult);
    return true;
  }

  return SVGStyleElementBase::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                             aResult);
}

//----------------------------------------------------------------------
// nsIMutationObserver methods

void
SVGStyleElement::CharacterDataChanged(nsIDocument* aDocument,
                                      nsIContent* aContent,
                                      CharacterDataChangeInfo* aInfo)
{
  ContentChanged(aContent);
}

void
SVGStyleElement::ContentAppended(nsIDocument* aDocument,
                                 nsIContent* aContainer,
                                 nsIContent* aFirstNewContent,
                                 int32_t aNewIndexInContainer)
{
  ContentChanged(aContainer);
}

void
SVGStyleElement::ContentInserted(nsIDocument* aDocument,
                                 nsIContent* aContainer,
                                 nsIContent* aChild,
                                 int32_t aIndexInContainer)
{
  ContentChanged(aChild);
}

void
SVGStyleElement::ContentRemoved(nsIDocument* aDocument,
                                nsIContent* aContainer,
                                nsIContent* aChild,
                                int32_t aIndexInContainer,
                                nsIContent* aPreviousSibling)
{
  ContentChanged(aChild);
}

void
SVGStyleElement::ContentChanged(nsIContent* aContent)
{
  if (nsContentUtils::IsInSameAnonymousTree(this, aContent)) {
    UpdateStyleSheetInternal(nullptr, nullptr);
  }
}

//----------------------------------------------------------------------

void
SVGStyleElement::GetXmlspace(nsAString & aXmlspace)
{
  GetAttr(kNameSpaceID_XML, nsGkAtoms::space, aXmlspace);
}

void
SVGStyleElement::SetXmlspace(const nsAString & aXmlspace, ErrorResult& rv)
{
  rv = SetAttr(kNameSpaceID_XML, nsGkAtoms::space, aXmlspace, true);
}

void
SVGStyleElement::GetMedia(nsAString & aMedia)
{
  GetAttr(kNameSpaceID_None, nsGkAtoms::media, aMedia);
}

void
SVGStyleElement::SetMedia(const nsAString& aMedia, ErrorResult& rv)
{
  rv = SetAttr(kNameSpaceID_None, nsGkAtoms::media, aMedia, true);
}

bool
SVGStyleElement::Scoped() const
{
  return GetBoolAttr(nsGkAtoms::scoped);
}

void
SVGStyleElement::SetScoped(bool aScoped, ErrorResult& rv)
{
  rv = SetBoolAttr(nsGkAtoms::scoped, aScoped);
}

void
SVGStyleElement::GetType(nsAString & aType)
{
  GetAttr(kNameSpaceID_None, nsGkAtoms::type, aType);
}

void
SVGStyleElement::SetType(const nsAString& aType, ErrorResult& rv)
{
  rv = SetAttr(kNameSpaceID_None, nsGkAtoms::type, aType, true);
}

void
SVGStyleElement::GetTitle(nsAString & aTitle)
{
  GetAttr(kNameSpaceID_None, nsGkAtoms::title, aTitle);
}

void
SVGStyleElement::SetTitle(const nsAString& aTitle, ErrorResult& rv)
{
  rv = SetAttr(kNameSpaceID_None, nsGkAtoms::title, aTitle, true);
}

//----------------------------------------------------------------------
// nsStyleLinkElement methods

already_AddRefed<nsIURI>
SVGStyleElement::GetStyleSheetURL(bool* aIsInline)
{
  *aIsInline = true;
  return nullptr;
}

void
SVGStyleElement::GetStyleSheetInfo(nsAString& aTitle,
                                   nsAString& aType,
                                   nsAString& aMedia,
                                   bool* aIsScoped,
                                   bool* aIsAlternate)
{
  *aIsAlternate = false;

  nsAutoString title;
  GetAttr(kNameSpaceID_None, nsGkAtoms::title, title);
  title.CompressWhitespace();
  aTitle.Assign(title);

  GetAttr(kNameSpaceID_None, nsGkAtoms::media, aMedia);
  // The SVG spec is formulated in terms of the CSS2 spec,
  // which specifies that media queries are case insensitive.
  nsContentUtils::ASCIIToLower(aMedia);

  GetAttr(kNameSpaceID_None, nsGkAtoms::type, aType);
  if (aType.IsEmpty()) {
    aType.AssignLiteral("text/css");
  }

  *aIsScoped = HasAttr(kNameSpaceID_None, nsGkAtoms::scoped);

  return;
}

CORSMode
SVGStyleElement::GetCORSMode() const
{
  return AttrValueToCORSMode(GetParsedAttr(nsGkAtoms::crossorigin));
}

} // namespace dom
} // namespace mozilla

