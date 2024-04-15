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
#include "nsCOMPtr.h"
#include "nsCompatibility.h"

class nsIPrincipal;
struct JSContext;
class JSObject;

namespace mozilla {
enum class StyleCssRuleType : uint8_t;
class DeclarationBlock;
struct DeclarationBlockMutationClosure;
namespace css {
class Loader;
class Rule;
}  // namespace css
namespace dom {
class Document;
class Element;
}  // namespace dom

struct MutationClosureData {
  MutationClosureData() = default;

  mozilla::dom::Element* mElement = nullptr;
  Maybe<nsAttrValue> mOldValue;
  uint8_t mModType = 0;
  bool mWasCalled = false;
  bool mShouldBeCalled = false;
};

}  // namespace mozilla

class nsDOMCSSDeclaration : public nsICSSDeclaration {
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
  virtual void GetPropertyValue(const nsCSSPropertyID aPropID,
                                nsACString& aValue);

  /**
   * Method analogous to CSSStyleDeclaration::SetProperty.  This
   * method does NOT allow setting a priority (the priority will
   * always be set to default priority).
   */
  virtual void SetPropertyValue(const nsCSSPropertyID aPropID,
                                const nsACString& aValue,
                                nsIPrincipal* aSubjectPrincipal,
                                mozilla::ErrorResult& aRv);

  // Require subclasses to implement |GetParentRule|.
  // NS_DECL_NSIDOMCSSSTYLEDECLARATION
  void GetCssText(nsACString& aCssText) override;
  void SetCssText(const nsACString& aCssText, nsIPrincipal* aSubjectPrincipal,
                  mozilla::ErrorResult& aRv) override;
  void GetPropertyValue(const nsACString& propertyName,
                        nsACString& _retval) override;
  void RemoveProperty(const nsACString& propertyName, nsACString& _retval,
                      mozilla::ErrorResult& aRv) override;
  void GetPropertyPriority(const nsACString& propertyName,
                           nsACString& aPriority) override;
  void SetProperty(const nsACString& propertyName, const nsACString& value,
                   const nsACString& priority, nsIPrincipal* aSubjectPrincipal,
                   mozilla::ErrorResult& aRv) override;
  uint32_t Length() override;

  // WebIDL interface for CSS2Properties
  virtual void IndexedGetter(uint32_t aIndex, bool& aFound,
                             nsACString& aPropName) override;

  JSObject* WrapObject(JSContext*, JS::Handle<JSObject*> aGivenProto) override;

  // Information needed to parse a declaration for Servo side.
  // Put this in public so other Servo parsing functions can reuse this.
  struct MOZ_STACK_CLASS ParsingEnvironment {
    RefPtr<mozilla::URLExtraData> mUrlExtraData;
    nsCompatibility mCompatMode = eCompatibility_FullStandards;
    mozilla::css::Loader* mLoader = nullptr;
    mozilla::StyleCssRuleType mRuleType{1 /* Style */};
  };

 protected:
  // The reason for calling GetOrCreateCSSDeclaration.
  enum class Operation {
    // We are calling GetOrCreateCSSDeclaration so that we can read from it.
    // Does not allocate a new declaration if we don't have one yet; returns
    // nullptr in this case.
    Read,

    // We are calling GetOrCreateCSSDeclaration so that we can set a property on
    // it or re-parse the whole declaration.  Allocates a new declaration if we
    // don't have one yet. A nullptr return value indicates an error allocating
    // the declaration.
    Modify,

    // We are calling GetOrCreateCSSDeclaration so that we can remove a property
    // from it. Does not allocate a new declaration if we don't have one yet;
    // returns nullptr in this case.
    RemoveProperty,
  };

  // If aOperation is Modify, aCreated must be non-null and the call may set it
  // to point to the newly created object.
  virtual mozilla::DeclarationBlock* GetOrCreateCSSDeclaration(
      Operation aOperation, mozilla::DeclarationBlock** aCreated) = 0;

  virtual nsresult SetCSSDeclaration(
      mozilla::DeclarationBlock* aDecl,
      mozilla::MutationClosureData* aClosureData) = 0;
  // Document that we must call BeginUpdate/EndUpdate on around the
  // calls to SetCSSDeclaration and the style rule mutation that leads
  // to it.
  virtual mozilla::dom::Document* DocToUpdate() { return nullptr; }

  // mUrlExtraData returns URL data for parsing url values in
  // CSS. Returns nullptr on failure. If mUrlExtraData is nullptr,
  // mCompatMode may not be set to anything meaningful.
  // If aSubjectPrincipal is passed, it should be the subject principal of the
  // scripted caller that initiated the parser.
  virtual ParsingEnvironment GetParsingEnvironment(
      nsIPrincipal* aSubjectPrincipal = nullptr) const = 0;

  // An implementation for GetParsingEnvironment for callers wrapping a
  // css::Rule.
  //
  // The RuleType argument is just to avoid a virtual call, since all callers
  // know it statically. Should be equal to aRule->Type().
  static ParsingEnvironment GetParsingEnvironmentForRule(
      const mozilla::css::Rule* aRule, mozilla::StyleCssRuleType);

  nsresult ParsePropertyValue(const nsCSSPropertyID aPropID,
                              const nsACString& aPropValue, bool aIsImportant,
                              nsIPrincipal* aSubjectPrincipal);

  nsresult ParseCustomPropertyValue(const nsACString& aPropertyName,
                                    const nsACString& aPropValue,
                                    bool aIsImportant,
                                    nsIPrincipal* aSubjectPrincipal);

  void RemovePropertyInternal(nsCSSPropertyID aPropID,
                              mozilla::ErrorResult& aRv);
  void RemovePropertyInternal(const nsACString& aPropert,
                              mozilla::ErrorResult& aRv);

  virtual void GetPropertyChangeClosure(
      mozilla::DeclarationBlockMutationClosure* aClosure,
      mozilla::MutationClosureData* aClosureData) {}

 protected:
  virtual ~nsDOMCSSDeclaration();

 private:
  template <typename ServoFunc>
  inline nsresult ModifyDeclaration(nsIPrincipal* aSubjectPrincipal,
                                    mozilla::MutationClosureData* aClosureData,
                                    ServoFunc aServoFunc);
};

#endif  // nsDOMCSSDeclaration_h___
