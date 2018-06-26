/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class for DOM objects for element.style and cssStyleRule.style */

#ifndef nsDOMCSSDeclaration_h___
#define nsDOMCSSDeclaration_h___

#include "nsICSSDeclaration.h"

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/URLExtraData.h"
#include "nsAttrValue.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "nsCompatibility.h"

class nsIPrincipal;
class nsIDocument;
struct JSContext;
class JSObject;
struct DeclarationBlockMutationClosure;

namespace mozilla {
class DeclarationBlock;
namespace css {
class Loader;
class Rule;
} // namespace css
namespace dom {
class Element;
}

struct MutationClosureData
{
  MutationClosureData()
    : mClosure(nullptr)
    , mElement(nullptr)
    , mModType(0)
  {
  }

  // mClosure is non-null as long as the closure hasn't been called.
  // This is needed so that it can be guaranteed that
  // InlineStyleDeclarationWillChange is always called before
  // SetInlineStyleDeclaration.
  void (*mClosure)(void*);
  mozilla::dom::Element* mElement;
  Maybe<nsAttrValue> mOldValue;
  uint8_t mModType;
};

} // namespace mozilla

class nsDOMCSSDeclaration : public nsICSSDeclaration
{
public:
  // Only implement QueryInterface; subclasses have the responsibility
  // of implementing AddRef/Release.
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;

  // Declare addref and release so they can be called on us, but don't
  // implement them.  Our subclasses must handle their own
  // refcounting.
  NS_IMETHOD_(MozExternalRefCountType) AddRef() override = 0;
  NS_IMETHOD_(MozExternalRefCountType) Release() override = 0;

  /**
   * Method analogous to CSSStyleDeclaration::GetPropertyValue,
   * which obeys all the same restrictions.
   */
  virtual nsresult GetPropertyValue(const nsCSSPropertyID aPropID,
                                    nsAString& aValue);

  /**
   * Method analogous to CSSStyleDeclaration::SetProperty.  This
   * method does NOT allow setting a priority (the priority will
   * always be set to default priority).
   */
  virtual nsresult SetPropertyValue(const nsCSSPropertyID aPropID,
                                    const nsAString& aValue,
                                    nsIPrincipal* aSubjectPrincipal);

  // Require subclasses to implement |GetParentRule|.
  //NS_DECL_NSIDOMCSSSTYLEDECLARATION
  void GetCssText(nsAString & aCssText) override;
  void SetCssText(const nsAString & aCssText,
                  nsIPrincipal* aSubjectPrincipal,
                  mozilla::ErrorResult& aRv) override;
  NS_IMETHOD GetPropertyValue(const nsAString & propertyName,
                              nsAString & _retval) override;
  NS_IMETHOD RemoveProperty(const nsAString & propertyName,
                            nsAString & _retval) override;
  void GetPropertyPriority(const nsAString & propertyName,
                           nsAString & aPriority) override;
  NS_IMETHOD SetProperty(const nsAString& propertyName,
                         const nsAString& value,
                         const nsAString& priority,
                         nsIPrincipal* aSubjectPrincipal) override;
  uint32_t Length() override;

  // WebIDL interface for CSS2Properties
#define CSS_PROP_PUBLIC_OR_PRIVATE(publicname_, privatename_) publicname_
#define CSS_PROP(id_, method_)                                               \
  void                                                                       \
  Get##method_(nsAString& aValue, mozilla::ErrorResult& rv)                  \
  {                                                                          \
    rv = GetPropertyValue(eCSSProperty_##id_, aValue);                       \
  }                                                                          \
                                                                             \
  void                                                                       \
  Set##method_(const nsAString& aValue, nsIPrincipal* aSubjectPrincipal,     \
               mozilla::ErrorResult& rv)                                     \
  {                                                                          \
    rv = SetPropertyValue(eCSSProperty_##id_, aValue, aSubjectPrincipal);    \
  }

#define CSS_PROP_LIST_EXCLUDE_INTERNAL
#define CSS_PROP_LONGHAND(name_, id_, method_, ...) CSS_PROP(id_, method_)
#define CSS_PROP_SHORTHAND(name_, id_, method_, ...)  CSS_PROP(id_, method_)
#define CSS_PROP_ALIAS(name_, aliasid_, id_, method_, ...) CSS_PROP(id_, method_)
#include "mozilla/ServoCSSPropList.h"
#undef CSS_PROP_ALIAS
#undef CSS_PROP_SHORTHAND
#undef CSS_PROP_LONGHAND
#undef CSS_PROP_LIST_EXCLUDE_INTERNAL
#undef CSS_PROP
#undef CSS_PROP_PUBLIC_OR_PRIVATE

  virtual void IndexedGetter(uint32_t aIndex, bool& aFound, nsAString& aPropName) override;

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // Information needed to parse a declaration for Servo side.
  // Put this in public so other Servo parsing functions can reuse this.
  struct MOZ_STACK_CLASS ParsingEnvironment
  {
    RefPtr<mozilla::URLExtraData> mUrlExtraData;
    nsCompatibility mCompatMode;
    mozilla::css::Loader* mLoader;

    ParsingEnvironment(mozilla::URLExtraData* aUrlData,
                               nsCompatibility aCompatMode,
                               mozilla::css::Loader* aLoader)
      : mUrlExtraData(aUrlData)
      , mCompatMode(aCompatMode)
      , mLoader(aLoader)
    {}

    ParsingEnvironment(already_AddRefed<mozilla::URLExtraData> aUrlData,
                               nsCompatibility aCompatMode,
                               mozilla::css::Loader* aLoader)
      : mUrlExtraData(aUrlData)
      , mCompatMode(aCompatMode)
      , mLoader(aLoader)
    {}
  };

protected:
  // The reason for calling GetOrCreateCSSDeclaration.
  enum Operation {
    // We are calling GetOrCreateCSSDeclaration so that we can read from it.
    // Does not allocate a new declaration if we don't have one yet; returns
    // nullptr in this case.
    eOperation_Read,

    // We are calling GetOrCreateCSSDeclaration so that we can set a property on
    // it or re-parse the whole declaration.  Allocates a new declaration if we
    // don't have one yet. A nullptr return value indicates an error allocating
    // the declaration.
    eOperation_Modify,

    // We are calling GetOrCreateCSSDeclaration so that we can remove a property
    // from it. Does not allocate a new declaration if we don't have one yet;
    // returns nullptr in this case.
    eOperation_RemoveProperty
  };

  // If aOperation is eOperation_Modify, aCreated must be non-null and
  // the call may set it to point to the newly created object.
  virtual mozilla::DeclarationBlock* GetOrCreateCSSDeclaration(
    Operation aOperation, mozilla::DeclarationBlock** aCreated) = 0;

  virtual nsresult SetCSSDeclaration(mozilla::DeclarationBlock* aDecl,
                                     mozilla::MutationClosureData* aClosureData) = 0;
  // Document that we must call BeginUpdate/EndUpdate on around the
  // calls to SetCSSDeclaration and the style rule mutation that leads
  // to it.
  virtual nsIDocument* DocToUpdate() = 0;

  // mUrlExtraData returns URL data for parsing url values in
  // CSS. Returns nullptr on failure. If mUrlExtraData is nullptr,
  // mCompatMode may not be set to anything meaningful.
  // If aSubjectPrincipal is passed, it should be the subject principal of the
  // scripted caller that initiated the parser.
  virtual ParsingEnvironment
  GetParsingEnvironment(nsIPrincipal* aSubjectPrincipal = nullptr) const = 0;

  // An implementation for GetParsingEnvironment for callers wrapping a
  // css::Rule.
  static ParsingEnvironment
    GetParsingEnvironmentForRule(const mozilla::css::Rule* aRule);

  nsresult ParsePropertyValue(const nsCSSPropertyID aPropID,
                              const nsAString& aPropValue,
                              bool aIsImportant,
                              nsIPrincipal* aSubjectPrincipal);

  nsresult ParseCustomPropertyValue(const nsAString& aPropertyName,
                                    const nsAString& aPropValue,
                                    bool aIsImportant,
                                    nsIPrincipal* aSubjectPrincipal);

  nsresult RemovePropertyInternal(nsCSSPropertyID aPropID);
  nsresult RemovePropertyInternal(const nsAString& aProperty);

  virtual void
  GetPropertyChangeClosure(DeclarationBlockMutationClosure* aClosure,
                           mozilla::MutationClosureData* aClosureData)
  {
  }

protected:
  virtual ~nsDOMCSSDeclaration();

private:
  template<typename ServoFunc>
  inline nsresult ModifyDeclaration(nsIPrincipal* aSubjectPrincipal,
                                    mozilla::MutationClosureData* aClosureData,
                                    ServoFunc aServoFunc);
};

#endif // nsDOMCSSDeclaration_h___
