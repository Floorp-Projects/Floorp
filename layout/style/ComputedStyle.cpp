/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* the interface (to internal code) for retrieving computed style data */

#include "mozilla/ComputedStyle.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Maybe.h"

#include "nsCSSAnonBoxes.h"
#include "nsCSSPseudoElements.h"
#include "nsFontMetrics.h"
#include "nsStyleConsts.h"
#include "nsStyleStruct.h"
#include "nsStyleStructInlines.h"
#include "nsString.h"
#include "nsPresContext.h"
#include "nsWindowSizes.h"

#include "nsCOMPtr.h"
#include "nsIPresShell.h"

#include "GeckoProfiler.h"
#include "nsIDocument.h"
#include "nsPrintfCString.h"
#include "RubyUtils.h"
#include "mozilla/ArenaObjectID.h"
#include "mozilla/ComputedStyleInlines.h"
#include "mozilla/Preferences.h"

#include "mozilla/ReflowInput.h"
#include "nsLayoutUtils.h"
#include "nsCoord.h"

// Ensure the binding function declarations in ComputedStyle.h matches
// those in ServoBindings.h.
#include "mozilla/ServoBindings.h"

namespace mozilla {

//----------------------------------------------------------------------

ComputedStyle::ComputedStyle(nsPresContext* aPresContext,
                             nsAtom* aPseudoTag,
                             CSSPseudoElementType aPseudoType,
                             ServoComputedDataForgotten aComputedValues)
  : mPresContext(aPresContext)
  , mSource(aComputedValues)
  , mPseudoTag(aPseudoTag)
  , mBits(static_cast<Bit>(Servo_ComputedValues_GetStyleBits(this)))
  , mPseudoType(aPseudoType)
{
  MOZ_ASSERT(ComputedData());
}

nsChangeHint
ComputedStyle::CalcStyleDifference(ComputedStyle* aNewContext,
                                   uint32_t* aEqualStructs)
{
  MOZ_ASSERT(aNewContext);
  AUTO_PROFILER_LABEL("ComputedStyle::CalcStyleDifference", LAYOUT);
  static_assert(StyleStructConstants::kStyleStructCount <= 32,
                "aEqualStructs is not big enough");

  *aEqualStructs = 0;

  nsChangeHint hint = nsChangeHint(0);
  // We must always ensure that we populate the structs on the new style
  // context that are filled in on the old context, so that if we get
  // two style changes in succession, the second of which causes a real
  // style change, the PeekStyleData doesn't return null (implying that
  // nobody ever looked at that struct's data).  In other words, we
  // can't skip later structs if we get a big change up front, because
  // we could later get a small change in one of those structs that we
  // don't want to miss.

  DebugOnly<uint32_t> structsFound = 0;

  DebugOnly<int> styleStructCount = 0;

  // Servo's optimization to stop the cascade when there are no style changes
  // that children need to be recascade for relies on comparing all of the
  // structs, not just those that are returned from PeekStyleData, although
  // if PeekStyleData does return null we could avoid to accumulate any change
  // hints for those structs.
  //
  // FIXME(emilio): Reintroduce that optimization either for all kind of structs
  // after bug 1368290 with a weak parent pointer from text, or just for reset
  // structs.
#define STYLE_STRUCT_BIT(name_) \
  StyleStructConstants::BitFor(StyleStructID::name_)
#define PEEK(struct_) \
   ComputedData()->GetStyle##struct_()

#define EXPAND(...) __VA_ARGS__
#define DO_STRUCT_DIFFERENCE_WITH_ARGS(struct_, extra_args_)                  \
  PR_BEGIN_MACRO                                                              \
    const nsStyle##struct_* this##struct_ = PEEK(struct_);                    \
    if (this##struct_) {                                                      \
      structsFound |= STYLE_STRUCT_BIT(struct_);                              \
                                                                              \
      const nsStyle##struct_* other##struct_ =                                \
        aNewContext->ThreadsafeStyle##struct_();                              \
      if (this##struct_ == other##struct_) {                                  \
        /* The very same struct, so we know that there will be no */          \
        /* differences.                                           */          \
        *aEqualStructs |= STYLE_STRUCT_BIT(struct_);                          \
      } else {                                                                \
        nsChangeHint difference =                                             \
          this##struct_->CalcDifference(*other##struct_ EXPAND extra_args_);  \
        hint |= difference;                                                   \
        if (!difference) {                                                    \
          *aEqualStructs |= STYLE_STRUCT_BIT(struct_);                        \
        }                                                                     \
      }                                                                       \
    } else {                                                                  \
      *aEqualStructs |= STYLE_STRUCT_BIT(struct_);                            \
    }                                                                         \
    styleStructCount++;                                                       \
  PR_END_MACRO
#define DO_STRUCT_DIFFERENCE(struct_) \
  DO_STRUCT_DIFFERENCE_WITH_ARGS(struct_, ())

  // FIXME: The order of these DO_STRUCT_DIFFERENCE calls is no longer
  // significant.  With a small amount of effort, we could replace them with a
  // #include "nsStyleStructList.h".
  DO_STRUCT_DIFFERENCE(Display);
  DO_STRUCT_DIFFERENCE(XUL);
  DO_STRUCT_DIFFERENCE(Column);
  DO_STRUCT_DIFFERENCE(Content);
  DO_STRUCT_DIFFERENCE(UserInterface);
  DO_STRUCT_DIFFERENCE(Visibility);
  DO_STRUCT_DIFFERENCE(Outline);
  DO_STRUCT_DIFFERENCE(TableBorder);
  DO_STRUCT_DIFFERENCE(Table);
  DO_STRUCT_DIFFERENCE(UIReset);
  DO_STRUCT_DIFFERENCE(Text);
  DO_STRUCT_DIFFERENCE_WITH_ARGS(List, (, PEEK(Display)));
  DO_STRUCT_DIFFERENCE(SVGReset);
  DO_STRUCT_DIFFERENCE(SVG);
  DO_STRUCT_DIFFERENCE_WITH_ARGS(Position, (, PEEK(Visibility)));
  DO_STRUCT_DIFFERENCE(Font);
  DO_STRUCT_DIFFERENCE(Margin);
  DO_STRUCT_DIFFERENCE(Padding);
  DO_STRUCT_DIFFERENCE(Border);
  DO_STRUCT_DIFFERENCE(TextReset);
  DO_STRUCT_DIFFERENCE(Effects);
  DO_STRUCT_DIFFERENCE(Background);
  DO_STRUCT_DIFFERENCE(Color);

#undef DO_STRUCT_DIFFERENCE
#undef DO_STRUCT_DIFFERENCE_WITH_ARGS
#undef EXPAND

  MOZ_ASSERT(styleStructCount == StyleStructConstants::kStyleStructCount,
             "missing a call to DO_STRUCT_DIFFERENCE");

#ifdef DEBUG
  #define STYLE_STRUCT(name_)                                             \
    MOZ_ASSERT(!!(structsFound & STYLE_STRUCT_BIT(name_)) ==              \
               (PEEK(name_) != nullptr),                                  \
               "PeekStyleData results must not change in the middle of "  \
               "difference calculation.");
  #include "nsStyleStructList.h"
  #undef STYLE_STRUCT
#endif

  // Note that we do not check whether this->RelevantLinkVisited() !=
  // aNewContext->RelevantLinkVisited(); we don't need to since
  // nsCSSFrameConstructor::DoContentStateChanged always adds
  // nsChangeHint_RepaintFrame for NS_EVENT_STATE_VISITED changes (and
  // needs to, since HasStateDependentStyle probably doesn't work right
  // for NS_EVENT_STATE_VISITED).  Hopefully this doesn't actually
  // expose whether links are visited to performance tests since all
  // link coloring happens asynchronously at a time when it's hard for
  // the page to measure.
  // However, we do need to compute the larger of the changes that can
  // happen depending on whether the link is visited or unvisited, since
  // doing only the one that's currently appropriate would expose which
  // links are in history to easy performance measurement.  Therefore,
  // here, we add nsChangeHint_RepaintFrame hints (the maximum for
  // things that can depend on :visited) for the properties on which we
  // call GetVisitedDependentColor.
  ComputedStyle* thisVis = GetStyleIfVisited();
  ComputedStyle* otherVis = aNewContext->GetStyleIfVisited();
  if (!thisVis != !otherVis) {
    // One style has a style-if-visited and the other doesn't.
    // Presume a difference.
#define STYLE_STRUCT(name_, fields_) \
    *aEqualStructs &= ~STYLE_STRUCT_BIT(name_);
#include "nsCSSVisitedDependentPropList.h"
#undef STYLE_STRUCT
    hint |= nsChangeHint_RepaintFrame;
  } else if (thisVis) {
    // Both styles have a style-if-visited.
    bool change = false;

    // NB: Calling Peek on |this|, not |thisVis|, since callers may look
    // at a struct on |this| without looking at the same struct on
    // |thisVis| (including this function if we skip one of these checks
    // due to change being true already or due to the old style not having a
    // style-if-visited), but not the other way around.
#define STYLE_FIELD(name_) thisVisStruct->name_ != otherVisStruct->name_
#define STYLE_STRUCT(name_, fields_)                                    \
    if (PEEK(name_)) {                                                  \
      const nsStyle##name_* thisVisStruct =                             \
        thisVis->ThreadsafeStyle##name_();                              \
      const nsStyle##name_* otherVisStruct =                            \
        otherVis->ThreadsafeStyle##name_();                             \
      if (MOZ_FOR_EACH_SEPARATED(STYLE_FIELD, (||), (), fields_)) {     \
        *aEqualStructs &= ~STYLE_STRUCT_BIT(name_);                     \
        change = true;                                                  \
      }                                                                 \
    }
#include "nsCSSVisitedDependentPropList.h"
#undef STYLE_STRUCT
#undef STYLE_FIELD
#undef STYLE_STRUCT_BIT

    if (change) {
      hint |= nsChangeHint_RepaintFrame;
    }
  }

  if (hint & nsChangeHint_UpdateContainingBlock) {
    // If a struct returned nsChangeHint_UpdateContainingBlock, that
    // means that one property's influence on whether we're a containing
    // block for abs-pos or fixed-pos elements has changed.  However, we
    // only need to return the hint if the overall computation of
    // whether we establish a containing block has changed.

    // This depends on data in nsStyleDisplay and nsStyleEffects, so we do it
    // here

    // Note that it's perhaps good for this test to be last because it
    // doesn't use Peek* functions to get the structs on the old
    // context.  But this isn't a big concern because these struct
    // getters should be called during frame construction anyway.
    if (ThreadsafeStyleDisplay()->IsAbsPosContainingBlockForAppropriateFrame(*this) ==
        aNewContext->ThreadsafeStyleDisplay()->
          IsAbsPosContainingBlockForAppropriateFrame(*aNewContext) &&
        ThreadsafeStyleDisplay()->IsFixedPosContainingBlockForAppropriateFrame(*this) ==
        aNewContext->ThreadsafeStyleDisplay()->
          IsFixedPosContainingBlockForAppropriateFrame(*aNewContext)) {
      // While some styles that cause the frame to be a containing block
      // has changed, the overall result hasn't.
      hint &= ~nsChangeHint_UpdateContainingBlock;
    }
  }

  MOZ_ASSERT(NS_IsHintSubset(hint, nsChangeHint_AllHints),
             "Added a new hint without bumping AllHints?");
  return hint & ~nsChangeHint_NeutralChange;
}

#ifdef DEBUG
void ComputedStyle::List(FILE* out, int32_t aIndent)
{
  nsAutoCString str;
  // Indent
  int32_t ix;
  for (ix = aIndent; --ix >= 0; ) {
    str.AppendLiteral("  ");
  }
  str.Append(nsPrintfCString("%p(%d) parent=%p ",
                             (void*)this,
                             0, nullptr
                             ));
  if (mPseudoTag) {
    nsAutoString  buffer;
    mPseudoTag->ToString(buffer);
    AppendUTF16toUTF8(buffer, str);
    str.Append(' ');
  }

  fprintf_stderr(out, "%s{ServoComputedData}\n", str.get());
}
#endif


nsIPresShell*
ComputedStyle::Arena()
{
  return mPresContext->PresShell();
}

template<typename Func>
static nscolor
GetVisitedDependentColorInternal(ComputedStyle* aSc, Func aColorFunc)
{
  nscolor colors[2];
  colors[0] = aColorFunc(aSc);
  if (ComputedStyle* visitedStyle = aSc->GetStyleIfVisited()) {
    colors[1] = aColorFunc(visitedStyle);
    return ComputedStyle::
      CombineVisitedColors(colors, aSc->RelevantLinkVisited());
  }
  return colors[0];
}

static nscolor
ExtractColor(ComputedStyle* aStyle, const nscolor& aColor)
{
  return aColor;
}

static nscolor
ExtractColor(ComputedStyle* aStyle, const StyleComplexColor& aColor)
{
  return aColor.CalcColor(aStyle);
}

static nscolor
ExtractColor(ComputedStyle* aStyle, const nsStyleSVGPaint& aPaintServer)
{
  return aPaintServer.Type() == eStyleSVGPaintType_Color
    ? aPaintServer.GetColor() : NS_RGBA(0, 0, 0, 0);
}

#define STYLE_FIELD(struct_, field_) aField == &struct_::field_ ||
#define STYLE_STRUCT(name_, fields_)                                          \
  template<> nscolor                                                          \
  ComputedStyle::GetVisitedDependentColor(                                   \
    decltype(nsStyle##name_::MOZ_ARG_1 fields_) nsStyle##name_::* aField)     \
  {                                                                           \
    MOZ_ASSERT(MOZ_FOR_EACH(STYLE_FIELD, (nsStyle##name_,), fields_) false,   \
               "Getting visited-dependent color for a field in nsStyle"#name_ \
               " which is not listed in nsCSSVisitedDependentPropList.h");    \
    return GetVisitedDependentColorInternal(this,                             \
                                            [aField](ComputedStyle* sc) {    \
      return ExtractColor(sc, sc->Style##name_()->*aField);                   \
    });                                                                       \
  }
#include "nsCSSVisitedDependentPropList.h"
#undef STYLE_STRUCT
#undef STYLE_FIELD

struct ColorIndexSet {
  uint8_t colorIndex, alphaIndex;
};

static const ColorIndexSet gVisitedIndices[2] = { { 0, 0 }, { 1, 0 } };

/* static */ nscolor
ComputedStyle::CombineVisitedColors(nscolor *aColors, bool aLinkIsVisited)
{
  if (NS_GET_A(aColors[1]) == 0) {
    // If the style-if-visited is transparent, then just use the
    // unvisited style rather than using the (meaningless) color
    // components of the visited style along with a potentially
    // non-transparent alpha value.
    aLinkIsVisited = false;
  }

  // NOTE: We want this code to have as little timing dependence as
  // possible on whether this->RelevantLinkVisited() is true.
  const ColorIndexSet &set =
    gVisitedIndices[aLinkIsVisited ? 1 : 0];

  nscolor colorColor = aColors[set.colorIndex];
  nscolor alphaColor = aColors[set.alphaIndex];
  return NS_RGBA(NS_GET_R(colorColor), NS_GET_G(colorColor),
                 NS_GET_B(colorColor), NS_GET_A(alphaColor));
}

#ifdef DEBUG
/* static */ const char*
ComputedStyle::StructName(StyleStructID aSID)
{
  switch (aSID) {
#define STYLE_STRUCT(name_)     \
    case StyleStructID::name_:  \
      return #name_;
#include "nsStyleStructList.h"
#undef STYLE_STRUCT
    default:
      return "Unknown";
  }
}

/* static */ Maybe<StyleStructID>
ComputedStyle::LookupStruct(const nsACString& aName)
{
#define STYLE_STRUCT(name_)             \
  if (aName.EqualsLiteral(#name_)) \
    return Some(StyleStructID::name_);
#include "nsStyleStructList.h"
#undef STYLE_STRUCT
  return Nothing();
}
#endif // DEBUG

ComputedStyle*
ComputedStyle::GetCachedLazyPseudoStyle(CSSPseudoElementType aPseudo) const
{
  MOZ_ASSERT(aPseudo != CSSPseudoElementType::NotPseudo &&
             aPseudo != CSSPseudoElementType::InheritingAnonBox &&
             aPseudo != CSSPseudoElementType::NonInheritingAnonBox);
  MOZ_ASSERT(!IsLazilyCascadedPseudoElement(), "Lazy pseudos can't inherit lazy pseudos");

  if (nsCSSPseudoElements::PseudoElementSupportsUserActionState(aPseudo)) {
    return nullptr;
  }

  return mCachedInheritingStyles.Lookup(nsCSSPseudoElements::GetPseudoAtom(aPseudo));
}

MOZ_DEFINE_MALLOC_ENCLOSING_SIZE_OF(ServoComputedValuesMallocEnclosingSizeOf)

void
ComputedStyle::AddSizeOfIncludingThis(nsWindowSizes& aSizes,
                                      size_t* aCVsSize) const
{
  // Note: |this| sits within a servo_arc::Arc, i.e. it is preceded by a
  // refcount. So we need to measure it with a function that can handle an
  // interior pointer. We use ServoComputedValuesMallocEnclosingSizeOf to
  // clearly identify in DMD's output the memory measured here.
  *aCVsSize += ServoComputedValuesMallocEnclosingSizeOf(this);
  mSource.AddSizeOfExcludingThis(aSizes);
  mCachedInheritingStyles.AddSizeOfIncludingThis(aSizes, aCVsSize);
}

} // namespace mozilla
