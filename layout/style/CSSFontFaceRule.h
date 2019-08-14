/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CSSFontFaceRule_h
#define mozilla_CSSFontFaceRule_h

#include "mozilla/ServoBindingTypes.h"
#include "mozilla/css/Rule.h"
#include "nsICSSDeclaration.h"

namespace mozilla {
namespace dom {

// A CSSFontFaceRuleDecl is always embeded in a CSSFontFaceRule.
class CSSFontFaceRule;
class CSSFontFaceRuleDecl final : public nsICSSDeclaration {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMCSSSTYLEDECLARATION_HELPER

  nsINode* GetParentObject() final;
  void IndexedGetter(uint32_t aIndex, bool& aFound, nsAString& aPropName) final;

  void GetPropertyValue(nsCSSFontDesc aFontDescID, nsAString& aResult) const;

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;

 protected:
  // For accessing the constructor.
  friend class CSSFontFaceRule;

  explicit CSSFontFaceRuleDecl(already_AddRefed<RawServoFontFaceRule> aDecl)
      : mRawRule(std::move(aDecl)) {}

  ~CSSFontFaceRuleDecl() = default;

  inline CSSFontFaceRule* ContainingRule();
  inline const CSSFontFaceRule* ContainingRule() const;

  RefPtr<RawServoFontFaceRule> mRawRule;

 private:
  void* operator new(size_t size) noexcept(true) = delete;
};

class CSSFontFaceRule final : public css::Rule {
 public:
  CSSFontFaceRule(already_AddRefed<RawServoFontFaceRule> aRawRule,
                  StyleSheet* aSheet, css::Rule* aParentRule, uint32_t aLine,
                  uint32_t aColumn)
      : css::Rule(aSheet, aParentRule, aLine, aColumn),
        mDecl(std::move(aRawRule)) {}

  CSSFontFaceRule(const CSSFontFaceRule&) = delete;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(CSSFontFaceRule,
                                                         css::Rule)
  bool IsCCLeaf() const final;

  RawServoFontFaceRule* Raw() const { return mDecl.mRawRule; }

  // WebIDL interface
  uint16_t Type() const final;
  void GetCssText(nsAString& aCssText) const final;
  nsICSSDeclaration* Style();

  // Methods of mozilla::css::Rule
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const final;

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;

#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif

 private:
  virtual ~CSSFontFaceRule() = default;

  // For computing the offset of mDecl.
  friend class CSSFontFaceRuleDecl;

  CSSFontFaceRuleDecl mDecl;
};

inline CSSFontFaceRule* CSSFontFaceRuleDecl::ContainingRule() {
  return reinterpret_cast<CSSFontFaceRule*>(reinterpret_cast<char*>(this) -
                                            offsetof(CSSFontFaceRule, mDecl));
}

inline const CSSFontFaceRule* CSSFontFaceRuleDecl::ContainingRule() const {
  return reinterpret_cast<const CSSFontFaceRule*>(
      reinterpret_cast<const char*>(this) - offsetof(CSSFontFaceRule, mDecl));
}

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_CSSFontFaceRule_h
