/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_CounterStyleManager_h_
#define mozilla_CounterStyleManager_h_

#include "nsIAtom.h"
#include "nsStringFwd.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"

#include "nsStyleConsts.h"

#include "mozilla/Attributes.h"

#include "nsCSSValue.h"

class nsPresContext;

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
  explicit constexpr CounterStyle(int32_t aStyle)
    : mStyle(aStyle)
  {
  }

private:
  CounterStyle(const CounterStyle& aOther) = delete;
  void operator=(const CounterStyle& other) = delete;

public:
  int32_t GetStyle() const { return mStyle; }
  bool IsNone() const { return mStyle == NS_STYLE_LIST_STYLE_NONE; }
  bool IsCustomStyle() const { return mStyle == NS_STYLE_LIST_STYLE_CUSTOM; }
  // A style is dependent if it depends on the counter style manager.
  // Custom styles are certainly dependent. In addition, some builtin
  // styles are dependent for fallback.
  bool IsDependentStyle() const;

  virtual void GetStyleName(nsSubstring& aResult) = 0;
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

protected:
  int32_t mStyle;
};

class AnonymousCounterStyle final : public CounterStyle
{
public:
  explicit AnonymousCounterStyle(const nsSubstring& aContent);
  AnonymousCounterStyle(uint8_t aSystem, nsTArray<nsString> aSymbols);
  explicit AnonymousCounterStyle(const nsCSSValue::Array* aValue);

  virtual void GetStyleName(nsAString& aResult) override;
  virtual void GetPrefix(nsAString& aResult) override;
  virtual void GetSuffix(nsAString& aResult) override;
  virtual bool IsBullet() override;

  virtual void GetNegative(NegativeType& aResult) override;
  virtual bool IsOrdinalInRange(CounterValue aOrdinal) override;
  virtual bool IsOrdinalInAutoRange(CounterValue aOrdinal) override;
  virtual void GetPad(PadType& aResult) override;
  virtual CounterStyle* GetFallback() override;
  virtual uint8_t GetSpeakAs() override;
  virtual bool UseNegativeSign() override;

  virtual bool GetInitialCounterText(CounterValue aOrdinal,
                                     WritingMode aWritingMode,
                                     nsSubstring& aResult,
                                     bool& aIsRTL) override;

  virtual AnonymousCounterStyle* AsAnonymous() override { return this; }

  bool IsSingleString() const { return mSingleString; }
  uint8_t GetSystem() const { return mSystem; }
  const nsTArray<nsString>& GetSymbols() const { return mSymbols; }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AnonymousCounterStyle)

private:
  ~AnonymousCounterStyle() {}

  bool mSingleString;
  uint8_t mSystem;
  nsTArray<nsString> mSymbols;
};

// A smart pointer to CounterStyle. It either owns a reference to an
// anonymous counter style, or weakly refers to a named counter style
// managed by counter style manager.
class CounterStylePtr
{
public:
  CounterStylePtr() : mRaw(0) {}
  CounterStylePtr(const CounterStylePtr& aOther)
    : mRaw(aOther.mRaw)
  {
    switch (GetType()) {
      case eCounterStyle:
        break;
      case eAnonymousCounterStyle:
        AsAnonymous()->AddRef();
        break;
      case eUnresolvedAtom:
        AsAtom()->AddRef();
        break;
      case eMask:
        MOZ_ASSERT_UNREACHABLE("Unknown type");
        break;
    }
  }
  CounterStylePtr(CounterStylePtr&& aOther)
    : mRaw(aOther.mRaw)
  {
    aOther.mRaw = 0;
  }
  ~CounterStylePtr() { Reset(); }

  CounterStylePtr& operator=(const CounterStylePtr& aOther)
  {
    if (this != &aOther) {
      Reset();
      new (this) CounterStylePtr(aOther);
    }
    return *this;
  }
  CounterStylePtr& operator=(CounterStylePtr&& aOther)
  {
    if (this != &aOther) {
      Reset();
      mRaw = aOther.mRaw;
      aOther.mRaw = 0;
    }
    return *this;
  }
  CounterStylePtr& operator=(decltype(nullptr))
  {
    Reset();
    return *this;
  }
  CounterStylePtr& operator=(already_AddRefed<nsIAtom> aAtom)
  {
    Reset();
    if (nsIAtom* raw = aAtom.take()) {
      AssertPointerAligned(raw);
      mRaw = reinterpret_cast<uintptr_t>(raw) | eUnresolvedAtom;
    }
    return *this;
  }
  CounterStylePtr& operator=(AnonymousCounterStyle* aCounterStyle)
  {
    Reset();
    if (aCounterStyle) {
      CounterStyle* raw = do_AddRef(aCounterStyle).take();
      AssertPointerAligned(raw);
      mRaw = reinterpret_cast<uintptr_t>(raw) | eAnonymousCounterStyle;
    }
    return *this;
  }
  CounterStylePtr& operator=(CounterStyle* aCounterStyle)
  {
    Reset();
    if (aCounterStyle) {
      MOZ_ASSERT(!aCounterStyle->AsAnonymous());
      AssertPointerAligned(aCounterStyle);
      mRaw = reinterpret_cast<uintptr_t>(aCounterStyle) | eCounterStyle;
    }
    return *this;
  }

  operator CounterStyle*() const & { return Get(); }
  operator CounterStyle*() const && = delete;
  CounterStyle* operator->() const { return Get(); }
  explicit operator bool() const { return !!mRaw; }
  bool operator!() const { return !mRaw; }
  bool operator==(const CounterStylePtr& aOther) const
    { return mRaw == aOther.mRaw; }
  bool operator!=(const CounterStylePtr& aOther) const
    { return mRaw != aOther.mRaw; }

  bool IsResolved() const { return !IsUnresolved(); }
  inline void Resolve(CounterStyleManager* aManager);

private:
  CounterStyle* Get() const
  {
    MOZ_ASSERT(IsResolved());
    return reinterpret_cast<CounterStyle*>(mRaw & ~eMask);
  }
  template<typename T>
  void AssertPointerAligned(T* aPointer)
  {
    // This can be checked at compile time via
    // > static_assert(alignof(CounterStyle) >= 4);
    // > static_assert(alignof(nsIAtom) >= 4);
    // but MSVC2015 doesn't support using alignof on an abstract class.
    // Once we move to MSVC2017, we can replace this runtime check with
    // the compile time check above.
    MOZ_ASSERT(!(reinterpret_cast<uintptr_t>(aPointer) & eMask));
  }

  enum Type : uintptr_t {
    eCounterStyle = 0,
    eAnonymousCounterStyle = 1,
    eUnresolvedAtom = 2,
    eMask = 3,
  };

  Type GetType() const { return static_cast<Type>(mRaw & eMask); }
  bool IsUnresolved() const { return GetType() == eUnresolvedAtom; }
  bool IsAnonymous() const { return GetType() == eAnonymousCounterStyle; }
  nsIAtom* AsAtom()
  {
    MOZ_ASSERT(IsUnresolved());
    return reinterpret_cast<nsIAtom*>(mRaw & ~eMask);
  }
  AnonymousCounterStyle* AsAnonymous()
  {
    MOZ_ASSERT(IsAnonymous());
    return static_cast<AnonymousCounterStyle*>(
      reinterpret_cast<CounterStyle*>(mRaw & ~eMask));
  }

  void Reset()
  {
    switch (GetType()) {
      case eCounterStyle:
        break;
      case eAnonymousCounterStyle:
        AsAnonymous()->Release();
        break;
      case eUnresolvedAtom:
        AsAtom()->Release();
        break;
      case eMask:
        MOZ_ASSERT_UNREACHABLE("Unknown type");
        break;
    }
    mRaw = 0;
  }

  // mRaw contains the pointer, and its last two bits are used for type
  // of the pointer.
  // If the type is eUnresolvedAtom, the pointer owns a reference to an
  // nsIAtom, and it needs to be resolved to a counter style before use.
  // If the type is eAnonymousCounterStyle, it owns a reference to an
  // anonymous counter style.
  // Otherwise it is a weak pointer referring a named counter style
  // managed by CounterStyleManager.
  uintptr_t mRaw;
};

class CounterStyleManager final
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
    return mStyles.Count() == 2;
  }

  // Returns the counter style object for the given name from the style
  // table if it is already built, and nullptr otherwise.
  CounterStyle* GetCounterStyle(nsIAtom* aName) const {
    return mStyles.Get(aName);
  }
  // Same as GetCounterStyle but try to build the counter style object
  // rather than returning nullptr if that hasn't been built.
  CounterStyle* BuildCounterStyle(nsIAtom* aName);

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
  // NotifyRuleChanged will evict no longer needed counter styles into
  // mRetiredStyles, and this function destroys all objects listed there.
  // It should be called only after no one may ever use those objects.
  void CleanRetiredStyles();

  nsPresContext* PresContext() const { return mPresContext; }

  NS_INLINE_DECL_REFCOUNTING(CounterStyleManager)

private:
  void DestroyCounterStyle(CounterStyle* aCounterStyle);

  nsPresContext* mPresContext;
  nsDataHashtable<nsRefPtrHashKey<nsIAtom>, CounterStyle*> mStyles;
  nsTArray<CounterStyle*> mRetiredStyles;
};

void
CounterStylePtr::Resolve(CounterStyleManager* aManager)
{
  if (IsUnresolved()) {
    *this = aManager->BuildCounterStyle(AsAtom());
  }
}

} // namespace mozilla

#endif /* !defined(mozilla_CounterStyleManager_h_) */
