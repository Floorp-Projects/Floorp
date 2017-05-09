/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCSSCounterStyleRule_h
#define nsCSSCounterStyleRule_h

#include "mozilla/css/Rule.h"
#include "nsCSSValue.h"
#include "nsIDOMCSSCounterStyleRule.h"

class nsCSSCounterStyleRule final : public mozilla::css::Rule,
                                    public nsIDOMCSSCounterStyleRule
{
public:
  explicit nsCSSCounterStyleRule(nsIAtom* aName,
                                 uint32_t aLineNumber, uint32_t aColumnNumber)
    : mozilla::css::Rule(aLineNumber, aColumnNumber)
    , mName(aName)
    , mGeneration(0)
  {
    MOZ_ASSERT(aName, "Must have non-null name");
  }

private:
  nsCSSCounterStyleRule(const nsCSSCounterStyleRule& aCopy);
  ~nsCSSCounterStyleRule();

public:
  NS_DECL_ISUPPORTS_INHERITED
  virtual bool IsCCLeaf() const override;

#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif
  virtual int32_t GetType() const override;
  using Rule::GetType;
  virtual already_AddRefed<mozilla::css::Rule> Clone() const override;

  // nsIDOMCSSCounterStyleRule
  NS_DECL_NSIDOMCSSCOUNTERSTYLERULE

  // WebIDL interface
  uint16_t Type() const override;
  void GetCssTextImpl(nsAString& aCssText) const override;
  // The XPCOM GetName is OK
  // The XPCOM SetName is OK
  // The XPCOM GetSystem is OK
  // The XPCOM SetSystem is OK
  // The XPCOM GetSymbols is OK
  // The XPCOM SetSymbols is OK
  // The XPCOM GetAdditiveSymbols is OK
  // The XPCOM SetAdditiveSymbols is OK
  // The XPCOM GetNegative is OK
  // The XPCOM SetNegative is OK
  // The XPCOM GetPrefix is OK
  // The XPCOM SetPrefix is OK
  // The XPCOM GetSuffix is OK
  // The XPCOM SetSuffix is OK
  // The XPCOM GetRange is OK
  // The XPCOM SetRange is OK
  // The XPCOM GetPad is OK
  // The XPCOM SetPad is OK
  // The XPCOM GetSpeakAs is OK
  // The XPCOM SetSpeakAs is OK
  // The XPCOM GetFallback is OK
  // The XPCOM SetFallback is OK

  // This function is only used to check whether a non-empty value, which has
  // been accepted by parser, is valid for the given system and descriptor.
  static bool CheckDescValue(int32_t aSystem,
                             nsCSSCounterDesc aDescID,
                             const nsCSSValue& aValue);

  nsIAtom* Name() const { return mName; }

  uint32_t GetGeneration() const { return mGeneration; }

  int32_t GetSystem() const;
  const nsCSSValue& GetSystemArgument() const;

  const nsCSSValue& GetDesc(nsCSSCounterDesc aDescID) const
  {
    MOZ_ASSERT(aDescID >= 0 && aDescID < eCSSCounterDesc_COUNT,
               "descriptor ID out of range");
    return mValues[aDescID];
  }

  void SetDesc(nsCSSCounterDesc aDescID, const nsCSSValue& aValue);

  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

private:
  typedef decltype(&nsCSSCounterStyleRule::GetSymbols) Getter;
  static const Getter kGetters[];

  nsresult GetDescriptor(nsCSSCounterDesc aDescID, nsAString& aValue);
  nsresult SetDescriptor(nsCSSCounterDesc aDescID, const nsAString& aValue);

  nsCOMPtr<nsIAtom> mName;
  nsCSSValue mValues[eCSSCounterDesc_COUNT];
  uint32_t   mGeneration;
};

#endif // nsCSSCounterStyleRule_h
