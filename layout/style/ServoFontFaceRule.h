/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoFontFaceRule_h
#define mozilla_ServoFontFaceRule_h

#include "mozilla/ServoBindingTypes.h"
#include "mozilla/css/Rule.h"
#include "nsICSSDeclaration.h"

namespace mozilla {

// A ServoFontFaceRuleDecl is always embeded in a ServoFontFaceRule.
class ServoFontFaceRule;
class ServoFontFaceRuleDecl final : public nsICSSDeclaration
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMCSSSTYLEDECLARATION_HELPER

  nsINode* GetParentObject() final;
  void IndexedGetter(uint32_t aIndex, bool& aFound, nsAString& aPropName) final;

  void GetPropertyValue(nsCSSFontDesc aFontDescID, nsAString& aResult) const;

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;

protected:
  // For accessing the constructor.
  friend class ServoFontFaceRule;

  explicit ServoFontFaceRuleDecl(already_AddRefed<RawServoFontFaceRule> aDecl)
    : mRawRule(std::move(aDecl)) {}

  ~ServoFontFaceRuleDecl() = default;

  inline ServoFontFaceRule* ContainingRule();
  inline const ServoFontFaceRule* ContainingRule() const;

  RefPtr<RawServoFontFaceRule> mRawRule;

private:
  void* operator new(size_t size) CPP_THROW_NEW = delete;
};

class ServoFontFaceRule final : public css::Rule
{
public:
  ServoFontFaceRule(already_AddRefed<RawServoFontFaceRule> aRawRule,
                    uint32_t aLine, uint32_t aColumn)
    : css::Rule(aLine, aColumn)
    , mDecl(std::move(aRawRule))
  {}

  ServoFontFaceRule(const ServoFontFaceRule&) = delete;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(
      ServoFontFaceRule, css::Rule)
  bool IsCCLeaf() const final;

  RawServoFontFaceRule* Raw() const { return mDecl.mRawRule; }

  // WebIDL interface
  uint16_t Type() const final;
  void GetCssText(nsAString& aCssText) const final;
  nsICSSDeclaration* Style();

  // Methods of mozilla::css::Rule
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const final;

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;

#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif

private:
  virtual ~ServoFontFaceRule() = default;

  // For computing the offset of mDecl.
  friend class ServoFontFaceRuleDecl;

  ServoFontFaceRuleDecl mDecl;
};

inline ServoFontFaceRule*
ServoFontFaceRuleDecl::ContainingRule()
{
  return reinterpret_cast<ServoFontFaceRule*>
    (reinterpret_cast<char*>(this) - offsetof(ServoFontFaceRule, mDecl));
}

inline const ServoFontFaceRule*
ServoFontFaceRuleDecl::ContainingRule() const
{
  return reinterpret_cast<const ServoFontFaceRule*>
    (reinterpret_cast<const char*>(this) - offsetof(ServoFontFaceRule, mDecl));
}

} // namespace mozilla

#endif // mozilla_ServoFontFaceRule_h
