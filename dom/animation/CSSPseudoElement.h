/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSPseudoElement_h
#define mozilla_dom_CSSPseudoElement_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/RefPtr.h"
#include "nsCSSPseudoElements.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {

class Animation;
class Element;
class UnrestrictedDoubleOrKeyframeAnimationOptions;

class CSSPseudoElement final : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(CSSPseudoElement)
  NS_DECL_CYCLE_COLLECTION_NATIVE_WRAPPERCACHE_CLASS(CSSPseudoElement)

 protected:
  virtual ~CSSPseudoElement();

 public:
  ParentObject GetParentObject() const;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  PseudoStyleType GetType() const { return mPseudoType; }
  void GetType(nsString& aRetVal) const {
    MOZ_ASSERT(nsCSSPseudoElements::GetPseudoAtom(mPseudoType),
               "All pseudo-types allowed by this class should have a"
               " corresponding atom");
    // Our atoms use one colon and we would like to return two colons syntax
    // for the returned pseudo type string, so serialize this to the
    // non-deprecated two colon syntax.
    aRetVal.Assign(char16_t(':'));
    aRetVal.Append(
        nsDependentAtomString(nsCSSPseudoElements::GetPseudoAtom(mPseudoType)));
  }
  dom::Element* Element() const { return mOriginatingElement.get(); }

  // Given an element:pseudoType pair, returns the CSSPseudoElement stored as a
  // property on |aElement|. If there is no CSSPseudoElement for the specified
  // pseudo-type on element, a new CSSPseudoElement will be created and stored
  // on the element.
  static already_AddRefed<CSSPseudoElement> GetCSSPseudoElement(
      dom::Element* aElement, PseudoStyleType aType);

 private:
  // Only ::before, ::after and ::marker are supported.
  CSSPseudoElement(dom::Element* aElement, PseudoStyleType aType);

  static nsAtom* GetCSSPseudoElementPropertyAtom(PseudoStyleType aType);

  // mOriginatingElement needs to be an owning reference since if script is
  // holding on to the pseudo-element, it needs to continue to be able to refer
  // to the originating element.
  RefPtr<dom::Element> mOriginatingElement;
  PseudoStyleType mPseudoType;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_CSSPseudoElement_h
