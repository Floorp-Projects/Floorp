/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
/* the interface (to internal code) for retrieving computed style data */

#include "nsStyleConsts.h"
#include "nsString.h"
#include "nsPresContext.h"
#include "nsIStyleRule.h"

#include "nsCOMPtr.h"
#include "nsStyleSet.h"
#include "nsIPresShell.h"

#include "nsRuleNode.h"
#include "nsStyleContext.h"
#include "prlog.h"
#include "nsStyleAnimation.h"

#ifdef DEBUG
// #define NOISY_DEBUG
#endif

//----------------------------------------------------------------------


nsStyleContext::nsStyleContext(nsStyleContext* aParent,
                               nsIAtom* aPseudoTag,
                               nsCSSPseudoElements::Type aPseudoType,
                               nsRuleNode* aRuleNode,
                               nsPresContext* aPresContext)
  : mParent(aParent),
    mChild(nsnull),
    mEmptyChild(nsnull),
    mPseudoTag(aPseudoTag),
    mRuleNode(aRuleNode),
    mAllocations(nsnull),
    mCachedResetData(nsnull),
    mBits(((PRUint32)aPseudoType) << NS_STYLE_CONTEXT_TYPE_SHIFT),
    mRefCnt(0)
{
  // This check has to be done "backward", because if it were written the
  // more natural way it wouldn't fail even when it needed to.
  MOZ_STATIC_ASSERT((PR_UINT32_MAX >> NS_STYLE_CONTEXT_TYPE_SHIFT) >=
                    nsCSSPseudoElements::ePseudo_MAX,
                    "pseudo element bits no longer fit in a PRUint32");

  mNextSibling = this;
  mPrevSibling = this;
  if (mParent) {
    mParent->AddRef();
    mParent->AddChild(this);
#ifdef DEBUG
    nsRuleNode *r1 = mParent->GetRuleNode(), *r2 = aRuleNode;
    while (r1->GetParent())
      r1 = r1->GetParent();
    while (r2->GetParent())
      r2 = r2->GetParent();
    NS_ASSERTION(r1 == r2, "must be in the same rule tree as parent");
#endif
  }

  ApplyStyleFixups(aPresContext);

  #define eStyleStruct_LastItem (nsStyleStructID_Length - 1)
  NS_ASSERTION(NS_STYLE_INHERIT_MASK & NS_STYLE_INHERIT_BIT(LastItem),
               "NS_STYLE_INHERIT_MASK must be bigger, and other bits shifted");
  #undef eStyleStruct_LastItem

  mRuleNode->AddRef();
}

nsStyleContext::~nsStyleContext()
{
  NS_ASSERTION((nsnull == mChild) && (nsnull == mEmptyChild), "destructing context with children");

  nsPresContext *presContext = mRuleNode->GetPresContext();

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

  nsStyleContext **list = aChild->mRuleNode->IsRoot() ? &mEmptyChild : &mChild;

  // Insert at the beginning of the list.  See also FindChildWithRules.
  if (*list) {
    // Link into existing elements, if there are any.
    aChild->mNextSibling = (*list);
    aChild->mPrevSibling = (*list)->mPrevSibling;
    (*list)->mPrevSibling->mNextSibling = aChild;
    (*list)->mPrevSibling = aChild;
  }
  (*list) = aChild;
}

void nsStyleContext::RemoveChild(nsStyleContext* aChild)
{
  NS_PRECONDITION(nsnull != aChild && this == aChild->mParent, "bad argument");

  nsStyleContext **list = aChild->mRuleNode->IsRoot() ? &mEmptyChild : &mChild;

  if (aChild->mPrevSibling != aChild) { // has siblings
    if ((*list) == aChild) {
      (*list) = (*list)->mNextSibling;
    }
  } 
  else {
    NS_ASSERTION((*list) == aChild, "bad sibling pointers");
    (*list) = nsnull;
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
  PRUint32 threshold = 10; // The # of siblings we're willing to examine
                           // before just giving this whole thing up.

  nsStyleContext* result = nsnull;
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

    // Add reference for the caller.
    result->AddRef();
  }

  return result;
}

const void* nsStyleContext::GetCachedStyleData(nsStyleStructID aSID)
{
  const void* cachedData;
  if (nsCachedStyleData::IsReset(aSID)) {
    if (mCachedResetData) {
      cachedData = mCachedResetData->mStyleStructs[aSID];
    } else {
      cachedData = nsnull;
    }
  } else {
    cachedData = mCachedInheritedData.mStyleStructs[aSID];
  }
  return cachedData;
}

const void* nsStyleContext::GetStyleData(nsStyleStructID aSID)
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
  const void *current = GetStyleData(aSID);
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
    return nsnull;
  }

  if (!result) {
    NS_WARNING("Ran out of memory while trying to allocate memory for a unique style struct! "
               "Returning the non-unique data.");
    return const_cast<void*>(current);
  }

  SetStyle(aSID, result);
  mBits &= ~nsCachedStyleData::GetBitForSID(aSID);

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
      mCachedResetData = new (mRuleNode->GetPresContext()) nsResetStyleData;
      // XXXbz And if that fails?
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
nsStyleContext::ApplyStyleFixups(nsPresContext* aPresContext)
{
  // See if we have any text decorations.
  // First see if our parent has text decorations.  If our parent does, then we inherit the bit.
  if (mParent && mParent->HasTextDecorationLines()) {
    mBits |= NS_STYLE_HAS_TEXT_DECORATION_LINES;
  } else {
    // We might have defined a decoration.
    const nsStyleTextReset* text = GetStyleTextReset();
    PRUint8 decorationLine = text->mTextDecorationLine;
    if (decorationLine != NS_STYLE_TEXT_DECORATION_LINE_NONE &&
        decorationLine != NS_STYLE_TEXT_DECORATION_LINE_OVERRIDE_ALL) {
      mBits |= NS_STYLE_HAS_TEXT_DECORATION_LINES;
    }
  }

  if ((mParent && mParent->HasPseudoElementData()) || mPseudoTag) {
    mBits |= NS_STYLE_HAS_PSEUDO_ELEMENT_DATA;
  }

  // Correct tables.
  const nsStyleDisplay* disp = GetStyleDisplay();
  if (disp->mDisplay == NS_STYLE_DISPLAY_TABLE) {
    // -moz-center and -moz-right are used for HTML's alignment
    // This is covering the <div align="right"><table>...</table></div> case.
    // In this case, we don't want to inherit the text alignment into the table.
    const nsStyleText* text = GetStyleText();
    
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

  // Computer User Interface style, to trigger loads of cursors
  GetStyleUserInterface();
}

nsChangeHint
nsStyleContext::CalcStyleDifference(nsStyleContext* aOther)
{
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

  // If our rule nodes are the same, then we are looking at the same
  // style data.  We know this because CalcStyleDifference is always
  // called on two style contexts that point to the same element, so we
  // know that our position in the style context tree is the same and
  // our position in the rule node tree is also the same.
  bool compare = mRuleNode != aOther->mRuleNode;

#define DO_STRUCT_DIFFERENCE(struct_)                                         \
  PR_BEGIN_MACRO                                                              \
    NS_ASSERTION(NS_IsHintSubset(nsStyle##struct_::MaxDifference(), maxHint), \
                 "Struct placed in the wrong maxHint section");               \
    const nsStyle##struct_* this##struct_ = PeekStyle##struct_();             \
    if (this##struct_) {                                                      \
      const nsStyle##struct_* other##struct_ = aOther->GetStyle##struct_();   \
      if ((compare || nsStyle##struct_::ForceCompare()) &&                    \
          !NS_IsHintSubset(maxHint, hint) &&                                  \
          this##struct_ != other##struct_) {                                  \
        NS_ASSERTION(NS_IsHintSubset(                                         \
             this##struct_->CalcDifference(*other##struct_),                  \
             nsStyle##struct_::MaxDifference()),                              \
             "CalcDifference() returned bigger hint than MaxDifference()");   \
        NS_UpdateHint(hint, this##struct_->CalcDifference(*other##struct_));  \
      }                                                                       \
    }                                                                         \
  PR_END_MACRO

  // We begin by examining those style structs that are capable of
  // causing the maximal difference, a FRAMECHANGE.
  // FRAMECHANGE Structs: Display, XUL, Content, UserInterface,
  // Visibility, Outline, TableBorder, Table, Text, UIReset, Quotes
  nsChangeHint maxHint = nsChangeHint(NS_STYLE_HINT_FRAMECHANGE |
      nsChangeHint_UpdateTransformLayer | nsChangeHint_UpdateOpacityLayer |
      nsChangeHint_UpdateOverflow);
  DO_STRUCT_DIFFERENCE(Display);

  maxHint = nsChangeHint(NS_STYLE_HINT_FRAMECHANGE |
      nsChangeHint_UpdateCursor);
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
  // If the quotes implementation is ever going to change we might not need
  // a framechange here and a reflow should be sufficient.  See bug 35768.
  DO_STRUCT_DIFFERENCE(Quotes);

  maxHint = nsChangeHint(NS_STYLE_HINT_REFLOW | nsChangeHint_UpdateEffects);
  DO_STRUCT_DIFFERENCE(SVGReset);
  DO_STRUCT_DIFFERENCE(SVG);

  // At this point, we know that the worst kind of damage we could do is
  // a reflow.
  maxHint = NS_STYLE_HINT_REFLOW;
      
  // The following structs cause (as their maximal difference) a reflow
  // to occur.  REFLOW Structs: Font, Margin, Padding, Border, List,
  // Position, Text, TextReset
  DO_STRUCT_DIFFERENCE(Font);
  DO_STRUCT_DIFFERENCE(Margin);
  DO_STRUCT_DIFFERENCE(Padding);
  DO_STRUCT_DIFFERENCE(Border);
  DO_STRUCT_DIFFERENCE(Position);
  DO_STRUCT_DIFFERENCE(TextReset);

  // Most backgrounds only require a re-render (i.e., a VISUAL change), but
  // backgrounds using -moz-element need to reset SVG effects, too.
  maxHint = nsChangeHint(NS_STYLE_HINT_VISUAL | nsChangeHint_UpdateEffects);
  DO_STRUCT_DIFFERENCE(Background);

  // Color only needs a repaint.
  maxHint = NS_STYLE_HINT_VISUAL;
  DO_STRUCT_DIFFERENCE(Color);

#undef DO_STRUCT_DIFFERENCE

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
      if (thisVis->GetStyleColor()->mColor !=
          otherVis->GetStyleColor()->mColor) {
        change = true;
      }
    }

    // NB: Calling Peek on |this|, not |thisVis| (see above).
    if (!change && PeekStyleBackground()) {
      if (thisVis->GetStyleBackground()->mBackgroundColor !=
          otherVis->GetStyleBackground()->mBackgroundColor) {
        change = true;
      }
    }

    // NB: Calling Peek on |this|, not |thisVis| (see above).
    if (!change && PeekStyleBorder()) {
      const nsStyleBorder *thisVisBorder = thisVis->GetStyleBorder();
      const nsStyleBorder *otherVisBorder = otherVis->GetStyleBorder();
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
      const nsStyleOutline *thisVisOutline = thisVis->GetStyleOutline();
      const nsStyleOutline *otherVisOutline = otherVis->GetStyleOutline();
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
      const nsStyleColumn *thisVisColumn = thisVis->GetStyleColumn();
      const nsStyleColumn *otherVisColumn = otherVis->GetStyleColumn();
      if (thisVisColumn->mColumnRuleColor != otherVisColumn->mColumnRuleColor ||
          thisVisColumn->mColumnRuleColorIsForeground !=
            otherVisColumn->mColumnRuleColorIsForeground) {
        change = true;
      }
    }

    // NB: Calling Peek on |this|, not |thisVis| (see above).
    if (!change && PeekStyleTextReset()) {
      const nsStyleTextReset *thisVisTextReset = thisVis->GetStyleTextReset();
      const nsStyleTextReset *otherVisTextReset = otherVis->GetStyleTextReset();
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
      const nsStyleSVG *thisVisSVG = thisVis->GetStyleSVG();
      const nsStyleSVG *otherVisSVG = otherVis->GetStyleSVG();
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
void nsStyleContext::List(FILE* out, PRInt32 aIndent)
{
  // Indent
  PRInt32 ix;
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

  if (nsnull != mChild) {
    nsStyleContext* child = mChild;
    do {
      child->List(out, aIndent + 1);
      child = child->mNextSibling;
    } while (mChild != child);
  }
  if (nsnull != mEmptyChild) {
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
  return aPresContext->AllocateFromShell(sz);
}

// Overridden to prevent the global delete from being called, since the memory
// came out of an nsIArena instead of the global delete operator's heap.
void 
nsStyleContext::Destroy()
{
  // Get the pres context from our rule node.
  nsRefPtr<nsPresContext> presContext = mRuleNode->GetPresContext();

  // Call our destructor.
  this->~nsStyleContext();

  // Don't let the memory be freed, since it will be recycled
  // instead. Don't call the global operator delete.
  presContext->FreeToShell(sizeof(nsStyleContext), this);
}

already_AddRefed<nsStyleContext>
NS_NewStyleContext(nsStyleContext* aParentContext,
                   nsIAtom* aPseudoTag,
                   nsCSSPseudoElements::Type aPseudoType,
                   nsRuleNode* aRuleNode,
                   nsPresContext* aPresContext)
{
  nsStyleContext* context =
    new (aPresContext) nsStyleContext(aParentContext, aPseudoTag, aPseudoType,
                                      aRuleNode, aPresContext);
  if (context)
    context->AddRef();
  return context;
}

static nscolor ExtractColor(nsCSSProperty aProperty,
                            nsStyleContext *aStyleContext)
{
  nsStyleAnimation::Value val;
#ifdef DEBUG
  bool success =
#endif
    nsStyleAnimation::ExtractComputedValue(aProperty, aStyleContext, val);
  NS_ABORT_IF_FALSE(success,
                    "aProperty must be extractable by nsStyleAnimation");
  return val.GetColorValue();
}

struct ColorIndexSet {
  PRUint8 colorIndex, alphaIndex;
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

  nscolor colors[2];
  colors[0] = ExtractColor(aProperty, this);

  nsStyleContext *visitedStyle = this->GetStyleIfVisited();
  if (!visitedStyle) {
    return colors[0];
  }

  colors[1] = ExtractColor(aProperty, visitedStyle);

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
