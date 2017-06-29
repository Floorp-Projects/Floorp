/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* the interface (to internal code) for retrieving computed style data */

#include "nsStyleContext.h"
#include "CSSVariableImageTable.h"
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
#include "nsIStyleRule.h"

#include "nsCOMPtr.h"
#include "nsStyleSet.h"
#include "nsIPresShell.h"

#include "nsRuleNode.h"
#include "GeckoProfiler.h"
#include "nsIDocument.h"
#include "nsPrintfCString.h"
#include "RubyUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/ArenaObjectID.h"
#include "mozilla/StyleSetHandle.h"
#include "mozilla/StyleSetHandleInlines.h"
#include "mozilla/GeckoStyleContext.h"
#include "mozilla/ServoStyleContext.h"
#include "nsStyleContextInlines.h"

#include "mozilla/ReflowInput.h"
#include "nsLayoutUtils.h"
#include "nsCoord.h"

// Ensure the binding function declarations in nsStyleContext.h matches
// those in ServoBindings.h.
#include "mozilla/ServoBindings.h"

using namespace mozilla;

//----------------------------------------------------------------------

#ifdef DEBUG

// Check that the style struct IDs are in the same order as they are
// in nsStyleStructList.h, since when we set up the IDs, we include
// the inherited and reset structs spearately from nsStyleStructList.h
enum DebugStyleStruct {
#define STYLE_STRUCT(name, checkdata_cb) eDebugStyleStruct_##name,
#include "nsStyleStructList.h"
#undef STYLE_STRUCT
};

#define STYLE_STRUCT(name, checkdata_cb) \
  static_assert(static_cast<int>(eDebugStyleStruct_##name) == \
                  static_cast<int>(eStyleStruct_##name), \
                "Style struct IDs are not declared in order?");
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

const uint32_t nsStyleContext::sDependencyTable[] = {
#define STYLE_STRUCT(name, checkdata_cb)
#define STYLE_STRUCT_DEP(dep) NS_STYLE_INHERIT_BIT(dep) |
#define STYLE_STRUCT_END() 0,
#include "nsStyleStructList.h"
#undef STYLE_STRUCT
#undef STYLE_STRUCT_DEP
#undef STYLE_STRUCT_END
};

// Whether to perform expensive assertions in the nsStyleContext destructor.
static bool sExpensiveStyleStructAssertionsEnabled;
#endif

nsStyleContext::nsStyleContext(nsStyleContext* aParent,
                               nsIAtom* aPseudoTag,
                               CSSPseudoElementType aPseudoType)
  : mParent(aParent)
  , mPseudoTag(aPseudoTag)
  , mBits(((uint64_t)aPseudoType) << NS_STYLE_CONTEXT_TYPE_SHIFT)
  , mRefCnt(0)
#ifdef DEBUG
  , mFrameRefCnt(0)
  , mComputingStruct(nsStyleStructID_None)
#endif
{}

void
nsStyleContext::FinishConstruction()
{
  // This check has to be done "backward", because if it were written the
  // more natural way it wouldn't fail even when it needed to.
  static_assert((UINT64_MAX >> NS_STYLE_CONTEXT_TYPE_SHIFT) >=
                 static_cast<CSSPseudoElementTypeBase>(
                   CSSPseudoElementType::MAX),
                "pseudo element bits no longer fit in a uint64_t");

#ifdef DEBUG
  if (auto servo = GetAsServo()) {
    MOZ_ASSERT(servo->ComputedValues());
  } else {
    MOZ_ASSERT(RuleNode());
  }

  static_assert(MOZ_ARRAY_LENGTH(nsStyleContext::sDependencyTable)
                  == nsStyleStructID_Length,
                "Number of items in dependency table doesn't match IDs");
#endif

  if (mParent) {
    mParent->AddChild(this);
  }

  SetStyleBits();

  #define eStyleStruct_LastItem (nsStyleStructID_Length - 1)
  static_assert(NS_STYLE_INHERIT_MASK & NS_STYLE_INHERIT_BIT(LastItem),
                "NS_STYLE_INHERIT_MASK must be bigger, and other bits shifted");
  #undef eStyleStruct_LastItem
}

void
nsStyleContext::Destructor()
{
  GeckoStyleContext* gecko = GetAsGecko();
#ifdef DEBUG
  if (gecko) {
    NS_ASSERTION(gecko->HasNoChildren(), "destructing context with children");
    if (sExpensiveStyleStructAssertionsEnabled) {
      // Assert that the style structs we are about to destroy are not referenced
      // anywhere else in the style context tree.  These checks are expensive,
      // which is why they are not enabled by default.
      GeckoStyleContext* root = gecko;
      while (root->GetParent()) {
        root = root->GetParent();
      }
      root->AssertStructsNotUsedElsewhere(gecko,
                                          std::numeric_limits<int32_t>::max());
    } else {
      // In DEBUG builds when the pref is not enabled, we perform a more limited
      // check just of the children of this style context.
      gecko->AssertStructsNotUsedElsewhere(gecko, 2);
    }
  }
#endif

  nsPresContext *presContext = PresContext();
  DebugOnly<nsStyleSet*> geckoStyleSet = presContext->PresShell()->StyleSet()->GetAsGecko();
  NS_ASSERTION(!geckoStyleSet ||
               geckoStyleSet->GetRuleTree() == AsGecko()->RuleNode()->RuleTree() ||
               geckoStyleSet->IsInRuleTreeReconstruct(),
               "destroying style context from old rule tree too late");

  if (mParent) {
    mParent->RemoveChild(this);
  } else {
    presContext->StyleSet()->RootStyleContextRemoved();
  }

  // Free up our data structs.
  if (gecko) {
    gecko->DestroyCachedStructs(presContext);
  }

  // Free any ImageValues we were holding on to for CSS variable values.
  CSSVariableImageTable::RemoveAll(this);
}

void nsStyleContext::AddChild(nsStyleContext* aChild)
{
  if (GeckoStyleContext* gecko = GetAsGecko()) {
    gecko->AddChild(aChild->AsGecko());
  }
}

void nsStyleContext::RemoveChild(nsStyleContext* aChild)
{
  if (GeckoStyleContext* gecko = GetAsGecko()) {
    gecko->RemoveChild(aChild->AsGecko());
  }
}

void
nsStyleContext::MoveTo(nsStyleContext* aNewParent)
{
  MOZ_ASSERT(aNewParent != mParent);

  // This function shouldn't be getting called if the parents have different
  // values for some flags in mBits (unless the flag is also set on this style
  // context) because if that were the case we would need to recompute those
  // bits for |this|.

#define CHECK_FLAG(bit_) \
  MOZ_ASSERT((mParent->mBits & (bit_)) == (aNewParent->mBits & (bit_)) ||     \
             (mBits & (bit_)),                                                \
             "MoveTo cannot be called if " #bit_ " value on old and new "     \
             "style context parents do not match, unless the flag is set "    \
             "on this style context");

  CHECK_FLAG(NS_STYLE_HAS_PSEUDO_ELEMENT_DATA)
  CHECK_FLAG(NS_STYLE_IN_DISPLAY_NONE_SUBTREE)
  CHECK_FLAG(NS_STYLE_HAS_TEXT_DECORATION_LINES)
  CHECK_FLAG(NS_STYLE_RELEVANT_LINK_VISITED)

#undef CHECK_FLAG

  // Assertions checking for visited style are just to avoid some tricky
  // cases we can't be bothered handling at the moment.
  MOZ_ASSERT(!IsStyleIfVisited());
  MOZ_ASSERT(!mParent->IsStyleIfVisited());
  MOZ_ASSERT(!aNewParent->IsStyleIfVisited());
  MOZ_ASSERT(!mStyleIfVisited || mStyleIfVisited->mParent == mParent);

  if (mParent->HasChildThatUsesResetStyle()) {
    aNewParent->AddStyleBit(NS_STYLE_HAS_CHILD_THAT_USES_RESET_STYLE);
  }

  mParent->RemoveChild(this);
  mParent = aNewParent;
  mParent->AddChild(this);

  if (mStyleIfVisited) {
    mStyleIfVisited->mParent->RemoveChild(mStyleIfVisited);
    mStyleIfVisited->mParent = aNewParent;
    mStyleIfVisited->mParent->AddChild(mStyleIfVisited);
  }
}

template<class StyleContextLike>
nsChangeHint
nsStyleContext::CalcStyleDifferenceInternal(StyleContextLike* aNewContext,
                                            uint32_t* aEqualStructs,
                                            uint32_t* aSamePointerStructs)
{
  AUTO_PROFILER_LABEL("nsStyleContext::CalcStyleDifferenceInternal", CSS);

  static_assert(nsStyleStructID_Length <= 32,
                "aEqualStructs is not big enough");

  *aEqualStructs = 0;

  nsChangeHint hint = nsChangeHint(0);
  NS_ENSURE_TRUE(aNewContext, hint);
  // We must always ensure that we populate the structs on the new style
  // context that are filled in on the old context, so that if we get
  // two style changes in succession, the second of which causes a real
  // style change, the PeekStyleData doesn't return null (implying that
  // nobody ever looked at that struct's data).  In other words, we
  // can't skip later structs if we get a big change up front, because
  // we could later get a small change in one of those structs that we
  // don't want to miss.

  DebugOnly<uint32_t> structsFound = 0;

  // FIXME(heycam): We should just do the comparison in
  // nsStyleVariables::CalcDifference, returning NeutralChange if there are
  // any Variables differences.
  const nsStyleVariables* thisVariables = PeekStyleVariables();
  if (thisVariables) {
    structsFound |= NS_STYLE_INHERIT_BIT(Variables);
    const nsStyleVariables* otherVariables = aNewContext->StyleVariables();
    if (thisVariables->mVariables == otherVariables->mVariables) {
      *aEqualStructs |= NS_STYLE_INHERIT_BIT(Variables);
    }
  } else {
    *aEqualStructs |= NS_STYLE_INHERIT_BIT(Variables);
  }

  DebugOnly<int> styleStructCount = 1;  // count Variables already

  // Servo's optimization to stop the cascade when there are no style changes
  // that children need to be recascade for relies on comparing all of the
  // structs, not just those that are returned from PeekStyleData, although
  // if PeekStyleData does return null we still don't want to accumulate
  // any change hints for those structs.
  bool checkUnrequestedServoStructs = IsServo();

#define EXPAND(...) __VA_ARGS__
#define DO_STRUCT_DIFFERENCE_WITH_ARGS(struct_, extra_args_)                  \
  PR_BEGIN_MACRO                                                              \
    const nsStyle##struct_* this##struct_ = PeekStyle##struct_();             \
    bool unrequestedStruct;                                                   \
    if (this##struct_) {                                                      \
      unrequestedStruct = false;                                              \
      structsFound |= NS_STYLE_INHERIT_BIT(struct_);                          \
    } else if (checkUnrequestedServoStructs) {                                \
      this##struct_ =                                                         \
        Servo_GetStyle##struct_(AsServo()->ComputedValues());                 \
      unrequestedStruct = true;                                               \
    } else {                                                                  \
      unrequestedStruct = false;                                              \
    }                                                                         \
    if (this##struct_) {                                                      \
      const nsStyle##struct_* other##struct_ = aNewContext->Style##struct_(); \
      if (this##struct_ == other##struct_) {                                  \
        /* The very same struct, so we know that there will be no */          \
        /* differences.                                           */          \
        *aEqualStructs |= NS_STYLE_INHERIT_BIT(struct_);                      \
      } else {                                                                \
        nsChangeHint difference =                                             \
          this##struct_->CalcDifference(*other##struct_ EXPAND extra_args_);  \
        if (!unrequestedStruct) {                                             \
          hint |= difference;                                                 \
        }                                                                     \
        if (!difference) {                                                    \
          *aEqualStructs |= NS_STYLE_INHERIT_BIT(struct_);                    \
        }                                                                     \
      }                                                                       \
    } else {                                                                  \
      *aEqualStructs |= NS_STYLE_INHERIT_BIT(struct_);                        \
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
  DO_STRUCT_DIFFERENCE_WITH_ARGS(List, (, PeekStyleDisplay()));
  DO_STRUCT_DIFFERENCE(SVGReset);
  DO_STRUCT_DIFFERENCE(SVG);
  DO_STRUCT_DIFFERENCE_WITH_ARGS(Position, (, PeekStyleVisibility()));
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

  MOZ_ASSERT(styleStructCount == nsStyleStructID_Length,
             "missing a call to DO_STRUCT_DIFFERENCE");

#ifdef DEBUG
  #define STYLE_STRUCT(name_, callback_)                                      \
    MOZ_ASSERT(!!(structsFound & NS_STYLE_INHERIT_BIT(name_)) ==              \
               !!PeekStyle##name_(),                                          \
               "PeekStyleData results must not change in the middle of "      \
               "difference calculation.");
  #include "nsStyleStructList.h"
  #undef STYLE_STRUCT
#endif

  // We check for struct pointer equality here rather than as part of the
  // DO_STRUCT_DIFFERENCE calls, since those calls can result in structs
  // we previously examined and found to be null on this style context
  // getting computed by later DO_STRUCT_DIFFERENCE calls (which can
  // happen when the nsRuleNode::ComputeXXXData method looks up another
  // struct.)  This is important for callers in RestyleManager that
  // need to know the equality or not of the final set of cached struct
  // pointers.
  *aSamePointerStructs = 0;

#define STYLE_STRUCT(name_, callback_)                                        \
  {                                                                           \
    const nsStyle##name_* data = PeekStyle##name_();                          \
    if (!data || data == aNewContext->Style##name_()) {                       \
      *aSamePointerStructs |= NS_STYLE_INHERIT_BIT(name_);                    \
    }                                                                         \
  }
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

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
  nsStyleContext *thisVis = GetStyleIfVisited(),
                *otherVis = aNewContext->GetStyleIfVisited();
  if (!thisVis != !otherVis) {
    // One style context has a style-if-visited and the other doesn't.
    // Presume a difference.
    hint |= nsChangeHint_RepaintFrame;
  } else if (thisVis && !NS_IsHintSubset(nsChangeHint_RepaintFrame, hint)) {
    // Bug 1364484: Update comments here and potentially remove the assertion
    // below once we return a non-null visited context in CalcStyleDifference
    // using Servo values.  The approach is becoming quite similar to Gecko.
    // We'll handle visited style differently in servo. Assert against being
    // in the parallel traversal to avoid static analysis hazards when calling
    // StyleFoo() below.
    MOZ_ASSERT(!ServoStyleSet::IsInServoTraversal());

    // Both style contexts have a style-if-visited.
    bool change = false;

    // NB: Calling Peek on |this|, not |thisVis|, since callers may look
    // at a struct on |this| without looking at the same struct on
    // |thisVis| (including this function if we skip one of these checks
    // due to change being true already or due to the old style context
    // not having a style-if-visited), but not the other way around.
#define STYLE_FIELD(name_) thisVisStruct->name_ != otherVisStruct->name_
#define STYLE_STRUCT(name_, fields_)                                    \
    if (!change && PeekStyle##name_()) {                                \
      const nsStyle##name_* thisVisStruct = thisVis->Style##name_();    \
      const nsStyle##name_* otherVisStruct = otherVis->Style##name_();  \
      if (MOZ_FOR_EACH_SEPARATED(STYLE_FIELD, (||), (), fields_)) {     \
        change = true;                                                  \
      }                                                                 \
    }
#include "nsCSSVisitedDependentPropList.h"
#undef STYLE_STRUCT
#undef STYLE_FIELD

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

    // This depends on data in nsStyleDisplay, nsStyleEffects and
    // nsStyleSVGReset, so we do it here.

    // Note that it's perhaps good for this test to be last because it
    // doesn't use Peek* functions to get the structs on the old
    // context.  But this isn't a big concern because these struct
    // getters should be called during frame construction anyway.
    if (ThreadsafeStyleDisplay()->IsAbsPosContainingBlockForAppropriateFrame(this) ==
        aNewContext->ThreadsafeStyleDisplay()->
          IsAbsPosContainingBlockForAppropriateFrame(aNewContext) &&
        ThreadsafeStyleDisplay()->IsFixedPosContainingBlockForAppropriateFrame(this) ==
        aNewContext->ThreadsafeStyleDisplay()->
          IsFixedPosContainingBlockForAppropriateFrame(aNewContext)) {
      // While some styles that cause the frame to be a containing block
      // has changed, the overall result hasn't.
      hint &= ~nsChangeHint_UpdateContainingBlock;
    }
  }

  MOZ_ASSERT(NS_IsHintSubset(hint, nsChangeHint_AllHints),
             "Added a new hint without bumping AllHints?");
  return hint & ~nsChangeHint_NeutralChange;
}

nsChangeHint
nsStyleContext::CalcStyleDifference(nsStyleContext* aNewContext,
                                    uint32_t* aEqualStructs,
                                    uint32_t* aSamePointerStructs)
{
  return CalcStyleDifferenceInternal(aNewContext,
                                     aEqualStructs,
                                     aSamePointerStructs);
}

class MOZ_STACK_CLASS FakeStyleContext
{
public:
  explicit FakeStyleContext(const ServoComputedValues* aComputedValues)
    : mComputedValues(aComputedValues) {}

  nsStyleContext* GetStyleIfVisited() {
    // Bug 1364484: Figure out what to do here for Stylo visited values.  We can
    // get the visited computed values:
    // RefPtr<ServoComputedValues> visitedComputedValues =
    //   Servo_ComputedValues_GetVisitedStyle(mComputedValues).Consume();
    // But what's the best way to create the nsStyleContext?
    return nullptr;
  }

  #define STYLE_STRUCT(name_, checkdata_cb_)                                  \
  const nsStyle##name_ * Style##name_() {                                     \
    return Servo_GetStyle##name_(mComputedValues);                            \
  }                                                                           \
  const nsStyle##name_ * ThreadsafeStyle##name_() {                           \
    return Servo_GetStyle##name_(mComputedValues);                            \
  }
  #include "nsStyleStructList.h"
  #undef STYLE_STRUCT

private:
  const ServoComputedValues* MOZ_NON_OWNING_REF mComputedValues;
};

nsChangeHint
nsStyleContext::CalcStyleDifference(const ServoComputedValues* aNewComputedValues,
                                    uint32_t* aEqualStructs,
                                    uint32_t* aSamePointerStructs)
{
  FakeStyleContext newContext(aNewComputedValues);
  return CalcStyleDifferenceInternal(&newContext,
                                     aEqualStructs,
                                     aSamePointerStructs);
}

namespace mozilla {

void
GeckoStyleContext::EnsureSameStructsCached(nsStyleContext* aOldContext)
{
  // NOTE(emilio): We could do better here for stylo, where we only call
  // Style##name_() because we need to run FinishStyle, but otherwise this
  // is only a bitwise or.
  //
  // We could reduce the FFI traffic we do only doing it for structs that have
  // non-trivial FinishStyle.

#define STYLE_STRUCT(name_, checkdata_cb_)                                    \
  if (aOldContext->PeekStyle##name_()) {                                      \
    Style##name_();                                                           \
  }
#include "nsStyleStructList.h"
#undef STYLE_STRUCT
}

} // namespace mozilla

#ifdef DEBUG
void nsStyleContext::List(FILE* out, int32_t aIndent, bool aListDescendants)
{
  nsAutoCString str;
  // Indent
  int32_t ix;
  for (ix = aIndent; --ix >= 0; ) {
    str.AppendLiteral("  ");
  }
  str.Append(nsPrintfCString("%p(%d) parent=%p ",
                             (void*)this, mRefCnt, (void *)mParent));
  if (mPseudoTag) {
    nsAutoString  buffer;
    mPseudoTag->ToString(buffer);
    AppendUTF16toUTF8(buffer, str);
    str.Append(' ');
  }

  if (IsServo()) {
    fprintf_stderr(out, "%s{ServoComputedValues}\n", str.get());
  } else if (nsRuleNode* ruleNode = AsGecko()->RuleNode()) {
    fprintf_stderr(out, "%s{\n", str.get());
    str.Truncate();
    while (ruleNode) {
      nsIStyleRule *styleRule = ruleNode->GetRule();
      if (styleRule) {
        styleRule->List(out, aIndent + 1);
      }
      ruleNode = ruleNode->GetParent();
    }
    for (ix = aIndent; --ix >= 0; ) {
      str.AppendLiteral("  ");
    }
    fprintf_stderr(out, "%s}\n", str.get());
  }
  else {
    fprintf_stderr(out, "%s{}\n", str.get());
  }

  if (aListDescendants) {
    if (GeckoStyleContext* gecko = GetAsGecko()) {
      gecko->ListDescendants(out, aIndent);
    }
  }
}
#endif

// Overridden to prevent the global delete from being called, since the memory
// came out of an nsIArena instead of the global delete operator's heap.
void
nsStyleContext::Destroy()
{
  if (IsGecko()) {
    // Get the pres context.
    RefPtr<nsPresContext> presContext = PresContext();
    // Call our destructor.
    this->AsGecko()->~GeckoStyleContext();
    // Don't let the memory be freed, since it will be recycled
    // instead. Don't call the global operator delete.
    presContext->PresShell()->
      FreeByObjectID(eArenaObjectID_GeckoStyleContext, this);
  } else {
    delete static_cast<ServoStyleContext*>(this);
  }
}

already_AddRefed<nsStyleContext>
NS_NewStyleContext(nsStyleContext* aParentContext,
                   nsIAtom* aPseudoTag,
                   CSSPseudoElementType aPseudoType,
                   nsRuleNode* aRuleNode,
                   bool aSkipParentDisplayBasedStyleFixup)
{
  RefPtr<nsRuleNode> node = aRuleNode;
  RefPtr<nsStyleContext> context =
    new (aRuleNode->PresContext())
    GeckoStyleContext(aParentContext, aPseudoTag, aPseudoType, node.forget(),
                   aSkipParentDisplayBasedStyleFixup);
  return context.forget();
}

namespace mozilla {

already_AddRefed<ServoStyleContext>
ServoStyleContext::Create(nsStyleContext* aParentContext,
                          nsPresContext* aPresContext,
                          nsIAtom* aPseudoTag,
                          CSSPseudoElementType aPseudoType,
                          already_AddRefed<ServoComputedValues> aComputedValues)
{
  RefPtr<ServoStyleContext> context =
    new ServoStyleContext(aParentContext, aPresContext, aPseudoTag, aPseudoType,
                          Move(aComputedValues));
  return context.forget();
}

} // namespace mozilla

nsIPresShell*
nsStyleContext::Arena()
{
  return PresContext()->PresShell();
}

template<typename Func>
static nscolor
GetVisitedDependentColorInternal(nsStyleContext* aSc, Func aColorFunc)
{
  nscolor colors[2];
  colors[0] = aColorFunc(aSc);
  if (nsStyleContext* visitedStyle = aSc->GetStyleIfVisited()) {
    colors[1] = aColorFunc(visitedStyle);
    return nsStyleContext::
      CombineVisitedColors(colors, aSc->RelevantLinkVisited());
  }
  return colors[0];
}

static nscolor
ExtractColor(nsStyleContext* aContext, const nscolor& aColor)
{
  return aColor;
}

static nscolor
ExtractColor(nsStyleContext* aContext, const StyleComplexColor& aColor)
{
  return aContext->StyleColor()->CalcComplexColor(aColor);
}

static nscolor
ExtractColor(nsStyleContext* aContext, const nsStyleSVGPaint& aPaintServer)
{
  return aPaintServer.Type() == eStyleSVGPaintType_Color
    ? aPaintServer.GetColor() : NS_RGBA(0, 0, 0, 0);
}

#define STYLE_FIELD(struct_, field_) aField == &struct_::field_ ||
#define STYLE_STRUCT(name_, fields_)                                          \
  template<> nscolor                                                          \
  nsStyleContext::GetVisitedDependentColor(                                   \
    decltype(nsStyle##name_::MOZ_ARG_1 fields_) nsStyle##name_::* aField)     \
  {                                                                           \
    MOZ_ASSERT(MOZ_FOR_EACH(STYLE_FIELD, (nsStyle##name_,), fields_) false,   \
               "Getting visited-dependent color for a field in nsStyle"#name_ \
               " which is not listed in nsCSSVisitedDependentPropList.h");    \
    return GetVisitedDependentColorInternal(this,                             \
                                            [aField](nsStyleContext* sc) {    \
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
nsStyleContext::CombineVisitedColors(nscolor *aColors, bool aLinkIsVisited)
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
nsStyleContext::StructName(nsStyleStructID aSID)
{
  switch (aSID) {
#define STYLE_STRUCT(name_, checkdata_cb)                                     \
    case eStyleStruct_##name_:                                                \
      return #name_;
#include "nsStyleStructList.h"
#undef STYLE_STRUCT
    default:
      return "Unknown";
  }
}

/* static */ bool
nsStyleContext::LookupStruct(const nsACString& aName, nsStyleStructID& aResult)
{
  if (false)
    ;
#define STYLE_STRUCT(name_, checkdata_cb_)                                    \
  else if (aName.EqualsLiteral(#name_))                                       \
    aResult = eStyleStruct_##name_;
#include "nsStyleStructList.h"
#undef STYLE_STRUCT
  else
    return false;
  return true;
}
#endif

#ifdef DEBUG
/* static */ void
nsStyleContext::Initialize()
{
  Preferences::AddBoolVarCache(
      &sExpensiveStyleStructAssertionsEnabled,
      "layout.css.expensive-style-struct-assertions.enabled");
}
#endif
