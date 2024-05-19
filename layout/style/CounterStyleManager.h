/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_CounterStyleManager_h_
#define mozilla_CounterStyleManager_h_

#include "nsGkAtoms.h"
#include "nsStringFwd.h"
#include "nsTHashMap.h"
#include "nsHashKeys.h"

#include "nsStyleConsts.h"

#include "mozilla/Attributes.h"

class nsPresContext;

namespace mozilla {

enum class SpeakAs : uint8_t {
  Bullets = 0,
  Numbers = 1,
  Words = 2,
  Spellout = 3,
  Other = 255
};

class WritingMode;

typedef int32_t CounterValue;

class CounterStyleManager;
class AnonymousCounterStyle;

struct NegativeType;
struct PadType;

class CounterStyle {
 protected:
  explicit constexpr CounterStyle(ListStyle aStyle) : mStyle(aStyle) {}

 private:
  CounterStyle(const CounterStyle& aOther) = delete;
  void operator=(const CounterStyle& other) = delete;

 public:
  constexpr ListStyle GetStyle() const { return mStyle; }
  bool IsNone() const { return mStyle == ListStyle::None; }
  bool IsCustomStyle() const { return mStyle == ListStyle::Custom; }
  // A style is dependent if it depends on the counter style manager.
  // Custom styles are certainly dependent. In addition, some builtin
  // styles are dependent for fallback.
  bool IsDependentStyle() const;

  virtual void GetPrefix(nsAString& aResult) = 0;
  virtual void GetSuffix(nsAString& aResult) = 0;
  void GetCounterText(CounterValue aOrdinal, WritingMode aWritingMode,
                      nsAString& aResult, bool& aIsRTL);
  virtual void GetSpokenCounterText(CounterValue aOrdinal,
                                    WritingMode aWritingMode,
                                    nsAString& aResult, bool& aIsBullet);

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
  virtual SpeakAs GetSpeakAs() = 0;
  virtual bool UseNegativeSign() = 0;

  virtual void CallFallbackStyle(CounterValue aOrdinal,
                                 WritingMode aWritingMode, nsAString& aResult,
                                 bool& aIsRTL);
  virtual bool GetInitialCounterText(CounterValue aOrdinal,
                                     WritingMode aWritingMode,
                                     nsAString& aResult, bool& aIsRTL) = 0;

  virtual AnonymousCounterStyle* AsAnonymous() { return nullptr; }

 protected:
  const ListStyle mStyle;
};

class MOZ_STACK_CLASS AnonymousCounterStyle final : public CounterStyle {
 public:
  explicit AnonymousCounterStyle(const nsAString& aContent);
  AnonymousCounterStyle(StyleSymbolsType, Span<const StyleSymbol> aSymbols);

  void GetPrefix(nsAString& aResult) override;
  void GetSuffix(nsAString& aResult) override;
  bool IsBullet() override;

  void GetNegative(NegativeType& aResult) override;
  bool IsOrdinalInRange(CounterValue aOrdinal) override;
  bool IsOrdinalInAutoRange(CounterValue aOrdinal) override;
  void GetPad(PadType& aResult) override;
  CounterStyle* GetFallback() override;
  SpeakAs GetSpeakAs() override;
  bool UseNegativeSign() override;

  bool GetInitialCounterText(CounterValue aOrdinal, WritingMode aWritingMode,
                             nsAString& aResult, bool& aIsRTL) override;

  AnonymousCounterStyle* AsAnonymous() override { return this; }

  auto GetSymbols() const { return mSymbols; }

  StyleCounterSystem GetSystem() const;

  ~AnonymousCounterStyle() = default;

  StyleSymbolsType mSymbolsType;
  Span<const StyleSymbol> mSymbols;
};

class CounterStyleManager final {
 private:
  ~CounterStyleManager();

 public:
  explicit CounterStyleManager(nsPresContext* aPresContext);

  void Disconnect();

  bool IsInitial() const {
    // only 'none', 'decimal', and 'disc'
    return mStyles.Count() == 3;
  }

  // Returns the counter style object for the given name from the style
  // table if it is already built, and nullptr otherwise.
  CounterStyle* GetCounterStyle(nsAtom* aName) const {
    return mStyles.Get(aName);
  }
  // Same as GetCounterStyle but try to build the counter style object
  // rather than returning nullptr if that hasn't been built.
  CounterStyle* ResolveCounterStyle(nsAtom* aName);
  template <typename F>
  void WithCounterStyleNameOrSymbols(const StyleCounterStyle& aStyle,
                                     F&& aCallback) {
    using Tag = StyleCounterStyle::Tag;
    switch (aStyle.tag) {
      case Tag::None:
      case Tag::String:
        MOZ_CRASH("Unexpected counter style");
      case Tag::Symbols: {
        AnonymousCounterStyle s(aStyle.AsSymbols().ty,
                                aStyle.AsSymbols().symbols._0.AsSpan());
        return aCallback(&s);
      }
      case Tag::Name: {
        return aCallback(ResolveCounterStyle(aStyle.AsName().AsAtom()));
      }
    }
  }

  static CounterStyle* GetBuiltinStyle(ListStyle aStyle);
  static CounterStyle* GetNoneStyle() {
    return GetBuiltinStyle(ListStyle::None);
  }
  static CounterStyle* GetDecimalStyle() {
    return GetBuiltinStyle(ListStyle::Decimal);
  }
  static CounterStyle* GetDiscStyle() {
    return GetBuiltinStyle(ListStyle::Disc);
  }

  // This method will scan all existing counter styles generated by this
  // manager, and remove or mark data dirty accordingly. It returns true
  // if any counter style is changed, false elsewise. This method should
  // be called when any counter style may be affected.
  bool NotifyRuleChanged();
  // NotifyRuleChanged will evict no longer needed counter styles into
  // mRetiredStyles, and this function destroys all objects listed there.
  // It should be called only after no one may ever use those objects.
  void CleanRetiredStyles();

  nsPresContext* PresContext() const { return mPresContext; }

  NS_INLINE_DECL_REFCOUNTING(CounterStyleManager)

 private:
  void DestroyCounterStyle(CounterStyle* aCounterStyle);

  nsPresContext* mPresContext;
  nsTHashMap<RefPtr<nsAtom>, CounterStyle*> mStyles;
  nsTArray<CounterStyle*> mRetiredStyles;
};

}  // namespace mozilla

#endif /* !defined(mozilla_CounterStyleManager_h_) */
