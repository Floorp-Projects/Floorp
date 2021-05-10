/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* the interface (to internal code) for retrieving computed style data */

#include "mozilla/ComputedStyle.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Maybe.h"
#include "mozilla/ToString.h"

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

#include "mozilla/dom/Document.h"
#include "nsPrintfCString.h"
#include "RubyUtils.h"
#include "mozilla/ComputedStyleInlines.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProfilerLabels.h"

#include "mozilla/ReflowInput.h"
#include "nsLayoutUtils.h"
#include "nsCoord.h"

// Ensure the binding function declarations in ComputedStyle.h matches
// those in ServoBindings.h.
#include "mozilla/ServoBindings.h"

namespace mozilla {

ComputedStyle::ComputedStyle(PseudoStyleType aPseudoType,
                             ServoComputedDataForgotten aComputedValues)
    : mSource(aComputedValues), mPseudoType(aPseudoType) {}

// If a struct returned nsChangeHint_UpdateContainingBlock, that means that one
// property's influence on whether we're a containing block for abs-pos or
// fixed-pos elements has changed.
//
// However, we only need to return the hint if the overall computation of
// whether we establish a containing block has really changed.
static bool ContainingBlockMayHaveChanged(const ComputedStyle& aOldStyle,
                                          const ComputedStyle& aNewStyle) {
  auto* oldDisp = aOldStyle.StyleDisplay();
  auto* newDisp = aNewStyle.StyleDisplay();

  if (oldDisp->IsPositionedStyle() != newDisp->IsPositionedStyle()) {
    return true;
  }

  bool fixedCB =
      oldDisp->IsFixedPosContainingBlockForNonSVGTextFrames(aOldStyle);
  if (fixedCB !=
      newDisp->IsFixedPosContainingBlockForNonSVGTextFrames(aNewStyle)) {
    return true;
  }
  // If we were both before and after a fixed-pos containing-block that means
  // that everything else doesn't matter, since all the other conditions are a
  // subset of this.
  if (fixedCB) {
    return false;
  }
  // Note that neither of these two following sets of frames
  // (transform-supporting and layout-and-paint-supporting frames) is a subset
  // of the other, because table frames support contain: layout/paint but not
  // transforms (which are instead inherited to the table wrapper), and quite a
  // few frame types support transforms but not contain: layout/paint (e.g.,
  // table rows and row groups, many SVG frames).
  if (oldDisp->IsFixedPosContainingBlockForTransformSupportingFrames() !=
      newDisp->IsFixedPosContainingBlockForTransformSupportingFrames()) {
    return true;
  }
  if (oldDisp
          ->IsFixedPosContainingBlockForContainLayoutAndPaintSupportingFrames() !=
      newDisp
          ->IsFixedPosContainingBlockForContainLayoutAndPaintSupportingFrames()) {
    return true;
  }
  return false;
}

nsChangeHint ComputedStyle::CalcStyleDifference(const ComputedStyle& aNewStyle,
                                                uint32_t* aEqualStructs) const {
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

#define EXPAND(...) __VA_ARGS__
#define DO_STRUCT_DIFFERENCE_WITH_ARGS(struct_, extra_args_)               \
  PR_BEGIN_MACRO                                                           \
  const nsStyle##struct_* this##struct_ = Style##struct_();                \
  structsFound |= STYLE_STRUCT_BIT(struct_);                               \
                                                                           \
  const nsStyle##struct_* other##struct_ = aNewStyle.Style##struct_();     \
  if (this##struct_ == other##struct_) {                                   \
    /* The very same struct, so we know that there will be no */           \
    /* differences.                                           */           \
    *aEqualStructs |= STYLE_STRUCT_BIT(struct_);                           \
  } else {                                                                 \
    nsChangeHint difference =                                              \
        this##struct_->CalcDifference(*other##struct_ EXPAND extra_args_); \
    hint |= difference;                                                    \
    if (!difference) {                                                     \
      *aEqualStructs |= STYLE_STRUCT_BIT(struct_);                         \
    }                                                                      \
  }                                                                        \
  styleStructCount++;                                                      \
  PR_END_MACRO
#define DO_STRUCT_DIFFERENCE(struct_) \
  DO_STRUCT_DIFFERENCE_WITH_ARGS(struct_, ())

  // FIXME: The order of these DO_STRUCT_DIFFERENCE calls is no longer
  // significant.  With a small amount of effort, we could replace them with a
  // #include "nsStyleStructList.h".
  DO_STRUCT_DIFFERENCE_WITH_ARGS(Display, (, *StylePosition()));
  DO_STRUCT_DIFFERENCE(XUL);
  DO_STRUCT_DIFFERENCE(Column);
  DO_STRUCT_DIFFERENCE(Content);
  DO_STRUCT_DIFFERENCE(UI);
  DO_STRUCT_DIFFERENCE(Visibility);
  DO_STRUCT_DIFFERENCE(Outline);
  DO_STRUCT_DIFFERENCE(TableBorder);
  DO_STRUCT_DIFFERENCE(Table);
  DO_STRUCT_DIFFERENCE(UIReset);
  DO_STRUCT_DIFFERENCE(Text);
  DO_STRUCT_DIFFERENCE_WITH_ARGS(List, (, *StyleDisplay()));
  DO_STRUCT_DIFFERENCE(SVGReset);
  DO_STRUCT_DIFFERENCE(SVG);
  DO_STRUCT_DIFFERENCE_WITH_ARGS(Position, (, *StyleVisibility()));
  DO_STRUCT_DIFFERENCE(Font);
  DO_STRUCT_DIFFERENCE(Margin);
  DO_STRUCT_DIFFERENCE(Padding);
  DO_STRUCT_DIFFERENCE(Border);
  DO_STRUCT_DIFFERENCE(TextReset);
  DO_STRUCT_DIFFERENCE(Effects);
  DO_STRUCT_DIFFERENCE(Background);
  DO_STRUCT_DIFFERENCE(Page);

#undef DO_STRUCT_DIFFERENCE
#undef DO_STRUCT_DIFFERENCE_WITH_ARGS
#undef EXPAND

  MOZ_ASSERT(styleStructCount == StyleStructConstants::kStyleStructCount,
             "missing a call to DO_STRUCT_DIFFERENCE");

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
  const ComputedStyle* thisVis = GetStyleIfVisited();
  const ComputedStyle* otherVis = aNewStyle.GetStyleIfVisited();
  if (!thisVis != !otherVis) {
    // One style has a style-if-visited and the other doesn't.
    // Presume a difference.
#define STYLE_STRUCT(name_, fields_) *aEqualStructs &= ~STYLE_STRUCT_BIT(name_);
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
#define STYLE_STRUCT(name_, fields_)                                 \
  {                                                                  \
    const nsStyle##name_* thisVisStruct = thisVis->Style##name_();   \
    const nsStyle##name_* otherVisStruct = otherVis->Style##name_(); \
    if (MOZ_FOR_EACH_SEPARATED(STYLE_FIELD, (||), (), fields_)) {    \
      *aEqualStructs &= ~STYLE_STRUCT_BIT(name_);                    \
      change = true;                                                 \
    }                                                                \
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
    if (!ContainingBlockMayHaveChanged(*this, aNewStyle)) {
      // While some styles that cause the frame to be a containing block
      // has changed, the overall result cannot have changed (no matter
      // what the frame type is).
      hint &= ~nsChangeHint_UpdateContainingBlock;
    }
  }

  if (HasAuthorSpecifiedBorderOrBackground() !=
      aNewStyle.HasAuthorSpecifiedBorderOrBackground()) {
    const StyleAppearance appearance = StyleDisplay()->EffectiveAppearance();
    if (appearance != StyleAppearance::None &&
        nsLayoutUtils::AuthorSpecifiedBorderBackgroundDisablesTheming(
            appearance)) {
      // A background-specified change may cause padding to change, so we may
      // need to reflow.  We use the same hint here as we do for "appearance"
      // changes.
      hint |= nsChangeHint_AllReflowHints | nsChangeHint_RepaintFrame;
    }
  }

  MOZ_ASSERT(NS_IsHintSubset(hint, nsChangeHint_AllHints),
             "Added a new hint without bumping AllHints?");
  return hint & ~nsChangeHint_NeutralChange;
}

#ifdef DEBUG
void ComputedStyle::List(FILE* out, int32_t aIndent) {
  nsAutoCString str;
  // Indent
  int32_t ix;
  for (ix = aIndent; --ix >= 0;) {
    str.AppendLiteral("  ");
  }
  str.Append(nsPrintfCString("%p(%d) parent=%p ", (void*)this, 0, nullptr));
  if (mPseudoType != PseudoStyleType::NotPseudo) {
    str.Append(nsPrintfCString("%s ", ToString(mPseudoType).c_str()));
  }

  fprintf_stderr(out, "%s{ServoComputedData}\n", str.get());
}
#endif

template <typename Func>
static nscolor GetVisitedDependentColorInternal(const ComputedStyle& aStyle,
                                                Func aColorFunc) {
  nscolor colors[2];
  colors[0] = aColorFunc(aStyle);
  if (const ComputedStyle* visitedStyle = aStyle.GetStyleIfVisited()) {
    colors[1] = aColorFunc(*visitedStyle);
    return ComputedStyle::CombineVisitedColors(colors,
                                               aStyle.RelevantLinkVisited());
  }
  return colors[0];
}

static nscolor ExtractColor(const ComputedStyle& aStyle,
                            const StyleRGBA& aColor) {
  return aColor.ToColor();
}

static nscolor ExtractColor(const ComputedStyle& aStyle,
                            const StyleColor& aColor) {
  return aColor.CalcColor(aStyle);
}

// Currently caret-color, the only property in the list which is a ColorOrAuto,
// always maps auto to currentcolor.
static nscolor ExtractColor(const ComputedStyle& aStyle,
                            const StyleColorOrAuto& aColor) {
  if (aColor.IsAuto()) {
    return ExtractColor(aStyle, StyleColor::CurrentColor());
  }
  return ExtractColor(aStyle, aColor.AsColor());
}

static nscolor ExtractColor(const ComputedStyle& aStyle,
                            const StyleSVGPaint& aPaintServer) {
  return aPaintServer.kind.IsColor()
             ? ExtractColor(aStyle, aPaintServer.kind.AsColor())
             : NS_RGBA(0, 0, 0, 0);
}

#define STYLE_FIELD(struct_, field_) aField == &struct_::field_ ||
#define STYLE_STRUCT(name_, fields_)                                           \
  template <>                                                                  \
  nscolor ComputedStyle::GetVisitedDependentColor(                             \
      decltype(nsStyle##name_::MOZ_ARG_1 fields_) nsStyle##name_::*aField)     \
      const {                                                                  \
    MOZ_ASSERT(MOZ_FOR_EACH(STYLE_FIELD, (nsStyle##name_, ), fields_) false,   \
               "Getting visited-dependent color for a field in nsStyle" #name_ \
               " which is not listed in nsCSSVisitedDependentPropList.h");     \
    return GetVisitedDependentColorInternal(                                   \
        *this, [aField](const ComputedStyle& aStyle) {                         \
          return ExtractColor(aStyle, aStyle.Style##name_()->*aField);         \
        });                                                                    \
  }
#include "nsCSSVisitedDependentPropList.h"
#undef STYLE_STRUCT
#undef STYLE_FIELD

struct ColorIndexSet {
  uint8_t colorIndex, alphaIndex;
};

static const ColorIndexSet gVisitedIndices[2] = {{0, 0}, {1, 0}};

/* static */
nscolor ComputedStyle::CombineVisitedColors(nscolor* aColors,
                                            bool aLinkIsVisited) {
  if (NS_GET_A(aColors[1]) == 0) {
    // If the style-if-visited is transparent, then just use the
    // unvisited style rather than using the (meaningless) color
    // components of the visited style along with a potentially
    // non-transparent alpha value.
    aLinkIsVisited = false;
  }

  // NOTE: We want this code to have as little timing dependence as
  // possible on whether this->RelevantLinkVisited() is true.
  const ColorIndexSet& set = gVisitedIndices[aLinkIsVisited ? 1 : 0];

  nscolor colorColor = aColors[set.colorIndex];
  nscolor alphaColor = aColors[set.alphaIndex];
  return NS_RGBA(NS_GET_R(colorColor), NS_GET_G(colorColor),
                 NS_GET_B(colorColor), NS_GET_A(alphaColor));
}

#ifdef DEBUG
/* static */ const char* ComputedStyle::StructName(StyleStructID aSID) {
  switch (aSID) {
#  define STYLE_STRUCT(name_)  \
    case StyleStructID::name_: \
      return #name_;
#  include "nsStyleStructList.h"
#  undef STYLE_STRUCT
    default:
      return "Unknown";
  }
}

/* static */
Maybe<StyleStructID> ComputedStyle::LookupStruct(const nsACString& aName) {
#  define STYLE_STRUCT(name_) \
    if (aName.EqualsLiteral(#name_)) return Some(StyleStructID::name_);
#  include "nsStyleStructList.h"
#  undef STYLE_STRUCT
  return Nothing();
}
#endif  // DEBUG

ComputedStyle* ComputedStyle::GetCachedLazyPseudoStyle(
    PseudoStyleType aPseudo) const {
  MOZ_ASSERT(PseudoStyle::IsPseudoElement(aPseudo));

  if (nsCSSPseudoElements::PseudoElementSupportsUserActionState(aPseudo)) {
    return nullptr;
  }

  return mCachedInheritingStyles.Lookup(aPseudo);
}

MOZ_DEFINE_MALLOC_ENCLOSING_SIZE_OF(ServoComputedValuesMallocEnclosingSizeOf)

void ComputedStyle::AddSizeOfIncludingThis(nsWindowSizes& aSizes,
                                           size_t* aCVsSize) const {
  // Note: |this| sits within a servo_arc::Arc, i.e. it is preceded by a
  // refcount. So we need to measure it with a function that can handle an
  // interior pointer. We use ServoComputedValuesMallocEnclosingSizeOf to
  // clearly identify in DMD's output the memory measured here.
  *aCVsSize += ServoComputedValuesMallocEnclosingSizeOf(this);
  mSource.AddSizeOfExcludingThis(aSizes);
  mCachedInheritingStyles.AddSizeOfIncludingThis(aSizes, aCVsSize);
}

#ifdef DEBUG
bool ComputedStyle::EqualForCachedAnonymousContentStyle(
    const ComputedStyle& aOther) const {
  // One thing we can't add UA rules to prevent is different -x-lang
  // values being inherited in.  So we use this FFI function function rather
  // than rely on CalcStyleDifference, which can't tell us which specific
  // properties have changed.
  return Servo_ComputedValues_EqualForCachedAnonymousContentStyle(this,
                                                                  &aOther);
}

#endif

}  // namespace mozilla
