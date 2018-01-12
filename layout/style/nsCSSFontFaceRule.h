/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCSSFontFaceRule_h
#define nsCSSFontFaceRule_h

#include "mozilla/css/Rule.h"
#include "nsCSSValue.h"
#include "nsICSSDeclaration.h"

namespace mozilla {

namespace dom {
class DocGroup;
} // namespace dom

struct CSSFontFaceDescriptors
{
#define CSS_FONT_DESC(name_, method_) nsCSSValue m##method_;
#include "nsCSSFontDescList.h"
#undef CSS_FONT_DESC

  const nsCSSValue& Get(nsCSSFontDesc aFontDescID) const;
  nsCSSValue& Get(nsCSSFontDesc aFontDescID);

private:
  static nsCSSValue CSSFontFaceDescriptors::* const Fields[];
};

} // namespace mozilla

// A nsCSSFontFaceStyleDecl is always embedded in a nsCSSFontFaceRule.
class nsCSSFontFaceRule;
class nsCSSFontFaceStyleDecl final : public nsICSSDeclaration
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMCSSSTYLEDECLARATION_HELPER
  NS_DECL_NSICSSDECLARATION
  virtual already_AddRefed<mozilla::dom::CSSValue>
  GetPropertyCSSValue(const nsAString& aProp, mozilla::ErrorResult& aRv)
    override;
  using nsICSSDeclaration::GetPropertyCSSValue;

  virtual nsINode *GetParentObject() override;
  virtual mozilla::dom::DocGroup* GetDocGroup() const override;
  virtual void IndexedGetter(uint32_t aIndex, bool& aFound, nsAString& aPropName) override;

  nsresult GetPropertyValue(nsCSSFontDesc aFontDescID,
                            nsAString & aResult) const;

  virtual JSObject* WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;

protected:
  ~nsCSSFontFaceStyleDecl() {}

  friend class nsCSSFontFaceRule;

  inline nsCSSFontFaceRule* ContainingRule();
  inline const nsCSSFontFaceRule* ContainingRule() const;

  mozilla::CSSFontFaceDescriptors mDescriptors;

  // The actual implementation of GetCssText, so we can make it const.
  void GetCssTextImpl(nsAString& aCssText) const;

private:
  // NOT TO BE IMPLEMENTED
  // This object cannot be allocated on its own, only as part of
  // nsCSSFontFaceRule.
  void* operator new(size_t size) CPP_THROW_NEW;
};

class nsCSSFontFaceRule final : public mozilla::css::Rule
{
public:
  nsCSSFontFaceRule(uint32_t aLineNumber, uint32_t aColumnNumber)
    : mozilla::css::Rule(aLineNumber, aColumnNumber)
  {
  }

  nsCSSFontFaceRule(const nsCSSFontFaceRule& aCopy)
    // copy everything except our reference count
    : mozilla::css::Rule(aCopy), mDecl(aCopy.mDecl)
  {
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(nsCSSFontFaceRule,
                                                         mozilla::css::Rule)
  virtual bool IsCCLeaf() const override;

#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif
  virtual int32_t GetType() const override;
  virtual already_AddRefed<mozilla::css::Rule> Clone() const override;

  void SetDesc(nsCSSFontDesc aDescID, nsCSSValue const & aValue);
  void GetDesc(nsCSSFontDesc aDescID, nsCSSValue & aValue);

  // WebIDL interface
  uint16_t Type() const override;
  void GetCssTextImpl(nsAString& aCssText) const override;
  nsICSSDeclaration* Style();

  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  void GetDescriptors(mozilla::CSSFontFaceDescriptors& aDescriptors) const
    { aDescriptors = mDecl.mDescriptors; }

protected:
  ~nsCSSFontFaceRule() {}

  friend class nsCSSFontFaceStyleDecl;
  nsCSSFontFaceStyleDecl mDecl;
};

// nsFontFaceRuleContainer - used for associating sheet type with
// specific @font-face rules
struct nsFontFaceRuleContainer {
  RefPtr<nsCSSFontFaceRule> mRule;
  mozilla::SheetType mSheetType;
};

inline nsCSSFontFaceRule*
nsCSSFontFaceStyleDecl::ContainingRule()
{
  return reinterpret_cast<nsCSSFontFaceRule*>
    (reinterpret_cast<char*>(this) - offsetof(nsCSSFontFaceRule, mDecl));
}

inline const nsCSSFontFaceRule*
nsCSSFontFaceStyleDecl::ContainingRule() const
{
  return reinterpret_cast<const nsCSSFontFaceRule*>
    (reinterpret_cast<const char*>(this) - offsetof(nsCSSFontFaceRule, mDecl));
}

#endif // nsCSSFontFaceRule_h
