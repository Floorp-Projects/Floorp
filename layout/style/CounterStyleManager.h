/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_CounterStyleManager_h_
#define mozilla_CounterStyleManager_h_

#include "nsStringFwd.h"
#include "nsRefPtrHashtable.h"
#include "nsHashKeys.h"

#include "nsStyleConsts.h"

#include "mozilla/NullPtr.h"
#include "mozilla/Attributes.h"

#include "nsCSSValue.h"

class nsPresContext;
class nsCSSCounterStyleRule;

namespace mozilla {

class WritingMode;

typedef int32_t CounterValue;

class CounterStyleManager;
class AnonymousCounterStyle;

struct NegativeType;
struct PadType;

class CounterStyle
{
protected:
  explicit MOZ_CONSTEXPR CounterStyle(int32_t aStyle)
    : mStyle(aStyle)
  {
  }

private:
  CounterStyle(const CounterStyle& aOther) MOZ_DELETE;
  void operator=(const CounterStyle& other) MOZ_DELETE;

public:
  int32_t GetStyle() const { return mStyle; }
  bool IsNone() const { return mStyle == NS_STYLE_LIST_STYLE_NONE; }
  bool IsCustomStyle() const { return mStyle == NS_STYLE_LIST_STYLE_CUSTOM; }
  // A style is dependent if it depends on the counter style manager.
  // Custom styles are certainly dependent. In addition, some builtin
  // styles are dependent for fallback.
  bool IsDependentStyle() const;

  virtual void GetPrefix(nsSubstring& aResult) = 0;
  virtual void GetSuffix(nsSubstring& aResult) = 0;
  void GetCounterText(CounterValue aOrdinal,
                      WritingMode aWritingMode,
                      nsSubstring& aResult,
                      bool& aIsRTL);
  virtual void GetSpokenCounterText(CounterValue aOrdinal,
                                    WritingMode aWritingMode,
                                    nsSubstring& aResult,
                                    bool& aIsBullet);

  // XXX This method could be removed once ::-moz-list-bullet and
  //     ::-moz-list-number are completely merged into ::marker.
  virtual bool IsBullet() = 0;

  virtual void GetNegative(NegativeType& aResult) = 0;
  /**
   * This method returns whether an ordinal is in the range of this
   * counter style. Note that, it is possible that an ordinal in range
   * is rejected by the generating algorithm.
   */
  virtual bool IsOrdinalInRange(CounterValue aOrdinal) = 0;
  /**
   * This method returns whether an ordinal is in the default range of
   * this counter style. This is the effective range when no 'range'
   * descriptor is specified.
   */
  virtual bool IsOrdinalInAutoRange(CounterValue aOrdinal) = 0;
  virtual void GetPad(PadType& aResult) = 0;
  virtual CounterStyle* GetFallback() = 0;
  virtual uint8_t GetSpeakAs() = 0;
  virtual bool UseNegativeSign() = 0;

  virtual void CallFallbackStyle(CounterValue aOrdinal,
                                 WritingMode aWritingMode,
                                 nsSubstring& aResult,
                                 bool& aIsRTL);
  virtual bool GetInitialCounterText(CounterValue aOrdinal,
                                     WritingMode aWritingMode,
                                     nsSubstring& aResult,
                                     bool& aIsRTL) = 0;

  virtual AnonymousCounterStyle* AsAnonymous() { return nullptr; }

  NS_IMETHOD_(MozExternalRefCountType) AddRef() = 0;
  NS_IMETHOD_(MozExternalRefCountType) Release() = 0;

protected:
  int32_t mStyle;
};

class AnonymousCounterStyle MOZ_FINAL : public CounterStyle
{
public:
  AnonymousCounterStyle(const nsCSSValue::Array* aValue);

  virtual void GetPrefix(nsAString& aResult) MOZ_OVERRIDE;
  virtual void GetSuffix(nsAString& aResult) MOZ_OVERRIDE;
  virtual bool IsBullet() MOZ_OVERRIDE;

  virtual void GetNegative(NegativeType& aResult) MOZ_OVERRIDE;
  virtual bool IsOrdinalInRange(CounterValue aOrdinal) MOZ_OVERRIDE;
  virtual bool IsOrdinalInAutoRange(CounterValue aOrdinal) MOZ_OVERRIDE;
  virtual void GetPad(PadType& aResult) MOZ_OVERRIDE;
  virtual CounterStyle* GetFallback() MOZ_OVERRIDE;
  virtual uint8_t GetSpeakAs() MOZ_OVERRIDE;
  virtual bool UseNegativeSign() MOZ_OVERRIDE;

  virtual bool GetInitialCounterText(CounterValue aOrdinal,
                                     WritingMode aWritingMode,
                                     nsSubstring& aResult,
                                     bool& aIsRTL) MOZ_OVERRIDE;

  virtual AnonymousCounterStyle* AsAnonymous() MOZ_OVERRIDE { return this; }

  uint8_t GetSystem() const { return mSystem; }
  const nsTArray<nsString>& GetSymbols() const { return mSymbols; }

  NS_INLINE_DECL_REFCOUNTING(AnonymousCounterStyle)

private:
  ~AnonymousCounterStyle() {}

  uint8_t mSystem;
  nsTArray<nsString> mSymbols;
};

class CounterStyleManager MOZ_FINAL
{
private:
  ~CounterStyleManager();
public:
  explicit CounterStyleManager(nsPresContext* aPresContext);

  static void InitializeBuiltinCounterStyles();

  void Disconnect();

  bool IsInitial() const
  {
    // only 'none' and 'decimal'
    return mCacheTable.Count() == 2;
  }

  CounterStyle* BuildCounterStyle(const nsSubstring& aName);
  CounterStyle* BuildCounterStyle(const nsCSSValue::Array* aParams);

  static CounterStyle* GetBuiltinStyle(int32_t aStyle);
  static CounterStyle* GetNoneStyle()
  {
    return GetBuiltinStyle(NS_STYLE_LIST_STYLE_NONE);
  }
  static CounterStyle* GetDecimalStyle()
  {
    return GetBuiltinStyle(NS_STYLE_LIST_STYLE_DECIMAL);
  }

  // This method will scan all existing counter styles generated by this
  // manager, and remove or mark data dirty accordingly. It returns true
  // if any counter style is changed, false elsewise. This method should
  // be called when any counter style may be affected.
  bool NotifyRuleChanged();

  NS_INLINE_DECL_REFCOUNTING(CounterStyleManager)

private:
  nsPresContext* mPresContext;
  nsRefPtrHashtable<nsStringHashKey, CounterStyle> mCacheTable;
};

} // namespace mozilla

#endif /* !defined(mozilla_CounterStyleManager_h_) */
