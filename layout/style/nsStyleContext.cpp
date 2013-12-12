/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
/* the interface (to internal code) for retrieving computed style data */

#include "mozilla/DebugOnly.h"

#include "nsCSSAnonBoxes.h"
#include "nsStyleConsts.h"
#include "nsString.h"
#include "nsPresContext.h"
#include "nsIStyleRule.h"

#include "nsCOMPtr.h"
#include "nsStyleSet.h"
#include "nsIPresShell.h"

#include "nsRuleNode.h"
#include "nsStyleContext.h"
#include "nsStyleAnimation.h"
#include "GeckoProfiler.h"

#ifdef DEBUG
// #define NOISY_DEBUG
#endif

using namespace mozilla;

//----------------------------------------------------------------------


nsStyleContext::nsStyleContext(nsStyleContext* aParent,
                               nsIAtom* aPseudoTag,
                               nsCSSPseudoElements::Type aPseudoType,
                               nsRuleNode* aRuleNode,
                               bool aSkipFlexItemStyleFixup)
  : mParent(aParent),
    mChild(nullptr),
    mEmptyChild(nullptr),
    mPseudoTag(aPseudoTag),
    mRuleNode(aRuleNode),
    mAllocations(nullptr),
    mCachedResetData(nullptr),
    mBits(((uint64_t)aPseudoType) << NS_STYLE_CONTEXT_TYPE_SHIFT),
    mRefCnt(0)
{
  // This check has to be done "backward", because if it were written the
  // more natural way it wouldn't fail even when it needed to.
  static_assert((UINT64_MAX >> NS_STYLE_CONTEXT_TYPE_SHIFT) >=
                nsCSSPseudoElements::ePseudo_MAX,
                "pseudo element bits no longer fit in a uint64_t");
  MOZ_ASSERT(aRuleNode);

  mNextSibling = this;
  mPrevSibling = this;
  if (mParent) {
    mParent->AddRef();
    mParent->AddChild(this);
#ifdef DEBUG
    nsRuleNode *r1 = mParent->RuleNode(), *r2 = aRuleNode;
    while (r1->GetParent())
      r1 = r1->GetParent();
    while (r2->GetParent())
      r2 = r2->GetParent();
    NS_ASSERTION(r1 == r2, "must be in the same rule tree as parent");
#endif
  }

  mRuleNode->AddRef();
  mRuleNode->SetUsedDirectly(); // before ApplyStyleFixups()!

  ApplyStyleFixups(aSkipFlexItemStyleFixup);

  #define eStyleStruct_LastItem (nsStyleStructID_Length - 1)
  NS_ASSERTION(NS_STYLE_INHERIT_MASK & NS_STYLE_INHERIT_BIT(LastItem),
               "NS_STYLE_INHERIT_MASK must be bigger, and other bits shifted");
  #undef eStyleStruct_LastItem
}

nsStyleContext::~nsStyleContext()
{
  NS_ASSERTION((nullptr == mChild) && (nullptr == mEmptyChild), "destructing context with children");

  nsPresContext *presContext = mRuleNode->PresContext();

  mRuleNode->Release();

  presContext->PresShell()->StyleSet()->
    NotifyStyleContextDestroyed(presContext, this);

  if (mParent) {
    mParent->RemoveChild(this);
    mParent->Release();
  }

  // Free up our data structs.
  mCachedInheritedData.DestroyStructs(mBits, presContext);
  if (mCachedResetData) {
    mCachedResetData->Destroy(mBits, presContext);
  }

  FreeAllocations(presContext);
}

void nsStyleContext::AddChild(nsStyleContext* aChild)
{
  NS_ASSERTION(aChild->mPrevSibling == aChild &&
               aChild->mNextSibling == aChild,
               "child already in a child list");

  nsStyleContext **listPtr = aChild->mRuleNode->IsRoot() ? &mEmptyChild : &mChild;
  // Explicitly dereference listPtr so that compiler doesn't have to know that mNextSibling
  // etc. don't alias with what ever listPtr points at.
  nsStyleContext *list = *listPtr;

  // Insert at the beginning of the list.  See also FindChildWithRules.
  if (list) {
    // Link into existing elements, if there are any.
    aChild->mNextSibling = list;
    aChild->mPrevSibling = list->mPrevSibling;
    list->mPrevSibling->mNextSibling = aChild;
    list->mPrevSibling = aChild;
  }
  (*listPtr) = aChild;
}

void nsStyleContext::RemoveChild(nsStyleContext* aChild)
{
  NS_PRECONDITION(nullptr != aChild && this == aChild->mParent, "bad argument");

  nsStyleContext **list = aChild->mRuleNode->IsRoot() ? &mEmptyChild : &mChild;

  if (aChild->mPrevSibling != aChild) { // has siblings
    if ((*list) == aChild) {
      (*list) = (*list)->mNextSibling;
    }
  } 
  else {
    NS_ASSERTION((*list) == aChild, "bad sibling pointers");
    (*list) = nullptr;
  }

  aChild->mPrevSibling->mNextSibling = aChild->mNextSibling;
  aChild->mNextSibling->mPrevSibling = aChild->mPrevSibling;
  aChild->mNextSibling = aChild;
  aChild->mPrevSibling = aChild;
}

already_AddRefed<nsStyleContext>
nsStyleContext::FindChildWithRules(const nsIAtom* aPseudoTag, 
                                   nsRuleNode* aRuleNode,
                                   nsRuleNode* aRulesIfVisited,
                                   bool aRelevantLinkVisited)
{
  NS_ABORT_IF_FALSE(aRulesIfVisited || !aRelevantLinkVisited,
    "aRelevantLinkVisited should only be set when we have a separate style");
  uint32_t threshold = 10; // The # of siblings we're willing to examine
                           // before just giving this whole thing up.

  nsRefPtr<nsStyleContext> result;
  nsStyleContext *list = aRuleNode->IsRoot() ? mEmptyChild : mChild;

  if (list) {
    nsStyleContext *child = list;
    do {
      if (child->mRuleNode == aRuleNode &&
          child->mPseudoTag == aPseudoTag &&
          !child->IsStyleIfVisited() &&
          child->RelevantLinkVisited() == aRelevantLinkVisited) {
        bool match = false;
        if (aRulesIfVisited) {
          match = child->GetStyleIfVisited() &&
                  child->GetStyleIfVisited()->mRuleNode == aRulesIfVisited;
        } else {
          match = !child->GetStyleIfVisited();
        }
        if (match) {
          result = child;
          break;
        }
      }
      child = child->mNextSibling;
      threshold--;
      if (threshold == 0)
        break;
    } while (child != list);
  }

  if (result) {
    if (result != list) {
      // Move result to the front of the list.
      RemoveChild(result);
      AddChild(result);
    }
  }

  return result.forget();
}

const void* nsStyleContext::GetCachedStyleData(nsStyleStructID aSID)
{
  const void* cachedData;
  if (nsCachedStyleData::IsReset(aSID)) {
    if (mCachedResetData) {
      cachedData = mCachedResetData->mStyleStructs[aSID];
    } else {
      cachedData = nullptr;
    }
  } else {
    cachedData = mCachedInheritedData.mStyleStructs[aSID];
  }
  return cachedData;
}

const void* nsStyleContext::StyleData(nsStyleStructID aSID)
{
  const void* cachedData = GetCachedStyleData(aSID);
  if (cachedData)
    return cachedData; // We have computed data stored on this node in the context tree.
  return mRuleNode->GetStyleData(aSID, this, true); // Our rule node will take care of it for us.
}

// This is an evil evil function, since it forces you to alloc your own separate copy of
// style data!  Do not use this function unless you absolutely have to!  You should avoid
// this at all costs! -dwh
void* 
nsStyleContext::GetUniqueStyleData(const nsStyleStructID& aSID)
{
  // If we already own the struct and no kids could depend on it, then
  // just return it.  (We leak in this case if there are kids -- and this
  // function really shouldn't be called for style contexts that could
  // have kids depending on the data.  ClearStyleData would be OK, but
  // this test for no mChild or mEmptyChild doesn't catch that case.)
  const void *current = StyleData(aSID);
  if (!mChild && !mEmptyChild &&
      !(mBits & nsCachedStyleData::GetBitForSID(aSID)) &&
      GetCachedStyleData(aSID))
    return const_cast<void*>(current);

  void* result;
  nsPresContext *presContext = PresContext();
  switch (aSID) {

#define UNIQUE_CASE(c_)                                                       \
  case eStyleStruct_##c_:                                                     \
    result = new (presContext) nsStyle##c_(                                   \
      * static_cast<const nsStyle##c_ *>(current));                           \
    break;

  UNIQUE_CASE(Display)
  UNIQUE_CASE(Background)
  UNIQUE_CASE(Text)
  UNIQUE_CASE(TextReset)

#undef UNIQUE_CASE

  default:
    NS_ERROR("Struct type not supported.  Please find another way to do this if you can!");
    return nullptr;
  }

  SetStyle(aSID, result);
  mBits &= ~static_cast<uint64_t>(nsCachedStyleData::GetBitForSID(aSID));

  return result;
}

void
nsStyleContext::SetStyle(nsStyleStructID aSID, void* aStruct)
{
  // This method should only be called from nsRuleNode!  It is not a public
  // method!
  
  NS_ASSERTION(aSID >= 0 && aSID < nsStyleStructID_Length, "out of bounds");

  // NOTE:  nsCachedStyleData::GetStyleData works roughly the same way.
  // See the comments there (in nsRuleNode.h) for more details about
  // what this is doing and why.

  void** dataSlot;
  if (nsCachedStyleData::IsReset(aSID)) {
    if (!mCachedResetData) {
      mCachedResetData = new (mRuleNode->PresContext()) nsResetStyleData;
    }
    dataSlot = &mCachedResetData->mStyleStructs[aSID];
  } else {
    dataSlot = &mCachedInheritedData.mStyleStructs[aSID];
  }
  NS_ASSERTION(!*dataSlot || (mBits & nsCachedStyleData::GetBitForSID(aSID)),
               "Going to leak style data");
  *dataSlot = aStruct;
}

void
nsStyleContext::ApplyStyleFixups(bool aSkipFlexItemStyleFixup)
{
  // See if we have any text decorations.
  // First see if our parent has text decorations.  If our parent does, then we inherit the bit.
  if (mParent && mParent->HasTextDecorationLines()) {
    mBits |= NS_STYLE_HAS_TEXT_DECORATION_LINES;
  } else {
    // We might have defined a decoration.
    const nsStyleTextReset* text = StyleTextReset();
    uint8_t decorationLine = text->mTextDecorationLine;
    if (decorationLine != NS_STYLE_TEXT_DECORATION_LINE_NONE &&
        decorationLine != NS_STYLE_TEXT_DECORATION_LINE_OVERRIDE_ALL) {
      mBits |= NS_STYLE_HAS_TEXT_DECORATION_LINES;
    }
  }

  if ((mParent && mParent->HasPseudoElementData()) || mPseudoTag) {
    mBits |= NS_STYLE_HAS_PSEUDO_ELEMENT_DATA;
  }

  // Correct tables.
  const nsStyleDisplay* disp = StyleDisplay();
  if (disp->mDisplay == NS_STYLE_DISPLAY_TABLE) {
    // -moz-center and -moz-right are used for HTML's alignment
    // This is covering the <div align="right"><table>...</table></div> case.
    // In this case, we don't want to inherit the text alignment into the table.
    const nsStyleText* text = StyleText();
    
    if (text->mTextAlign == NS_STYLE_TEXT_ALIGN_MOZ_CENTER ||
        text->mTextAlign == NS_STYLE_TEXT_ALIGN_MOZ_RIGHT)
    {
      nsStyleText* uniqueText = (nsStyleText*)GetUniqueStyleData(eStyleStruct_Text);
      uniqueText->mTextAlign = NS_STYLE_TEXT_ALIGN_DEFAULT;
    }
  }

  // CSS2.1 section 9.2.4 specifies fixups for the 'display' property of
  // the root element.  We can't implement them in nsRuleNode because we
  // don't want to store all display structs that aren't 'block',
  // 'inline', or 'table' in the style context tree on the off chance
  // that the root element has its style reresolved later.  So do them
  // here if needed, by changing the style data, so that other code
  // doesn't get confused by looking at the style data.
  if (!mParent) {
    if (disp->mDisplay != NS_STYLE_DISPLAY_NONE &&
        disp->mDisplay != NS_STYLE_DISPLAY_BLOCK &&
        disp->mDisplay != NS_STYLE_DISPLAY_TABLE) {
      nsStyleDisplay *mutable_display = static_cast<nsStyleDisplay*>
                                                   (GetUniqueStyleData(eStyleStruct_Display));
      // If we're in this code, then mOriginalDisplay doesn't matter
      // for purposes of the cascade (because this nsStyleDisplay
      // isn't living in the ruletree anyway), and for determining
      // hypothetical boxes it's better to have mOriginalDisplay
      // matching mDisplay here.
      if (mutable_display->mDisplay == NS_STYLE_DISPLAY_INLINE_TABLE)
        mutable_display->mOriginalDisplay = mutable_display->mDisplay =
          NS_STYLE_DISPLAY_TABLE;
      else
        mutable_display->mOriginalDisplay = mutable_display->mDisplay =
          NS_STYLE_DISPLAY_BLOCK;
    }
  }

  // Adjust the "display" values of flex items (but not for raw text,
  // placeholders, or table-parts). CSS3 Flexbox section 4 says:
  //   # The computed 'display' of a flex item is determined
  //   # by applying the table in CSS 2.1 Chapter 9.7.
  // ...which converts inline-level elements to their block-level equivalents.
  if (!aSkipFlexItemStyleFixup && mParent) {
    const nsStyleDisplay* parentDisp = mParent->StyleDisplay();
    if ((parentDisp->mDisplay == NS_STYLE_DISPLAY_FLEX ||
         parentDisp->mDisplay == NS_STYLE_DISPLAY_INLINE_FLEX) &&
        GetPseudo() != nsCSSAnonBoxes::mozNonElement) {
      uint8_t displayVal = disp->mDisplay;
      // Skip table parts.
      // NOTE: This list needs to be kept in sync with
      // nsCSSFrameConstructor.cpp's "sDisplayData" array -- specifically,
      // this should be the list of display-values that have
      // FCDATA_DESIRED_PARENT_TYPE_TO_BITS specified in that array.
      if (NS_STYLE_DISPLAY_TABLE_CAPTION      != displayVal &&
          NS_STYLE_DISPLAY_TABLE_ROW_GROUP    != displayVal &&
          NS_STYLE_DISPLAY_TABLE_HEADER_GROUP != displayVal &&
          NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP != displayVal &&
          NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP != displayVal &&
          NS_STYLE_DISPLAY_TABLE_COLUMN       != displayVal &&
          NS_STYLE_DISPLAY_TABLE_ROW          != displayVal &&
          NS_STYLE_DISPLAY_TABLE_CELL         != displayVal) {

        // NOTE: Technically, we shouldn't modify the 'display' value of
        // positioned elements, since they aren't flex items. However, we don't
        // need to worry about checking for that, because if we're positioned,
        // we'll have already been through a call to EnsureBlockDisplay() in
        // nsRuleNode, so this call here won't change anything. So we're OK.
        nsRuleNode::EnsureBlockDisplay(displayVal);
        if (displayVal != disp->mDisplay) {
          NS_ASSERTION(!disp->IsAbsolutelyPositionedStyle(),
                       "We shouldn't be changing the display value of "
                       "positioned content (and we should have already "
                       "converted its display value to be block-level...)");
          nsStyleDisplay *mutable_display =
            static_cast<nsStyleDisplay*>(GetUniqueStyleData(eStyleStruct_Display));
          mutable_display->mDisplay = displayVal;
        }
      }
    }
  }

  // Compute User Interface style, to trigger loads of cursors
  StyleUserInterface();
}

nsChangeHint
nsStyleContext::CalcStyleDifference(nsStyleContext* aOther,
                                    nsChangeHint aParentHintsNotHandledForDescendants)
{
  PROFILER_LABEL("nsStyleContext", "CalcStyleDifference");

  NS_ABORT_IF_FALSE(NS_IsHintSubset(aParentHintsNotHandledForDescendants,
                                    nsChangeHint_Hints_NotHandledForDescendants),
                    "caller is passing inherited hints, but shouldn't be");

  nsChangeHint hint = NS_STYLE_HINT_NONE;
  NS_ENSURE_TRUE(aOther, hint);
  // We must always ensure that we populate the structs on the new style
  // context that are filled in on the old context, so that if we get
  // two style changes in succession, the second of which causes a real
  // style change, the PeekStyleData doesn't return null (implying that
  // nobody ever looked at that struct's data).  In other words, we
  // can't skip later structs if we get a big change up front, because
  // we could later get a small change in one of those structs that we
  // don't want to miss.

  // If our rule nodes are the same, then any differences in style data
  // are already accounted for by differences on ancestors.  We know
  // this because CalcStyleDifference is always called on two style
  // contexts that point to the same element, so we know that our
  // position in the style context tree is the same and our position in
  // the rule node tree is also the same.
  // However, if there were noninherited style change hints on the
  // parent, we might produce these same noninherited hints on this
  // style context's frame due to 'inherit' values, so we do need to
  // compare.
  // (Things like 'em' units are handled by the change hint produced
  // by font-size changing, so we don't need to worry about them like
  // we worry about 'inherit' values.)
  bool compare = mRuleNode != aOther->mRuleNode;

  // If we had any change in variable values, then we'll need to examine
  // all of the other style structs too, even if the new style context has
  // the same rule node as the old one.
  const nsStyleVariables* thisVariables = PeekStyleVariables();
  if (thisVariables) {
    const nsStyleVariables* otherVariables = aOther->StyleVariables();
    if (thisVariables->mVariables != otherVariables->mVariables) {
      compare = true;
    }
  }

  DebugOnly<int> styleStructCount = 1;  // count Variables already

#define DO_STRUCT_DIFFERENCE(struct_)                                         \
  PR_BEGIN_MACRO                                                              \
    const nsStyle##struct_* this##struct_ = PeekStyle##struct_();             \
    if (this##struct_) {                                                      \
      const nsStyle##struct_* other##struct_ = aOther->Style##struct_();      \
      nsChangeHint maxDifference = nsStyle##struct_::MaxDifference();         \
      if ((compare ||                                                         \
           (maxDifference & aParentHintsNotHandledForDescendants)) &&         \
          !NS_IsHintSubset(maxDifference, hint) &&                            \
          this##struct_ != other##struct_) {                                  \
        NS_ASSERTION(NS_IsHintSubset(                                         \
             this##struct_->CalcDifference(*other##struct_),                  \
             nsStyle##struct_::MaxDifference()),                              \
             "CalcDifference() returned bigger hint than MaxDifference()");   \
        NS_UpdateHint(hint, this##struct_->CalcDifference(*other##struct_));  \
      }                                                                       \
    }                                                                         \
    styleStructCount++;                                                       \
  PR_END_MACRO

  // In general, we want to examine structs starting with those that can
  // cause the largest style change, down to those that can cause the
  // smallest.  This lets us skip later ones if we already have a hint
  // that subsumes their MaxDifference.  (As the hints get
  // finer-grained, this optimization is becoming less useful, though.)
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
  DO_STRUCT_DIFFERENCE(List);
  DO_STRUCT_DIFFERENCE(Quotes);
  DO_STRUCT_DIFFERENCE(SVGReset);
  DO_STRUCT_DIFFERENCE(SVG);
  DO_STRUCT_DIFFERENCE(Position);
  DO_STRUCT_DIFFERENCE(Font);
  DO_STRUCT_DIFFERENCE(Margin);
  DO_STRUCT_DIFFERENCE(Padding);
  DO_STRUCT_DIFFERENCE(Border);
  DO_STRUCT_DIFFERENCE(TextReset);
  DO_STRUCT_DIFFERENCE(Background);
  DO_STRUCT_DIFFERENCE(Color);

#undef DO_STRUCT_DIFFERENCE

  MOZ_ASSERT(styleStructCount == nsStyleStructID_Length,
             "missing a call to DO_STRUCT_DIFFERENCE");

  // Note that we do not check whether this->RelevantLinkVisited() !=
  // aOther->RelevantLinkVisited(); we don't need to since
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
                *otherVis = aOther->GetStyleIfVisited();
  if (!thisVis != !otherVis) {
    // One style context has a style-if-visited and the other doesn't.
    // Presume a difference.
    NS_UpdateHint(hint, nsChangeHint_RepaintFrame);
  } else if (thisVis && !NS_IsHintSubset(nsChangeHint_RepaintFrame, hint)) {
    // Both style contexts have a style-if-visited.
    bool change = false;

    // NB: Calling Peek on |this|, not |thisVis|, since callers may look
    // at a struct on |this| without looking at the same struct on
    // |thisVis| (including this function if we skip one of these checks
    // due to change being true already or due to the old style context
    // not having a style-if-visited), but not the other way around.
    if (PeekStyleColor()) {
      if (thisVis->StyleColor()->mColor !=
          otherVis->StyleColor()->mColor) {
        change = true;
      }
    }

    // NB: Calling Peek on |this|, not |thisVis| (see above).
    if (!change && PeekStyleBackground()) {
      if (thisVis->StyleBackground()->mBackgroundColor !=
          otherVis->StyleBackground()->mBackgroundColor) {
        change = true;
      }
    }

    // NB: Calling Peek on |this|, not |thisVis| (see above).
    if (!change && PeekStyleBorder()) {
      const nsStyleBorder *thisVisBorder = thisVis->StyleBorder();
      const nsStyleBorder *otherVisBorder = otherVis->StyleBorder();
      NS_FOR_CSS_SIDES(side) {
        bool thisFG, otherFG;
        nscolor thisColor, otherColor;
        thisVisBorder->GetBorderColor(side, thisColor, thisFG);
        otherVisBorder->GetBorderColor(side, otherColor, otherFG);
        if (thisFG != otherFG || (!thisFG && thisColor != otherColor)) {
          change = true;
          break;
        }
      }
    }

    // NB: Calling Peek on |this|, not |thisVis| (see above).
    if (!change && PeekStyleOutline()) {
      const nsStyleOutline *thisVisOutline = thisVis->StyleOutline();
      const nsStyleOutline *otherVisOutline = otherVis->StyleOutline();
      bool haveColor;
      nscolor thisColor, otherColor;
      if (thisVisOutline->GetOutlineInitialColor() != 
            otherVisOutline->GetOutlineInitialColor() ||
          (haveColor = thisVisOutline->GetOutlineColor(thisColor)) != 
            otherVisOutline->GetOutlineColor(otherColor) ||
          (haveColor && thisColor != otherColor)) {
        change = true;
      }
    }

    // NB: Calling Peek on |this|, not |thisVis| (see above).
    if (!change && PeekStyleColumn()) {
      const nsStyleColumn *thisVisColumn = thisVis->StyleColumn();
      const nsStyleColumn *otherVisColumn = otherVis->StyleColumn();
      if (thisVisColumn->mColumnRuleColor != otherVisColumn->mColumnRuleColor ||
          thisVisColumn->mColumnRuleColorIsForeground !=
            otherVisColumn->mColumnRuleColorIsForeground) {
        change = true;
      }
    }

    // NB: Calling Peek on |this|, not |thisVis| (see above).
    if (!change && PeekStyleTextReset()) {
      const nsStyleTextReset *thisVisTextReset = thisVis->StyleTextReset();
      const nsStyleTextReset *otherVisTextReset = otherVis->StyleTextReset();
      nscolor thisVisDecColor, otherVisDecColor;
      bool thisVisDecColorIsFG, otherVisDecColorIsFG;
      thisVisTextReset->GetDecorationColor(thisVisDecColor,
                                           thisVisDecColorIsFG);
      otherVisTextReset->GetDecorationColor(otherVisDecColor,
                                            otherVisDecColorIsFG);
      if (thisVisDecColorIsFG != otherVisDecColorIsFG ||
          (!thisVisDecColorIsFG && thisVisDecColor != otherVisDecColor)) {
        change = true;
      }
    }

    // NB: Calling Peek on |this|, not |thisVis| (see above).
    if (!change && PeekStyleSVG()) {
      const nsStyleSVG *thisVisSVG = thisVis->StyleSVG();
      const nsStyleSVG *otherVisSVG = otherVis->StyleSVG();
      if (thisVisSVG->mFill != otherVisSVG->mFill ||
          thisVisSVG->mStroke != otherVisSVG->mStroke) {
        change = true;
      }
    }

    if (change) {
      NS_UpdateHint(hint, nsChangeHint_RepaintFrame);
    }
  }

  return hint;
}

void
nsStyleContext::Mark()
{
  // Mark our rule node.
  mRuleNode->Mark();

  // Mark our children (i.e., tell them to mark their rule nodes, etc.).
  if (mChild) {
    nsStyleContext* child = mChild;
    do {
      child->Mark();
      child = child->mNextSibling;
    } while (mChild != child);
  }
  
  if (mEmptyChild) {
    nsStyleContext* child = mEmptyChild;
    do {
      child->Mark();
      child = child->mNextSibling;
    } while (mEmptyChild != child);
  }
}

#ifdef DEBUG
void nsStyleContext::List(FILE* out, int32_t aIndent)
{
  // Indent
  int32_t ix;
  for (ix = aIndent; --ix >= 0; ) fputs("  ", out);
  fprintf(out, "%p(%d) parent=%p ",
          (void*)this, mRefCnt, (void *)mParent);
  if (mPseudoTag) {
    nsAutoString  buffer;
    mPseudoTag->ToString(buffer);
    fputs(NS_LossyConvertUTF16toASCII(buffer).get(), out);
    fputs(" ", out);
  }

  if (mRuleNode) {
    fputs("{\n", out);
    nsRuleNode* ruleNode = mRuleNode;
    while (ruleNode) {
      nsIStyleRule *styleRule = ruleNode->GetRule();
      if (styleRule) {
        styleRule->List(out, aIndent + 1);
      }
      ruleNode = ruleNode->GetParent();
    }
    for (ix = aIndent; --ix >= 0; ) fputs("  ", out);
    fputs("}\n", out);
  }
  else {
    fputs("{}\n", out);
  }

  if (nullptr != mChild) {
    nsStyleContext* child = mChild;
    do {
      child->List(out, aIndent + 1);
      child = child->mNextSibling;
    } while (mChild != child);
  }
  if (nullptr != mEmptyChild) {
    nsStyleContext* child = mEmptyChild;
    do {
      child->List(out, aIndent + 1);
      child = child->mNextSibling;
    } while (mEmptyChild != child);
  }
}
#endif

// Overloaded new operator. Initializes the memory to 0 and relies on an arena
// (which comes from the presShell) to perform the allocation.
void* 
nsStyleContext::operator new(size_t sz, nsPresContext* aPresContext) CPP_THROW_NEW
{
  // Check the recycle list first.
  return aPresContext->PresShell()->AllocateByObjectID(nsPresArena::nsStyleContext_id, sz);
}

// Overridden to prevent the global delete from being called, since the memory
// came out of an nsIArena instead of the global delete operator's heap.
void 
nsStyleContext::Destroy()
{
  // Get the pres context from our rule node.
  nsRefPtr<nsPresContext> presContext = mRuleNode->PresContext();

  // Call our destructor.
  this->~nsStyleContext();

  // Don't let the memory be freed, since it will be recycled
  // instead. Don't call the global operator delete.
  presContext->PresShell()->FreeByObjectID(nsPresArena::nsStyleContext_id, this);
}

already_AddRefed<nsStyleContext>
NS_NewStyleContext(nsStyleContext* aParentContext,
                   nsIAtom* aPseudoTag,
                   nsCSSPseudoElements::Type aPseudoType,
                   nsRuleNode* aRuleNode,
                   bool aSkipFlexItemStyleFixup)
{
  nsRefPtr<nsStyleContext> context =
    new (aRuleNode->PresContext())
    nsStyleContext(aParentContext, aPseudoTag, aPseudoType, aRuleNode,
                   aSkipFlexItemStyleFixup);
  return context.forget();
}

static inline void
ExtractAnimationValue(nsCSSProperty aProperty,
                      nsStyleContext* aStyleContext,
                      nsStyleAnimation::Value& aResult)
{
  DebugOnly<bool> success =
    nsStyleAnimation::ExtractComputedValue(aProperty, aStyleContext, aResult);
  NS_ABORT_IF_FALSE(success,
                    "aProperty must be extractable by nsStyleAnimation");
}

static nscolor
ExtractColor(nsCSSProperty aProperty,
             nsStyleContext *aStyleContext)
{
  nsStyleAnimation::Value val;
  ExtractAnimationValue(aProperty, aStyleContext, val);
  return val.GetColorValue();
}

static nscolor
ExtractColorLenient(nsCSSProperty aProperty,
                    nsStyleContext *aStyleContext)
{
  nsStyleAnimation::Value val;
  ExtractAnimationValue(aProperty, aStyleContext, val);
  if (val.GetUnit() == nsStyleAnimation::eUnit_Color) {
    return val.GetColorValue();
  }
  return NS_RGBA(0, 0, 0, 0);
}

struct ColorIndexSet {
  uint8_t colorIndex, alphaIndex;
};

static const ColorIndexSet gVisitedIndices[2] = { { 0, 0 }, { 1, 0 } };

nscolor
nsStyleContext::GetVisitedDependentColor(nsCSSProperty aProperty)
{
  NS_ASSERTION(aProperty == eCSSProperty_color ||
               aProperty == eCSSProperty_background_color ||
               aProperty == eCSSProperty_border_top_color ||
               aProperty == eCSSProperty_border_right_color_value ||
               aProperty == eCSSProperty_border_bottom_color ||
               aProperty == eCSSProperty_border_left_color_value ||
               aProperty == eCSSProperty_outline_color ||
               aProperty == eCSSProperty__moz_column_rule_color ||
               aProperty == eCSSProperty_text_decoration_color ||
               aProperty == eCSSProperty_fill ||
               aProperty == eCSSProperty_stroke,
               "we need to add to nsStyleContext::CalcStyleDifference");

  bool isPaintProperty = aProperty == eCSSProperty_fill ||
                         aProperty == eCSSProperty_stroke;

  nscolor colors[2];
  colors[0] = isPaintProperty ? ExtractColorLenient(aProperty, this)
                              : ExtractColor(aProperty, this);

  nsStyleContext *visitedStyle = this->GetStyleIfVisited();
  if (!visitedStyle) {
    return colors[0];
  }

  colors[1] = isPaintProperty ? ExtractColorLenient(aProperty, visitedStyle)
                              : ExtractColor(aProperty, visitedStyle);

  return nsStyleContext::CombineVisitedColors(colors,
                                              this->RelevantLinkVisited());
}

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

void*
nsStyleContext::Alloc(size_t aSize)
{
  nsIPresShell *shell = PresContext()->PresShell();

  aSize += offsetof(AllocationHeader, mStorageStart);
  AllocationHeader *alloc =
    static_cast<AllocationHeader*>(shell->AllocateMisc(aSize));

  alloc->mSize = aSize; // NOTE: inflated by header

  alloc->mNext = mAllocations;
  mAllocations = alloc;

  return static_cast<void*>(&alloc->mStorageStart);
}

void
nsStyleContext::FreeAllocations(nsPresContext *aPresContext)
{
  nsIPresShell *shell = aPresContext->PresShell();

  for (AllocationHeader *alloc = mAllocations, *next; alloc; alloc = next) {
    next = alloc->mNext;
    shell->FreeMisc(alloc->mSize, alloc);
  }
}
