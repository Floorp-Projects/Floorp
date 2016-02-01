/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSPseudoElement_h
#define mozilla_dom_CSSPseudoElement_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class Animation;
class Element;
class UnrestrictedDoubleOrKeyframeAnimationOptions;

class CSSPseudoElement final : public nsWrapperCache
{
public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(CSSPseudoElement)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(CSSPseudoElement)

protected:
  virtual ~CSSPseudoElement() = default;

public:
  ParentObject GetParentObject() const
  {
    // This will be implemented in later patch.
    return ParentObject(nullptr, nullptr);
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  void GetType(nsString& aRetVal) const { }
  already_AddRefed<Element> ParentElement() const { return nullptr; }

  void GetAnimations(nsTArray<RefPtr<Animation>>& aRetVal);
  already_AddRefed<Animation>
    Animate(JSContext* aContext,
            JS::Handle<JSObject*> aFrames,
            const UnrestrictedDoubleOrKeyframeAnimationOptions& aOptions,
            ErrorResult& aError);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CSSPseudoElement_h
