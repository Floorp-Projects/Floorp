/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSKeyframeRule_h
#define mozilla_dom_CSSKeyframeRule_h

#include "mozilla/css/Rule.h"
#include "mozilla/ServoBindingTypes.h"

class nsICSSDeclaration;
class DeclarationBlock;

namespace mozilla {
namespace dom {

class CSSKeyframeDeclaration;

class CSSKeyframeRule final : public css::Rule
{
public:
  CSSKeyframeRule(already_AddRefed<RawServoKeyframe> aRaw,
                  StyleSheet* aSheet,
                  css::Rule* aParentRule,
                  uint32_t aLine,
                  uint32_t aColumn);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(CSSKeyframeRule,
                                           css::Rule)
  bool IsCCLeaf() const final;

#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif

  RawServoKeyframe* Raw() const { return mRaw; }

  // WebIDL interface
  uint16_t Type() const final { return CSSRule_Binding::KEYFRAME_RULE; }
  void GetCssText(nsAString& aCssText) const final;
  void GetKeyText(nsAString& aKey);
  void SetKeyText(const nsAString& aKey);
  nsICSSDeclaration* Style();

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const final;

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;

private:
  virtual ~CSSKeyframeRule();

  friend class CSSKeyframeDeclaration;

  template<typename Func>
  void UpdateRule(Func aCallback);

  RefPtr<RawServoKeyframe> mRaw;
  // lazily created when needed
  RefPtr<CSSKeyframeDeclaration> mDeclaration;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CSSKeyframeRule_h
