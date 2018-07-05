/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGkAtoms.h"
#include "nsNetUtil.h"
#include "nsContentUtils.h"
#include "mozilla/dom/SVGScriptElement.h"
#include "mozilla/dom/SVGScriptElementBinding.h"
#include "nsIScriptError.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT_CHECK_PARSER(Script)

namespace mozilla {
namespace dom {

JSObject*
SVGScriptElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGScriptElement_Binding::Wrap(aCx, this, aGivenProto);
}

nsSVGElement::StringInfo SVGScriptElement::sStringInfo[2] =
{
  { &nsGkAtoms::href, kNameSpaceID_None, false },
  { &nsGkAtoms::href, kNameSpaceID_XLink, false }
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ISUPPORTS_INHERITED(SVGScriptElement, SVGScriptElementBase,
                            nsIScriptLoaderObserver,
                            nsIScriptElement, nsIMutationObserver)

//----------------------------------------------------------------------
// Implementation

SVGScriptElement::SVGScriptElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
                                   FromParser aFromParser)
  : SVGScriptElementBase(aNodeInfo)
  , ScriptElement(aFromParser)
{
  AddMutationObserver(this);
}

SVGScriptElement::~SVGScriptElement()
{
}

//----------------------------------------------------------------------
// nsINode methods

nsresult
SVGScriptElement::Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                        bool aPreallocateChildren) const
{
  *aResult = nullptr;

  already_AddRefed<mozilla::dom::NodeInfo> ni = RefPtr<mozilla::dom::NodeInfo>(aNodeInfo).forget();
  SVGScriptElement* it = new SVGScriptElement(ni, NOT_FROM_PARSER);

  nsCOMPtr<nsINode> kungFuDeathGrip = it;
  nsresult rv1 = it->Init();
  nsresult rv2 = const_cast<SVGScriptElement*>(this)->CopyInnerTo(it, aPreallocateChildren);
  NS_ENSURE_SUCCESS(rv1, rv1);
  NS_ENSURE_SUCCESS(rv2, rv2);

  // The clone should be marked evaluated if we are.
  it->mAlreadyStarted = mAlreadyStarted;
  it->mLineNumber = mLineNumber;
  it->mMalformed = mMalformed;

  kungFuDeathGrip.swap(*aResult);

  return NS_OK;
}

//----------------------------------------------------------------------
void
SVGScriptElement::GetType(nsAString & aType)
{
  GetScriptType(aType);
}

void
SVGScriptElement::SetType(const nsAString & aType, ErrorResult& rv)
{
  rv = SetAttr(kNameSpaceID_None, nsGkAtoms::type, aType, true);
}

void
SVGScriptElement::GetCrossOrigin(nsAString & aCrossOrigin)
{
  // Null for both missing and invalid defaults is ok, since we
  // always parse to an enum value, so we don't need an invalid
  // default, and we _want_ the missing default to be null.
  GetEnumAttr(nsGkAtoms::crossorigin, nullptr, aCrossOrigin);
}

void
SVGScriptElement::SetCrossOrigin(const nsAString & aCrossOrigin,
                                 ErrorResult& aError)
{
  SetOrRemoveNullableStringAttr(nsGkAtoms::crossorigin, aCrossOrigin, aError);
}

already_AddRefed<SVGAnimatedString>
SVGScriptElement::Href()
{
  return mStringAttributes[HREF].IsExplicitlySet()
         ? mStringAttributes[HREF].ToDOMAnimatedString(this)
         : mStringAttributes[XLINK_HREF].ToDOMAnimatedString(this);
}

//----------------------------------------------------------------------
// nsIScriptElement methods

bool
SVGScriptElement::GetScriptType(nsAString& type)
{
  return GetAttr(kNameSpaceID_None, nsGkAtoms::type, type);
}

void
SVGScriptElement::GetScriptText(nsAString& text)
{
  nsContentUtils::GetNodeTextContent(this, false, text);
}

void
SVGScriptElement::GetScriptCharset(nsAString& charset)
{
  charset.Truncate();
}

void
SVGScriptElement::FreezeExecutionAttrs(nsIDocument* aOwnerDoc)
{
  if (mFrozen) {
    return;
  }

  if (mStringAttributes[HREF].IsExplicitlySet() ||
      mStringAttributes[XLINK_HREF].IsExplicitlySet()) {
    // variation of this code in nsHTMLScriptElement - check if changes
    // need to be transferred when modifying
    bool isHref = false;
    nsAutoString src;
    if (mStringAttributes[HREF].IsExplicitlySet()) {
      mStringAttributes[HREF].GetAnimValue(src, this);
      isHref = true;
    } else {
      mStringAttributes[XLINK_HREF].GetAnimValue(src, this);
    }

    // Empty src should be treated as invalid URL.
    if (!src.IsEmpty()) {
      nsCOMPtr<nsIURI> baseURI = GetBaseURI();
      NS_NewURI(getter_AddRefs(mUri), src, nullptr, baseURI);

      if (!mUri) {
        const char16_t* params[] = { isHref ? u"href" : u"xlink:href", src.get() };

        nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
          NS_LITERAL_CSTRING("SVG"), OwnerDoc(),
          nsContentUtils::eDOM_PROPERTIES, "ScriptSourceInvalidUri",
          params, ArrayLength(params), nullptr,
          EmptyString(), GetScriptLineNumber(), GetScriptColumnNumber());
      }
    } else {
      const char16_t* params[] = { isHref ? u"href" : u"xlink:href" };

      nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
        NS_LITERAL_CSTRING("SVG"), OwnerDoc(),
        nsContentUtils::eDOM_PROPERTIES, "ScriptSourceEmpty",
        params, ArrayLength(params), nullptr,
        EmptyString(), GetScriptLineNumber(), GetScriptColumnNumber());
    }

    // At this point mUri will be null for invalid URLs.
    mExternal = true;
  }

  mFrozen = true;
}

//----------------------------------------------------------------------
// ScriptElement methods

bool
SVGScriptElement::HasScriptContent()
{
  return (mFrozen ? mExternal
                  : mStringAttributes[HREF].IsExplicitlySet() ||
                    mStringAttributes[XLINK_HREF].IsExplicitlySet()) ||
         nsContentUtils::HasNonEmptyTextContent(this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::StringAttributesInfo
SVGScriptElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

//----------------------------------------------------------------------
// nsIContent methods

nsresult
SVGScriptElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                             nsIContent* aBindingParent,
                             bool aCompileEventHandlers)
{
  nsresult rv = SVGScriptElementBase::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDocument) {
    MaybeProcessScript();
  }

  return NS_OK;
}

nsresult
SVGScriptElement::AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                               const nsAttrValue* aValue,
                               const nsAttrValue* aOldValue,
                               nsIPrincipal* aSubjectPrincipal,
                               bool aNotify)
{
  if ((aNamespaceID == kNameSpaceID_XLink ||
       aNamespaceID == kNameSpaceID_None) &&
      aName == nsGkAtoms::href) {
    MaybeProcessScript();
  }
  return SVGScriptElementBase::AfterSetAttr(aNamespaceID, aName,
                                            aValue, aOldValue,
                                            aSubjectPrincipal, aNotify);
}

bool
SVGScriptElement::ParseAttribute(int32_t aNamespaceID,
                                 nsAtom* aAttribute,
                                 const nsAString& aValue,
                                 nsIPrincipal* aMaybeScriptedPrincipal,
                                 nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None &&
      aAttribute == nsGkAtoms::crossorigin) {
    ParseCORSValue(aValue, aResult);
    return true;
  }

  return SVGScriptElementBase::ParseAttribute(aNamespaceID, aAttribute,
                                              aValue,
                                              aMaybeScriptedPrincipal,
                                              aResult);
}

CORSMode
SVGScriptElement::GetCORSMode() const
{
  return AttrValueToCORSMode(GetParsedAttr(nsGkAtoms::crossorigin));
}

} // namespace dom
} // namespace mozilla
