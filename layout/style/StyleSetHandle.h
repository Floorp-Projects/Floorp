/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StyleSetHandle_h
#define mozilla_StyleSetHandle_h

#include "mozilla/AtomArray.h"
#include "mozilla/EventStates.h"
#include "mozilla/MediaFeatureChange.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ServoTypes.h"
#include "mozilla/SheetType.h"
#include "mozilla/StyleSheet.h"
#include "nsChangeHint.h"
#include "nsCSSPseudoElements.h"
#include "nsTArray.h"

class nsBindingManager;
class nsCSSCounterStyleRule;
struct nsFontFaceRuleContainer;
class nsAtom;
class nsICSSAnonBoxPseudo;
class nsIContent;
class nsIDocument;
class nsStyleSet;
class nsPresContext;
class gfxFontFeatureValueSet;

namespace mozilla {

class ComputedStyle;
class CSSStyleSheet;
class ServoStyleSet;
namespace dom {
class Element;
class ShadowRoot;
} // namespace dom
namespace css {
class Rule;
} // namespace css

/**
 * Smart pointer class that can hold a pointer to either an nsStyleSet
 * or a ServoStyleSet.
 */
class StyleSetHandle
{
public:
  // We define this Ptr class with a StyleSet API that forwards on to the
  // wrapped pointer, rather than putting these methods on StyleSetHandle
  // itself, so that we can have StyleSetHandle behave like a smart pointer and
  // be dereferenced with operator->.
  class Ptr
  {
  public:
    friend class ::mozilla::StyleSetHandle;

    ServoStyleSet* AsServo()
    {
      return reinterpret_cast<ServoStyleSet*>(mValue);
    }

    ServoStyleSet* GetAsServo() { return AsServo(); }


    const ServoStyleSet* AsServo() const
    {
      return const_cast<Ptr*>(this)->AsServo();
    }

    const ServoStyleSet* GetAsServo() const { return AsServo(); }

    // These inline methods are defined in StyleSetHandleInlines.h.
    inline void Delete();

    // Style set interface.  These inline methods are defined in
    // StyleSetHandleInlines.h and just forward to the underlying
    // nsStyleSet or ServoStyleSet.  See corresponding comments in
    // nsStyleSet.h for descriptions of these methods.

    inline void Init(nsPresContext* aPresContext);
    inline void BeginShutdown();
    inline void Shutdown();
    inline bool GetAuthorStyleDisabled() const;
    inline void SetAuthorStyleDisabled(bool aStyleDisabled);
    inline void BeginUpdate();
    inline nsresult EndUpdate();
    inline already_AddRefed<ComputedStyle>
    ResolveStyleFor(dom::Element* aElement,
                    ComputedStyle* aParentContext,
                    LazyComputeBehavior aMayCompute);
    inline already_AddRefed<ComputedStyle>
    ResolveStyleFor(dom::Element* aElement, LazyComputeBehavior aMayCompute);

    // TODO(emilio): This might be nicer (albeit a bit slower) if we just grab
    // the style from the parent in ServoStyleSet.
    //
    // It may be faster if we account not having to pass it around in
    // nsCSSFrameConstructor though.
    inline already_AddRefed<ComputedStyle>
    ResolveStyleForText(nsIContent* aTextNode, ComputedStyle* aParentContext);

    inline already_AddRefed<ComputedStyle>
    ResolveStyleForFirstLetterContinuation(ComputedStyle* aParentContext);
    inline already_AddRefed<ComputedStyle>
    ResolveStyleForPlaceholder();
    inline already_AddRefed<ComputedStyle>
    ResolvePseudoElementStyle(dom::Element* aParentElement,
                              mozilla::CSSPseudoElementType aType,
                              ComputedStyle* aParentContext,
                              dom::Element* aPseudoElement);
    inline already_AddRefed<ComputedStyle>
    ResolveInheritingAnonymousBoxStyle(nsAtom* aPseudoTag,
                                       ComputedStyle* aParentContext);
    inline already_AddRefed<ComputedStyle>
    ResolveNonInheritingAnonymousBoxStyle(nsAtom* aPseudoTag);
#ifdef MOZ_XUL
    inline already_AddRefed<ComputedStyle>
    ResolveXULTreePseudoStyle(dom::Element* aParentElement,
                              nsICSSAnonBoxPseudo* aPseudoTag,
                              ComputedStyle* aParentContext,
                              const AtomArray& aInputWord);
#endif
    inline nsresult AppendStyleSheet(SheetType aType, StyleSheet* aSheet);
    inline nsresult PrependStyleSheet(SheetType aType, StyleSheet* aSheet);
    inline nsresult RemoveStyleSheet(SheetType aType, StyleSheet* aSheet);
    inline nsresult ReplaceSheets(SheetType aType,
                           const nsTArray<RefPtr<StyleSheet>>& aNewSheets);
    inline nsresult InsertStyleSheetBefore(SheetType aType,
                                    StyleSheet* aNewSheet,
                                    StyleSheet* aReferenceSheet);
    inline int32_t SheetCount(SheetType aType) const;
    inline StyleSheet* StyleSheetAt(SheetType aType, int32_t aIndex) const;
    inline void AppendAllXBLStyleSheets(nsTArray<StyleSheet*>& aArray) const;
    inline nsresult RemoveDocStyleSheet(StyleSheet* aSheet);
    inline nsresult AddDocStyleSheet(StyleSheet* aSheet, nsIDocument* aDocument);

    inline void RuleRemoved(StyleSheet&, css::Rule&);
    inline void RuleAdded(StyleSheet&, css::Rule&);
    inline void RuleChanged(StyleSheet&, css::Rule*);

    inline void RecordShadowStyleChange(mozilla::dom::ShadowRoot& aShadowRoot);
    inline bool StyleSheetsHaveChanged() const;
    inline void InvalidateStyleForCSSRuleChanges();
    inline nsRestyleHint MediumFeaturesChanged(mozilla::MediaFeatureChangeReason);
    inline already_AddRefed<ComputedStyle>
    ProbePseudoElementStyle(dom::Element* aParentElement,
                            mozilla::CSSPseudoElementType aType,
                            ComputedStyle* aParentContext);
    inline already_AddRefed<ComputedStyle>
    ProbePseudoElementStyle(dom::Element* aParentElement,
                            mozilla::CSSPseudoElementType aType);

    inline bool AppendFontFaceRules(nsTArray<nsFontFaceRuleContainer>& aArray);
    inline nsCSSCounterStyleRule* CounterStyleRuleForName(nsAtom* aName);
    inline already_AddRefed<gfxFontFeatureValueSet> BuildFontFeatureValueSet();

    inline bool EnsureUniqueInnerOnCSSSheets();
    inline void SetNeedsRestyleAfterEnsureUniqueInner();

  private:
    // Stores a pointer to an nsStyleSet or a ServoStyleSet.  The least
    // significant bit is 0 for the former, 1 for the latter.  This is
    // valid as the least significant bit will never be used for a pointer
    // value on platforms we care about.
    uintptr_t mValue;
  };

  StyleSetHandle() { mPtr.mValue = 0; }
  StyleSetHandle(const StyleSetHandle& aOth) { mPtr.mValue = aOth.mPtr.mValue; }
  MOZ_IMPLICIT StyleSetHandle(nsStyleSet* aSet) { *this = aSet; }
  MOZ_IMPLICIT StyleSetHandle(ServoStyleSet* aSet) { *this = aSet; }

  StyleSetHandle& operator=(nsStyleSet* aStyleSet)
  {
    mPtr.mValue = reinterpret_cast<uintptr_t>(aStyleSet);
    return *this;
  }

  StyleSetHandle& operator=(ServoStyleSet* aStyleSet)
  {
    mPtr.mValue =
      aStyleSet ? reinterpret_cast<uintptr_t>(aStyleSet) : 0;
    return *this;
  }

  // Make StyleSetHandle usable in boolean contexts.
  explicit operator bool() const { return !!mPtr.mValue; }
  bool operator!() const { return !mPtr.mValue; }
  bool operator==(const StyleSetHandle& aOth) const
  {
    return mPtr.mValue == aOth.mPtr.mValue;
  }
  bool operator!=(const StyleSetHandle& aOth) const { return !(*this == aOth); }

  // Make StyleSetHandle behave like a smart pointer.
  Ptr* operator->() { return &mPtr; }
  const Ptr* operator->() const { return &mPtr; }

private:
  Ptr mPtr;
};

} // namespace mozilla

#endif // mozilla_StyleSetHandle_h
