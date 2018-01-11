/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCSSCounterStyleRule_h
#define nsCSSCounterStyleRule_h

#include "mozilla/css/Rule.h"
#include "nsCSSValue.h"

class nsCSSCounterStyleRule final : public mozilla::css::Rule
{
public:
  explicit nsCSSCounterStyleRule(nsAtom* aName,
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
  virtual bool IsCCLeaf() const override;

#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif
  virtual int32_t GetType() const override;
  virtual already_AddRefed<mozilla::css::Rule> Clone() const override;

  // WebIDL interface
  uint16_t Type() const override;
  void GetCssTextImpl(nsAString& aCssText) const override;
  void GetName(nsAString& aName);
  void SetName(const nsAString& aName);
  void GetSystem(nsAString& aSystem);
  void SetSystem(const nsAString& aSystem);
  void GetSymbols(nsAString& aSymbols);
  void SetSymbols(const nsAString& aSymbols);
  void GetAdditiveSymbols(nsAString& aAdditiveSymbols);
  void SetAdditiveSymbols(const nsAString& aAdditiveSymbols);
  void GetNegative(nsAString& aNegative);
  void SetNegative(const nsAString& aNegative);
  void GetPrefix(nsAString& aPrefix);
  void SetPrefix(const nsAString& aPrefix);
  void GetSuffix(nsAString& aSuffix);
  void SetSuffix(const nsAString& aSuffix);
  void GetRange(nsAString& aRange);
  void SetRange(const nsAString& aRange);
  void GetPad(nsAString& aPad);
  void SetPad(const nsAString& aPad);
  void GetSpeakAs(nsAString& aSpeakAs);
  void SetSpeakAs(const nsAString& aSpeakAs);
  void GetFallback(nsAString& aFallback);
  void SetFallback(const nsAString& aFallback);

  // This function is only used to check whether a non-empty value, which has
  // been accepted by parser, is valid for the given system and descriptor.
  static bool CheckDescValue(int32_t aSystem,
                             nsCSSCounterDesc aDescID,
                             const nsCSSValue& aValue);

  nsAtom* Name() const { return mName; }

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

  void GetDescriptor(nsCSSCounterDesc aDescID, nsAString& aValue);
  void SetDescriptor(nsCSSCounterDesc aDescID, const nsAString& aValue);

  RefPtr<nsAtom> mName;
  nsCSSValue mValues[eCSSCounterDesc_COUNT];
  uint32_t   mGeneration;
};

#endif // nsCSSCounterStyleRule_h
