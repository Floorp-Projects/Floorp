/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
 *   Daniel Glazman <glazman@netscape.com>
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *   L. David Baron <dbaron@dbaron.org>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Michael Ventnor <m.ventnor@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * a node in the lexicographic tree of rules that match an element,
 * responsible for converting the rules' information into computed style
 */

#include "nsRuleNode.h"
#include "nscore.h"
#include "nsIServiceManager.h"
#include "nsIDeviceContext.h"
#include "nsILookAndFeel.h"
#include "nsIPresShell.h"
#include "nsIThebesFontMetrics.h"
#include "gfxFont.h"
#include "nsStyleUtil.h"
#include "nsCSSPseudoElements.h"
#include "nsThemeConstants.h"
#include "nsITheme.h"
#include "pldhash.h"
#include "nsStyleContext.h"
#include "nsStyleSet.h"
#include "nsSize.h"
#include "imgIRequest.h"
#include "nsRuleData.h"
#include "nsILanguageAtomService.h"
#include "nsIStyleRule.h"
#include "nsBidiUtils.h"
#include "nsStyleStructInlines.h"
#include "nsStyleTransformMatrix.h"
#include "nsCSSKeywords.h"
#include "nsCSSProps.h"

/*
 * For storage of an |nsRuleNode|'s children in a PLDHashTable.
 */

struct ChildrenHashEntry : public PLDHashEntryHdr {
  // key is |mRuleNode->GetKey()|
  nsRuleNode *mRuleNode;
};

/* static */ PLDHashNumber
nsRuleNode::ChildrenHashHashKey(PLDHashTable *aTable, const void *aKey)
{
  const nsRuleNode::Key *key =
    static_cast<const nsRuleNode::Key*>(aKey);
  // Disagreement on importance and level for the same rule is extremely
  // rare, so hash just on the rule.
  return PL_DHashVoidPtrKeyStub(aTable, key->mRule);
}

/* static */ PRBool
nsRuleNode::ChildrenHashMatchEntry(PLDHashTable *aTable,
                                   const PLDHashEntryHdr *aHdr,
                                   const void *aKey)
{
  const ChildrenHashEntry *entry =
    static_cast<const ChildrenHashEntry*>(aHdr);
  const nsRuleNode::Key *key =
    static_cast<const nsRuleNode::Key*>(aKey);
  return entry->mRuleNode->GetKey() == *key;
}

/* static */ PLDHashTableOps
nsRuleNode::ChildrenHashOps = {
  // It's probably better to allocate the table itself using malloc and
  // free rather than the pres shell's arena because the table doesn't
  // grow very often and the pres shell's arena doesn't recycle very
  // large size allocations.
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  ChildrenHashHashKey,
  ChildrenHashMatchEntry,
  PL_DHashMoveEntryStub,
  PL_DHashClearEntryStub,
  PL_DHashFinalizeStub,
  NULL
};


// EnsureBlockDisplay:
//  - if the display value (argument) is not a block-type
//    then we set it to a valid block display value
//  - For enforcing the floated/positioned element CSS2 rules
static void EnsureBlockDisplay(PRUint8& display)
{
  // see if the display value is already a block
  switch (display) {
  case NS_STYLE_DISPLAY_NONE :
    // never change display:none *ever*
  case NS_STYLE_DISPLAY_TABLE :
  case NS_STYLE_DISPLAY_BLOCK :
  case NS_STYLE_DISPLAY_LIST_ITEM :
    // do not muck with these at all - already blocks
    // This is equivalent to nsStyleDisplay::IsBlockOutside.  (XXX Maybe we
    // should just call that?)
    break;

  case NS_STYLE_DISPLAY_INLINE_TABLE :
    // make inline tables into tables
    display = NS_STYLE_DISPLAY_TABLE;
    break;

  default :
    // make it a block
    display = NS_STYLE_DISPLAY_BLOCK;
  }
}

// XXX This should really be done in the CSS parser.
static nsString& Unquote(nsString& aString)
{
  PRUnichar start = aString.First();
  PRUnichar end = aString.Last();

  if ((start == end) && 
      ((start == PRUnichar('\"')) || 
       (start == PRUnichar('\'')))) {
    PRInt32 length = aString.Length();
    aString.Truncate(length - 1);
    aString.Cut(0, 1);
  }
  return aString;
}

static nscoord CalcLengthWith(const nsCSSValue& aValue,
                              nscoord aFontSize,
                              const nsStyleFont* aStyleFont,
                              nsStyleContext* aStyleContext,
                              nsPresContext* aPresContext,
                              PRBool& aInherited)
{
  NS_ASSERTION(aValue.IsLengthUnit(), "not a length unit");
  NS_ASSERTION(aStyleFont || aStyleContext, "Must have style data");
  NS_ASSERTION(aPresContext, "Must have prescontext");

  if (aValue.IsFixedLengthUnit()) {
    return aPresContext->TwipsToAppUnits(aValue.GetLengthTwips());
  }
  nsCSSUnit unit = aValue.GetUnit();
  if (unit == eCSSUnit_Pixel) {
    return nsPresContext::CSSPixelsToAppUnits(aValue.GetFloatValue());
  }
  // Common code for all units other than pixels:
  aInherited = PR_TRUE;
  if (!aStyleFont) {
    aStyleFont = aStyleContext->GetStyleFont();
  }
  if (aFontSize == -1) {
    // XXX Should this be aStyleFont->mSize instead to avoid taking minfontsize
    // prefs into account?
    aFontSize = aStyleFont->mFont.size;
  }
  switch (unit) {
    case eCSSUnit_EM: {
      return NSToCoordRoundWithClamp(aValue.GetFloatValue() * float(aFontSize));
      // XXX scale against font metrics height instead?
    }
    case eCSSUnit_XHeight: {
      nsFont font = aStyleFont->mFont;
      font.size = aFontSize;
      nsCOMPtr<nsIFontMetrics> fm = aPresContext->GetMetricsFor(font);
      nscoord xHeight;
      fm->GetXHeight(xHeight);
      return NSToCoordRoundWithClamp(aValue.GetFloatValue() * float(xHeight));
    }
    case eCSSUnit_Char: {
      nsFont font = aStyleFont->mFont;
      font.size = aFontSize;
      nsCOMPtr<nsIFontMetrics> fm = aPresContext->GetMetricsFor(font);
      nsCOMPtr<nsIThebesFontMetrics> tfm(do_QueryInterface(fm));
      gfxFloat zeroWidth = (tfm->GetThebesFontGroup()->GetFontAt(0)
                            ->GetMetrics().zeroOrAveCharWidth);

      return NSToCoordRoundWithClamp(aValue.GetFloatValue() *
                            NS_ceil(aPresContext->AppUnitsPerDevPixel() *
                                    zeroWidth));
    }
    default:
      NS_NOTREACHED("unexpected unit");
      break;
  }
  return 0;
}

/* static */ nscoord
nsRuleNode::CalcLength(const nsCSSValue& aValue,
                       nsStyleContext* aStyleContext,
                       nsPresContext* aPresContext,
                       PRBool& aInherited)
{
  NS_ASSERTION(aStyleContext, "Must have style data");

  return CalcLengthWith(aValue, -1, nsnull, aStyleContext, aPresContext, aInherited);
}

/* Inline helper function to redirect requests to CalcLength. */
static inline nscoord CalcLength(const nsCSSValue& aValue,
                                 nsStyleContext* aStyleContext,
                                 nsPresContext* aPresContext,
                                 PRBool& aInherited)
{
  return nsRuleNode::CalcLength(aValue, aStyleContext,
                                aPresContext, aInherited);
}

/* static */ nscoord
nsRuleNode::CalcLengthWithInitialFont(nsPresContext* aPresContext,
                                      const nsCSSValue& aValue)
{
  nsStyleFont defaultFont(aPresContext);
  PRBool inherited;
  return CalcLengthWith(aValue, -1, &defaultFont, nsnull, aPresContext,
                        inherited);
}

#define SETCOORD_NORMAL                 0x01   // N
#define SETCOORD_AUTO                   0x02   // A
#define SETCOORD_INHERIT                0x04   // H
#define SETCOORD_PERCENT                0x08   // P
#define SETCOORD_FACTOR                 0x10   // F
#define SETCOORD_LENGTH                 0x20   // L
#define SETCOORD_INTEGER                0x40   // I
#define SETCOORD_ENUMERATED             0x80   // E
#define SETCOORD_NONE                   0x100  // O
#define SETCOORD_INITIAL_ZERO           0x200
#define SETCOORD_INITIAL_AUTO           0x400
#define SETCOORD_INITIAL_NONE           0x800
#define SETCOORD_INITIAL_NORMAL         0x1000
#define SETCOORD_INITIAL_HALF           0x2000

#define SETCOORD_LP     (SETCOORD_LENGTH | SETCOORD_PERCENT)
#define SETCOORD_LH     (SETCOORD_LENGTH | SETCOORD_INHERIT)
#define SETCOORD_AH     (SETCOORD_AUTO | SETCOORD_INHERIT)
#define SETCOORD_LAH    (SETCOORD_AUTO | SETCOORD_LENGTH | SETCOORD_INHERIT)
#define SETCOORD_LPH    (SETCOORD_LP | SETCOORD_INHERIT)
#define SETCOORD_LPAH   (SETCOORD_LP | SETCOORD_AH)
#define SETCOORD_LPEH   (SETCOORD_LP | SETCOORD_ENUMERATED | SETCOORD_INHERIT)
#define SETCOORD_LPAEH  (SETCOORD_LPAH | SETCOORD_ENUMERATED)
#define SETCOORD_LPOH   (SETCOORD_LPH | SETCOORD_NONE)
#define SETCOORD_LPOEH  (SETCOORD_LPOH | SETCOORD_ENUMERATED)
#define SETCOORD_LE     (SETCOORD_LENGTH | SETCOORD_ENUMERATED)
#define SETCOORD_LEH    (SETCOORD_LE | SETCOORD_INHERIT)
#define SETCOORD_IA     (SETCOORD_INTEGER | SETCOORD_AUTO)
#define SETCOORD_LAE    (SETCOORD_LENGTH | SETCOORD_AUTO | SETCOORD_ENUMERATED)

// changes aCoord iff it returns PR_TRUE
static PRBool SetCoord(const nsCSSValue& aValue, nsStyleCoord& aCoord, 
                       const nsStyleCoord& aParentCoord,
                       PRInt32 aMask, nsStyleContext* aStyleContext,
                       nsPresContext* aPresContext, PRBool& aInherited)
{
  PRBool  result = PR_TRUE;
  if (aValue.GetUnit() == eCSSUnit_Null) {
    result = PR_FALSE;
  }
  else if (((aMask & SETCOORD_LENGTH) != 0) && 
           aValue.IsLengthUnit()) {
    aCoord.SetCoordValue(CalcLength(aValue, aStyleContext, aPresContext, aInherited));
  }
  else if (((aMask & SETCOORD_PERCENT) != 0) && 
           (aValue.GetUnit() == eCSSUnit_Percent)) {
    aCoord.SetPercentValue(aValue.GetPercentValue());
  } 
  else if (((aMask & SETCOORD_INTEGER) != 0) && 
           (aValue.GetUnit() == eCSSUnit_Integer)) {
    aCoord.SetIntValue(aValue.GetIntValue(), eStyleUnit_Integer);
  } 
  else if (((aMask & SETCOORD_ENUMERATED) != 0) && 
           (aValue.GetUnit() == eCSSUnit_Enumerated)) {
    aCoord.SetIntValue(aValue.GetIntValue(), eStyleUnit_Enumerated);
  } 
  else if (((aMask & SETCOORD_AUTO) != 0) && 
           (aValue.GetUnit() == eCSSUnit_Auto)) {
    aCoord.SetAutoValue();
  } 
  else if (((aMask & SETCOORD_INHERIT) != 0) && 
           (aValue.GetUnit() == eCSSUnit_Inherit)) {
    aCoord = aParentCoord;  // just inherit value from parent
    aInherited = PR_TRUE;
  }
  else if (((aMask & SETCOORD_NORMAL) != 0) && 
           (aValue.GetUnit() == eCSSUnit_Normal)) {
    aCoord.SetNormalValue();
  }
  else if (((aMask & SETCOORD_NONE) != 0) && 
           (aValue.GetUnit() == eCSSUnit_None)) {
    aCoord.SetNoneValue();
  }
  else if (((aMask & SETCOORD_FACTOR) != 0) && 
           (aValue.GetUnit() == eCSSUnit_Number)) {
    aCoord.SetFactorValue(aValue.GetFloatValue());
  }
  else if (((aMask & SETCOORD_INITIAL_AUTO) != 0) && 
           (aValue.GetUnit() == eCSSUnit_Initial)) {
    aCoord.SetAutoValue();
  }
  else if (((aMask & SETCOORD_INITIAL_ZERO) != 0) && 
           (aValue.GetUnit() == eCSSUnit_Initial)) {
    aCoord.SetCoordValue(0);
  }
  else if (((aMask & SETCOORD_INITIAL_NONE) != 0) && 
           (aValue.GetUnit() == eCSSUnit_Initial)) {
    aCoord.SetNoneValue();
  }
  else if (((aMask & SETCOORD_INITIAL_NORMAL) != 0) && 
           (aValue.GetUnit() == eCSSUnit_Initial)) {
    aCoord.SetNormalValue();
  }
  else if (((aMask & SETCOORD_INITIAL_HALF) != 0) &&
           (aValue.GetUnit() == eCSSUnit_Initial)) {
    aCoord.SetPercentValue(0.5f);
  }
  else {
    result = PR_FALSE;  // didn't set anything
  }
  return result;
}

/* Given an enumerated value that represents a box position, converts it to
 * a float representing the percentage of the box it corresponds to.  For
 * example, "center" becomes 0.5f.
 *
 * @param aEnumValue The enumerated value.
 * @return The float percent it corresponds to.
 */
static float GetFloatFromBoxPosition(PRInt32 aEnumValue)
{
  switch (aEnumValue) {
  case NS_STYLE_BG_POSITION_LEFT:
  case NS_STYLE_BG_POSITION_TOP:
    return 0.0f;
  case NS_STYLE_BG_POSITION_RIGHT:
  case NS_STYLE_BG_POSITION_BOTTOM:
    return 1.0f;
  default:
    NS_NOTREACHED("unexpected value");
    // fall through
  case NS_STYLE_BG_POSITION_CENTER:
    return 0.5f;
  }
}

static PRBool SetColor(const nsCSSValue& aValue, const nscolor aParentColor, 
                       nsPresContext* aPresContext, nsStyleContext *aContext,
                       nscolor& aResult, PRBool& aInherited)
{
  PRBool  result = PR_FALSE;
  nsCSSUnit unit = aValue.GetUnit();

  if (eCSSUnit_Color == unit) {
    aResult = aValue.GetColorValue();
    result = PR_TRUE;
  }
  else if (eCSSUnit_String == unit) {
    nsAutoString  value;
    aValue.GetStringValue(value);
    nscolor rgba;
    if (NS_ColorNameToRGB(value, &rgba)) {
      aResult = rgba;
      result = PR_TRUE;
    }
  }
  else if (eCSSUnit_EnumColor == unit) {
    PRInt32 intValue = aValue.GetIntValue();
    if (0 <= intValue) {
      nsILookAndFeel* look = aPresContext->LookAndFeel();
      nsILookAndFeel::nsColorID colorID = (nsILookAndFeel::nsColorID) intValue;
      if (NS_SUCCEEDED(look->GetColor(colorID, aResult))) {
        result = PR_TRUE;
      }
    }
    else {
      switch (intValue) {
        case NS_COLOR_MOZ_HYPERLINKTEXT:
          aResult = aPresContext->DefaultLinkColor();
          break;
        case NS_COLOR_MOZ_VISITEDHYPERLINKTEXT:
          aResult = aPresContext->DefaultVisitedLinkColor();
          break;
        case NS_COLOR_MOZ_ACTIVEHYPERLINKTEXT:
          aResult = aPresContext->DefaultActiveLinkColor();
          break;
        case NS_COLOR_CURRENTCOLOR:
          // The data computed from this can't be shared in the rule tree 
          // because they could be used on a node with a different color
          aInherited = PR_TRUE;
          aResult = aContext->GetStyleColor()->mColor;
          break;
        default:
          NS_NOTREACHED("Should never have an unknown negative colorID.");
          break;
      }
      result = PR_TRUE;
    }
  }
  else if (eCSSUnit_Inherit == unit) {
    aResult = aParentColor;
    result = PR_TRUE;
    aInherited = PR_TRUE;
  }
  return result;
}

// flags for SetDiscrete - align values with SETCOORD_* constants
// where possible

#define SETDSC_NORMAL                 0x01   // N
#define SETDSC_AUTO                   0x02   // A
#define SETDSC_INTEGER                0x40   // I
#define SETDSC_ENUMERATED             0x80   // E
#define SETDSC_NONE                   0x100  // O
#define SETDSC_SYSTEM_FONT            0x2000

// no caller cares whether aField was changed or not
template <typename FieldT,
          typename T1, typename T2, typename T3, typename T4, typename T5>
static void
SetDiscrete(const nsCSSValue& aValue, FieldT & aField,
            PRBool& aInherited, PRUint32 aMask,
            FieldT aParentValue,
            T1 aInitialValue,
            T2 aAutoValue,
            T3 aNoneValue,
            T4 aNormalValue,
            T5 aSystemFontValue)
{
  switch (aValue.GetUnit()) {
  case eCSSUnit_Null:
    return;

    // every caller of SetDiscrete provides inherit and initial
    // alternatives, so we don't require them to say so in the mask
  case eCSSUnit_Inherit:
    aInherited = PR_TRUE;
    aField = aParentValue;
    return;

  case eCSSUnit_Initial:
    aField = aInitialValue;
    return;

    // every caller provides one or other of these alternatives,
    // but they have to say which
  case eCSSUnit_Enumerated:
    if (aMask & SETDSC_ENUMERATED) {
      aField = aValue.GetIntValue();
      return;
    }
    break;

  case eCSSUnit_Integer:
    if (aMask & SETDSC_INTEGER) {
      aField = aValue.GetIntValue();
      return;
    }
    break;

    // remaining possibilities in descending order of frequency of use
  case eCSSUnit_Auto:
    if (aMask & SETDSC_AUTO) {
      aField = aAutoValue;
      return;
    }
    break;

  case eCSSUnit_None:
    if (aMask & SETDSC_NONE) {
      aField = aNoneValue;
      return;
    }
    break;
    
  case eCSSUnit_Normal:
    if (aMask & SETDSC_NORMAL) {
      aField = aNormalValue;
      return;
    }
    break;

  case eCSSUnit_System_Font:
    if (aMask & SETDSC_SYSTEM_FONT) {
      aField = aSystemFontValue;
      return;
    }
    break;

  default:
    break;
  }

  NS_NOTREACHED("SetDiscrete: inappropriate unit");
}

// flags for SetFactor
#define SETFCT_POSITIVE 0x01        // assert value is >= 0.0f
#define SETFCT_OPACITY  0x02        // clamp value to [0.0f .. 1.0f]
#define SETFCT_NONE     0x04        // allow _None (uses aInitialValue).

static void
SetFactor(const nsCSSValue& aValue, float& aField, PRBool& aInherited,
          float aParentValue, float aInitialValue, PRUint32 aFlags = 0)
{
  switch (aValue.GetUnit()) {
  case eCSSUnit_Null:
    return;

  case eCSSUnit_Number:
    aField = aValue.GetFloatValue();
    if (aFlags & SETFCT_POSITIVE) {
      NS_ASSERTION(aField >= 0.0f, "negative value for positive-only property");
      if (aField < 0.0f)
        aField = 0.0f;
    }
    if (aFlags & SETFCT_OPACITY) {
      if (aField < 0.0f)
        aField = 0.0f;
      if (aField > 1.0f)
        aField = 1.0f;
    }
    return;

  case eCSSUnit_Inherit:
    aInherited = PR_TRUE;
    aField = aParentValue;
    return;

  case eCSSUnit_Initial:
    aField = aInitialValue;
    return;

  case eCSSUnit_None:
    if (aFlags & SETFCT_NONE) {
      aField = aInitialValue;
      return;
    }
    break;

  default:
    break;
  }

  NS_NOTREACHED("SetFactor: inappropriate unit");
}

// Overloaded new operator. Initializes the memory to 0 and relies on an arena
// (which comes from the presShell) to perform the allocation.
void* 
nsRuleNode::operator new(size_t sz, nsPresContext* aPresContext) CPP_THROW_NEW
{
  // Check the recycle list first.
  return aPresContext->AllocateFromShell(sz);
}

/* static */ PLDHashOperator
nsRuleNode::EnqueueRuleNodeChildren(PLDHashTable *table, PLDHashEntryHdr *hdr,
                                    PRUint32 number, void *arg)
{
  ChildrenHashEntry *entry = static_cast<ChildrenHashEntry*>(hdr);
  nsRuleNode ***destroyQueueTail = static_cast<nsRuleNode***>(arg);
  **destroyQueueTail = entry->mRuleNode;
  *destroyQueueTail = &entry->mRuleNode->mNextSibling;
  return PL_DHASH_NEXT;
}

// Overridden to prevent the global delete from being called, since the memory
// came out of an nsIArena instead of the global delete operator's heap.
void 
nsRuleNode::DestroyInternal(nsRuleNode ***aDestroyQueueTail)
{
  nsRuleNode *destroyQueue, **destroyQueueTail;
  if (aDestroyQueueTail) {
    destroyQueueTail = *aDestroyQueueTail;
  } else {
    destroyQueue = nsnull;
    destroyQueueTail = &destroyQueue;
  }

  if (ChildrenAreHashed()) {
    PLDHashTable *children = ChildrenHash();
    PL_DHashTableEnumerate(children, EnqueueRuleNodeChildren,
                           &destroyQueueTail);
    *destroyQueueTail = nsnull; // ensure null-termination
    PL_DHashTableDestroy(children);
  } else if (HaveChildren()) {
    *destroyQueueTail = ChildrenList();
    do {
      destroyQueueTail = &(*destroyQueueTail)->mNextSibling;
    } while (*destroyQueueTail);
  }
  mChildren.asVoid = nsnull;

  if (aDestroyQueueTail) {
    // Our caller destroys the queue.
    *aDestroyQueueTail = destroyQueueTail;
  } else {
    // We have to do destroy the queue.  When we destroy each node, it
    // will add its children to the queue.
    while (destroyQueue) {
      nsRuleNode *cur = destroyQueue;
      destroyQueue = destroyQueue->mNextSibling;
      if (!destroyQueue) {
        NS_ASSERTION(destroyQueueTail == &cur->mNextSibling, "mangled list");
        destroyQueueTail = &destroyQueue;
      }
      cur->DestroyInternal(&destroyQueueTail);
    }
  }

  // Destroy ourselves.
  this->~nsRuleNode();
  
  // Don't let the memory be freed, since it will be recycled
  // instead. Don't call the global operator delete.
  mPresContext->FreeToShell(sizeof(nsRuleNode), this);
}

nsRuleNode* nsRuleNode::CreateRootNode(nsPresContext* aPresContext)
{
  return new (aPresContext)
    nsRuleNode(aPresContext, nsnull, nsnull, 0xff, PR_FALSE);
}

nsILanguageAtomService* nsRuleNode::gLangService = nsnull;

nsRuleNode::nsRuleNode(nsPresContext* aContext, nsRuleNode* aParent,
                       nsIStyleRule* aRule, PRUint8 aLevel,
                       PRBool aIsImportant)
  : mPresContext(aContext),
    mParent(aParent),
    mRule(aRule),
    mDependentBits((PRUint32(aLevel) << NS_RULE_NODE_LEVEL_SHIFT) |
                   (aIsImportant ? NS_RULE_NODE_IS_IMPORTANT : 0)),
    mNoneBits(0)
{
  mChildren.asVoid = nsnull;
  MOZ_COUNT_CTOR(nsRuleNode);
  NS_IF_ADDREF(mRule);

  NS_ASSERTION(IsRoot() || GetLevel() == aLevel, "not enough bits");
  NS_ASSERTION(IsRoot() || IsImportantRule() == aIsImportant, "yikes");
}

nsRuleNode::~nsRuleNode()
{
  MOZ_COUNT_DTOR(nsRuleNode);
  if (mStyleData.mResetData || mStyleData.mInheritedData)
    mStyleData.Destroy(0, mPresContext);
  NS_IF_RELEASE(mRule);
}

nsRuleNode*
nsRuleNode::Transition(nsIStyleRule* aRule, PRUint8 aLevel,
                       PRPackedBool aIsImportantRule)
{
  nsRuleNode* next = nsnull;
  nsRuleNode::Key key(aRule, aLevel, aIsImportantRule);

  if (HaveChildren() && !ChildrenAreHashed()) {
    PRInt32 numKids = 0;
    nsRuleNode* curr = ChildrenList();
    while (curr && curr->GetKey() != key) {
      curr = curr->mNextSibling;
      ++numKids;
    }
    if (curr)
      next = curr;
    else if (numKids >= kMaxChildrenInList)
      ConvertChildrenToHash();
  }

  if (ChildrenAreHashed()) {
    ChildrenHashEntry *entry = static_cast<ChildrenHashEntry*>
                                          (PL_DHashTableOperate(ChildrenHash(), &key, PL_DHASH_ADD));
    if (!entry) {
      return nsnull;
    }
    if (entry->mRuleNode)
      next = entry->mRuleNode;
    else {
      next = entry->mRuleNode = new (mPresContext)
        nsRuleNode(mPresContext, this, aRule, aLevel, aIsImportantRule);
      if (!next) {
        PL_DHashTableRawRemove(ChildrenHash(), entry);
        return nsnull;
      }
    }
  } else if (!next) {
    // Create the new entry in our list.
    next = new (mPresContext)
      nsRuleNode(mPresContext, this, aRule, aLevel, aIsImportantRule);
    if (!next) {
      return nsnull;
    }
    next->mNextSibling = ChildrenList();
    SetChildrenList(next);
  }
  
  return next;
}

void
nsRuleNode::ConvertChildrenToHash()
{
  NS_ASSERTION(!ChildrenAreHashed() && HaveChildren(),
               "must have a non-empty list of children");
  PLDHashTable *hash = PL_NewDHashTable(&ChildrenHashOps, nsnull,
                                        sizeof(ChildrenHashEntry),
                                        kMaxChildrenInList * 4);
  if (!hash)
    return;
  for (nsRuleNode* curr = ChildrenList(); curr; curr = curr->mNextSibling) {
    // This will never fail because of the initial size we gave the table.
    ChildrenHashEntry *entry = static_cast<ChildrenHashEntry*>(
      PL_DHashTableOperate(hash, curr->mRule, PL_DHASH_ADD));
    NS_ASSERTION(!entry->mRuleNode, "duplicate entries in list");
    entry->mRuleNode = curr;
  }
  SetChildrenHash(hash);
}

inline void
nsRuleNode::PropagateNoneBit(PRUint32 aBit, nsRuleNode* aHighestNode)
{
  nsRuleNode* curr = this;
  for (;;) {
    NS_ASSERTION(!(curr->mNoneBits & aBit), "propagating too far");
    curr->mNoneBits |= aBit;
    if (curr == aHighestNode)
      break;
    curr = curr->mParent;
  }
}

inline void
nsRuleNode::PropagateDependentBit(PRUint32 aBit, nsRuleNode* aHighestNode)
{
  for (nsRuleNode* curr = this; curr != aHighestNode; curr = curr->mParent) {
    if (curr->mDependentBits & aBit) {
#ifdef DEBUG
      while (curr != aHighestNode) {
        NS_ASSERTION(curr->mDependentBits & aBit, "bit not set");
        curr = curr->mParent;
      }
#endif
      break;
    }

    curr->mDependentBits |= aBit;
  }
}

/*
 * The following "Check" functions are used for determining what type of
 * sharing can be used for the data on this rule node.  MORE HERE...
 */

/* the information for a property (or in some cases, a rect group of
   properties) */

struct PropertyCheckData {
  size_t offset;
  // These duplicate the same data in nsCSSProps::kTypeTable and
  // kFlagsTable, except that we have some extra entries for
  // CSS_PROP_INCLUDE_NOT_CSS.
  nsCSSType type;
  PRUint32 flags;
};

/*
 * a callback function that that can revise the result of
 * CheckSpecifiedProperties before finishing; aResult is the current
 * result, and it returns the revised one.
 */
typedef nsRuleNode::RuleDetail
  (* CheckCallbackFn)(const nsRuleDataStruct& aData,
                      nsRuleNode::RuleDetail aResult);

/* the information for all the properties in a style struct */
struct StructCheckData {
  const PropertyCheckData* props;
  PRInt32 nprops;
  CheckCallbackFn callback;
};


/**
 * @param aValue the value being examined
 * @param aSpecifiedCount to be incremented by one if the value is specified
 * @param aInherited to be incremented by one if the value is set to inherit
 */
inline void
ExamineCSSValue(const nsCSSValue& aValue,
                PRUint32& aSpecifiedCount, PRUint32& aInheritedCount)
{
  if (aValue.GetUnit() != eCSSUnit_Null) {
    ++aSpecifiedCount;
    if (aValue.GetUnit() == eCSSUnit_Inherit) {
      ++aInheritedCount;
    }
  }
}

static void
ExamineCSSValuePair(const nsCSSValuePair* aValuePair,
                    PRUint32& aSpecifiedCount, PRUint32& aInheritedCount)
{
  NS_PRECONDITION(aValuePair, "Must have a value pair");
  
  ExamineCSSValue(aValuePair->mXValue, aSpecifiedCount, aInheritedCount);
  ExamineCSSValue(aValuePair->mYValue, aSpecifiedCount, aInheritedCount);
}

static void
ExamineCSSRect(const nsCSSRect* aRect,
               PRUint32& aSpecifiedCount, PRUint32& aInheritedCount)
{
  NS_PRECONDITION(aRect, "Must have a rect");

  NS_FOR_CSS_SIDES(side) {
    ExamineCSSValue(aRect->*(nsCSSRect::sides[side]),
                    aSpecifiedCount, aInheritedCount);
  }
}

static nsRuleNode::RuleDetail
CheckFontCallback(const nsRuleDataStruct& aData,
                  nsRuleNode::RuleDetail aResult)
{
  const nsRuleDataFont& fontData =
      static_cast<const nsRuleDataFont&>(aData);

  // em, ex, percent, 'larger', and 'smaller' values on font-size depend
  // on the parent context's font-size
  // Likewise, 'lighter' and 'bolder' values of 'font-weight' depend on
  // the parent.
  const nsCSSValue& size = fontData.mSize;
  const nsCSSValue& weight = fontData.mWeight;
  if ((size.IsRelativeLengthUnit() && size.GetUnit() != eCSSUnit_Pixel) ||
      size.GetUnit() == eCSSUnit_Percent ||
      (size.GetUnit() == eCSSUnit_Enumerated &&
       (size.GetIntValue() == NS_STYLE_FONT_SIZE_SMALLER ||
        size.GetIntValue() == NS_STYLE_FONT_SIZE_LARGER)) ||
#ifdef MOZ_MATHML
      fontData.mScriptLevel.GetUnit() == eCSSUnit_Integer ||
#endif
      (weight.GetUnit() == eCSSUnit_Enumerated &&
       (weight.GetIntValue() == NS_STYLE_FONT_WEIGHT_BOLDER ||
        weight.GetIntValue() == NS_STYLE_FONT_WEIGHT_LIGHTER))) {
    NS_ASSERTION(aResult == nsRuleNode::eRulePartialReset ||
                 aResult == nsRuleNode::eRuleFullReset ||
                 aResult == nsRuleNode::eRulePartialMixed ||
                 aResult == nsRuleNode::eRuleFullMixed,
                 "we know we already have a reset-counted property");
    // Promote reset to mixed since we have something that depends on
    // the parent.  But never promote to inherited since that could
    // cause inheritance of the exact value.
    if (aResult == nsRuleNode::eRulePartialReset)
      aResult = nsRuleNode::eRulePartialMixed;
    else if (aResult == nsRuleNode::eRuleFullReset)
      aResult = nsRuleNode::eRuleFullMixed;
  }

  return aResult;
}

static nsRuleNode::RuleDetail
CheckColorCallback(const nsRuleDataStruct& aData,
                   nsRuleNode::RuleDetail aResult)
{
  const nsRuleDataColor& colorData =
      static_cast<const nsRuleDataColor&>(aData);

  // currentColor values for color require inheritance
  if (colorData.mColor.GetUnit() == eCSSUnit_EnumColor && 
      colorData.mColor.GetIntValue() == NS_COLOR_CURRENTCOLOR) {
    NS_ASSERTION(aResult == nsRuleNode::eRuleFullReset,
                 "we should already be counted as full-reset");
    aResult = nsRuleNode::eRuleFullInherited;
  }

  return aResult;
}


// for nsCSSPropList.h, so we get information on things in the style
// structs but not nsCSS*
#define CSS_PROP_INCLUDE_NOT_CSS

#define CHECK_DATA_FOR_PROPERTY(name_, id_, method_, flags_, datastruct_, member_, type_, kwtable_) \
  { offsetof(nsRuleData##datastruct_, member_), type_, flags_ },

static const PropertyCheckData FontCheckProperties[] = {
#define CSS_PROP_FONT CHECK_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_FONT
};

static const PropertyCheckData DisplayCheckProperties[] = {
#define CSS_PROP_DISPLAY CHECK_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_DISPLAY
};

static const PropertyCheckData VisibilityCheckProperties[] = {
#define CSS_PROP_VISIBILITY CHECK_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_VISIBILITY
};

static const PropertyCheckData MarginCheckProperties[] = {
#define CSS_PROP_MARGIN CHECK_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_MARGIN
};

static const PropertyCheckData BorderCheckProperties[] = {
#define CSS_PROP_BORDER CHECK_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_BORDER
};

static const PropertyCheckData PaddingCheckProperties[] = {
#define CSS_PROP_PADDING CHECK_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_PADDING
};

static const PropertyCheckData OutlineCheckProperties[] = {
#define CSS_PROP_OUTLINE CHECK_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_OUTLINE
};

static const PropertyCheckData ListCheckProperties[] = {
#define CSS_PROP_LIST CHECK_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_LIST
};

static const PropertyCheckData ColorCheckProperties[] = {
#define CSS_PROP_COLOR CHECK_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_COLOR
};

static const PropertyCheckData BackgroundCheckProperties[] = {
#define CSS_PROP_BACKGROUND CHECK_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_BACKGROUND
};

static const PropertyCheckData PositionCheckProperties[] = {
#define CSS_PROP_POSITION CHECK_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_POSITION
};

static const PropertyCheckData TableCheckProperties[] = {
#define CSS_PROP_TABLE CHECK_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_TABLE
};

static const PropertyCheckData TableBorderCheckProperties[] = {
#define CSS_PROP_TABLEBORDER CHECK_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_TABLEBORDER
};

static const PropertyCheckData ContentCheckProperties[] = {
#define CSS_PROP_CONTENT CHECK_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_CONTENT
};

static const PropertyCheckData QuotesCheckProperties[] = {
#define CSS_PROP_QUOTES CHECK_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_QUOTES
};

static const PropertyCheckData TextCheckProperties[] = {
#define CSS_PROP_TEXT CHECK_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_TEXT
};

static const PropertyCheckData TextResetCheckProperties[] = {
#define CSS_PROP_TEXTRESET CHECK_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_TEXTRESET
};

static const PropertyCheckData UserInterfaceCheckProperties[] = {
#define CSS_PROP_USERINTERFACE CHECK_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_USERINTERFACE
};

static const PropertyCheckData UIResetCheckProperties[] = {
#define CSS_PROP_UIRESET CHECK_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_UIRESET
};

static const PropertyCheckData XULCheckProperties[] = {
#define CSS_PROP_XUL CHECK_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_XUL
};

#ifdef MOZ_SVG
static const PropertyCheckData SVGCheckProperties[] = {
#define CSS_PROP_SVG CHECK_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_SVG
};

static const PropertyCheckData SVGResetCheckProperties[] = {
#define CSS_PROP_SVGRESET CHECK_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_SVGRESET
};  
#endif

static const PropertyCheckData ColumnCheckProperties[] = {
#define CSS_PROP_COLUMN CHECK_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_COLUMN
};

#undef CSS_PROP_INCLUDE_NOT_CSS
#undef CHECK_DATA_FOR_PROPERTY
  
static const StructCheckData gCheckProperties[] = {

#define STYLE_STRUCT(name, checkdata_cb, ctor_args) \
  {name##CheckProperties, \
   sizeof(name##CheckProperties)/sizeof(PropertyCheckData), \
   checkdata_cb},
#include "nsStyleStructList.h"
#undef STYLE_STRUCT
  {nsnull, 0, nsnull}

};



// XXXldb Taking the address of a reference is evil.

inline nsCSSValue&
ValueAtOffset(nsRuleDataStruct& aRuleDataStruct, size_t aOffset)
{
  return * reinterpret_cast<nsCSSValue*>
                           (reinterpret_cast<char*>(&aRuleDataStruct) + aOffset);
}

inline const nsCSSValue&
ValueAtOffset(const nsRuleDataStruct& aRuleDataStruct, size_t aOffset)
{
  return * reinterpret_cast<const nsCSSValue*>
                           (reinterpret_cast<const char*>(&aRuleDataStruct) + aOffset);
}

inline nsCSSRect*
RectAtOffset(nsRuleDataStruct& aRuleDataStruct, size_t aOffset)
{
  return reinterpret_cast<nsCSSRect*>
                         (reinterpret_cast<char*>(&aRuleDataStruct) + aOffset);
}

inline const nsCSSRect*
RectAtOffset(const nsRuleDataStruct& aRuleDataStruct, size_t aOffset)
{
  return reinterpret_cast<const nsCSSRect*>
                         (reinterpret_cast<const char*>(&aRuleDataStruct) + aOffset);
}

inline nsCSSValuePair*
ValuePairAtOffset(nsRuleDataStruct& aRuleDataStruct, size_t aOffset)
{
  return reinterpret_cast<nsCSSValuePair*>
                         (reinterpret_cast<char*>(&aRuleDataStruct) + aOffset);
}

inline const nsCSSValuePair*
ValuePairAtOffset(const nsRuleDataStruct& aRuleDataStruct, size_t aOffset)
{
  return reinterpret_cast<const nsCSSValuePair*>
                         (reinterpret_cast<const char*>(&aRuleDataStruct) + aOffset);
}

inline nsCSSValueList*&
ValueListAtOffset(nsRuleDataStruct& aRuleDataStruct, size_t aOffset)
{
  return * reinterpret_cast<nsCSSValueList**>
                           (reinterpret_cast<char*>(&aRuleDataStruct) + aOffset);
}

inline const nsCSSValueList*
ValueListAtOffset(const nsRuleDataStruct& aRuleDataStruct, size_t aOffset)
{
  return * reinterpret_cast<const nsCSSValueList*const*>
                           (reinterpret_cast<const char*>(&aRuleDataStruct) + aOffset);
}

inline nsCSSValuePairList*&
ValuePairListAtOffset(nsRuleDataStruct& aRuleDataStruct, size_t aOffset)
{
  return * reinterpret_cast<nsCSSValuePairList**>
                           (reinterpret_cast<char*>(&aRuleDataStruct) + aOffset);
}

inline const nsCSSValuePairList*
ValuePairListAtOffset(const nsRuleDataStruct& aRuleDataStruct, size_t aOffset)
{
  return * reinterpret_cast<const nsCSSValuePairList*const*>
                           (reinterpret_cast<const char*>(&aRuleDataStruct) + aOffset);
}

#if defined(MOZ_MATHML) && defined(DEBUG)
static PRBool
AreAllMathMLPropertiesUndefined(const nsCSSFont& aRuleData)
{
  return aRuleData.mScriptLevel.GetUnit() == eCSSUnit_Null &&
         aRuleData.mScriptSizeMultiplier.GetUnit() == eCSSUnit_Null &&
         aRuleData.mScriptMinSize.GetUnit() == eCSSUnit_Null;
}
#endif

inline nsRuleNode::RuleDetail
nsRuleNode::CheckSpecifiedProperties(const nsStyleStructID aSID,
                                     const nsRuleDataStruct& aRuleDataStruct)
{
  const StructCheckData *structData = gCheckProperties + aSID;

  // Build a count of the:
  PRUint32 total = 0,      // total number of props in the struct
           specified = 0,  // number that were specified for this node
           inherited = 0;  // number that were 'inherit' (and not
                           //   eCSSUnit_Inherit) for this node

  for (const PropertyCheckData *prop = structData->props,
                           *prop_end = prop + structData->nprops;
       prop != prop_end;
       ++prop)
    switch (prop->type) {

      case eCSSType_Value:
        ++total;
        ExamineCSSValue(ValueAtOffset(aRuleDataStruct, prop->offset),
                        specified, inherited);
        break;

      case eCSSType_Rect:
        total += 4;
        ExamineCSSRect(RectAtOffset(aRuleDataStruct, prop->offset),
                       specified, inherited);
        break;

      case eCSSType_ValuePair:
        total += 2;
        ExamineCSSValuePair(ValuePairAtOffset(aRuleDataStruct, prop->offset),
                            specified, inherited);
        break;
        
      case eCSSType_ValueList:
        {
          ++total;
          const nsCSSValueList* valueList =
              ValueListAtOffset(aRuleDataStruct, prop->offset);
          if (valueList) {
            ++specified;
            if (eCSSUnit_Inherit == valueList->mValue.GetUnit()) {
              ++inherited;
            }
          }
        }
        break;

      case eCSSType_ValuePairList:
        {
          ++total;
          const nsCSSValuePairList* valuePairList =
              ValuePairListAtOffset(aRuleDataStruct, prop->offset);
          if (valuePairList) {
            ++specified;
            if (eCSSUnit_Inherit == valuePairList->mXValue.GetUnit()) {
              ++inherited;
            }
          }
        }
        break;

      default:
        NS_NOTREACHED("unknown type");
        break;

    }

#if 0
  printf("CheckSpecifiedProperties: SID=%d total=%d spec=%d inh=%d.\n",
         aSID, total, specified, inherited);
#endif

#ifdef MOZ_MATHML
  NS_ASSERTION(aSID != eStyleStruct_Font ||
               mPresContext->Document()->GetMathMLEnabled() ||
               AreAllMathMLPropertiesUndefined(static_cast<const nsCSSFont&>(aRuleDataStruct)),
               "MathML style property was defined even though MathML is disabled");
#endif

  /*
   * Return the most specific information we can: prefer None or Full
   * over Partial, and Reset or Inherited over Mixed, since we can
   * optimize based on the edge cases and not the in-between cases.
   */
  nsRuleNode::RuleDetail result;
  if (inherited == total)
    result = eRuleFullInherited;
  else if (specified == total
#ifdef MOZ_MATHML
           // MathML defines 3 properties in Font that will never be set when
           // MathML is not in use. Therefore if all but three
           // properties have been set, and MathML is not enabled, we can treat
           // this as fully specified. Code in nsMathMLElementFactory will
           // rebuild the rule tree and style data when MathML is first enabled
           // (see nsMathMLElement::BindToTree).
           || (aSID == eStyleStruct_Font && specified + 3 == total &&
               !mPresContext->Document()->GetMathMLEnabled())
#endif
          ) {
    if (inherited == 0)
      result = eRuleFullReset;
    else
      result = eRuleFullMixed;
  } else if (specified == 0)
    result = eRuleNone;
  else if (specified == inherited)
    result = eRulePartialInherited;
  else if (inherited == 0)
    result = eRulePartialReset;
  else
    result = eRulePartialMixed;

  if (structData->callback) {
    result = (*structData->callback)(aRuleDataStruct, result);
  }

  return result;
}

const void*
nsRuleNode::GetDisplayData(nsStyleContext* aContext)
{
  nsRuleDataDisplay displayData; // Declare a struct with null CSS values.
  nsRuleData ruleData(NS_STYLE_INHERIT_BIT(Display), mPresContext, aContext);
  ruleData.mDisplayData = &displayData;

  return WalkRuleTree(eStyleStruct_Display, aContext, &ruleData, &displayData);
}

const void*
nsRuleNode::GetVisibilityData(nsStyleContext* aContext)
{
  nsRuleDataDisplay displayData; // Declare a struct with null CSS values.
  nsRuleData ruleData(NS_STYLE_INHERIT_BIT(Visibility), mPresContext, aContext);
  ruleData.mDisplayData = &displayData;

  return WalkRuleTree(eStyleStruct_Visibility, aContext, &ruleData, &displayData);
}

const void*
nsRuleNode::GetTextData(nsStyleContext* aContext)
{
  nsRuleDataText textData; // Declare a struct with null CSS values.
  nsRuleData ruleData(NS_STYLE_INHERIT_BIT(Text), mPresContext, aContext);
  ruleData.mTextData = &textData;

  const void* res = WalkRuleTree(eStyleStruct_Text, aContext, &ruleData, &textData);
  textData.mTextShadow = nsnull; // We are sharing with some style rule.  It really owns the data.
  return res;
}

const void*
nsRuleNode::GetTextResetData(nsStyleContext* aContext)
{
  nsRuleDataText textData; // Declare a struct with null CSS values.
  nsRuleData ruleData(NS_STYLE_INHERIT_BIT(TextReset), mPresContext, aContext);
  ruleData.mTextData = &textData;

  return WalkRuleTree(eStyleStruct_TextReset, aContext, &ruleData, &textData);
}

const void*
nsRuleNode::GetUserInterfaceData(nsStyleContext* aContext)
{
  nsRuleDataUserInterface uiData; // Declare a struct with null CSS values.
  nsRuleData ruleData(NS_STYLE_INHERIT_BIT(UserInterface), mPresContext, aContext);
  ruleData.mUserInterfaceData = &uiData;

  const void* res = WalkRuleTree(eStyleStruct_UserInterface, aContext, &ruleData, &uiData);
  uiData.mCursor = nsnull; // We are sharing with some style rule.  It really owns the data.
  return res;
}

const void*
nsRuleNode::GetUIResetData(nsStyleContext* aContext)
{
  nsRuleDataUserInterface uiData; // Declare a struct with null CSS values.
  nsRuleData ruleData(NS_STYLE_INHERIT_BIT(UIReset), mPresContext, aContext);
  ruleData.mUserInterfaceData = &uiData;

  const void* res = WalkRuleTree(eStyleStruct_UIReset, aContext, &ruleData, &uiData);
  return res;
}

const void*
nsRuleNode::GetFontData(nsStyleContext* aContext)
{
  nsRuleDataFont fontData; // Declare a struct with null CSS values.
  nsRuleData ruleData(NS_STYLE_INHERIT_BIT(Font), mPresContext, aContext);
  ruleData.mFontData = &fontData;

  return WalkRuleTree(eStyleStruct_Font, aContext, &ruleData, &fontData);
}

const void*
nsRuleNode::GetColorData(nsStyleContext* aContext)
{
  nsRuleDataColor colorData; // Declare a struct with null CSS values.
  nsRuleData ruleData(NS_STYLE_INHERIT_BIT(Color), mPresContext, aContext);
  ruleData.mColorData = &colorData;

  return WalkRuleTree(eStyleStruct_Color, aContext, &ruleData, &colorData);
}

const void*
nsRuleNode::GetBackgroundData(nsStyleContext* aContext)
{
  nsRuleDataColor colorData; // Declare a struct with null CSS values.
  nsRuleData ruleData(NS_STYLE_INHERIT_BIT(Background), mPresContext, aContext);
  ruleData.mColorData = &colorData;

  // If any members need to be set to null here, they must also be set to
  // null in HasAuthorSpecifiedRules (look at mBoxShadow in GetBorderData
  // and HasAuthorSpecifiedRules).

  return WalkRuleTree(eStyleStruct_Background, aContext, &ruleData, &colorData);
}

const void*
nsRuleNode::GetMarginData(nsStyleContext* aContext)
{
  nsRuleDataMargin marginData; // Declare a struct with null CSS values.
  nsRuleData ruleData(NS_STYLE_INHERIT_BIT(Margin), mPresContext, aContext);
  ruleData.mMarginData = &marginData;

  return WalkRuleTree(eStyleStruct_Margin, aContext, &ruleData, &marginData);
}

const void*
nsRuleNode::GetBorderData(nsStyleContext* aContext)
{
  nsRuleDataMargin marginData; // Declare a struct with null CSS values.
  nsRuleData ruleData(NS_STYLE_INHERIT_BIT(Border), mPresContext, aContext);
  ruleData.mMarginData = &marginData;

  const void* res = WalkRuleTree(eStyleStruct_Border, aContext, &ruleData, &marginData);
  // We are sharing with some style rule.  It really owns the data.
  // This nulling must also happen in HasAuthorSpecifiedRules.
  marginData.mBoxShadow = nsnull;
  return res;
}

const void*
nsRuleNode::GetPaddingData(nsStyleContext* aContext)
{
  nsRuleDataMargin marginData; // Declare a struct with null CSS values.
  nsRuleData ruleData(NS_STYLE_INHERIT_BIT(Padding), mPresContext, aContext);
  ruleData.mMarginData = &marginData;

  // If any members need to be set to null here, they must also be set to
  // null in HasAuthorSpecifiedRules (look at mBoxShadow in GetBorderData
  // and HasAuthorSpecifiedRules).

  return WalkRuleTree(eStyleStruct_Padding, aContext, &ruleData, &marginData);
}

const void*
nsRuleNode::GetOutlineData(nsStyleContext* aContext)
{
  nsRuleDataMargin marginData; // Declare a struct with null CSS values.
  nsRuleData ruleData(NS_STYLE_INHERIT_BIT(Outline), mPresContext, aContext);
  ruleData.mMarginData = &marginData;

  return WalkRuleTree(eStyleStruct_Outline, aContext, &ruleData, &marginData);
}

const void*
nsRuleNode::GetListData(nsStyleContext* aContext)
{
  nsRuleDataList listData; // Declare a struct with null CSS values.
  nsRuleData ruleData(NS_STYLE_INHERIT_BIT(List), mPresContext, aContext);
  ruleData.mListData = &listData;

  return WalkRuleTree(eStyleStruct_List, aContext, &ruleData, &listData);
}

const void*
nsRuleNode::GetPositionData(nsStyleContext* aContext)
{
  nsRuleDataPosition posData; // Declare a struct with null CSS values.
  nsRuleData ruleData(NS_STYLE_INHERIT_BIT(Position), mPresContext, aContext);
  ruleData.mPositionData = &posData;

  return WalkRuleTree(eStyleStruct_Position, aContext, &ruleData, &posData);
}

const void*
nsRuleNode::GetTableData(nsStyleContext* aContext)
{
  nsRuleDataTable tableData; // Declare a struct with null CSS values.
  nsRuleData ruleData(NS_STYLE_INHERIT_BIT(Table), mPresContext, aContext);
  ruleData.mTableData = &tableData;

  return WalkRuleTree(eStyleStruct_Table, aContext, &ruleData, &tableData);
}

const void*
nsRuleNode::GetTableBorderData(nsStyleContext* aContext)
{
  nsRuleDataTable tableData; // Declare a struct with null CSS values.
  nsRuleData ruleData(NS_STYLE_INHERIT_BIT(TableBorder), mPresContext, aContext);
  ruleData.mTableData = &tableData;

  return WalkRuleTree(eStyleStruct_TableBorder, aContext, &ruleData, &tableData);
}

const void*
nsRuleNode::GetContentData(nsStyleContext* aContext)
{
  nsRuleDataContent contentData; // Declare a struct with null CSS values.
  nsRuleData ruleData(NS_STYLE_INHERIT_BIT(Content), mPresContext, aContext);
  ruleData.mContentData = &contentData;

  const void* res = WalkRuleTree(eStyleStruct_Content, aContext, &ruleData, &contentData);
  contentData.mCounterIncrement = contentData.mCounterReset = nsnull;
  contentData.mContent = nsnull; // We are sharing with some style rule.  It really owns the data.
  return res;
}

const void*
nsRuleNode::GetQuotesData(nsStyleContext* aContext)
{
  nsRuleDataContent contentData; // Declare a struct with null CSS values.
  nsRuleData ruleData(NS_STYLE_INHERIT_BIT(Quotes), mPresContext, aContext);
  ruleData.mContentData = &contentData;

  const void* res = WalkRuleTree(eStyleStruct_Quotes, aContext, &ruleData, &contentData);
  contentData.mQuotes = nsnull; // We are sharing with some style rule.  It really owns the data.
  return res;
}

const void*
nsRuleNode::GetXULData(nsStyleContext* aContext)
{
  nsRuleDataXUL xulData; // Declare a struct with null CSS values.
  nsRuleData ruleData(NS_STYLE_INHERIT_BIT(XUL), mPresContext, aContext);
  ruleData.mXULData = &xulData;

  return WalkRuleTree(eStyleStruct_XUL, aContext, &ruleData, &xulData);
}

const void*
nsRuleNode::GetColumnData(nsStyleContext* aContext)
{
  nsRuleDataColumn columnData; // Declare a struct with null CSS values.
  nsRuleData ruleData(NS_STYLE_INHERIT_BIT(Column), mPresContext, aContext);
  ruleData.mColumnData = &columnData;

  return WalkRuleTree(eStyleStruct_Column, aContext, &ruleData, &columnData);
}

#ifdef MOZ_SVG
const void*
nsRuleNode::GetSVGData(nsStyleContext* aContext)
{
  nsRuleDataSVG svgData; // Declare a struct with null CSS values.
  nsRuleData ruleData(NS_STYLE_INHERIT_BIT(SVG), mPresContext, aContext);
  ruleData.mSVGData = &svgData;

  const void *res = WalkRuleTree(eStyleStruct_SVG, aContext, &ruleData, &svgData);
  svgData.mStrokeDasharray = nsnull; // We are sharing with some style rule.  It really owns the data.
  return res;
}

const void*
nsRuleNode::GetSVGResetData(nsStyleContext* aContext)
{
  nsRuleDataSVG svgData; // Declare a struct with null CSS values.
  nsRuleData ruleData(NS_STYLE_INHERIT_BIT(SVGReset), mPresContext, aContext);
  ruleData.mSVGData = &svgData;

  return WalkRuleTree(eStyleStruct_SVGReset, aContext, &ruleData, &svgData);
}
#endif

// If we need to restrict which properties apply to the style context,
// return the bit to check in nsCSSProp's flags table.  Otherwise,
// return 0.
inline PRUint32
GetPseudoRestriction(nsStyleContext *aContext)
{
  // This needs to match nsStyleSet::WalkRestrictionRule.
  PRUint32 pseudoRestriction = 0;
  nsIAtom *pseudoType = aContext->GetPseudoType();
  if (pseudoType) {
    if (pseudoType == nsCSSPseudoElements::firstLetter) {
      pseudoRestriction = CSS_PROPERTY_APPLIES_TO_FIRST_LETTER;
    } else if (pseudoType == nsCSSPseudoElements::firstLine) {
      pseudoRestriction = CSS_PROPERTY_APPLIES_TO_FIRST_LINE;
    }
  }
  return pseudoRestriction;
}

static void
UnsetPropertiesWithoutFlags(const nsStyleStructID aSID,
                            nsRuleDataStruct& aRuleDataStruct,
                            PRUint32 aFlags)
{
  NS_ASSERTION(aFlags != 0, "aFlags must be nonzero");
  const StructCheckData *structData = gCheckProperties + aSID;

  for (const PropertyCheckData *prop = structData->props,
                           *prop_end = prop + structData->nprops;
       prop != prop_end;
       ++prop) {
    if ((prop->flags & aFlags) == aFlags)
      // Don't unset the property.
      continue;

    switch (prop->type) {
      case eCSSType_Value:
        ValueAtOffset(aRuleDataStruct, prop->offset).Reset();
        break;
      case eCSSType_Rect:
        RectAtOffset(aRuleDataStruct, prop->offset)->Reset();
        break;
      case eCSSType_ValuePair:
        ValuePairAtOffset(aRuleDataStruct, prop->offset)->Reset();
        break;
      case eCSSType_ValueList:
        ValueListAtOffset(aRuleDataStruct, prop->offset) = nsnull;
        break;
      case eCSSType_ValuePairList:
        ValuePairListAtOffset(aRuleDataStruct, prop->offset) = nsnull;
        break;
      default:
        NS_NOTREACHED("unknown type");
        break;
    }
  }
}

const void*
nsRuleNode::WalkRuleTree(const nsStyleStructID aSID,
                         nsStyleContext* aContext, 
                         nsRuleData* aRuleData,
                         nsRuleDataStruct* aSpecificData)
{
  // We start at the most specific rule in the tree.  
  void* startStruct = nsnull;
  
  nsRuleNode* ruleNode = this;
  nsRuleNode* highestNode = nsnull; // The highest node in the rule tree
                                    // that has the same properties
                                    // specified for struct |aSID| as
                                    // |this| does.
  nsRuleNode* rootNode = this; // After the loop below, this will be the
                               // highest node that we've walked without
                               // finding cached data on the rule tree.
                               // If we don't find any cached data, it
                               // will be the root.  (XXX misnamed)
  RuleDetail detail = eRuleNone;
  PRUint32 bit = nsCachedStyleData::GetBitForSID(aSID);

  while (ruleNode) {
    // See if this rule node has cached the fact that the remaining
    // nodes along this path specify no data whatsoever.
    if (ruleNode->mNoneBits & bit)
      break;

    // If the dependent bit is set on a rule node for this struct, that
    // means its rule won't have any information to add, so skip it.
    // XXXldb I don't understand why we need to check |detail| here, but
    // we do.
    if (detail == eRuleNone)
      while (ruleNode->mDependentBits & bit) {
        NS_ASSERTION(ruleNode->mStyleData.GetStyleData(aSID) == nsnull,
                     "dependent bit with cached data makes no sense");
        // Climb up to the next rule in the tree (a less specific rule).
        rootNode = ruleNode;
        ruleNode = ruleNode->mParent;
        NS_ASSERTION(!(ruleNode->mNoneBits & bit), "can't have both bits set");
      }

    // Check for cached data after the inner loop above -- otherwise
    // we'll miss it.
    startStruct = ruleNode->mStyleData.GetStyleData(aSID);
    if (startStruct)
      break; // We found a rule with fully specified data.  We don't
             // need to go up the tree any further, since the remainder
             // of this branch has already been computed.

    // Ask the rule to fill in the properties that it specifies.
    nsIStyleRule *rule = ruleNode->mRule;
    if (rule) {
      aRuleData->mLevel = ruleNode->GetLevel();
      aRuleData->mIsImportantRule = ruleNode->IsImportantRule();
      rule->MapRuleInfoInto(aRuleData);
    }

    // Now we check to see how many properties have been specified by
    // the rules we've examined so far.
    RuleDetail oldDetail = detail;
    detail = CheckSpecifiedProperties(aSID, *aSpecificData);
  
    if (oldDetail == eRuleNone && detail != eRuleNone)
      highestNode = ruleNode;

    if (detail == eRuleFullReset ||
        detail == eRuleFullMixed ||
        detail == eRuleFullInherited)
      break; // We don't need to examine any more rules.  All properties
             // have been fully specified.

    // Climb up to the next rule in the tree (a less specific rule).
    rootNode = ruleNode;
    ruleNode = ruleNode->mParent;
  }

  // If needed, unset the properties that don't have a flag that allows
  // them to be set for this style context.  (For example, only some
  // properties apply to :first-line and :first-letter.)
  PRUint32 pseudoRestriction = GetPseudoRestriction(aContext);
  if (pseudoRestriction) {
    UnsetPropertiesWithoutFlags(aSID, *aSpecificData, pseudoRestriction);

    // Recompute |detail| based on the restrictions we just applied.
    // We can adjust |detail| arbitrarily because of the restriction
    // rule added in nsStyleSet::WalkRestrictionRule.
    detail = CheckSpecifiedProperties(aSID, *aSpecificData);
  }

  NS_ASSERTION(!startStruct || (detail != eRuleFullReset &&
                                detail != eRuleFullMixed &&
                                detail != eRuleFullInherited),
               "can't have start struct and be fully specified");

  PRBool isReset = nsCachedStyleData::IsReset(aSID);
  if (!highestNode)
    highestNode = rootNode;

  if (!aRuleData->mCanStoreInRuleTree)
    detail = eRulePartialMixed; // Treat as though some data is specified to avoid
                                // the optimizations and force data computation.

  if (detail == eRuleNone && startStruct && !aRuleData->mPostResolveCallback) {
    // We specified absolutely no rule information, but a parent rule in the tree
    // specified all the rule information.  We set a bit along the branch from our
    // node in the tree to the node that specified the data that tells nodes on that
    // branch that they never need to examine their rules for this particular struct type
    // ever again.
    PropagateDependentBit(bit, ruleNode);
    return startStruct;
  }
  // FIXME Do we need to check for mPostResolveCallback?
  if ((!startStruct && !isReset &&
       (detail == eRuleNone || detail == eRulePartialInherited)) ||
      detail == eRuleFullInherited) {
    // We specified no non-inherited information and neither did any of
    // our parent rules.

    // We set a bit along the branch from the highest node (ruleNode)
    // down to our node (this) indicating that no non-inherited data was
    // specified.  This bit is guaranteed to be set already on the path
    // from the highest node to the root node in the case where
    // (detail == eRuleNone), which is the most common case here.
    // We must check |!isReset| because the Compute*Data functions for
    // reset structs wouldn't handle none bits correctly.
    if (highestNode != this && !isReset)
      PropagateNoneBit(bit, highestNode);
    
    // All information must necessarily be inherited from our parent style context.
    // In the absence of any computed data in the rule tree and with
    // no rules specified that didn't have values of 'inherit', we should check our parent.
    nsStyleContext* parentContext = aContext->GetParent();
    if (isReset) {
      /* Reset structs don't inherit from first-line. */
      /* See similar code in COMPUTE_START_RESET */
      while (parentContext &&
             parentContext->GetPseudoType() == nsCSSPseudoElements::firstLine) {
        parentContext = parentContext->GetParent();
      }
    }
    if (parentContext) {
      // We have a parent, and so we should just inherit from the parent.
      // Set the inherit bits on our context.  These bits tell the style context that
      // it never has to go back to the rule tree for data.  Instead the style context tree
      // should be walked to find the data.
      const void* parentStruct = parentContext->GetStyleData(aSID);
      aContext->AddStyleBit(bit); // makes const_cast OK.
      aContext->SetStyle(aSID, const_cast<void*>(parentStruct));
      return parentStruct;
    }
    else
      // We are the root.  In the case of fonts, the default values just
      // come from the pres context.
      return SetDefaultOnRoot(aSID, aContext);
  }

  // We need to compute the data from the information that the rules specified.
  const void* res;
#define STYLE_STRUCT_TEST aSID
#define STYLE_STRUCT(name, checkdata_cb, ctor_args)                           \
  res = Compute##name##Data(startStruct, *aSpecificData, aContext,            \
                      highestNode, detail, !aRuleData->mCanStoreInRuleTree);
#include "nsStyleStructList.h"
#undef STYLE_STRUCT
#undef STYLE_STRUCT_TEST

  // If we have a post-resolve callback, handle that now.
  if (aRuleData->mPostResolveCallback && (NS_LIKELY(res != nsnull)))
    (*aRuleData->mPostResolveCallback)(const_cast<void*>(res), aRuleData);

  // Now return the result.
  return res;
}

const void*
nsRuleNode::SetDefaultOnRoot(const nsStyleStructID aSID, nsStyleContext* aContext)
{
  switch (aSID) {
    case eStyleStruct_Font: 
    {
      nsStyleFont* fontData = new (mPresContext) nsStyleFont(mPresContext);
      if (NS_LIKELY(fontData != nsnull)) {
        nscoord minimumFontSize =
          mPresContext->GetCachedIntPref(kPresContext_MinimumFontSize);

        if (minimumFontSize > 0 && !mPresContext->IsChrome()) {
          fontData->mFont.size = PR_MAX(fontData->mSize, minimumFontSize);
        }
        else {
          fontData->mFont.size = fontData->mSize;
        }
        aContext->SetStyle(eStyleStruct_Font, fontData);
      }
      return fontData;
    }
    case eStyleStruct_Display:
    {
      nsStyleDisplay* disp = new (mPresContext) nsStyleDisplay();
      if (NS_LIKELY(disp != nsnull)) {
        aContext->SetStyle(eStyleStruct_Display, disp);
      }
      return disp;
    }
    case eStyleStruct_Visibility:
    {
      nsStyleVisibility* vis = new (mPresContext) nsStyleVisibility(mPresContext);
      if (NS_LIKELY(vis != nsnull)) {
        aContext->SetStyle(eStyleStruct_Visibility, vis);
      }
      return vis;
    }
    case eStyleStruct_Text:
    {
      nsStyleText* text = new (mPresContext) nsStyleText();
      if (NS_LIKELY(text != nsnull)) {
        aContext->SetStyle(eStyleStruct_Text, text);
      }
      return text;
    }
    case eStyleStruct_TextReset:
    {
      nsStyleTextReset* text = new (mPresContext) nsStyleTextReset();
      if (NS_LIKELY(text != nsnull)) {
        aContext->SetStyle(eStyleStruct_TextReset, text);
      }
      return text;
    }
    case eStyleStruct_Color:
    {
      nsStyleColor* color = new (mPresContext) nsStyleColor(mPresContext);
      if (NS_LIKELY(color != nsnull)) {
        aContext->SetStyle(eStyleStruct_Color, color);
      }
      return color;
    }
    case eStyleStruct_Background:
    {
      nsStyleBackground* bg = new (mPresContext) nsStyleBackground();
      if (NS_LIKELY(bg != nsnull)) {
        aContext->SetStyle(eStyleStruct_Background, bg);
      }
      return bg;
    }
    case eStyleStruct_Margin:
    {
      nsStyleMargin* margin = new (mPresContext) nsStyleMargin();
      if (NS_LIKELY(margin != nsnull)) {
        aContext->SetStyle(eStyleStruct_Margin, margin);
      }
      return margin;
    }
    case eStyleStruct_Border:
    {
      nsStyleBorder* border = new (mPresContext) nsStyleBorder(mPresContext);
      if (NS_LIKELY(border != nsnull)) {
        aContext->SetStyle(eStyleStruct_Border, border);
      }
      return border;
    }
    case eStyleStruct_Padding:
    {
      nsStylePadding* padding = new (mPresContext) nsStylePadding();
      if (NS_LIKELY(padding != nsnull)) {
        aContext->SetStyle(eStyleStruct_Padding, padding);
      }
      return padding;
    }
    case eStyleStruct_Outline:
    {
      nsStyleOutline* outline = new (mPresContext) nsStyleOutline(mPresContext);
      if (NS_LIKELY(outline != nsnull)) {
        aContext->SetStyle(eStyleStruct_Outline, outline);
      }
      return outline;
    }
    case eStyleStruct_List:
    {
      nsStyleList* list = new (mPresContext) nsStyleList();
      if (NS_LIKELY(list != nsnull)) {
        aContext->SetStyle(eStyleStruct_List, list);
      }
      return list;
    }
    case eStyleStruct_Position:
    {
      nsStylePosition* pos = new (mPresContext) nsStylePosition();
      if (NS_LIKELY(pos != nsnull)) {
        aContext->SetStyle(eStyleStruct_Position, pos);
      }
      return pos;
    }
    case eStyleStruct_Table:
    {
      nsStyleTable* table = new (mPresContext) nsStyleTable();
      if (NS_LIKELY(table != nsnull)) {
        aContext->SetStyle(eStyleStruct_Table, table);
      }
      return table;
    }
    case eStyleStruct_TableBorder:
    {
      nsStyleTableBorder* table = new (mPresContext) nsStyleTableBorder(mPresContext);
      if (NS_LIKELY(table != nsnull)) {
        aContext->SetStyle(eStyleStruct_TableBorder, table);
      }
      return table;
    }
    case eStyleStruct_Content:
    {
      nsStyleContent* content = new (mPresContext) nsStyleContent();
      if (NS_LIKELY(content != nsnull)) {
        aContext->SetStyle(eStyleStruct_Content, content);
      }
      return content;
    }
    case eStyleStruct_Quotes:
    {
      nsStyleQuotes* quotes = new (mPresContext) nsStyleQuotes();
      if (NS_LIKELY(quotes != nsnull)) {
        aContext->SetStyle(eStyleStruct_Quotes, quotes);
      }
      return quotes;
    }
    case eStyleStruct_UserInterface:
    {
      nsStyleUserInterface* ui = new (mPresContext) nsStyleUserInterface();
      if (NS_LIKELY(ui != nsnull)) {
        aContext->SetStyle(eStyleStruct_UserInterface, ui);
      }
      return ui;
    }
    case eStyleStruct_UIReset:
    {
      nsStyleUIReset* ui = new (mPresContext) nsStyleUIReset();
      if (NS_LIKELY(ui != nsnull)) {
        aContext->SetStyle(eStyleStruct_UIReset, ui);
      }
      return ui;
    }

    case eStyleStruct_XUL:
    {
      nsStyleXUL* xul = new (mPresContext) nsStyleXUL();
      if (NS_LIKELY(xul != nsnull)) {
        aContext->SetStyle(eStyleStruct_XUL, xul);
      }
      return xul;
    }

    case eStyleStruct_Column:
    {
      nsStyleColumn* column = new (mPresContext) nsStyleColumn(mPresContext);
      if (NS_LIKELY(column != nsnull)) {
        aContext->SetStyle(eStyleStruct_Column, column);
      }
      return column;
    }

#ifdef MOZ_SVG
    case eStyleStruct_SVG:
    {
      nsStyleSVG* svg = new (mPresContext) nsStyleSVG();
      if (NS_LIKELY(svg != nsnull)) {
        aContext->SetStyle(eStyleStruct_SVG, svg);
      }
      return svg;
    }

    case eStyleStruct_SVGReset:
    {
      nsStyleSVGReset* svgReset = new (mPresContext) nsStyleSVGReset();
      if (NS_LIKELY(svgReset != nsnull)) {
        aContext->SetStyle(eStyleStruct_SVGReset, svgReset);
      }
      return svgReset;
    }
#endif
    default:
      /*
       * unhandled case: nsStyleStructID_Length.
       * last item of nsStyleStructID, to know its length.
       */
      return nsnull;
  }
  return nsnull;
}

/*
 * This function handles cascading of *-left or *-right box properties
 * against *-start (which is L for LTR and R for RTL) or *-end (which is
 * R for LTR and L for RTL).
 *
 * Cascading these properties correctly is hard because we need to
 * cascade two properties as one, but which two properties depends on a
 * third property ('direction').  We solve this by treating each of
 * these properties (say, 'margin-start') as a shorthand that sets a
 * property containing the value of the property specified
 * ('margin-start-value') and sets a pair of properties
 * ('margin-left-ltr-source' and 'margin-right-rtl-source') saying which
 * of the properties we use.  Thus, when we want to compute the value of
 * 'margin-left' when 'direction' is 'ltr', we look at the value of
 * 'margin-left-ltr-source', which tells us whether to use the highest
 * 'margin-left' in the cascade or the highest 'margin-start'.
 *
 * Finally, since we can compute the normal (*-left and *-right)
 * properties in a loop, this function works by modifying the data we
 * will use in that loop (which the caller must copy from the const
 * input).
 */
void
nsRuleNode::AdjustLogicalBoxProp(nsStyleContext* aContext,
                                 const nsCSSValue& aLTRSource,
                                 const nsCSSValue& aRTLSource,
                                 const nsCSSValue& aLTRLogicalValue,
                                 const nsCSSValue& aRTLLogicalValue,
                                 PRUint8 aSide,
                                 nsCSSRect& aValueRect,
                                 PRBool& aInherited)
{
  PRBool LTRlogical = aLTRSource.GetUnit() == eCSSUnit_Enumerated &&
                      aLTRSource.GetIntValue() == NS_BOXPROP_SOURCE_LOGICAL;
  PRBool RTLlogical = aRTLSource.GetUnit() == eCSSUnit_Enumerated &&
                      aRTLSource.GetIntValue() == NS_BOXPROP_SOURCE_LOGICAL;
  if (LTRlogical || RTLlogical) {
    // We can't cache anything on the rule tree if we use any data from
    // the style context, since data cached in the rule tree could be
    // used with a style context with a different value.
    aInherited = PR_TRUE;
    PRUint8 dir = aContext->GetStyleVisibility()->mDirection;

    if (dir == NS_STYLE_DIRECTION_LTR) {
      if (LTRlogical)
        aValueRect.*(nsCSSRect::sides[aSide]) = aLTRLogicalValue;
    } else {
      if (RTLlogical)
        aValueRect.*(nsCSSRect::sides[aSide]) = aRTLLogicalValue;
    }
  }
}

/**
 * Begin an nsRuleNode::Compute*Data function for an inherited struct.
 *
 * @param type_ The nsStyle* type this function computes.
 * @param ctorargs_ The arguments used for the default nsStyle* constructor.
 * @param data_ Variable (declared here) holding the result of this
 *              function.
 * @param parentdata_ Variable (declared here) holding the parent style
 *                    context's data for this struct.
 * @param rdtype_ The nsCSS* struct type used to compute this struct's data.
 * @param rdata_ Variable (declared here) holding the nsCSS* used here.
 */
#define COMPUTE_START_INHERITED(type_, ctorargs_, data_, parentdata_, rdtype_, rdata_) \
  NS_ASSERTION(aRuleDetail != eRuleFullInherited,                             \
               "should not have bothered calling Compute*Data");              \
                                                                              \
  nsStyleContext* parentContext = aContext->GetParent();                      \
                                                                              \
  const nsRuleData##rdtype_& rdata_ =                                         \
    static_cast<const nsRuleData##rdtype_&>(aData);                           \
  nsStyle##type_* data_ = nsnull;                                             \
  const nsStyle##type_* parentdata_ = nsnull;                                 \
  PRBool inherited = aInherited;                                              \
                                                                              \
  /* If |inherited| might be false by the time we're done, we can't call */   \
  /* parentContext->GetStyle##type_() since it could recur into setting */    \
  /* the same struct on the same rule node, causing a leak. */                \
  if (parentContext && aRuleDetail != eRuleFullReset &&                       \
      (!aStartStruct || (aRuleDetail != eRulePartialReset &&                  \
                         aRuleDetail != eRuleNone)))                          \
    parentdata_ = parentContext->GetStyle##type_();                           \
  if (aStartStruct)                                                           \
    /* We only need to compute the delta between this computed data and */    \
    /* our computed data. */                                                  \
    data_ = new (mPresContext)                                                \
            nsStyle##type_(*static_cast<nsStyle##type_*>(aStartStruct));      \
  else {                                                                      \
    if (aRuleDetail != eRuleFullMixed && aRuleDetail != eRuleFullReset) {     \
      /* No question. We will have to inherit. Go ahead and init */           \
      /* with inherited vals from parent. */                                  \
      inherited = PR_TRUE;                                                    \
      if (parentdata_)                                                        \
        data_ = new (mPresContext) nsStyle##type_(*parentdata_);              \
      else                                                                    \
        data_ = new (mPresContext) nsStyle##type_ ctorargs_;                  \
    }                                                                         \
    else                                                                      \
      data_ = new (mPresContext) nsStyle##type_ ctorargs_;                    \
  }                                                                           \
                                                                              \
  if (NS_UNLIKELY(!data_))                                                    \
    return nsnull;  /* Out Of Memory */                                       \
  if (!parentdata_)                                                           \
    parentdata_ = data_;

/**
 * Begin an nsRuleNode::Compute*Data function for a reset struct.
 *
 * @param type_ The nsStyle* type this function computes.
 * @param ctorargs_ The arguments used for the default nsStyle* constructor.
 * @param data_ Variable (declared here) holding the result of this
 *              function.
 * @param parentdata_ Variable (declared here) holding the parent style
 *                    context's data for this struct.
 * @param rdtype_ The nsCSS* struct type used to compute this struct's data.
 * @param rdata_ Variable (declared here) holding the nsCSS* used here.
 */
#define COMPUTE_START_RESET(type_, ctorargs_, data_, parentdata_, rdtype_, rdata_) \
  NS_ASSERTION(aRuleDetail != eRuleFullInherited,                             \
               "should not have bothered calling Compute*Data");              \
                                                                              \
  nsStyleContext* parentContext = aContext->GetParent();                      \
  /* Reset structs don't inherit from first-line */                           \
  /* See similar code in WalkRuleTree */                                      \
  while (parentContext &&                                                     \
         parentContext->GetPseudoType() == nsCSSPseudoElements::firstLine) {  \
    parentContext = parentContext->GetParent();                               \
  }                                                                           \
                                                                              \
  const nsRuleData##rdtype_& rdata_ =                                         \
    static_cast<const nsRuleData##rdtype_&>(aData);                           \
  nsStyle##type_* data_;                                                      \
  if (aStartStruct)                                                           \
    /* We only need to compute the delta between this computed data and */    \
    /* our computed data. */                                                  \
    data_ = new (mPresContext)                                                \
            nsStyle##type_(*static_cast<nsStyle##type_*>(aStartStruct));      \
  else                                                                        \
    data_ = new (mPresContext) nsStyle##type_ ctorargs_;                      \
                                                                              \
  if (NS_UNLIKELY(!data_))                                                    \
    return nsnull;  /* Out Of Memory */                                       \
                                                                              \
  /* If |inherited| might be false by the time we're done, we can't call */   \
  /* parentContext->GetStyle##type_() since it could recur into setting */    \
  /* the same struct on the same rule node, causing a leak. */                \
  const nsStyle##type_* parentdata_ = data_;                                  \
  if (parentContext &&                                                        \
      aRuleDetail != eRuleFullReset &&                                        \
      aRuleDetail != eRulePartialReset &&                                     \
      aRuleDetail != eRuleNone)                                               \
    parentdata_ = parentContext->GetStyle##type_();                           \
  PRBool inherited = aInherited;

/**
 * Begin an nsRuleNode::Compute*Data function for an inherited struct.
 *
 * @param type_ The nsStyle* type this function computes.
 * @param data_ Variable holding the result of this function.
 */
#define COMPUTE_END_INHERITED(type_, data_)                                   \
  if (inherited)                                                              \
    /* We inherited, and therefore can't be cached in the rule node.  We */   \
    /* have to be put right on the style context. */                          \
    aContext->SetStyle(eStyleStruct_##type_, data_);                          \
  else {                                                                      \
    /* We were fully specified and can therefore be cached right on the */    \
    /* rule node. */                                                          \
    if (!aHighestNode->mStyleData.mInheritedData) {                           \
      aHighestNode->mStyleData.mInheritedData =                               \
        new (mPresContext) nsInheritedStyleData;                              \
      if (NS_UNLIKELY(!aHighestNode->mStyleData.mInheritedData)) {            \
        data_->Destroy(mPresContext);                                         \
        return nsnull;                                                        \
      }                                                                       \
    }                                                                         \
    aHighestNode->mStyleData.mInheritedData->m##type_##Data = data_;          \
    /* Propagate the bit down. */                                             \
    PropagateDependentBit(NS_STYLE_INHERIT_BIT(type_), aHighestNode);         \
  }                                                                           \
                                                                              \
  return data_;

/**
 * Begin an nsRuleNode::Compute*Data function for a reset struct.
 *
 * @param type_ The nsStyle* type this function computes.
 * @param data_ Variable holding the result of this function.
 */
#define COMPUTE_END_RESET(type_, data_)                                       \
  if (inherited)                                                              \
    /* We inherited, and therefore can't be cached in the rule node.  We */   \
    /* have to be put right on the style context. */                          \
    aContext->SetStyle(eStyleStruct_##type_, data_);                          \
  else {                                                                      \
    /* We were fully specified and can therefore be cached right on the */    \
    /* rule node. */                                                          \
    if (!aHighestNode->mStyleData.mResetData) {                               \
      aHighestNode->mStyleData.mResetData =                                   \
        new (mPresContext) nsResetStyleData;                                  \
      if (NS_UNLIKELY(!aHighestNode->mStyleData.mResetData)) {                \
        data_->Destroy(mPresContext);                                         \
        return nsnull;                                                        \
      }                                                                       \
    }                                                                         \
    aHighestNode->mStyleData.mResetData->m##type_##Data = data_;              \
    /* Propagate the bit down. */                                             \
    PropagateDependentBit(NS_STYLE_INHERIT_BIT(type_), aHighestNode);         \
  }                                                                           \
                                                                              \
  return data_;

#ifdef MOZ_MATHML
// This function figures out how much scaling should be suppressed to
// satisfy scriptminsize. This is our attempt to implement
// http://www.w3.org/TR/MathML2/chapter3.html#id.3.3.4.2.2
// This is called after mScriptLevel, mScriptMinSize and mScriptSizeMultiplier
// have been set in aFont.
//
// Here are the invariants we enforce:
// 1) A decrease in size must not reduce the size below minscriptsize.
// 2) An increase in size must not increase the size above the size we would
// have if minscriptsize had not been applied anywhere.
// 3) The scriptlevel-induced size change must between 1.0 and the parent's
// scriptsizemultiplier^(new script level - old script level), as close to the
// latter as possible subject to constraints 1 and 2.
static nscoord
ComputeScriptLevelSize(const nsStyleFont* aFont, const nsStyleFont* aParentFont,
                       nsPresContext* aPresContext, nscoord* aUnconstrainedSize)
{
  PRInt32 scriptLevelChange =
    aFont->mScriptLevel - aParentFont->mScriptLevel;
  if (scriptLevelChange == 0) {
    *aUnconstrainedSize = aParentFont->mScriptUnconstrainedSize;
    // Constraint #3 says that we cannot change size, and #1 and #2 are always
    // satisfied with no change. It's important this be fast because it covers
    // all non-MathML content.
    return aParentFont->mSize;
  }

  // Compute actual value of minScriptSize
  nscoord minScriptSize =
    nsStyleFont::ZoomText(aPresContext, aParentFont->mScriptMinSize);

  double scriptLevelScale =
    pow(aParentFont->mScriptSizeMultiplier, scriptLevelChange);
  // Compute the size we would have had if minscriptsize had never been
  // applied, also prevent overflow (bug 413274)
  *aUnconstrainedSize =
    NSToCoordRound(PR_MIN(aParentFont->mScriptUnconstrainedSize*scriptLevelScale,
                          nscoord_MAX));
  // Compute the size we could get via scriptlevel change
  nscoord scriptLevelSize =
    NSToCoordRound(PR_MIN(aParentFont->mSize*scriptLevelScale,
                          nscoord_MAX));
  if (scriptLevelScale <= 1.0) {
    if (aParentFont->mSize <= minScriptSize) {
      // We can't decrease the font size at all, so just stick to no change
      // (authors are allowed to explicitly set the font size smaller than
      // minscriptsize)
      return aParentFont->mSize;
    }
    // We can decrease, so apply constraint #1
    return PR_MAX(minScriptSize, scriptLevelSize);
  } else {
    // scriptminsize can only make sizes larger than the unconstrained size
    NS_ASSERTION(*aUnconstrainedSize <= scriptLevelSize, "How can this ever happen?");
    // Apply constraint #2
    return PR_MIN(scriptLevelSize, PR_MAX(*aUnconstrainedSize, minScriptSize));
  }
}
#endif

/* static */ void
nsRuleNode::SetFontSize(nsPresContext* aPresContext,
                        const nsRuleDataFont& aFontData,
                        const nsStyleFont* aFont,
                        const nsStyleFont* aParentFont,
                        nscoord* aSize,
                        const nsFont& aSystemFont,
                        nscoord aParentSize,
                        nscoord aScriptLevelAdjustedParentSize,
                        PRBool aUsedStartStruct,
                        PRBool& aInherited)
{
  PRBool zoom = PR_FALSE;
  PRInt32 baseSize = (PRInt32) aPresContext->
    GetDefaultFont(aFont->mGenericID)->size;
  if (eCSSUnit_Enumerated == aFontData.mSize.GetUnit()) {
    PRInt32 value = aFontData.mSize.GetIntValue();
    PRInt32 scaler = aPresContext->FontScaler();
    float scaleFactor = nsStyleUtil::GetScalingFactor(scaler);

    zoom = PR_TRUE;
    if ((NS_STYLE_FONT_SIZE_XXSMALL <= value) && 
        (value <= NS_STYLE_FONT_SIZE_XXLARGE)) {
      *aSize = nsStyleUtil::CalcFontPointSize(value, baseSize,
                       scaleFactor, aPresContext, eFontSize_CSS);
    }
    else if (NS_STYLE_FONT_SIZE_XXXLARGE == value) {
      // <font size="7"> is not specified in CSS, so we don't use eFontSize_CSS.
      *aSize = nsStyleUtil::CalcFontPointSize(value, baseSize,
                       scaleFactor, aPresContext);
    }
    else if (NS_STYLE_FONT_SIZE_LARGER  == value ||
             NS_STYLE_FONT_SIZE_SMALLER == value) {
      aInherited = PR_TRUE;

      // Un-zoom so we use the tables correctly.  We'll then rezoom due
      // to the |zoom = PR_TRUE| above.
      // Note that relative units here use the parent's size unadjusted
      // for scriptlevel changes. A scriptlevel change between us and the parent
      // is simply ignored.
      nscoord parentSize =
        nsStyleFont::UnZoomText(aPresContext, aParentSize);

      if (NS_STYLE_FONT_SIZE_LARGER == value) {
        *aSize = nsStyleUtil::FindNextLargerFontSize(parentSize,
                         baseSize, scaleFactor, aPresContext, eFontSize_CSS);
        NS_ASSERTION(*aSize > parentSize,
                     "FindNextLargerFontSize failed");
      }
      else {
        *aSize = nsStyleUtil::FindNextSmallerFontSize(parentSize,
                         baseSize, scaleFactor, aPresContext, eFontSize_CSS);
        NS_ASSERTION(*aSize < parentSize ||
                     parentSize <= nsPresContext::CSSPixelsToAppUnits(1), 
                     "FindNextSmallerFontSize failed");
      }
    } else {
      NS_NOTREACHED("unexpected value");
    }
  }
  else if (aFontData.mSize.IsLengthUnit()) {
    // Note that font-based length units use the parent's size unadjusted
    // for scriptlevel changes. A scriptlevel change between us and the parent
    // is simply ignored.
    *aSize = CalcLengthWith(aFontData.mSize, aParentSize, aParentFont, nsnull,
                        aPresContext, aInherited);
    zoom = aFontData.mSize.IsFixedLengthUnit() ||
           aFontData.mSize.GetUnit() == eCSSUnit_Pixel;
  }
  else if (eCSSUnit_Percent == aFontData.mSize.GetUnit()) {
    aInherited = PR_TRUE;
    // Note that % units use the parent's size unadjusted for scriptlevel
    // changes. A scriptlevel change between us and the parent is simply
    // ignored.
    *aSize = NSToCoordRound(aParentSize *
                            aFontData.mSize.GetPercentValue());
    zoom = PR_FALSE;
  }
  else if (eCSSUnit_System_Font == aFontData.mSize.GetUnit()) {
    // this becomes our cascading size
    *aSize = aSystemFont.size;
    zoom = PR_TRUE;
  }
  else if (eCSSUnit_Inherit == aFontData.mSize.GetUnit()) {
    aInherited = PR_TRUE;
    // We apply scriptlevel change for this case, because the default is
    // to inherit and we don't want explicit "inherit" to differ from the
    // default.
    *aSize = aScriptLevelAdjustedParentSize;
    zoom = PR_FALSE;
  }
  else if (eCSSUnit_Initial == aFontData.mSize.GetUnit()) {
    // The initial value is 'medium', which has magical sizing based on
    // the generic font family, so do that here too.
    *aSize = baseSize;
    zoom = PR_TRUE;
  } else {
    NS_ASSERTION(eCSSUnit_Null == aFontData.mSize.GetUnit(),
                 "What kind of font-size value is this?");
#ifdef MOZ_MATHML
    // if aUsedStartStruct is true, then every single property in the
    // font struct is being set all at once. This means scriptlevel is not
    // going to have any influence on the font size; there is no need to
    // do anything here.
    if (!aUsedStartStruct && aParentSize != aScriptLevelAdjustedParentSize) {
      // There was no rule affecting the size but the size has been
      // affected by the parent's size via scriptlevel change. So treat
      // this as inherited.
      aInherited = PR_TRUE;
      *aSize = aScriptLevelAdjustedParentSize;
    }
#endif
  }

  // We want to zoom the cascaded size so that em-based measurements,
  // line-heights, etc., work.
  if (zoom) {
    *aSize = nsStyleFont::ZoomText(aPresContext, *aSize);
  }
}

static PRInt8 ClampTo8Bit(PRInt32 aValue) {
  if (aValue < -128)
    return -128;
  if (aValue > 127)
    return 127;
  return PRInt8(aValue);
}

/* static */ void
nsRuleNode::SetFont(nsPresContext* aPresContext, nsStyleContext* aContext,
                    nscoord aMinFontSize,
                    PRUint8 aGenericFontID, const nsRuleDataFont& aFontData,
                    const nsStyleFont* aParentFont,
                    nsStyleFont* aFont, PRBool aUsedStartStruct,
                    PRBool& aInherited)
{
  const nsFont* defaultVariableFont =
    aPresContext->GetDefaultFont(kPresContext_DefaultVariableFont_ID);

  // -moz-system-font: enum (never inherit!)
  nsFont systemFont;
  if (eCSSUnit_Enumerated == aFontData.mSystemFont.GetUnit()) {
    nsSystemFontID sysID;
    switch (aFontData.mSystemFont.GetIntValue()) {
      case NS_STYLE_FONT_CAPTION:       sysID = eSystemFont_Caption;      break;    // css2
      case NS_STYLE_FONT_ICON:          sysID = eSystemFont_Icon;         break;
      case NS_STYLE_FONT_MENU:          sysID = eSystemFont_Menu;         break;
      case NS_STYLE_FONT_MESSAGE_BOX:   sysID = eSystemFont_MessageBox;   break;
      case NS_STYLE_FONT_SMALL_CAPTION: sysID = eSystemFont_SmallCaption; break;
      case NS_STYLE_FONT_STATUS_BAR:    sysID = eSystemFont_StatusBar;    break;
      case NS_STYLE_FONT_WINDOW:        sysID = eSystemFont_Window;       break;    // css3
      case NS_STYLE_FONT_DOCUMENT:      sysID = eSystemFont_Document;     break;
      case NS_STYLE_FONT_WORKSPACE:     sysID = eSystemFont_Workspace;    break;
      case NS_STYLE_FONT_DESKTOP:       sysID = eSystemFont_Desktop;      break;
      case NS_STYLE_FONT_INFO:          sysID = eSystemFont_Info;         break;
      case NS_STYLE_FONT_DIALOG:        sysID = eSystemFont_Dialog;       break;
      case NS_STYLE_FONT_BUTTON:        sysID = eSystemFont_Button;       break;
      case NS_STYLE_FONT_PULL_DOWN_MENU:sysID = eSystemFont_PullDownMenu; break;
      case NS_STYLE_FONT_LIST:          sysID = eSystemFont_List;         break;
      case NS_STYLE_FONT_FIELD:         sysID = eSystemFont_Field;        break;
    }

    // GetSystemFont sets the font face but not necessarily the size
    // XXX Or at least it used to -- no longer true for thebes.  Maybe
    // it should be again, though.
    systemFont.size = defaultVariableFont->size;

    if (NS_FAILED(aPresContext->DeviceContext()->GetSystemFont(sysID,
                                                               &systemFont))) {
        systemFont.name = defaultVariableFont->name;
    }

    // XXXldb All of this platform-specific stuff should be in the
    // nsIDeviceContext implementations, not here.

#ifdef XP_WIN
    //
    // As far as I can tell the system default fonts and sizes for
    // on MS-Windows for Buttons, Listboxes/Comboxes and Text Fields are 
    // all pre-determined and cannot be changed by either the control panel 
    // or programmtically.
    //
    switch (sysID) {
      // Fields (text fields)
      // Button and Selects (listboxes/comboboxes)
      //    We use whatever font is defined by the system. Which it appears
      //    (and the assumption is) it is always a proportional font. Then we
      //    always use 2 points smaller than what the browser has defined as
      //    the default proportional font.
      case eSystemFont_Field:
      case eSystemFont_Button:
      case eSystemFont_List:
        // Assumption: system defined font is proportional
        systemFont.size = 
          PR_MAX(defaultVariableFont->size - aPresContext->PointsToAppUnits(2), 0);
        break;
    }
#endif
  } else {
    // In case somebody explicitly used -moz-use-system-font.
    systemFont = *defaultVariableFont;
  }


  // font-family: string list, enum, inherit
  NS_ASSERTION(eCSSUnit_Enumerated != aFontData.mFamily.GetUnit(),
               "system fonts should not be in mFamily anymore");
  if (eCSSUnit_String == aFontData.mFamily.GetUnit()) {
    // set the correct font if we are using DocumentFonts OR we are overriding for XUL
    // MJA: bug 31816
    if (aGenericFontID == kGenericFont_NONE) {
      // only bother appending fallback fonts if this isn't a fallback generic font itself
      if (!aFont->mFont.name.IsEmpty())
        aFont->mFont.name.Append((PRUnichar)',');
      // defaultVariableFont.name should always be "serif" or "sans-serif".
      aFont->mFont.name.Append(defaultVariableFont->name);
    }
    aFont->mFont.familyNameQuirks =
        (aPresContext->CompatibilityMode() == eCompatibility_NavQuirks &&
         aFontData.mFamilyFromHTML);
    aFont->mFont.systemFont = PR_FALSE;
    // Technically this is redundant with the code below, but it's good
    // to have since we'll still want it once we get rid of
    // SetGenericFont (bug 380915).
    aFont->mGenericID = aGenericFontID;
  }
  else if (eCSSUnit_System_Font == aFontData.mFamily.GetUnit()) {
    aFont->mFont.name = systemFont.name;
    aFont->mFont.familyNameQuirks = PR_FALSE;
    aFont->mFont.systemFont = PR_TRUE;
    aFont->mGenericID = kGenericFont_NONE;
  }
  else if (eCSSUnit_Inherit == aFontData.mFamily.GetUnit()) {
    aInherited = PR_TRUE;
    aFont->mFont.name = aParentFont->mFont.name;
    aFont->mFont.familyNameQuirks = aParentFont->mFont.familyNameQuirks;
    aFont->mFont.systemFont = aParentFont->mFont.systemFont;
    aFont->mGenericID = aParentFont->mGenericID;
  }
  else if (eCSSUnit_Initial == aFontData.mFamily.GetUnit()) {
    aFont->mFont.name = defaultVariableFont->name;
    aFont->mFont.familyNameQuirks = PR_FALSE;
    aFont->mFont.systemFont = defaultVariableFont->systemFont;
    aFont->mGenericID = kGenericFont_NONE;
  }

  // When we're in the loop in SetGenericFont, we must ensure that we
  // always keep aFont->mFlags set to the correct generic.  But we have
  // to be careful not to touch it when we're called directly from
  // ComputeFontData, because we could have a start struct.
  if (aGenericFontID != kGenericFont_NONE) {
    aFont->mGenericID = aGenericFontID;
  }

  // font-style: enum, normal, inherit, initial, -moz-system-font
  SetDiscrete(aFontData.mStyle, aFont->mFont.style, aInherited,
              SETDSC_ENUMERATED | SETDSC_NORMAL | SETDSC_SYSTEM_FONT,
              aParentFont->mFont.style,
              defaultVariableFont->style,
              0, 0,
              NS_STYLE_FONT_STYLE_NORMAL,
              systemFont.style);

  // font-variant: enum, normal, inherit, initial, -moz-system-font
  SetDiscrete(aFontData.mVariant, aFont->mFont.variant, aInherited,
              SETDSC_ENUMERATED | SETDSC_NORMAL | SETDSC_SYSTEM_FONT,
              aParentFont->mFont.variant,
              defaultVariableFont->variant,
              0, 0,
              NS_STYLE_FONT_VARIANT_NORMAL,
              systemFont.variant);

  // font-weight: int, enum, normal, inherit, initial, -moz-system-font
  // special handling for enum
  if (eCSSUnit_Enumerated == aFontData.mWeight.GetUnit()) {
    PRInt32 value = aFontData.mWeight.GetIntValue();
    switch (value) {
      case NS_STYLE_FONT_WEIGHT_NORMAL:
      case NS_STYLE_FONT_WEIGHT_BOLD:
        aFont->mFont.weight = value;
        break;
      case NS_STYLE_FONT_WEIGHT_BOLDER:
      case NS_STYLE_FONT_WEIGHT_LIGHTER:
        aInherited = PR_TRUE;
        aFont->mFont.weight = nsStyleUtil::ConstrainFontWeight(aParentFont->mFont.weight + value);
        break;
    }
  } else 
    SetDiscrete(aFontData.mWeight, aFont->mFont.weight, aInherited,
                SETDSC_INTEGER | SETDSC_NORMAL | SETDSC_SYSTEM_FONT,
                aParentFont->mFont.weight,
                defaultVariableFont->weight,
                0, 0,
                NS_STYLE_FONT_WEIGHT_NORMAL,
                systemFont.weight);

#ifdef MOZ_MATHML
  // Compute scriptlevel, scriptminsize and scriptsizemultiplier now so
  // they're available for font-size computation.

  // -moz-script-min-size: length
  if (aFontData.mScriptMinSize.IsLengthUnit()) {
    // scriptminsize in font units (em, ex) has to be interpreted relative
    // to the parent font, or the size definitions are circular and we
    // 
    aFont->mScriptMinSize =
      CalcLengthWith(aFontData.mScriptMinSize, aParentFont->mSize, aParentFont, nsnull,
                     aPresContext, aInherited);
  }

  // -moz-script-size-multiplier: factor, inherit, initial
  SetFactor(aFontData.mScriptSizeMultiplier, aFont->mScriptSizeMultiplier,
            aInherited, aParentFont->mScriptSizeMultiplier,
            NS_MATHML_DEFAULT_SCRIPT_SIZE_MULTIPLIER,
            SETFCT_POSITIVE);
  
  // -moz-script-level: integer, number, inherit
  if (eCSSUnit_Integer == aFontData.mScriptLevel.GetUnit()) {
    // "relative"
    aFont->mScriptLevel = ClampTo8Bit(aParentFont->mScriptLevel + aFontData.mScriptLevel.GetIntValue());
  }
  else if (eCSSUnit_Number == aFontData.mScriptLevel.GetUnit()) {
    // "absolute"
    aFont->mScriptLevel = ClampTo8Bit(PRInt32(aFontData.mScriptLevel.GetFloatValue()));
  }
  else if (eCSSUnit_Inherit == aFontData.mScriptSizeMultiplier.GetUnit()) {
    aInherited = PR_TRUE;
    aFont->mScriptLevel = aParentFont->mScriptLevel;
  }
  else if (eCSSUnit_Initial == aFontData.mScriptSizeMultiplier.GetUnit()) {
    aFont->mScriptLevel = 0;
  }
#endif

  // font-size: enum, length, percent, inherit
  nscoord scriptLevelAdjustedParentSize = aParentFont->mSize;
#ifdef MOZ_MATHML
  nscoord scriptLevelAdjustedUnconstrainedParentSize;
  scriptLevelAdjustedParentSize =
    ComputeScriptLevelSize(aFont, aParentFont, aPresContext,
                           &scriptLevelAdjustedUnconstrainedParentSize);
  NS_ASSERTION(!aUsedStartStruct || aFont->mScriptUnconstrainedSize == aFont->mSize,
               "If we have a start struct, we should have reset everything coming in here");
#endif
  SetFontSize(aPresContext, aFontData, aFont, aParentFont, &aFont->mSize,
              systemFont, aParentFont->mSize, scriptLevelAdjustedParentSize,
              aUsedStartStruct, aInherited);
#ifdef MOZ_MATHML
  if (aParentFont->mSize == aParentFont->mScriptUnconstrainedSize &&
      scriptLevelAdjustedParentSize == scriptLevelAdjustedUnconstrainedParentSize) {
    // Fast path: we have not been affected by scriptminsize so we don't
    // need to call SetFontSize again to compute the
    // scriptminsize-unconstrained size. This is OK even if we have a
    // start struct, because if we have a start struct then 'font-size'
    // was specified and so scriptminsize has no effect.
    aFont->mScriptUnconstrainedSize = aFont->mSize;
  } else {
    SetFontSize(aPresContext, aFontData, aFont, aParentFont,
                &aFont->mScriptUnconstrainedSize, systemFont,
                aParentFont->mScriptUnconstrainedSize,
                scriptLevelAdjustedUnconstrainedParentSize,
                aUsedStartStruct, aInherited);
  }
  NS_ASSERTION(aFont->mScriptUnconstrainedSize <= aFont->mSize,
               "scriptminsize should never be making things bigger");
#endif

  // enforce the user' specified minimum font-size on the value that we expose
  // (but don't change font-size:0)
  if (0 < aFont->mSize && aFont->mSize < aMinFontSize)
    aFont->mFont.size = aMinFontSize;
  else
    aFont->mFont.size = aFont->mSize;

  // font-size-adjust: number, none, inherit, initial, -moz-system-font
  if (eCSSUnit_System_Font == aFontData.mSizeAdjust.GetUnit()) {
    aFont->mFont.sizeAdjust = systemFont.sizeAdjust;
  } else
    SetFactor(aFontData.mSizeAdjust, aFont->mFont.sizeAdjust, aInherited,
              aParentFont->mFont.sizeAdjust, 0.0f, SETFCT_NONE);
}

// SetGenericFont:
//  - backtrack to an ancestor with the same generic font name (possibly
//    up to the root where default values come from the presentation context)
//  - re-apply cascading rules from there without caching intermediate values
/* static */ void
nsRuleNode::SetGenericFont(nsPresContext* aPresContext,
                           nsStyleContext* aContext,
                           PRUint8 aGenericFontID, nscoord aMinFontSize,
                           nsStyleFont* aFont)
{
  // walk up the contexts until a context with the desired generic font
  nsAutoVoidArray contextPath;
  contextPath.AppendElement(aContext);
  nsStyleContext* higherContext = aContext->GetParent();
  while (higherContext) {
    if (higherContext->GetStyleFont()->mGenericID == aGenericFontID) {
      // done walking up the higher contexts
      break;
    }
    contextPath.AppendElement(higherContext);
    higherContext = higherContext->GetParent();
  }

  // re-apply the cascading rules, starting from the higher context

  // If we stopped earlier because we reached the root of the style tree,
  // we will start with the default generic font from the presentation
  // context. Otherwise we start with the higher context.
  const nsFont* defaultFont = aPresContext->GetDefaultFont(aGenericFontID);
  nsStyleFont parentFont(*defaultFont, aPresContext);
  if (higherContext) {
    const nsStyleFont* tmpFont = higherContext->GetStyleFont();
    parentFont = *tmpFont;
  }
  *aFont = parentFont;

  PRBool dummy;
  PRUint32 fontBit = nsCachedStyleData::GetBitForSID(eStyleStruct_Font);
  
  for (PRInt32 i = contextPath.Count() - 1; i >= 0; --i) {
    nsStyleContext* context = (nsStyleContext*)contextPath[i];
    nsRuleDataFont fontData; // Declare a struct with null CSS values.
    nsRuleData ruleData(NS_STYLE_INHERIT_BIT(Font), aPresContext, context);
    ruleData.mFontData = &fontData;

    // Trimmed down version of ::WalkRuleTree() to re-apply the style rules
    // Note that we *do* need to do this for our own data, since what is
    // in |fontData| in ComputeFontData is only for the rules below
    // aStartStruct.
    for (nsRuleNode* ruleNode = context->GetRuleNode(); ruleNode;
         ruleNode = ruleNode->GetParent()) {
      if (ruleNode->mNoneBits & fontBit)
        // no more font rules on this branch, get out
        break;

      nsIStyleRule *rule = ruleNode->GetRule();
      if (rule) {
        ruleData.mLevel = ruleNode->GetLevel();
        ruleData.mIsImportantRule = ruleNode->IsImportantRule();
        rule->MapRuleInfoInto(&ruleData);
      }
    }

    // Compute the delta from the information that the rules specified

    // Avoid unnecessary operations in SetFont().  But we care if it's
    // the final value that we're computing.
    if (i != 0)
      fontData.mFamily.Reset();

    nsRuleNode::SetFont(aPresContext, context, aMinFontSize,
                        aGenericFontID, fontData, &parentFont, aFont,
                        PR_FALSE, dummy);

    // XXX Not sure if we need to do this here
    // If we have a post-resolve callback, handle that now.
    if (ruleData.mPostResolveCallback)
      (ruleData.mPostResolveCallback)(aFont, &ruleData);

    parentFont = *aFont;
  }
}

static PRBool ExtractGeneric(const nsString& aFamily, PRBool aGeneric,
                             void *aData)
{
  nsAutoString *data = static_cast<nsAutoString*>(aData);

  if (aGeneric) {
    *data = aFamily;
    return PR_FALSE; // stop enumeration
  }
  return PR_TRUE;
}

const void* 
nsRuleNode::ComputeFontData(void* aStartStruct,
                            const nsRuleDataStruct& aData, 
                            nsStyleContext* aContext, 
                            nsRuleNode* aHighestNode,
                            const RuleDetail aRuleDetail, PRBool aInherited)
{
  COMPUTE_START_INHERITED(Font, (mPresContext), font, parentFont,
                          Font, fontData)

  // NOTE:  The |aRuleDetail| passed in is a little bit conservative due
  // to the -moz-system-font property.  We really don't need to consider
  // it here in determining whether to cache in the rule tree.  However,
  // we do need to consider it in WalkRuleTree when deciding whether to
  // walk further up the tree.  So this means that when the font struct
  // is fully specified using *longhand* properties (excluding
  // -moz-system-font), we won't cache in the rule tree even though we
  // could.  However, it's pretty unlikely authors will do that
  // (although there is a pretty good chance they'll fully specify it
  // using the 'font' shorthand).

  // See if there is a minimum font-size constraint to honor
  nscoord minimumFontSize =
    mPresContext->GetCachedIntPref(kPresContext_MinimumFontSize);

  if (minimumFontSize < 0)
    minimumFontSize = 0;

  PRBool useDocumentFonts =
    mPresContext->GetCachedBoolPref(kPresContext_UseDocumentFonts);

  // See if we are in the chrome
  // We only need to know this to determine if we have to use the
  // document fonts (overriding the useDocumentFonts flag), or to
  // determine if we have to override the minimum font-size constraint.
  if ((!useDocumentFonts || minimumFontSize > 0) && mPresContext->IsChrome()) {
    // if we are not using document fonts, but this is a XUL document,
    // then we use the document fonts anyway
    useDocumentFonts = PR_TRUE;
    minimumFontSize = 0;
  }

  // Figure out if we are a generic font
  PRUint8 generic = kGenericFont_NONE;
  // XXXldb What if we would have had a string if we hadn't been doing
  // the optimization with a non-null aStartStruct?
  if (eCSSUnit_String == fontData.mFamily.GetUnit()) {
    fontData.mFamily.GetStringValue(font->mFont.name);
    // XXXldb Do we want to extract the generic for this if it's not only a
    // generic?
    nsFont::GetGenericID(font->mFont.name, &generic);

    // If we aren't allowed to use document fonts, then we are only entitled
    // to use the user's default variable-width font and fixed-width font
    if (!useDocumentFonts) {
      // Extract the generic from the specified font family...
      nsAutoString genericName;
      if (!font->mFont.EnumerateFamilies(ExtractGeneric, &genericName)) {
        // The specified font had a generic family.
        font->mFont.name = genericName;
        nsFont::GetGenericID(genericName, &generic);

        // ... and only use it if it's -moz-fixed or monospace
        if (generic != kGenericFont_moz_fixed &&
            generic != kGenericFont_monospace) {
          font->mFont.name.Truncate();
          generic = kGenericFont_NONE;
        }
      } else {
        // The specified font did not have a generic family.
        font->mFont.name.Truncate();
        generic = kGenericFont_NONE;
      }
    }
  }

  // Now compute our font struct
  if (generic == kGenericFont_NONE) {
    // continue the normal processing
    nsRuleNode::SetFont(mPresContext, aContext, minimumFontSize, generic,
                        fontData, parentFont, font,
                        aStartStruct != nsnull, inherited);
  }
  else {
    // re-calculate the font as a generic font
    inherited = PR_TRUE;
    nsRuleNode::SetGenericFont(mPresContext, aContext, generic,
                               minimumFontSize, font);
  }

  COMPUTE_END_INHERITED(Font, font)
}

already_AddRefed<nsCSSShadowArray>
nsRuleNode::GetShadowData(nsCSSValueList* aList,
                          nsStyleContext* aContext,
                          PRBool aUsesSpread,
                          PRBool& inherited)
{
  PRUint32 arrayLength = 0;
  for (nsCSSValueList *list2 = aList; list2; list2 = list2->mNext)
    ++arrayLength;

  NS_ASSERTION(arrayLength > 0, "Non-null text-shadow list, yet we counted 0 items.");
  nsCSSShadowArray* shadowList = new(arrayLength) nsCSSShadowArray(arrayLength);

  if (!shadowList)
    return nsnull;

  nsStyleCoord tempCoord;
  PRBool unitOK;
  for (nsCSSShadowItem* item = shadowList->ShadowAt(0);
       aList;
       aList = aList->mNext, ++item) {
    nsCSSValue::Array *arr = aList->mValue.GetArrayValue();
    // OK to pass bad aParentCoord since we're not passing SETCOORD_INHERIT
    unitOK = SetCoord(arr->Item(0), tempCoord, nsStyleCoord(),
                      SETCOORD_LENGTH, aContext, mPresContext, inherited);
    NS_ASSERTION(unitOK, "unexpected unit");
    item->mXOffset = tempCoord.GetCoordValue();

    unitOK = SetCoord(arr->Item(1), tempCoord, nsStyleCoord(),
                      SETCOORD_LENGTH, aContext, mPresContext, inherited);
    NS_ASSERTION(unitOK, "unexpected unit");
    item->mYOffset = tempCoord.GetCoordValue();

    // Blur radius is optional in the current box-shadow spec
    if (arr->Item(2).GetUnit() != eCSSUnit_Null) {
      unitOK = SetCoord(arr->Item(2), tempCoord, nsStyleCoord(),
                        SETCOORD_LENGTH, aContext, mPresContext, inherited);
      NS_ASSERTION(unitOK, "unexpected unit");
      item->mRadius = tempCoord.GetCoordValue();
    } else {
      item->mRadius = 0;
    }

    // Find the spread radius
    if (aUsesSpread && arr->Item(3).GetUnit() != eCSSUnit_Null) {
      unitOK = SetCoord(arr->Item(3), tempCoord, nsStyleCoord(),
                        SETCOORD_LENGTH, aContext, mPresContext, inherited);
      NS_ASSERTION(unitOK, "unexpected unit");
      item->mSpread = tempCoord.GetCoordValue();
    } else {
      item->mSpread = 0;
    }

    if (arr->Item(4).GetUnit() != eCSSUnit_Null) {
      item->mHasColor = PR_TRUE;
      // 2nd argument can be bogus since inherit is not a valid color
      unitOK = SetColor(arr->Item(4), 0, mPresContext, aContext, item->mColor,
                        inherited);
      NS_ASSERTION(unitOK, "unexpected unit");
    }
  }

  NS_ADDREF(shadowList);
  return shadowList;
}

const void*
nsRuleNode::ComputeTextData(void* aStartStruct,
                            const nsRuleDataStruct& aData, 
                            nsStyleContext* aContext, 
                            nsRuleNode* aHighestNode,
                            const RuleDetail aRuleDetail, PRBool aInherited)
{
  COMPUTE_START_INHERITED(Text, (), text, parentText, Text, textData)

    // letter-spacing: normal, length, inherit
  SetCoord(textData.mLetterSpacing, text->mLetterSpacing, parentText->mLetterSpacing,
           SETCOORD_LH | SETCOORD_NORMAL | SETCOORD_INITIAL_NORMAL,
           aContext, mPresContext, inherited);

  // text-shadow: none, list, inherit, initial
  nsCSSValueList* list = textData.mTextShadow;
  if (list) {
    text->mTextShadow = nsnull;

    // Don't need to handle none/initial explicitly: The above assignment
    // takes care of that
    if (eCSSUnit_Inherit == list->mValue.GetUnit()) {
      inherited = PR_TRUE;
      text->mTextShadow = parentText->mTextShadow;
    } else if (eCSSUnit_Array == list->mValue.GetUnit()) {
      // List of arrays
      text->mTextShadow = GetShadowData(list, aContext, PR_FALSE, inherited);
    }
  }

  // line-height: normal, number, length, percent, inherit
  if (eCSSUnit_Percent == textData.mLineHeight.GetUnit()) {
    inherited = PR_TRUE;
    // Use |mFont.size| to pick up minimum font size.
    text->mLineHeight.SetCoordValue(
        nscoord(float(aContext->GetStyleFont()->mFont.size) *
                textData.mLineHeight.GetPercentValue()));
  }
  else if (eCSSUnit_Initial == textData.mLineHeight.GetUnit() ||
           eCSSUnit_System_Font == textData.mLineHeight.GetUnit()) {
    text->mLineHeight.SetNormalValue();
  }
  else {
    SetCoord(textData.mLineHeight, text->mLineHeight, parentText->mLineHeight,
             SETCOORD_LH | SETCOORD_FACTOR | SETCOORD_NORMAL,
             aContext, mPresContext, inherited);
    if (textData.mLineHeight.IsFixedLengthUnit() ||
        textData.mLineHeight.GetUnit() == eCSSUnit_Pixel) {
      nscoord lh = nsStyleFont::ZoomText(mPresContext,
                                         text->mLineHeight.GetCoordValue());
      nscoord minimumFontSize =
        mPresContext->GetCachedIntPref(kPresContext_MinimumFontSize);

      if (minimumFontSize > 0 && !mPresContext->IsChrome()) {
        // If we applied a minimum font size, scale the line height by
        // the same ratio.  (If we *might* have applied a minimum font
        // size, we can't cache in the rule tree.)
        inherited = PR_TRUE;
        const nsStyleFont *font = aContext->GetStyleFont();
        if (font->mSize != 0) {
          lh = nscoord(float(lh) * float(font->mFont.size) / float(font->mSize));
        } else {
          lh = minimumFontSize;
        }
      }
      text->mLineHeight.SetCoordValue(lh);
    }
  }


  // text-align: enum, string, inherit, initial
  if (eCSSUnit_String == textData.mTextAlign.GetUnit()) {
    NS_NOTYETIMPLEMENTED("align string");
  } else
    SetDiscrete(textData.mTextAlign, text->mTextAlign, inherited,
                SETDSC_ENUMERATED, parentText->mTextAlign,
                NS_STYLE_TEXT_ALIGN_DEFAULT,
                0, 0, 0, 0);

  // text-indent: length, percent, inherit, initial
  SetCoord(textData.mTextIndent, text->mTextIndent, parentText->mTextIndent,
           SETCOORD_LPH | SETCOORD_INITIAL_ZERO, aContext,
           mPresContext, inherited);

  // text-transform: enum, none, inherit, initial
  SetDiscrete(textData.mTextTransform, text->mTextTransform, inherited,
              SETDSC_ENUMERATED | SETDSC_NONE, parentText->mTextTransform,
              NS_STYLE_TEXT_TRANSFORM_NONE, 0,
              NS_STYLE_TEXT_TRANSFORM_NONE, 0, 0);

  // white-space: enum, normal, inherit, initial
  SetDiscrete(textData.mWhiteSpace, text->mWhiteSpace, inherited,
              SETDSC_ENUMERATED | SETDSC_NORMAL, parentText->mWhiteSpace,
              NS_STYLE_WHITESPACE_NORMAL, 0, 0,
              NS_STYLE_WHITESPACE_NORMAL, 0);
 
  // word-spacing: normal, length, inherit
  SetCoord(textData.mWordSpacing, text->mWordSpacing, parentText->mWordSpacing,
           SETCOORD_LH | SETCOORD_NORMAL | SETCOORD_INITIAL_NORMAL,
           aContext, mPresContext, inherited);

  // word-wrap: enum, normal, inherit, initial
  SetDiscrete(textData.mWordWrap, text->mWordWrap, inherited,
              SETDSC_ENUMERATED | SETDSC_NORMAL, parentText->mWordWrap,
              NS_STYLE_WORDWRAP_NORMAL, 0, 0,
              NS_STYLE_WORDWRAP_NORMAL, 0);

  COMPUTE_END_INHERITED(Text, text)
}

const void*
nsRuleNode::ComputeTextResetData(void* aStartStruct,
                                 const nsRuleDataStruct& aData, 
                                 nsStyleContext* aContext, 
                                 nsRuleNode* aHighestNode,
                                 const RuleDetail aRuleDetail, PRBool aInherited)
{
  COMPUTE_START_RESET(TextReset, (), text, parentText, Text, textData)
  
  // vertical-align: enum, length, percent, inherit
  if (!SetCoord(textData.mVerticalAlign, text->mVerticalAlign,
                parentText->mVerticalAlign, SETCOORD_LPH | SETCOORD_ENUMERATED,
                aContext, mPresContext, inherited)) {
    if (eCSSUnit_Initial == textData.mVerticalAlign.GetUnit()) {
      text->mVerticalAlign.SetIntValue(NS_STYLE_VERTICAL_ALIGN_BASELINE,
                                       eStyleUnit_Enumerated);
    }
  }

  // text-decoration: none, enum (bit field), inherit, initial
  if (eCSSUnit_Enumerated == textData.mDecoration.GetUnit()) {
    PRInt32 td = textData.mDecoration.GetIntValue();
    text->mTextDecoration = td;
    if (td & NS_STYLE_TEXT_DECORATION_PREF_ANCHORS) {
      PRBool underlineLinks =
        mPresContext->GetCachedBoolPref(kPresContext_UnderlineLinks);
      if (underlineLinks) {
        text->mTextDecoration |= NS_STYLE_TEXT_DECORATION_UNDERLINE;
      }
      else {
        text->mTextDecoration &= ~NS_STYLE_TEXT_DECORATION_UNDERLINE;
      }
    }
  }
  else
    SetDiscrete(textData.mDecoration, text->mTextDecoration, inherited,
                SETDSC_NONE,
                parentText->mTextDecoration,
                NS_STYLE_TEXT_DECORATION_NONE, 0,
                NS_STYLE_TEXT_DECORATION_NONE, 0, 0);

  // unicode-bidi: enum, normal, inherit, initial
  SetDiscrete(textData.mUnicodeBidi, text->mUnicodeBidi, inherited,
              SETDSC_ENUMERATED | SETDSC_NORMAL,
              parentText->mUnicodeBidi,
              NS_STYLE_UNICODE_BIDI_NORMAL, 0, 0,
              NS_STYLE_UNICODE_BIDI_NORMAL, 0);

  COMPUTE_END_RESET(TextReset, text)
}

const void*
nsRuleNode::ComputeUserInterfaceData(void* aStartStruct,
                                     const nsRuleDataStruct& aData, 
                                     nsStyleContext* aContext, 
                                     nsRuleNode* aHighestNode,
                                     const RuleDetail aRuleDetail,
                                     PRBool aInherited)
{
  COMPUTE_START_INHERITED(UserInterface, (), ui, parentUI,
                          UserInterface, uiData)

  // cursor: enum, auto, url, inherit
  nsCSSValueList*  list = uiData.mCursor;
  if (nsnull != list) {
    delete [] ui->mCursorArray;
    ui->mCursorArray = nsnull;
    ui->mCursorArrayLength = 0;

    if (eCSSUnit_Inherit == list->mValue.GetUnit()) {
      inherited = PR_TRUE;
      ui->mCursor = parentUI->mCursor;
      ui->CopyCursorArrayFrom(*parentUI);
    }
    else if (eCSSUnit_Initial == list->mValue.GetUnit()) {
      ui->mCursor = NS_STYLE_CURSOR_AUTO;
    }
    else {
      // The parser will never create a list that is *all* URL values --
      // that's invalid.
      PRUint32 arrayLength = 0;
      nsCSSValueList* list2 = list;
      for ( ; list->mValue.GetUnit() == eCSSUnit_Array; list = list->mNext)
        if (list->mValue.GetArrayValue()->Item(0).GetImageValue())
          ++arrayLength;

      if (arrayLength != 0) {
        ui->mCursorArray = new nsCursorImage[arrayLength];
        if (ui->mCursorArray) {
          ui->mCursorArrayLength = arrayLength;

          for (nsCursorImage *item = ui->mCursorArray;
               list2->mValue.GetUnit() == eCSSUnit_Array;
               list2 = list2->mNext) {
            nsCSSValue::Array *arr = list2->mValue.GetArrayValue();
            imgIRequest *req = arr->Item(0).GetImageValue();
            if (req) {
              item->mImage = req;
              if (arr->Item(1).GetUnit() != eCSSUnit_Null) {
                item->mHaveHotspot = PR_TRUE;
                item->mHotspotX = arr->Item(1).GetFloatValue(),
                item->mHotspotY = arr->Item(2).GetFloatValue();
              }
              ++item;
            }
          }
        }
      }

      NS_ASSERTION(list, "Must have non-array value at the end");
      NS_ASSERTION(list->mValue.GetUnit() == eCSSUnit_Enumerated ||
                   list->mValue.GetUnit() == eCSSUnit_Auto,
                   "Unexpected fallback value at end of cursor list");

      if (eCSSUnit_Enumerated == list->mValue.GetUnit()) {
        ui->mCursor = list->mValue.GetIntValue();
      }
      else if (eCSSUnit_Auto == list->mValue.GetUnit()) {
        ui->mCursor = NS_STYLE_CURSOR_AUTO;
      }
    }
  }

  // user-input: auto, none, enum, inherit, initial
  SetDiscrete(uiData.mUserInput, ui->mUserInput, inherited,
              SETDSC_ENUMERATED | SETDSC_NONE | SETDSC_AUTO,
              parentUI->mUserInput,
              NS_STYLE_USER_INPUT_AUTO,
              NS_STYLE_USER_INPUT_AUTO,
              NS_STYLE_USER_INPUT_NONE,
              0, 0);

  // user-modify: enum, inherit, initial
  SetDiscrete(uiData.mUserModify, ui->mUserModify, inherited,
              SETDSC_ENUMERATED,
              parentUI->mUserModify,
              NS_STYLE_USER_MODIFY_READ_ONLY, 
              0, 0, 0, 0);

  // user-focus: none, normal, enum, inherit, initial
  SetDiscrete(uiData.mUserFocus, ui->mUserFocus, inherited,
              SETDSC_ENUMERATED | SETDSC_NONE | SETDSC_NORMAL,
              parentUI->mUserFocus,
              NS_STYLE_USER_FOCUS_NONE,
              0,
              NS_STYLE_USER_FOCUS_NONE,
              NS_STYLE_USER_FOCUS_NORMAL,
              0);

  COMPUTE_END_INHERITED(UserInterface, ui)
}

const void*
nsRuleNode::ComputeUIResetData(void* aStartStruct,
                               const nsRuleDataStruct& aData, 
                               nsStyleContext* aContext, 
                               nsRuleNode* aHighestNode,
                               const RuleDetail aRuleDetail, PRBool aInherited)
{
  COMPUTE_START_RESET(UIReset, (), ui, parentUI, UserInterface, uiData)
  
  // user-select: auto, none, enum, inherit, initial
  SetDiscrete(uiData.mUserSelect, ui->mUserSelect, inherited,
              SETDSC_ENUMERATED | SETDSC_NONE | SETDSC_AUTO,
              parentUI->mUserSelect,
              NS_STYLE_USER_SELECT_AUTO,
              NS_STYLE_USER_SELECT_AUTO,
              NS_STYLE_USER_SELECT_NONE,
              0, 0);

  // ime-mode: auto, normal, enum, inherit, initial
  SetDiscrete(uiData.mIMEMode, ui->mIMEMode, inherited,
              SETDSC_ENUMERATED | SETDSC_NORMAL | SETDSC_AUTO,
              parentUI->mIMEMode,
              NS_STYLE_IME_MODE_AUTO,
              NS_STYLE_IME_MODE_AUTO,
              0,
              NS_STYLE_IME_MODE_NORMAL,
              0);

  // force-broken-image-icons: integer, inherit, initial
  SetDiscrete(uiData.mForceBrokenImageIcon, ui->mForceBrokenImageIcon,
              inherited,
              SETDSC_INTEGER,
              parentUI->mForceBrokenImageIcon,
              0, 0, 0, 0, 0);

  // -moz-window-shadow: enum, none, inherit, initial
  SetDiscrete(uiData.mWindowShadow, ui->mWindowShadow, inherited,
              SETDSC_ENUMERATED | SETDSC_NONE, parentUI->mWindowShadow,
              NS_STYLE_WINDOW_SHADOW_DEFAULT, 0,
              NS_STYLE_WINDOW_SHADOW_NONE, 0, 0);

  COMPUTE_END_RESET(UIReset, ui)
}

/* Given a -moz-transform token stream, accumulates them into an
 * nsStyleTransformMatrix
 *
 * @param aList The nsCSSValueList of arrays to read into transform functions.
 * @param aContext The style context to use for unit conversion.
 * @param aPresContext The presentation context to use for unit conversion
 * @param aInherited If the value is inherited, this is set to PR_TRUE.
 * @return An nsStyleTransformMatrix corresponding to the net transform.
 */
static nsStyleTransformMatrix ReadTransforms(const nsCSSValueList* aList,
                                             nsStyleContext* aContext,
                                             nsPresContext* aPresContext,
                                             PRBool &aInherited)
{
  nsStyleTransformMatrix result;

  for (const nsCSSValueList* curr = aList; curr != nsnull; curr = curr->mNext) {
    const nsCSSValue &currElem = curr->mValue;
    NS_ASSERTION(currElem.GetUnit() == eCSSUnit_Function,
                 "Stream should consist solely of functions!");
    NS_ASSERTION(currElem.GetArrayValue()->Count() >= 1,
                 "Incoming function is too short!");

    /* Read in a single transform matrix, then accumulate it with the total. */
    nsStyleTransformMatrix currMatrix;
    currMatrix.SetToTransformFunction(currElem.GetArrayValue(), aContext,
                                      aPresContext, aInherited);
    result *= currMatrix;
  }
  return result;
}

const void*
nsRuleNode::ComputeDisplayData(void* aStartStruct,
                               const nsRuleDataStruct& aData, 
                               nsStyleContext* aContext, 
                               nsRuleNode* aHighestNode,
                               const RuleDetail aRuleDetail, PRBool aInherited)
{
  COMPUTE_START_RESET(Display, (), display, parentDisplay,
                      Display, displayData)

  // opacity: factor, inherit, initial
  SetFactor(displayData.mOpacity, display->mOpacity, inherited,
            parentDisplay->mOpacity, 1.0f, SETFCT_OPACITY);

  // display: enum, none, inherit, initial
  SetDiscrete(displayData.mDisplay, display->mDisplay, inherited,
              SETDSC_ENUMERATED | SETDSC_NONE, parentDisplay->mDisplay,
              NS_STYLE_DISPLAY_INLINE, 0,
              NS_STYLE_DISPLAY_NONE, 0, 0);

  // appearance: enum, none, inherit, initial
  SetDiscrete(displayData.mAppearance, display->mAppearance, inherited,
              SETDSC_ENUMERATED | SETDSC_NONE, parentDisplay->mAppearance,
              NS_THEME_NONE, 0,
              NS_THEME_NONE, 0, 0);

  // binding: url, none, inherit
  if (eCSSUnit_URL == displayData.mBinding.GetUnit()) {
    nsCSSValue::URL* url = displayData.mBinding.GetURLStructValue();
    NS_ASSERTION(url, "What's going on here?");
    
    if (NS_LIKELY(url->mURI)) {
      display->mBinding = url;
    } else {
      display->mBinding = nsnull;
    }
  }
  else if (eCSSUnit_None == displayData.mBinding.GetUnit() ||
           eCSSUnit_Initial == displayData.mBinding.GetUnit()) {
    display->mBinding = nsnull;
  }
  else if (eCSSUnit_Inherit == displayData.mBinding.GetUnit()) {
    inherited = PR_TRUE;
    display->mBinding = parentDisplay->mBinding;
  }

  // position: enum, inherit, initial
  SetDiscrete(displayData.mPosition, display->mPosition, inherited,
              SETDSC_ENUMERATED, parentDisplay->mPosition,
              NS_STYLE_POSITION_STATIC, 0, 0, 0, 0);

  // clear: enum, none, inherit, initial
  SetDiscrete(displayData.mClear, display->mBreakType, inherited,
              SETDSC_ENUMERATED | SETDSC_NONE, parentDisplay->mBreakType,
              NS_STYLE_CLEAR_NONE, 0,
              NS_STYLE_CLEAR_NONE, 0, 0);

  // temp fix for bug 24000
  // Map 'auto' and 'avoid' to PR_FALSE, and 'always', 'left', and
  // 'right' to PR_TRUE.
  // "A conforming user agent may interpret the values 'left' and
  // 'right' as 'always'." - CSS2.1, section 13.3.1
  if (eCSSUnit_Enumerated == displayData.mBreakBefore.GetUnit()) {
    display->mBreakBefore = (NS_STYLE_PAGE_BREAK_AVOID != displayData.mBreakBefore.GetIntValue());
  }
  else if (eCSSUnit_Auto == displayData.mBreakBefore.GetUnit() ||
           eCSSUnit_Initial == displayData.mBreakBefore.GetUnit()) {
    display->mBreakBefore = PR_FALSE;
  }
  else if (eCSSUnit_Inherit == displayData.mBreakBefore.GetUnit()) {
    inherited = PR_TRUE;
    display->mBreakBefore = parentDisplay->mBreakBefore;
  }

  if (eCSSUnit_Enumerated == displayData.mBreakAfter.GetUnit()) {
    display->mBreakAfter = (NS_STYLE_PAGE_BREAK_AVOID != displayData.mBreakAfter.GetIntValue());
  }
  else if (eCSSUnit_Auto == displayData.mBreakAfter.GetUnit() ||
           eCSSUnit_Initial == displayData.mBreakAfter.GetUnit()) {
    display->mBreakAfter = PR_FALSE;
  }
  else if (eCSSUnit_Inherit == displayData.mBreakAfter.GetUnit()) {
    inherited = PR_TRUE;
    display->mBreakAfter = parentDisplay->mBreakAfter;
  }
  // end temp fix

  // float: enum, none, inherit, initial
  SetDiscrete(displayData.mFloat, display->mFloats, inherited,
              SETDSC_ENUMERATED | SETDSC_NONE, parentDisplay->mFloats,
              NS_STYLE_FLOAT_NONE, 0,
              NS_STYLE_FLOAT_NONE, 0, 0);

  // overflow-x: enum, auto, inherit, initial
  SetDiscrete(displayData.mOverflowX, display->mOverflowX, inherited,
              SETDSC_ENUMERATED | SETDSC_AUTO,
              parentDisplay->mOverflowX,
              NS_STYLE_OVERFLOW_VISIBLE,
              NS_STYLE_OVERFLOW_AUTO,
              0, 0, 0);

  // overflow-y: enum, auto, inherit, initial
  SetDiscrete(displayData.mOverflowY, display->mOverflowY, inherited,
              SETDSC_ENUMERATED | SETDSC_AUTO,
              parentDisplay->mOverflowY,
              NS_STYLE_OVERFLOW_VISIBLE,
              NS_STYLE_OVERFLOW_AUTO,
              0, 0, 0);

  // CSS3 overflow-x and overflow-y require some fixup as well in some
  // cases.  NS_STYLE_OVERFLOW_VISIBLE and NS_STYLE_OVERFLOW_CLIP are
  // meaningful only when used in both dimensions.
  if (display->mOverflowX != display->mOverflowY &&
      (display->mOverflowX == NS_STYLE_OVERFLOW_VISIBLE ||
       display->mOverflowX == NS_STYLE_OVERFLOW_CLIP ||
       display->mOverflowY == NS_STYLE_OVERFLOW_VISIBLE ||
       display->mOverflowY == NS_STYLE_OVERFLOW_CLIP)) {
    // We can't store in the rule tree since a more specific rule might
    // change these conditions.
    inherited = PR_TRUE;

    // NS_STYLE_OVERFLOW_CLIP is a deprecated value, so if it's specified
    // in only one dimension, convert it to NS_STYLE_OVERFLOW_HIDDEN.
    if (display->mOverflowX == NS_STYLE_OVERFLOW_CLIP)
      display->mOverflowX = NS_STYLE_OVERFLOW_HIDDEN;
    if (display->mOverflowY == NS_STYLE_OVERFLOW_CLIP)
      display->mOverflowY = NS_STYLE_OVERFLOW_HIDDEN;

    // If 'visible' is specified but doesn't match the other dimension, it
    // turns into 'auto'.
    if (display->mOverflowX == NS_STYLE_OVERFLOW_VISIBLE)
      display->mOverflowX = NS_STYLE_OVERFLOW_AUTO;
    if (display->mOverflowY == NS_STYLE_OVERFLOW_VISIBLE)
      display->mOverflowY = NS_STYLE_OVERFLOW_AUTO;
  }

  // clip property: length, auto, inherit
  if (eCSSUnit_Inherit == displayData.mClip.mTop.GetUnit()) { // if one is inherit, they all are
    inherited = PR_TRUE;
    display->mClipFlags = parentDisplay->mClipFlags;
    display->mClip = parentDisplay->mClip;
  }
  // if one is initial, they all are
  else if (eCSSUnit_Initial == displayData.mClip.mTop.GetUnit()) {
    display->mClipFlags = NS_STYLE_CLIP_AUTO;
    display->mClip.SetRect(0,0,0,0);
  }
  else {
    PRBool  fullAuto = PR_TRUE;

    display->mClipFlags = 0; // clear it

    if (eCSSUnit_Auto == displayData.mClip.mTop.GetUnit()) {
      display->mClip.y = 0;
      display->mClipFlags |= NS_STYLE_CLIP_TOP_AUTO;
    } 
    else if (displayData.mClip.mTop.IsLengthUnit()) {
      display->mClip.y = CalcLength(displayData.mClip.mTop, aContext, mPresContext, inherited);
      fullAuto = PR_FALSE;
    }
    if (eCSSUnit_Auto == displayData.mClip.mBottom.GetUnit()) {
      // Setting to NS_MAXSIZE for the 'auto' case ensures that
      // the clip rect is nonempty. It is important that mClip be
      // nonempty if the actual clip rect could be nonempty.
      display->mClip.height = NS_MAXSIZE;
      display->mClipFlags |= NS_STYLE_CLIP_BOTTOM_AUTO;
    } 
    else if (displayData.mClip.mBottom.IsLengthUnit()) {
      display->mClip.height = CalcLength(displayData.mClip.mBottom, aContext, mPresContext, inherited) -
                              display->mClip.y;
      fullAuto = PR_FALSE;
    }
    if (eCSSUnit_Auto == displayData.mClip.mLeft.GetUnit()) {
      display->mClip.x = 0;
      display->mClipFlags |= NS_STYLE_CLIP_LEFT_AUTO;
    } 
    else if (displayData.mClip.mLeft.IsLengthUnit()) {
      display->mClip.x = CalcLength(displayData.mClip.mLeft, aContext, mPresContext, inherited);
      fullAuto = PR_FALSE;
    }
    if (eCSSUnit_Auto == displayData.mClip.mRight.GetUnit()) {
      // Setting to NS_MAXSIZE for the 'auto' case ensures that
      // the clip rect is nonempty. It is important that mClip be
      // nonempty if the actual clip rect could be nonempty.
      display->mClip.width = NS_MAXSIZE;
      display->mClipFlags |= NS_STYLE_CLIP_RIGHT_AUTO;
    } 
    else if (displayData.mClip.mRight.IsLengthUnit()) {
      display->mClip.width = CalcLength(displayData.mClip.mRight, aContext, mPresContext, inherited) -
                             display->mClip.x;
      fullAuto = PR_FALSE;
    }
    display->mClipFlags &= ~NS_STYLE_CLIP_TYPE_MASK;
    if (fullAuto) {
      display->mClipFlags |= NS_STYLE_CLIP_AUTO;
    }
    else {
      display->mClipFlags |= NS_STYLE_CLIP_RECT;
    }
  }

  if (display->mDisplay != NS_STYLE_DISPLAY_NONE) {
    // CSS2 9.7 specifies display type corrections dealing with 'float'
    // and 'position'.  Since generated content can't be floated or
    // positioned, we can deal with it here.

    if (nsCSSPseudoElements::firstLetter == aContext->GetPseudoType()) {
      // a non-floating first-letter must be inline
      // XXX this fix can go away once bug 103189 is fixed correctly
      display->mDisplay = NS_STYLE_DISPLAY_INLINE;

      // We can't cache the data in the rule tree since if a more specific
      // rule has 'float: left' we'll end up with the wrong 'display'
      // property.
      inherited = PR_TRUE;
    }

    if (display->IsAbsolutelyPositioned()) {
      // 1) if position is 'absolute' or 'fixed' then display must be
      // block-level and float must be 'none'

      // Backup original display value for calculation of a hypothetical
      // box (CSS2 10.6.4/10.6.5).
      // See nsHTMLReflowState::CalculateHypotheticalBox
      display->mOriginalDisplay = display->mDisplay;
      EnsureBlockDisplay(display->mDisplay);
      display->mFloats = NS_STYLE_FLOAT_NONE;

      // We can't cache the data in the rule tree since if a more specific
      // rule has 'position: static' we'll end up with problems with the
      // 'display' and 'float' properties.
      inherited = PR_TRUE;
    } else if (display->mFloats != NS_STYLE_FLOAT_NONE) {
      // 2) if float is not none, and display is not none, then we must
      // set a block-level 'display' type per CSS2.1 section 9.7.

      EnsureBlockDisplay(display->mDisplay);

      // We can't cache the data in the rule tree since if a more specific
      // rule has 'float: none' we'll end up with the wrong 'display'
      // property.
      inherited = PR_TRUE;
    }

  }
  
  /* Convert the nsCSSValueList into an nsTArray<nsTransformFunction *>. */
  const nsCSSValueList *head = displayData.mTransform;
  
  if (head != nsnull) {
    /* There is a chance that we will discover that
     * the transform property has been set to 'none,' 'initial,' or 'inherit.'
     * If so, process appropriately.
     */
    
    /* If it's 'none,' indicate that there are no transforms. */
    if (head->mValue.GetUnit() == eCSSUnit_None)
      display->mTransformPresent = PR_FALSE;
    
    /* If we need to inherit, do so by making a full deep-copy. */
    else if (head->mValue.GetUnit() == eCSSUnit_Inherit)  {
      display->mTransformPresent = parentDisplay->mTransformPresent;
      if (parentDisplay->mTransformPresent)
        display->mTransform = parentDisplay->mTransform;
      inherited = PR_TRUE;
    }
    /* If it's 'initial', then we reset to empty. */
    else if (head->mValue.GetUnit() == eCSSUnit_Initial)
      display->mTransformPresent = PR_FALSE;
    
    /* Otherwise, we are looking at a list of CSS tokens.  We'll read each of
     * them in as an array of nsTransformFunction objects, then will accumulate
     * them all together to form the final transform matrix.
     */
    else {
 
      display->mTransform = 
        ReadTransforms(head, aContext, mPresContext, inherited);

      /* Make sure to say that this data is valid! */
      display->mTransformPresent = PR_TRUE;
    }
  }
  
  /* Convert -moz-transform-origin. */
  if (displayData.mTransformOrigin.mXValue.GetUnit() != eCSSUnit_Null ||
      displayData.mTransformOrigin.mXValue.GetUnit() != eCSSUnit_Null) {

    /* If X coordinate is an enumerated type, handle it explicitly. */
    if (eCSSUnit_Enumerated == displayData.mTransformOrigin.mXValue.GetUnit())
      display->mTransformOrigin[0].SetPercentValue
        (GetFloatFromBoxPosition
         (displayData.mTransformOrigin.mXValue.GetIntValue()));
    else {
      /* Convert lengths, percents, and inherit.  Default value is 50%. */
#ifdef DEBUG
      PRBool result =
#endif
        SetCoord(displayData.mTransformOrigin.mXValue,
                 display->mTransformOrigin[0],
                 parentDisplay->mTransformOrigin[0],
                 SETCOORD_LPH | SETCOORD_INITIAL_HALF,
                 aContext, mPresContext, aInherited);
      NS_ASSERTION(result, "Malformed -moz-transform-origin parse!");
    }

    /* If Y coordinate is an enumerated type, handle it explicitly. */
    if (eCSSUnit_Enumerated == displayData.mTransformOrigin.mYValue.GetUnit())
      display->mTransformOrigin[1].SetPercentValue
        (GetFloatFromBoxPosition
         (displayData.mTransformOrigin.mYValue.GetIntValue()));
    else {
      /* Convert lengths, percents, initial, inherit. */
#ifdef DEBUG
      PRBool result =
#endif
        SetCoord(displayData.mTransformOrigin.mYValue,
                 display->mTransformOrigin[1],
                 parentDisplay->mTransformOrigin[1],
                 SETCOORD_LPH | SETCOORD_INITIAL_HALF,
                 aContext, mPresContext, aInherited);
      NS_ASSERTION(result, "Malformed -moz-transform-origin parse!");
    }
  }

  COMPUTE_END_RESET(Display, display)
}

const void*
nsRuleNode::ComputeVisibilityData(void* aStartStruct,
                                  const nsRuleDataStruct& aData, 
                                  nsStyleContext* aContext, 
                                  nsRuleNode* aHighestNode,
                                  const RuleDetail aRuleDetail, PRBool aInherited)
{
  COMPUTE_START_INHERITED(Visibility, (mPresContext),
                          visibility, parentVisibility,
                          Display, displayData)

  // direction: enum, inherit, initial
  SetDiscrete(displayData.mDirection, visibility->mDirection, inherited,
              SETDSC_ENUMERATED, parentVisibility->mDirection,
              (GET_BIDI_OPTION_DIRECTION(mPresContext->GetBidi())
               == IBMBIDI_TEXTDIRECTION_RTL)
              ? NS_STYLE_DIRECTION_RTL : NS_STYLE_DIRECTION_LTR,
              0, 0, 0, 0);

  // visibility: enum, inherit, initial
  SetDiscrete(displayData.mVisibility, visibility->mVisible, inherited,
              SETDSC_ENUMERATED, parentVisibility->mVisible,
              NS_STYLE_VISIBILITY_VISIBLE, 0, 0, 0, 0);

  // lang: string, inherit
  // this is not a real CSS property, it is a html attribute mapped to CSS struture
  if (eCSSUnit_String == displayData.mLang.GetUnit()) {
    if (!gLangService) {
      CallGetService(NS_LANGUAGEATOMSERVICE_CONTRACTID, &gLangService);
    }

    if (gLangService) {
      nsAutoString lang;
      displayData.mLang.GetStringValue(lang);
      visibility->mLangGroup = gLangService->LookupLanguage(lang);
    }
  } 

  COMPUTE_END_INHERITED(Visibility, visibility)
}

const void*
nsRuleNode::ComputeColorData(void* aStartStruct,
                             const nsRuleDataStruct& aData, 
                             nsStyleContext* aContext, 
                             nsRuleNode* aHighestNode,
                             const RuleDetail aRuleDetail, PRBool aInherited)
{
  COMPUTE_START_INHERITED(Color, (mPresContext), color, parentColor,
                          Color, colorData)

  // color: color, string, inherit
  // Special case for currentColor.  According to CSS3, setting color to 'currentColor'
  // should behave as if it is inherited
  if (colorData.mColor.GetUnit() == eCSSUnit_EnumColor && 
      colorData.mColor.GetIntValue() == NS_COLOR_CURRENTCOLOR) {
    color->mColor = parentColor->mColor;
    inherited = PR_TRUE;
  }
  else if (colorData.mColor.GetUnit() == eCSSUnit_Initial) {
    color->mColor = mPresContext->DefaultColor();
  }
  else {
    SetColor(colorData.mColor, parentColor->mColor, mPresContext, aContext, color->mColor, 
             inherited);
  }

  COMPUTE_END_INHERITED(Color, color)
}

const void*
nsRuleNode::ComputeBackgroundData(void* aStartStruct,
                                  const nsRuleDataStruct& aData, 
                                  nsStyleContext* aContext, 
                                  nsRuleNode* aHighestNode,
                                  const RuleDetail aRuleDetail,
                                  PRBool aInherited)
{
  COMPUTE_START_RESET(Background, (), bg, parentBG, Color, colorData)

  // save parentFlags in case bg == parentBG and we clobber them later
  PRUint8 parentFlags = parentBG->mBackgroundFlags;

  // background-color: color, string, inherit
  if (eCSSUnit_Initial == colorData.mBackColor.GetUnit()) {
    bg->mBackgroundColor = NS_RGBA(0, 0, 0, 0);
  } else if (!SetColor(colorData.mBackColor, parentBG->mBackgroundColor,
                       mPresContext, aContext, bg->mBackgroundColor,
                       inherited)) {
    NS_ASSERTION(eCSSUnit_Null == colorData.mBackColor.GetUnit(),
                 "unexpected color unit");
  }

  // background-image: url (stored as image), none, inherit
  if (eCSSUnit_Image == colorData.mBackImage.GetUnit()) {
    bg->mBackgroundImage = colorData.mBackImage.GetImageValue();
  }
  else if (eCSSUnit_None == colorData.mBackImage.GetUnit() ||
           eCSSUnit_Initial == colorData.mBackImage.GetUnit()) {
    bg->mBackgroundImage = nsnull;
  }
  else if (eCSSUnit_Inherit == colorData.mBackImage.GetUnit()) {
    inherited = PR_TRUE;
    bg->mBackgroundImage = parentBG->mBackgroundImage;
  }

  if (bg->mBackgroundImage) {
    bg->mBackgroundFlags &= ~NS_STYLE_BG_IMAGE_NONE;
  } else {
    bg->mBackgroundFlags |= NS_STYLE_BG_IMAGE_NONE;
  }

  // background-repeat: enum, inherit, initial
  SetDiscrete(colorData.mBackRepeat, bg->mBackgroundRepeat, inherited,
              SETDSC_ENUMERATED, parentBG->mBackgroundRepeat,
              NS_STYLE_BG_REPEAT_XY, 0, 0, 0, 0);

  // background-attachment: enum, inherit, initial
  SetDiscrete(colorData.mBackAttachment, bg->mBackgroundAttachment, inherited,
              SETDSC_ENUMERATED, parentBG->mBackgroundAttachment,
              NS_STYLE_BG_ATTACHMENT_SCROLL, 0, 0, 0, 0);

  // background-clip: enum, inherit, initial
  SetDiscrete(colorData.mBackClip, bg->mBackgroundClip, inherited,
              SETDSC_ENUMERATED, parentBG->mBackgroundClip,
              NS_STYLE_BG_CLIP_BORDER, 0, 0, 0, 0);

  // background-inline-policy: enum, inherit, initial
  SetDiscrete(colorData.mBackInlinePolicy, bg->mBackgroundInlinePolicy,
              inherited, SETDSC_ENUMERATED,
              parentBG->mBackgroundInlinePolicy,
              NS_STYLE_BG_INLINE_POLICY_CONTINUOUS, 0, 0, 0, 0);

  // background-origin: enum, inherit, initial
  SetDiscrete(colorData.mBackOrigin, bg->mBackgroundOrigin, inherited,
              SETDSC_ENUMERATED, parentBG->mBackgroundOrigin,
              NS_STYLE_BG_ORIGIN_PADDING, 0, 0, 0, 0);

  // background-position: enum, length, percent (flags), inherit
  if (eCSSUnit_Percent == colorData.mBackPosition.mXValue.GetUnit()) {
    bg->mBackgroundXPosition.mFloat = colorData.mBackPosition.mXValue.GetPercentValue();
    bg->mBackgroundFlags |= NS_STYLE_BG_X_POSITION_PERCENT;
    bg->mBackgroundFlags &= ~NS_STYLE_BG_X_POSITION_LENGTH;
  }
  else if (colorData.mBackPosition.mXValue.IsLengthUnit()) {
    bg->mBackgroundXPosition.mCoord = CalcLength(colorData.mBackPosition.mXValue, 
                                                 aContext, mPresContext, inherited);
    bg->mBackgroundFlags |= NS_STYLE_BG_X_POSITION_LENGTH;
    bg->mBackgroundFlags &= ~NS_STYLE_BG_X_POSITION_PERCENT;
  }
  else if (eCSSUnit_Enumerated == colorData.mBackPosition.mXValue.GetUnit()) {
    bg->mBackgroundXPosition.mFloat =
      GetFloatFromBoxPosition(colorData.mBackPosition.mXValue.GetIntValue());

    bg->mBackgroundFlags |= NS_STYLE_BG_X_POSITION_PERCENT;
    bg->mBackgroundFlags &= ~NS_STYLE_BG_X_POSITION_LENGTH;
  }
  else if (eCSSUnit_Inherit == colorData.mBackPosition.mXValue.GetUnit()) {
    inherited = PR_TRUE;
    bg->mBackgroundXPosition = parentBG->mBackgroundXPosition;
    bg->mBackgroundFlags &= ~(NS_STYLE_BG_X_POSITION_LENGTH | NS_STYLE_BG_X_POSITION_PERCENT);
    bg->mBackgroundFlags |= (parentFlags & (NS_STYLE_BG_X_POSITION_LENGTH | NS_STYLE_BG_X_POSITION_PERCENT));
  }
  else if (eCSSUnit_Initial == colorData.mBackPosition.mXValue.GetUnit()) {
    bg->mBackgroundFlags &= ~(NS_STYLE_BG_X_POSITION_LENGTH | NS_STYLE_BG_X_POSITION_PERCENT);
  }

  if (eCSSUnit_Percent == colorData.mBackPosition.mYValue.GetUnit()) {
    bg->mBackgroundYPosition.mFloat = colorData.mBackPosition.mYValue.GetPercentValue();
    bg->mBackgroundFlags |= NS_STYLE_BG_Y_POSITION_PERCENT;
    bg->mBackgroundFlags &= ~NS_STYLE_BG_Y_POSITION_LENGTH;
  }
  else if (colorData.mBackPosition.mYValue.IsLengthUnit()) {
    bg->mBackgroundYPosition.mCoord = CalcLength(colorData.mBackPosition.mYValue,
                                                 aContext, mPresContext, inherited);
    bg->mBackgroundFlags |= NS_STYLE_BG_Y_POSITION_LENGTH;
    bg->mBackgroundFlags &= ~NS_STYLE_BG_Y_POSITION_PERCENT;
  }
  else if (eCSSUnit_Enumerated == colorData.mBackPosition.mYValue.GetUnit()) {
    bg->mBackgroundYPosition.mFloat =
      GetFloatFromBoxPosition(colorData.mBackPosition.mYValue.GetIntValue());

    bg->mBackgroundFlags |= NS_STYLE_BG_Y_POSITION_PERCENT;
    bg->mBackgroundFlags &= ~NS_STYLE_BG_Y_POSITION_LENGTH;
  }
  else if (eCSSUnit_Inherit == colorData.mBackPosition.mYValue.GetUnit()) {
    inherited = PR_TRUE;
    bg->mBackgroundYPosition = parentBG->mBackgroundYPosition;
    bg->mBackgroundFlags &= ~(NS_STYLE_BG_Y_POSITION_LENGTH | NS_STYLE_BG_Y_POSITION_PERCENT);
    bg->mBackgroundFlags |= (parentFlags & (NS_STYLE_BG_Y_POSITION_LENGTH | NS_STYLE_BG_Y_POSITION_PERCENT));
  }
  else if (eCSSUnit_Initial == colorData.mBackPosition.mYValue.GetUnit()) {
    bg->mBackgroundFlags &= ~(NS_STYLE_BG_Y_POSITION_LENGTH | NS_STYLE_BG_Y_POSITION_PERCENT);
  }

  COMPUTE_END_RESET(Background, bg)
}

const void*
nsRuleNode::ComputeMarginData(void* aStartStruct,
                              const nsRuleDataStruct& aData, 
                              nsStyleContext* aContext, 
                              nsRuleNode* aHighestNode,
                              const RuleDetail aRuleDetail, PRBool aInherited)
{
  COMPUTE_START_RESET(Margin, (), margin, parentMargin, Margin, marginData)

  // margin: length, percent, auto, inherit
  nsStyleCoord  coord;
  nsCSSRect ourMargin(marginData.mMargin);
  AdjustLogicalBoxProp(aContext,
                       marginData.mMarginLeftLTRSource,
                       marginData.mMarginLeftRTLSource,
                       marginData.mMarginStart, marginData.mMarginEnd,
                       NS_SIDE_LEFT, ourMargin, inherited);
  AdjustLogicalBoxProp(aContext,
                       marginData.mMarginRightLTRSource,
                       marginData.mMarginRightRTLSource,
                       marginData.mMarginEnd, marginData.mMarginStart,
                       NS_SIDE_RIGHT, ourMargin, inherited);
  NS_FOR_CSS_SIDES(side) {
    nsStyleCoord parentCoord = parentMargin->mMargin.Get(side);
    if (SetCoord(ourMargin.*(nsCSSRect::sides[side]),
                 coord, parentCoord, SETCOORD_LPAH | SETCOORD_INITIAL_ZERO,
                 aContext, mPresContext, inherited)) {
      margin->mMargin.Set(side, coord);
    }
  }

  margin->RecalcData();
  COMPUTE_END_RESET(Margin, margin)
}

const void* 
nsRuleNode::ComputeBorderData(void* aStartStruct,
                              const nsRuleDataStruct& aData, 
                              nsStyleContext* aContext, 
                              nsRuleNode* aHighestNode,
                              const RuleDetail aRuleDetail, PRBool aInherited)
{
  COMPUTE_START_RESET(Border, (mPresContext), border, parentBorder,
                      Margin, marginData)

  // -moz-box-shadow: none, list, inherit, initial
  {
    nsCSSValueList* list = marginData.mBoxShadow;
    if (list) {
      // This handles 'none' and 'initial'
      border->mBoxShadow = nsnull;

      if (eCSSUnit_Inherit == list->mValue.GetUnit()) {
        inherited = PR_TRUE;
        border->mBoxShadow = parentBorder->mBoxShadow;
      } else if (eCSSUnit_Array == list->mValue.GetUnit()) {
        // List of arrays
        border->mBoxShadow = GetShadowData(list, aContext, PR_TRUE, inherited);
      }
    }
  }

  // border-width, border-*-width: length, enum, inherit
  nsStyleCoord  coord;
  nsCSSRect ourBorderWidth(marginData.mBorderWidth);
  AdjustLogicalBoxProp(aContext,
                       marginData.mBorderLeftWidthLTRSource,
                       marginData.mBorderLeftWidthRTLSource,
                       marginData.mBorderStartWidth,
                       marginData.mBorderEndWidth,
                       NS_SIDE_LEFT, ourBorderWidth, inherited);
  AdjustLogicalBoxProp(aContext,
                       marginData.mBorderRightWidthLTRSource,
                       marginData.mBorderRightWidthRTLSource,
                       marginData.mBorderEndWidth,
                       marginData.mBorderStartWidth,
                       NS_SIDE_RIGHT, ourBorderWidth, inherited);
  { // scope for compilers with broken |for| loop scoping
    NS_FOR_CSS_SIDES(side) {
      const nsCSSValue &value = ourBorderWidth.*(nsCSSRect::sides[side]);
      NS_ASSERTION(eCSSUnit_Percent != value.GetUnit(),
                   "Percentage borders not implemented yet "
                   "If implementing, make sure to fix all consumers of "
                   "nsStyleBorder, the IsPercentageAwareChild method, "
                   "the nsAbsoluteContainingBlock::FrameDependsOnContainer "
                   "method, the "
                   "nsLineLayout::IsPercentageAwareReplacedElement method "
                   "and probably some other places");
      if (eCSSUnit_Enumerated == value.GetUnit()) {
        NS_ASSERTION(value.GetIntValue() == NS_STYLE_BORDER_WIDTH_THIN ||
                     value.GetIntValue() == NS_STYLE_BORDER_WIDTH_MEDIUM ||
                     value.GetIntValue() == NS_STYLE_BORDER_WIDTH_THICK,
                     "Unexpected enum value");
        border->SetBorderWidth(side,
                               (mPresContext->GetBorderWidthTable())[value.GetIntValue()]);
      }
      // OK to pass bad aParentCoord since we're not passing SETCOORD_INHERIT
      else if (SetCoord(value, coord, nsStyleCoord(), SETCOORD_LENGTH,
                        aContext, mPresContext, inherited)) {
        NS_ASSERTION(coord.GetUnit() == eStyleUnit_Coord, "unexpected unit");
        border->SetBorderWidth(side, coord.GetCoordValue());
      }
      else if (eCSSUnit_Inherit == value.GetUnit()) {
        inherited = PR_TRUE;
        border->SetBorderWidth(side,
                               parentBorder->GetComputedBorder().side(side));
      }
      else if (eCSSUnit_Initial == value.GetUnit()) {
        border->SetBorderWidth(side,
          (mPresContext->GetBorderWidthTable())[NS_STYLE_BORDER_WIDTH_MEDIUM]);
      }
      else {
        NS_ASSERTION(eCSSUnit_Null == value.GetUnit(),
                     "missing case handling border width");
      }
    }
  }

  // border-style, border-*-style: enum, none, inherit
  nsCSSRect ourStyle(marginData.mBorderStyle);
  AdjustLogicalBoxProp(aContext,
                       marginData.mBorderLeftStyleLTRSource,
                       marginData.mBorderLeftStyleRTLSource,
                       marginData.mBorderStartStyle, marginData.mBorderEndStyle,
                       NS_SIDE_LEFT, ourStyle, inherited);
  AdjustLogicalBoxProp(aContext,
                       marginData.mBorderRightStyleLTRSource,
                       marginData.mBorderRightStyleRTLSource,
                       marginData.mBorderEndStyle, marginData.mBorderStartStyle,
                       NS_SIDE_RIGHT, ourStyle, inherited);
  { // scope for compilers with broken |for| loop scoping
    NS_FOR_CSS_SIDES(side) {
      const nsCSSValue &value = ourStyle.*(nsCSSRect::sides[side]);
      nsCSSUnit unit = value.GetUnit();
      if (eCSSUnit_Enumerated == unit) {
        border->SetBorderStyle(side, value.GetIntValue());
      }
      else if (eCSSUnit_None == unit || eCSSUnit_Initial == unit) {
        border->SetBorderStyle(side, NS_STYLE_BORDER_STYLE_NONE);
      }
      else if (eCSSUnit_Inherit == unit) {
        inherited = PR_TRUE;
        border->SetBorderStyle(side, parentBorder->GetBorderStyle(side));
      }
    }
  }

  // -moz-border-*-colors: color, string, enum, none, inherit/initial
  nscolor borderColor;
  nscolor unused = NS_RGB(0,0,0);
  
  { // scope for compilers with broken |for| loop scoping
    NS_FOR_CSS_SIDES(side) {
      nsCSSValueList* list =
          marginData.mBorderColors.*(nsCSSValueListRect::sides[side]);
      if (list) {
        if (eCSSUnit_Initial == list->mValue.GetUnit() ||
            eCSSUnit_None == list->mValue.GetUnit()) {
          NS_ASSERTION(!list->mNext, "should have only one item");
          border->ClearBorderColors(side);
        }
        else if (eCSSUnit_Inherit == list->mValue.GetUnit()) {
          NS_ASSERTION(!list->mNext, "should have only one item");
          nsBorderColors *parentColors;
          parentBorder->GetCompositeColors(side, &parentColors);
          if (parentColors) {
            border->EnsureBorderColors();
            border->ClearBorderColors(side);
            border->mBorderColors[side] = parentColors->Clone();
          } else {
            border->ClearBorderColors(side);
          }
        }
        else {
          // Some composite border color information has been specified for this
          // border side.
          border->EnsureBorderColors();
          border->ClearBorderColors(side);
          while (list) {
            if (SetColor(list->mValue, unused, mPresContext,
                         aContext, borderColor, inherited))
              border->AppendBorderColor(side, borderColor);
            else {
              NS_NOTREACHED("unexpected item in -moz-border-*-colors list");
            }
            list = list->mNext;
          }
        }
      }
    }
  }

  // border-color, border-*-color: color, string, enum, inherit
  nsCSSRect ourBorderColor(marginData.mBorderColor);
  PRBool foreground;
  AdjustLogicalBoxProp(aContext,
                       marginData.mBorderLeftColorLTRSource,
                       marginData.mBorderLeftColorRTLSource,
                       marginData.mBorderStartColor, marginData.mBorderEndColor,
                       NS_SIDE_LEFT, ourBorderColor, inherited);
  AdjustLogicalBoxProp(aContext,
                       marginData.mBorderRightColorLTRSource,
                       marginData.mBorderRightColorRTLSource,
                       marginData.mBorderEndColor, marginData.mBorderStartColor,
                       NS_SIDE_RIGHT, ourBorderColor, inherited);
  { // scope for compilers with broken |for| loop scoping
    NS_FOR_CSS_SIDES(side) {
      const nsCSSValue &value = ourBorderColor.*(nsCSSRect::sides[side]);
      if (eCSSUnit_Inherit == value.GetUnit()) {
        if (parentContext) {
          inherited = PR_TRUE;
          parentBorder->GetBorderColor(side, borderColor, foreground);
          if (foreground) {
            // We want to inherit the color from the parent, not use the
            // color on the element where this chunk of style data will be
            // used.  We can ensure that the data for the parent are fully
            // computed (unlike for the element where this will be used, for
            // which the color could be specified on a more specific rule).
            border->SetBorderColor(side, parentContext->GetStyleColor()->mColor);
          } else
            border->SetBorderColor(side, borderColor);
        } else {
          // We're the root
          border->SetBorderToForeground(side);
        }
      }
      else if (SetColor(value, unused, mPresContext, aContext, borderColor, inherited)) {
        border->SetBorderColor(side, borderColor);
      }
      else if (eCSSUnit_Enumerated == value.GetUnit()) {
        switch (value.GetIntValue()) {
          case NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR:
            border->SetBorderToForeground(side);
            break;
        }
      }
      else if (eCSSUnit_Initial == value.GetUnit()) {
        border->SetBorderToForeground(side);
      }
    }
  }

  // -moz-border-radius: length, percent, inherit
  {
    const nsCSSCornerSizes& borderRadius = marginData.mBorderRadius;
    NS_FOR_CSS_HALF_CORNERS(corner) {
      nsStyleCoord parentCoord = parentBorder->mBorderRadius.Get(corner);
      if (SetCoord(borderRadius.GetHalfCorner(corner),
                   coord, parentCoord, SETCOORD_LPH | SETCOORD_INITIAL_ZERO,
                   aContext, mPresContext, inherited))
        border->mBorderRadius.Set(corner, coord);
    }
  }

  // float-edge: enum, inherit, initial
  SetDiscrete(marginData.mFloatEdge, border->mFloatEdge, inherited,
              SETDSC_ENUMERATED, parentBorder->mFloatEdge,
              NS_STYLE_FLOAT_EDGE_CONTENT, 0, 0, 0, 0);
  
  // border-image
  if (eCSSUnit_Array == marginData.mBorderImage.GetUnit()) {
    nsCSSValue::Array *arr = marginData.mBorderImage.GetArrayValue();
    
    // the image
    if (eCSSUnit_Image == arr->Item(0).GetUnit()) {
      border->SetBorderImage(arr->Item(0).GetImageValue());
    }
    
    // the numbers saying where to split the image
    NS_FOR_CSS_SIDES(side) {
      // an uninitialized parentCoord is ok because I'm not passing SETCOORD_INHERIT
      if (SetCoord(arr->Item(1 + side), coord, nsStyleCoord(),
                   SETCOORD_FACTOR | SETCOORD_PERCENT, aContext,
                   mPresContext, inherited)) {
        border->mBorderImageSplit.Set(side, coord);
      }
    }
    
    // possible replacement for border-width
    // if have one - have all four (see CSSParserImpl::ParseBorderImage())
    if (eCSSUnit_Null != arr->Item(5).GetUnit()) {
      NS_FOR_CSS_SIDES(side) {
        // an uninitialized parentCoord is ok because I'm not passing SETCOORD_INHERIT
        if (!SetCoord(arr->Item(5 + side), coord, nsStyleCoord(),
                      SETCOORD_LENGTH, aContext, mPresContext, inherited)) {
          NS_NOTREACHED("SetCoord for border-width replacement from border-image failed");
        }
        if (coord.GetUnit() == eStyleUnit_Coord) {
          border->SetBorderImageWidthOverride(side, coord.GetCoordValue());
        } else {
          NS_WARNING("a border-width replacement from border-image "
                     "has a unit that's not eStyleUnit_Coord");
          border->SetBorderImageWidthOverride(side, 0);
        }
      }
      border->mHaveBorderImageWidth = PR_TRUE;
    } else {
      border->mHaveBorderImageWidth = PR_FALSE;
    }
    
    // stretch/round/repeat keywords
    if (eCSSUnit_Null == arr->Item(9).GetUnit()) {
      // default, both horizontal and vertical are stretch
      border->mBorderImageHFill = NS_STYLE_BORDER_IMAGE_STRETCH;
      border->mBorderImageVFill = NS_STYLE_BORDER_IMAGE_STRETCH;
    } else {
      // have horizontal value
      border->mBorderImageHFill = arr->Item(9).GetIntValue();
      if (eCSSUnit_Null == arr->Item(10).GetUnit()) {
        // vertical same as horizontal
        border->mBorderImageVFill = border->mBorderImageHFill;
      } else {
        // have vertical value
        border->mBorderImageVFill = arr->Item(10).GetIntValue();
      }
    }
  } else if (eCSSUnit_None == marginData.mBorderImage.GetUnit() ||
             eCSSUnit_Initial == marginData.mBorderImage.GetUnit()) {
    border->mHaveBorderImageWidth = PR_FALSE;
    border->SetBorderImage(nsnull);
  } else if (eCSSUnit_Inherit == marginData.mBorderImage.GetUnit()) {
    NS_FOR_CSS_SIDES(side) {
      border->SetBorderImageWidthOverride(side, parentBorder->mBorderImageWidth.side(side));
    }
    border->mBorderImageSplit = parentBorder->mBorderImageSplit;
    border->mBorderImageHFill = parentBorder->mBorderImageHFill;
    border->mBorderImageVFill = parentBorder->mBorderImageVFill;
    border->mHaveBorderImageWidth = parentBorder->mHaveBorderImageWidth;
    border->SetBorderImage(parentBorder->GetBorderImage());
  }

  COMPUTE_END_RESET(Border, border)
}
  
const void*
nsRuleNode::ComputePaddingData(void* aStartStruct,
                               const nsRuleDataStruct& aData, 
                               nsStyleContext* aContext, 
                               nsRuleNode* aHighestNode,
                               const RuleDetail aRuleDetail, PRBool aInherited)
{
  COMPUTE_START_RESET(Padding, (), padding, parentPadding, Margin, marginData)

  // padding: length, percent, inherit
  nsStyleCoord  coord;
  nsCSSRect ourPadding(marginData.mPadding);
  AdjustLogicalBoxProp(aContext,
                       marginData.mPaddingLeftLTRSource,
                       marginData.mPaddingLeftRTLSource,
                       marginData.mPaddingStart, marginData.mPaddingEnd,
                       NS_SIDE_LEFT, ourPadding, inherited);
  AdjustLogicalBoxProp(aContext,
                       marginData.mPaddingRightLTRSource,
                       marginData.mPaddingRightRTLSource,
                       marginData.mPaddingEnd, marginData.mPaddingStart,
                       NS_SIDE_RIGHT, ourPadding, inherited);
  NS_FOR_CSS_SIDES(side) {
    nsStyleCoord parentCoord = parentPadding->mPadding.Get(side);
    if (SetCoord(ourPadding.*(nsCSSRect::sides[side]),
                 coord, parentCoord, SETCOORD_LPH | SETCOORD_INITIAL_ZERO,
                 aContext, mPresContext, inherited)) {
      padding->mPadding.Set(side, coord);
    }
  }

  padding->RecalcData();
  COMPUTE_END_RESET(Padding, padding)
}

const void*
nsRuleNode::ComputeOutlineData(void* aStartStruct,
                               const nsRuleDataStruct& aData, 
                               nsStyleContext* aContext, 
                               nsRuleNode* aHighestNode,
                               const RuleDetail aRuleDetail, PRBool aInherited)
{
  COMPUTE_START_RESET(Outline, (mPresContext), outline, parentOutline,
                      Margin, marginData)

  // outline-width: length, enum, inherit
  if (eCSSUnit_Initial == marginData.mOutlineWidth.GetUnit()) {
    outline->mOutlineWidth =
      nsStyleCoord(NS_STYLE_BORDER_WIDTH_MEDIUM, eStyleUnit_Enumerated);
  }
  else {
    SetCoord(marginData.mOutlineWidth, outline->mOutlineWidth,
             parentOutline->mOutlineWidth, SETCOORD_LEH, aContext,
             mPresContext, inherited);
  }

  // outline-offset: length, inherit
  nsStyleCoord tempCoord;
  if (SetCoord(marginData.mOutlineOffset, tempCoord,
               parentOutline->mOutlineOffset,
               SETCOORD_LH | SETCOORD_INITIAL_ZERO, aContext, mPresContext,
               inherited)) {
    outline->mOutlineOffset = tempCoord.GetCoordValue();
  } else {
    NS_ASSERTION(marginData.mOutlineOffset.GetUnit() == eCSSUnit_Null,
                 "unexpected unit");
  }

  // outline-color: color, string, enum, inherit
  nscolor outlineColor;
  nscolor unused = NS_RGB(0,0,0);
  if (eCSSUnit_Inherit == marginData.mOutlineColor.GetUnit()) {
    if (parentContext) {
      inherited = PR_TRUE;
      if (parentOutline->GetOutlineColor(outlineColor))
        outline->SetOutlineColor(outlineColor);
      else {
#ifdef GFX_HAS_INVERT
        outline->SetOutlineInitialColor();
#else
        // We want to inherit the color from the parent, not use the
        // color on the element where this chunk of style data will be
        // used.  We can ensure that the data for the parent are fully
        // computed (unlike for the element where this will be used, for
        // which the color could be specified on a more specific rule).
        outline->SetOutlineColor(parentContext->GetStyleColor()->mColor);
#endif
      }
    } else {
      outline->SetOutlineInitialColor();
    }
  }
  else if (SetColor(marginData.mOutlineColor, unused, mPresContext, aContext, outlineColor, inherited))
    outline->SetOutlineColor(outlineColor);
  else if (eCSSUnit_Enumerated == marginData.mOutlineColor.GetUnit() ||
           eCSSUnit_Initial == marginData.mOutlineColor.GetUnit()) {
    outline->SetOutlineInitialColor();
  }

  // -moz-outline-radius: length, percent, inherit
  { 
    nsStyleCoord coord;
    const nsCSSCornerSizes& outlineRadius = marginData.mOutlineRadius;
    NS_FOR_CSS_HALF_CORNERS(corner) {
      nsStyleCoord parentCoord = parentOutline->mOutlineRadius.Get(corner);
      if (SetCoord(outlineRadius.GetHalfCorner(corner),
                   coord, parentCoord, SETCOORD_LPH | SETCOORD_INITIAL_ZERO,
                   aContext, mPresContext, inherited))
        outline->mOutlineRadius.Set(corner, coord);
    }
  }

  // outline-style: auto, enum, none, inherit, initial
  // cannot use SetDiscrete because of SetOutlineStyle
  if (eCSSUnit_Enumerated == marginData.mOutlineStyle.GetUnit())
    outline->SetOutlineStyle(marginData.mOutlineStyle.GetIntValue());
  else if (eCSSUnit_None == marginData.mOutlineStyle.GetUnit() ||
           eCSSUnit_Initial == marginData.mOutlineStyle.GetUnit())
    outline->SetOutlineStyle(NS_STYLE_BORDER_STYLE_NONE);
  else if (eCSSUnit_Auto == marginData.mOutlineStyle.GetUnit()) {
    outline->SetOutlineStyle(NS_STYLE_BORDER_STYLE_AUTO);
  } else if (eCSSUnit_Inherit == marginData.mOutlineStyle.GetUnit()) {
    inherited = PR_TRUE;
    outline->SetOutlineStyle(parentOutline->GetOutlineStyle());
  }

  outline->RecalcData(mPresContext);
  COMPUTE_END_RESET(Outline, outline)
}

const void* 
nsRuleNode::ComputeListData(void* aStartStruct,
                            const nsRuleDataStruct& aData, 
                            nsStyleContext* aContext, 
                            nsRuleNode* aHighestNode,
                            const RuleDetail aRuleDetail, PRBool aInherited)
{
  COMPUTE_START_INHERITED(List, (), list, parentList, List, listData)

  // list-style-type: enum, none, inherit, initial
  SetDiscrete(listData.mType, list->mListStyleType, inherited,
              SETDSC_ENUMERATED | SETDSC_NONE, parentList->mListStyleType,
              NS_STYLE_LIST_STYLE_DISC, 0,
              NS_STYLE_LIST_STYLE_NONE, 0, 0);

  // list-style-image: url, none, inherit
  if (eCSSUnit_Image == listData.mImage.GetUnit()) {
    list->mListStyleImage = listData.mImage.GetImageValue();
  }
  else if (eCSSUnit_None == listData.mImage.GetUnit() ||
           eCSSUnit_Initial == listData.mImage.GetUnit()) {
    list->mListStyleImage = nsnull;
  }
  else if (eCSSUnit_Inherit == listData.mImage.GetUnit()) {
    inherited = PR_TRUE;
    list->mListStyleImage = parentList->mListStyleImage;
  }

  // list-style-position: enum, inherit, initial
  SetDiscrete(listData.mPosition, list->mListStylePosition, inherited,
              SETDSC_ENUMERATED, parentList->mListStylePosition,
              NS_STYLE_LIST_STYLE_POSITION_OUTSIDE, 0, 0, 0, 0);

  // image region property: length, auto, inherit
  if (eCSSUnit_Inherit == listData.mImageRegion.mTop.GetUnit()) { // if one is inherit, they all are
    inherited = PR_TRUE;
    list->mImageRegion = parentList->mImageRegion;
  }
  // if one is -moz-initial, they all are
  else if (eCSSUnit_Initial == listData.mImageRegion.mTop.GetUnit()) {
    list->mImageRegion.Empty();
  }
  else {
    if (eCSSUnit_Auto == listData.mImageRegion.mTop.GetUnit())
      list->mImageRegion.y = 0;
    else if (listData.mImageRegion.mTop.IsLengthUnit())
      list->mImageRegion.y = CalcLength(listData.mImageRegion.mTop, aContext, mPresContext, inherited);
      
    if (eCSSUnit_Auto == listData.mImageRegion.mBottom.GetUnit())
      list->mImageRegion.height = 0;
    else if (listData.mImageRegion.mBottom.IsLengthUnit())
      list->mImageRegion.height = CalcLength(listData.mImageRegion.mBottom, aContext, 
                                            mPresContext, inherited) - list->mImageRegion.y;
  
    if (eCSSUnit_Auto == listData.mImageRegion.mLeft.GetUnit())
      list->mImageRegion.x = 0;
    else if (listData.mImageRegion.mLeft.IsLengthUnit())
      list->mImageRegion.x = CalcLength(listData.mImageRegion.mLeft, aContext, mPresContext, inherited);
      
    if (eCSSUnit_Auto == listData.mImageRegion.mRight.GetUnit())
      list->mImageRegion.width = 0;
    else if (listData.mImageRegion.mRight.IsLengthUnit())
      list->mImageRegion.width = CalcLength(listData.mImageRegion.mRight, aContext, mPresContext, inherited) -
                                list->mImageRegion.x;
  }

  COMPUTE_END_INHERITED(List, list)
}

const void* 
nsRuleNode::ComputePositionData(void* aStartStruct,
                                const nsRuleDataStruct& aData, 
                                nsStyleContext* aContext, 
                                nsRuleNode* aHighestNode,
                                const RuleDetail aRuleDetail, PRBool aInherited)
{
  COMPUTE_START_RESET(Position, (), pos, parentPos, Position, posData)

  // box offsets: length, percent, auto, inherit
  nsStyleCoord  coord;
  NS_FOR_CSS_SIDES(side) {
    nsStyleCoord parentCoord = parentPos->mOffset.Get(side);
    if (SetCoord(posData.mOffset.*(nsCSSRect::sides[side]),
                 coord, parentCoord, SETCOORD_LPAH | SETCOORD_INITIAL_AUTO,
                 aContext, mPresContext, inherited)) {
      pos->mOffset.Set(side, coord);
    }
  }

  SetCoord(posData.mWidth, pos->mWidth, parentPos->mWidth,
           SETCOORD_LPAEH | SETCOORD_INITIAL_AUTO, aContext,
           mPresContext, inherited);
  SetCoord(posData.mMinWidth, pos->mMinWidth, parentPos->mMinWidth,
           SETCOORD_LPEH | SETCOORD_INITIAL_ZERO, aContext,
           mPresContext, inherited);
  SetCoord(posData.mMaxWidth, pos->mMaxWidth, parentPos->mMaxWidth,
           SETCOORD_LPOEH | SETCOORD_INITIAL_NONE, aContext,
           mPresContext, inherited);

  SetCoord(posData.mHeight, pos->mHeight, parentPos->mHeight,
           SETCOORD_LPAH | SETCOORD_INITIAL_AUTO, aContext,
           mPresContext, inherited);
  SetCoord(posData.mMinHeight, pos->mMinHeight, parentPos->mMinHeight,
           SETCOORD_LPH | SETCOORD_INITIAL_ZERO, aContext,
           mPresContext, inherited);
  SetCoord(posData.mMaxHeight, pos->mMaxHeight, parentPos->mMaxHeight,
           SETCOORD_LPOH | SETCOORD_INITIAL_NONE, aContext,
           mPresContext, inherited);

  // box-sizing: enum, inherit, initial
  SetDiscrete(posData.mBoxSizing, pos->mBoxSizing, inherited,
              SETDSC_ENUMERATED, parentPos->mBoxSizing,
              NS_STYLE_BOX_SIZING_CONTENT, 0, 0, 0, 0);

  // z-index
  if (! SetCoord(posData.mZIndex, pos->mZIndex, parentPos->mZIndex,
                 SETCOORD_IA | SETCOORD_INITIAL_AUTO, aContext,
                 nsnull, inherited)) {
    if (eCSSUnit_Inherit == posData.mZIndex.GetUnit()) {
      // handle inherit, because it's ok to inherit 'auto' here
      inherited = PR_TRUE;
      pos->mZIndex = parentPos->mZIndex;
    }
  }

  COMPUTE_END_RESET(Position, pos)
}

const void* 
nsRuleNode::ComputeTableData(void* aStartStruct,
                             const nsRuleDataStruct& aData, 
                             nsStyleContext* aContext, 
                             nsRuleNode* aHighestNode,
                             const RuleDetail aRuleDetail, PRBool aInherited)
{
  COMPUTE_START_RESET(Table, (), table, parentTable, Table, tableData)

  // table-layout: auto, enum, inherit, initial
  SetDiscrete(tableData.mLayout, table->mLayoutStrategy, inherited,
              SETDSC_ENUMERATED | SETDSC_AUTO,
              parentTable->mLayoutStrategy,
              NS_STYLE_TABLE_LAYOUT_AUTO,
              NS_STYLE_TABLE_LAYOUT_AUTO, 0, 0, 0);

  // rules: enum (not a real CSS prop)
  if (eCSSUnit_Enumerated == tableData.mRules.GetUnit())
    table->mRules = tableData.mRules.GetIntValue();

  // frame: enum (not a real CSS prop)
  if (eCSSUnit_Enumerated == tableData.mFrame.GetUnit())
    table->mFrame = tableData.mFrame.GetIntValue();

  // cols: enum, int (not a real CSS prop)
  if (eCSSUnit_Enumerated == tableData.mCols.GetUnit() ||
      eCSSUnit_Integer == tableData.mCols.GetUnit())
    table->mCols = tableData.mCols.GetIntValue();

  // span: pixels (not a real CSS prop)
  if (eCSSUnit_Enumerated == tableData.mSpan.GetUnit() ||
      eCSSUnit_Integer == tableData.mSpan.GetUnit())
    table->mSpan = tableData.mSpan.GetIntValue();
    
  COMPUTE_END_RESET(Table, table)
}

const void* 
nsRuleNode::ComputeTableBorderData(void* aStartStruct,
                                   const nsRuleDataStruct& aData, 
                                   nsStyleContext* aContext, 
                                   nsRuleNode* aHighestNode,
                                   const RuleDetail aRuleDetail, PRBool aInherited)
{
  COMPUTE_START_INHERITED(TableBorder, (mPresContext), table, parentTable,
                          Table, tableData)

  // border-collapse: enum, inherit, initial
  SetDiscrete(tableData.mBorderCollapse, table->mBorderCollapse, inherited,
              SETDSC_ENUMERATED, parentTable->mBorderCollapse,
              NS_STYLE_BORDER_SEPARATE, 0, 0, 0, 0);

  // border-spacing-x: length, inherit
  nsStyleCoord tempCoord;
  if (SetCoord(tableData.mBorderSpacing.mXValue, tempCoord,
               parentTable->mBorderSpacingX,
               SETCOORD_LH | SETCOORD_INITIAL_ZERO,
               aContext, mPresContext, inherited)) {
    table->mBorderSpacingX = tempCoord.GetCoordValue();
  } else {
    NS_ASSERTION(tableData.mBorderSpacing.mXValue.GetUnit() == eCSSUnit_Null,
                 "unexpected unit");
  }

  // border-spacing-y: length, inherit
  if (SetCoord(tableData.mBorderSpacing.mYValue, tempCoord,
               parentTable->mBorderSpacingY,
               SETCOORD_LH | SETCOORD_INITIAL_ZERO,
               aContext, mPresContext, inherited)) {
    table->mBorderSpacingY = tempCoord.GetCoordValue();
  } else {
    NS_ASSERTION(tableData.mBorderSpacing.mYValue.GetUnit() == eCSSUnit_Null,
                 "unexpected unit");
  }

  // caption-side: enum, inherit, initial
  SetDiscrete(tableData.mCaptionSide, table->mCaptionSide, inherited,
              SETDSC_ENUMERATED, parentTable->mCaptionSide,
              NS_STYLE_CAPTION_SIDE_TOP, 0, 0, 0, 0);

  // empty-cells: enum, inherit, initial
  SetDiscrete(tableData.mEmptyCells, table->mEmptyCells, inherited,
              SETDSC_ENUMERATED, parentTable->mEmptyCells,
              (mPresContext->CompatibilityMode() == eCompatibility_NavQuirks)
              ? NS_STYLE_TABLE_EMPTY_CELLS_SHOW_BACKGROUND     
              : NS_STYLE_TABLE_EMPTY_CELLS_SHOW,
              0, 0, 0, 0);

  COMPUTE_END_INHERITED(TableBorder, table)
}

const void* 
nsRuleNode::ComputeContentData(void* aStartStruct,
                               const nsRuleDataStruct& aData, 
                               nsStyleContext* aContext, 
                               nsRuleNode* aHighestNode,
                               const RuleDetail aRuleDetail, PRBool aInherited)
{
  COMPUTE_START_RESET(Content, (), content, parentContent,
                      Content, contentData)

  // content: [string, url, counter, attr, enum]+, normal, none, inherit
  PRUint32 count;
  nsAutoString  buffer;
  nsCSSValueList* contentValue = contentData.mContent;
  if (contentValue) {
    if (eCSSUnit_Normal == contentValue->mValue.GetUnit() ||
        eCSSUnit_None == contentValue->mValue.GetUnit() ||
        eCSSUnit_Initial == contentValue->mValue.GetUnit()) {
      // "normal", "none", and "initial" all mean no content
      content->AllocateContents(0);
    }
    else if (eCSSUnit_Inherit == contentValue->mValue.GetUnit()) {
      inherited = PR_TRUE;
      count = parentContent->ContentCount();
      if (NS_SUCCEEDED(content->AllocateContents(count))) {
        while (0 < count--) {
          content->ContentAt(count) = parentContent->ContentAt(count);
        }
      }
    }
    else {
      count = 0;
      while (contentValue) {
        count++;
        contentValue = contentValue->mNext;
      }
      if (NS_SUCCEEDED(content->AllocateContents(count))) {
        const nsAutoString  nullStr;
        count = 0;
        contentValue = contentData.mContent;
        while (contentValue) {
          const nsCSSValue& value = contentValue->mValue;
          nsCSSUnit unit = value.GetUnit();
          nsStyleContentType type;
          nsStyleContentData &data = content->ContentAt(count++);
          switch (unit) {
            case eCSSUnit_String:   type = eStyleContentType_String;    break;
            case eCSSUnit_Image:    type = eStyleContentType_Image;       break;
            case eCSSUnit_Attr:     type = eStyleContentType_Attr;      break;
            case eCSSUnit_Counter:  type = eStyleContentType_Counter;   break;
            case eCSSUnit_Counters: type = eStyleContentType_Counters;  break;
            case eCSSUnit_Enumerated:
              switch (value.GetIntValue()) {
                case NS_STYLE_CONTENT_OPEN_QUOTE:     
                  type = eStyleContentType_OpenQuote;     break;
                case NS_STYLE_CONTENT_CLOSE_QUOTE:
                  type = eStyleContentType_CloseQuote;    break;
                case NS_STYLE_CONTENT_NO_OPEN_QUOTE:
                  type = eStyleContentType_NoOpenQuote;   break;
                case NS_STYLE_CONTENT_NO_CLOSE_QUOTE:
                  type = eStyleContentType_NoCloseQuote;  break;
                case NS_STYLE_CONTENT_ALT_CONTENT:
                  type = eStyleContentType_AltContent;    break;
                default:
                  NS_ERROR("bad content value");
              }
              break;
            default:
              NS_ERROR("bad content type");
          }
          data.mType = type;
          if (type == eStyleContentType_Image) {
            data.mContent.mImage = value.GetImageValue();
            NS_IF_ADDREF(data.mContent.mImage);
          }
          else if (type <= eStyleContentType_Attr) {
            value.GetStringValue(buffer);
            Unquote(buffer);
            data.mContent.mString = NS_strdup(buffer.get());
          }
          else if (type <= eStyleContentType_Counters) {
            data.mContent.mCounters = value.GetArrayValue();
            data.mContent.mCounters->AddRef();
          }
          else {
            data.mContent.mString = nsnull;
          }
          contentValue = contentValue->mNext;
        }
      } 
    }
  }

  // counter-increment: [string [int]]+, none, inherit
  nsCSSValuePairList* ourIncrement = contentData.mCounterIncrement;
  if (ourIncrement) {
    if (eCSSUnit_None == ourIncrement->mXValue.GetUnit() ||
        eCSSUnit_Initial == ourIncrement->mXValue.GetUnit()) {
      content->AllocateCounterIncrements(0);
    }
    else if (eCSSUnit_Inherit == ourIncrement->mXValue.GetUnit()) {
      inherited = PR_TRUE;
      count = parentContent->CounterIncrementCount();
      if (NS_SUCCEEDED(content->AllocateCounterIncrements(count))) {
        while (0 < count--) {
          const nsStyleCounterData *data =
            parentContent->GetCounterIncrementAt(count);
          content->SetCounterIncrementAt(count, data->mCounter, data->mValue);
        }
      }
    }
    else if (eCSSUnit_String == ourIncrement->mXValue.GetUnit()) {
      count = 0;
      while (ourIncrement) {
        count++;
        ourIncrement = ourIncrement->mNext;
      }
      if (NS_SUCCEEDED(content->AllocateCounterIncrements(count))) {
        count = 0;
        ourIncrement = contentData.mCounterIncrement;
        while (ourIncrement) {
          PRInt32 increment;
          if (eCSSUnit_Integer == ourIncrement->mYValue.GetUnit()) {
            increment = ourIncrement->mYValue.GetIntValue();
          }
          else {
            increment = 1;
          }
          ourIncrement->mXValue.GetStringValue(buffer);
          content->SetCounterIncrementAt(count++, buffer, increment);
          ourIncrement = ourIncrement->mNext;
        }
      }
    }
  }

  // counter-reset: [string [int]]+, none, inherit
  nsCSSValuePairList* ourReset = contentData.mCounterReset;
  if (ourReset) {
    if (eCSSUnit_None == ourReset->mXValue.GetUnit() ||
        eCSSUnit_Initial == ourReset->mXValue.GetUnit()) {
      content->AllocateCounterResets(0);
    }
    else if (eCSSUnit_Inherit == ourReset->mXValue.GetUnit()) {
      inherited = PR_TRUE;
      count = parentContent->CounterResetCount();
      if (NS_SUCCEEDED(content->AllocateCounterResets(count))) {
        while (0 < count--) {
          const nsStyleCounterData *data =
            parentContent->GetCounterResetAt(count);
          content->SetCounterResetAt(count, data->mCounter, data->mValue);
        }
      }
    }
    else if (eCSSUnit_String == ourReset->mXValue.GetUnit()) {
      count = 0;
      while (ourReset) {
        count++;
        ourReset = ourReset->mNext;
      }
      if (NS_SUCCEEDED(content->AllocateCounterResets(count))) {
        count = 0;
        ourReset = contentData.mCounterReset;
        while (ourReset) {
          PRInt32 reset;
          if (eCSSUnit_Integer == ourReset->mYValue.GetUnit()) {
            reset = ourReset->mYValue.GetIntValue();
          }
          else {
            reset = 0;
          }
          ourReset->mXValue.GetStringValue(buffer);
          content->SetCounterResetAt(count++, buffer, reset);
          ourReset = ourReset->mNext;
        }
      }
    }
  }

  // marker-offset: length, auto, inherit
  SetCoord(contentData.mMarkerOffset, content->mMarkerOffset, parentContent->mMarkerOffset,
           SETCOORD_LH | SETCOORD_AUTO | SETCOORD_INITIAL_AUTO, aContext,
           mPresContext, inherited);
    
  COMPUTE_END_RESET(Content, content)
}

const void* 
nsRuleNode::ComputeQuotesData(void* aStartStruct,
                              const nsRuleDataStruct& aData, 
                              nsStyleContext* aContext, 
                              nsRuleNode* aHighestNode,
                              const RuleDetail aRuleDetail, PRBool aInherited)
{
  COMPUTE_START_INHERITED(Quotes, (), quotes, parentQuotes,
                          Content, contentData)

  // quotes: inherit, initial, none, [string string]+
  nsCSSValuePairList* ourQuotes = contentData.mQuotes;
  if (ourQuotes) {
    if (eCSSUnit_Inherit == ourQuotes->mXValue.GetUnit()) {
      inherited = PR_TRUE;
      quotes->CopyFrom(*parentQuotes);
    }
    else if (eCSSUnit_Initial == ourQuotes->mXValue.GetUnit()) {
      quotes->SetInitial();
    }
    else if (eCSSUnit_None == ourQuotes->mXValue.GetUnit()) {
      quotes->AllocateQuotes(0);
    }
    else if (eCSSUnit_String == ourQuotes->mXValue.GetUnit()) {
      nsAutoString  buffer;
      nsAutoString  closeBuffer;
      PRUint32 count = 0;

      while (ourQuotes) {
        count++;
        ourQuotes = ourQuotes->mNext;
      }
      if (NS_SUCCEEDED(quotes->AllocateQuotes(count))) {
        count = 0;
        ourQuotes = contentData.mQuotes;
        while (ourQuotes) {
          ourQuotes->mXValue.GetStringValue(buffer);
          ourQuotes->mYValue.GetStringValue(closeBuffer);
          Unquote(buffer);
          Unquote(closeBuffer);
          quotes->SetQuotesAt(count++, buffer, closeBuffer);
          ourQuotes = ourQuotes->mNext;
        }
      }
    }
  }

  COMPUTE_END_INHERITED(Quotes, quotes)
}

const void* 
nsRuleNode::ComputeXULData(void* aStartStruct,
                           const nsRuleDataStruct& aData, 
                           nsStyleContext* aContext, 
                           nsRuleNode* aHighestNode,
                           const RuleDetail aRuleDetail, PRBool aInherited)
{
  COMPUTE_START_RESET(XUL, (), xul, parentXUL, XUL, xulData)

  // box-align: enum, inherit, initial
  SetDiscrete(xulData.mBoxAlign, xul->mBoxAlign, inherited,
              SETDSC_ENUMERATED, parentXUL->mBoxAlign,
              NS_STYLE_BOX_ALIGN_STRETCH, 0, 0, 0, 0);

  // box-direction: enum, inherit, initial
  SetDiscrete(xulData.mBoxDirection, xul->mBoxDirection, inherited,
              SETDSC_ENUMERATED, parentXUL->mBoxDirection,
              NS_STYLE_BOX_DIRECTION_NORMAL, 0, 0, 0, 0);

  // box-flex: factor, inherit
  SetFactor(xulData.mBoxFlex, xul->mBoxFlex, inherited,
            parentXUL->mBoxFlex, 0.0f);

  // box-orient: enum, inherit, initial
  SetDiscrete(xulData.mBoxOrient, xul->mBoxOrient, inherited,
              SETDSC_ENUMERATED, parentXUL->mBoxOrient,
              NS_STYLE_BOX_ORIENT_HORIZONTAL, 0, 0, 0, 0);

  // box-pack: enum, inherit, initial
  SetDiscrete(xulData.mBoxPack, xul->mBoxPack, inherited,
              SETDSC_ENUMERATED, parentXUL->mBoxPack,
              NS_STYLE_BOX_PACK_START, 0, 0, 0, 0);

  // box-ordinal-group: integer, inherit, initial
  SetDiscrete(xulData.mBoxOrdinal, xul->mBoxOrdinal, inherited,
              SETDSC_INTEGER, parentXUL->mBoxOrdinal, 1,
              0, 0, 0, 0);

  if (eCSSUnit_Inherit == xulData.mStackSizing.GetUnit()) {
    inherited = PR_TRUE;
    xul->mStretchStack = parentXUL->mStretchStack;
  } else if (eCSSUnit_Initial == xulData.mStackSizing.GetUnit()) {
    xul->mStretchStack = PR_TRUE;
  } else if (eCSSUnit_Enumerated == xulData.mStackSizing.GetUnit()) {
    xul->mStretchStack = xulData.mStackSizing.GetIntValue() ==
      NS_STYLE_STACK_SIZING_STRETCH_TO_FIT;
  }

  COMPUTE_END_RESET(XUL, xul)
}

const void* 
nsRuleNode::ComputeColumnData(void* aStartStruct,
                              const nsRuleDataStruct& aData, 
                              nsStyleContext* aContext, 
                              nsRuleNode* aHighestNode,
                              const RuleDetail aRuleDetail, PRBool aInherited)
{
  COMPUTE_START_RESET(Column, (mPresContext), column, parent, Column, columnData)

  // column-width: length, auto, inherit
  SetCoord(columnData.mColumnWidth,
           column->mColumnWidth, parent->mColumnWidth,
           SETCOORD_LAH | SETCOORD_INITIAL_AUTO,
           aContext, mPresContext, inherited);

  // column-gap: length, percentage, inherit, normal
  SetCoord(columnData.mColumnGap,
           column->mColumnGap, parent->mColumnGap,
           SETCOORD_LPH | SETCOORD_NORMAL | SETCOORD_INITIAL_NORMAL,
           aContext, mPresContext, inherited);

  // column-count: auto, integer, inherit
  if (eCSSUnit_Auto == columnData.mColumnCount.GetUnit() ||
      eCSSUnit_Initial == columnData.mColumnCount.GetUnit()) {
    column->mColumnCount = NS_STYLE_COLUMN_COUNT_AUTO;
  } else if (eCSSUnit_Integer == columnData.mColumnCount.GetUnit()) {
    column->mColumnCount = columnData.mColumnCount.GetIntValue();
    // Max 1000 columns - wallpaper for bug 345583.
    column->mColumnCount = PR_MIN(column->mColumnCount, 1000);
  } else if (eCSSUnit_Inherit == columnData.mColumnCount.GetUnit()) {
    inherited = PR_TRUE;
    column->mColumnCount = parent->mColumnCount;
  }

  // column-rule-width: length, enum, inherit
  const nsCSSValue& widthValue = columnData.mColumnRuleWidth;
  if (eCSSUnit_Initial == widthValue.GetUnit()) {
    column->SetColumnRuleWidth(
        (mPresContext->GetBorderWidthTable())[NS_STYLE_BORDER_WIDTH_MEDIUM]);
  }
  else if (eCSSUnit_Enumerated == widthValue.GetUnit()) {
    NS_ASSERTION(widthValue.GetIntValue() == NS_STYLE_BORDER_WIDTH_THIN ||
                 widthValue.GetIntValue() == NS_STYLE_BORDER_WIDTH_MEDIUM ||
                 widthValue.GetIntValue() == NS_STYLE_BORDER_WIDTH_THICK,
                 "Unexpected enum value");
    column->SetColumnRuleWidth(
        (mPresContext->GetBorderWidthTable())[widthValue.GetIntValue()]);
  }
  else if (eCSSUnit_Inherit == widthValue.GetUnit()) {
    column->SetColumnRuleWidth(parent->GetComputedColumnRuleWidth());
    inherited = PR_TRUE;
  }
  else if (widthValue.IsLengthUnit()) {
    column->SetColumnRuleWidth(CalcLength(widthValue, aContext,
                                          mPresContext, inherited));
  }

  // column-rule-style: enum, none, inherit
  const nsCSSValue& styleValue = columnData.mColumnRuleStyle;
  if (eCSSUnit_Enumerated == styleValue.GetUnit()) {
    column->mColumnRuleStyle = styleValue.GetIntValue();
  }
  else if (eCSSUnit_None == styleValue.GetUnit() ||
           eCSSUnit_Initial == styleValue.GetUnit()) {
    column->mColumnRuleStyle = NS_STYLE_BORDER_STYLE_NONE;
  }
  else if (eCSSUnit_Inherit == styleValue.GetUnit()) {
    inherited = PR_TRUE;
    column->mColumnRuleStyle = parent->mColumnRuleStyle;
  }

  // column-rule-color: color, inherit
  const nsCSSValue& colorValue = columnData.mColumnRuleColor;
  if (eCSSUnit_Inherit == colorValue.GetUnit()) {
    inherited = PR_TRUE;
    column->mColumnRuleColorIsForeground = PR_FALSE;
    if (parent->mColumnRuleColorIsForeground) {
      column->mColumnRuleColor = parentContext->GetStyleColor()->mColor;
    } else {
      column->mColumnRuleColor = parent->mColumnRuleColor;
    }
  }
  else if (eCSSUnit_Initial == colorValue.GetUnit()) {
    column->mColumnRuleColorIsForeground = PR_TRUE;
  }
  else if (SetColor(colorValue, 0, mPresContext, aContext, column->mColumnRuleColor, inherited)) {
    column->mColumnRuleColorIsForeground = PR_FALSE;
  }

  COMPUTE_END_RESET(Column, column)
}

#ifdef MOZ_SVG
static void
SetSVGPaint(const nsCSSValuePair& aValue, const nsStyleSVGPaint& parentPaint,
            nsPresContext* aPresContext, nsStyleContext *aContext, 
            nsStyleSVGPaint& aResult, nsStyleSVGPaintType aInitialPaintType,
            PRBool& aInherited)
{
  nscolor color;

  if (aValue.mXValue.GetUnit() == eCSSUnit_Inherit) {
    aResult = parentPaint;
    aInherited = PR_TRUE;
  } else if (aValue.mXValue.GetUnit() == eCSSUnit_None) {
    aResult.SetType(eStyleSVGPaintType_None);
  } else if (aValue.mXValue.GetUnit() == eCSSUnit_Initial) {
    aResult.SetType(aInitialPaintType);
    aResult.mPaint.mColor = NS_RGB(0, 0, 0);
    aResult.mFallbackColor = NS_RGB(0, 0, 0);
  } else if (aValue.mXValue.GetUnit() == eCSSUnit_URL) {
    aResult.SetType(eStyleSVGPaintType_Server);
    aResult.mPaint.mPaintServer = aValue.mXValue.GetURLValue();
    NS_IF_ADDREF(aResult.mPaint.mPaintServer);
    if (aValue.mYValue.GetUnit() == eCSSUnit_None) {
      aResult.mFallbackColor = NS_RGBA(0, 0, 0, 0);
    } else {
      NS_ASSERTION(aValue.mYValue.GetUnit() != eCSSUnit_Inherit, "cannot inherit fallback colour");
      SetColor(aValue.mYValue, NS_RGB(0, 0, 0), aPresContext, aContext, aResult.mFallbackColor, aInherited);
    }
  } else if (SetColor(aValue.mXValue, parentPaint.mPaint.mColor, aPresContext, aContext, color, aInherited)) {
    aResult.SetType(eStyleSVGPaintType_Color);
    aResult.mPaint.mColor = color;
  }
}

const void* 
nsRuleNode::ComputeSVGData(void* aStartStruct,
                           const nsRuleDataStruct& aData, 
                           nsStyleContext* aContext, 
                           nsRuleNode* aHighestNode,
                           const RuleDetail aRuleDetail, PRBool aInherited)
{
  COMPUTE_START_INHERITED(SVG, (), svg, parentSVG, SVG, SVGData)

  // clip-rule: enum, inherit, initial
  SetDiscrete(SVGData.mClipRule, svg->mClipRule, inherited,
              SETDSC_ENUMERATED, parentSVG->mClipRule,
              NS_STYLE_FILL_RULE_NONZERO, 0, 0, 0, 0);

  // color-interpolation: enum, auto, inherit, initial
  SetDiscrete(SVGData.mColorInterpolation, svg->mColorInterpolation, inherited,
              SETDSC_ENUMERATED | SETDSC_AUTO,
              parentSVG->mColorInterpolation,
              NS_STYLE_COLOR_INTERPOLATION_SRGB,
              NS_STYLE_COLOR_INTERPOLATION_AUTO, 0, 0, 0);

  // color-interpolation-filters: enum, auto, inherit, initial
  SetDiscrete(SVGData.mColorInterpolationFilters,
              svg->mColorInterpolationFilters, inherited,
              SETDSC_ENUMERATED | SETDSC_AUTO,
              parentSVG->mColorInterpolationFilters,
              NS_STYLE_COLOR_INTERPOLATION_LINEARRGB,
              NS_STYLE_COLOR_INTERPOLATION_AUTO, 0, 0, 0);

  // fill: 
  SetSVGPaint(SVGData.mFill, parentSVG->mFill, mPresContext, aContext,
              svg->mFill, eStyleSVGPaintType_Color, inherited);

  // fill-opacity: factor, inherit, initial
  SetFactor(SVGData.mFillOpacity, svg->mFillOpacity, inherited,
            parentSVG->mFillOpacity, 1.0f, SETFCT_OPACITY);

  // fill-rule: enum, inherit, initial
  SetDiscrete(SVGData.mFillRule, svg->mFillRule, inherited,
              SETDSC_ENUMERATED, parentSVG->mFillRule,
              NS_STYLE_FILL_RULE_NONZERO, 0, 0, 0, 0);

  // marker-end: url, none, inherit
  if (eCSSUnit_URL == SVGData.mMarkerEnd.GetUnit()) {
    svg->mMarkerEnd = SVGData.mMarkerEnd.GetURLValue();
  } else if (eCSSUnit_None == SVGData.mMarkerEnd.GetUnit() ||
             eCSSUnit_Initial == SVGData.mMarkerEnd.GetUnit()) {
    svg->mMarkerEnd = nsnull;
  } else if (eCSSUnit_Inherit == SVGData.mMarkerEnd.GetUnit()) {
    inherited = PR_TRUE;
    svg->mMarkerEnd = parentSVG->mMarkerEnd;
  }

  // marker-mid: url, none, inherit
  if (eCSSUnit_URL == SVGData.mMarkerMid.GetUnit()) {
    svg->mMarkerMid = SVGData.mMarkerMid.GetURLValue();
  } else if (eCSSUnit_None == SVGData.mMarkerMid.GetUnit() ||
             eCSSUnit_Initial == SVGData.mMarkerMid.GetUnit()) {
    svg->mMarkerMid = nsnull;
  } else if (eCSSUnit_Inherit == SVGData.mMarkerMid.GetUnit()) {
    inherited = PR_TRUE;
    svg->mMarkerMid = parentSVG->mMarkerMid;
  }

  // marker-start: url, none, inherit
  if (eCSSUnit_URL == SVGData.mMarkerStart.GetUnit()) {
    svg->mMarkerStart = SVGData.mMarkerStart.GetURLValue();
  } else if (eCSSUnit_None == SVGData.mMarkerStart.GetUnit() ||
             eCSSUnit_Initial == SVGData.mMarkerStart.GetUnit()) {
    svg->mMarkerStart = nsnull;
  } else if (eCSSUnit_Inherit == SVGData.mMarkerStart.GetUnit()) {
    inherited = PR_TRUE;
    svg->mMarkerStart = parentSVG->mMarkerStart;
  }

  // pointer-events: enum, none, inherit, initial
  SetDiscrete(SVGData.mPointerEvents, svg->mPointerEvents, inherited,
              SETDSC_ENUMERATED | SETDSC_NONE, parentSVG->mPointerEvents,
              NS_STYLE_POINTER_EVENTS_VISIBLEPAINTED, 0,
              NS_STYLE_POINTER_EVENTS_NONE, 0, 0);

  // shape-rendering: enum, auto, inherit
  SetDiscrete(SVGData.mShapeRendering, svg->mShapeRendering, inherited,
              SETDSC_ENUMERATED | SETDSC_AUTO,
              parentSVG->mShapeRendering,
              NS_STYLE_SHAPE_RENDERING_AUTO, 
              NS_STYLE_SHAPE_RENDERING_AUTO, 0, 0, 0);

  // stroke: 
  SetSVGPaint(SVGData.mStroke, parentSVG->mStroke, mPresContext, aContext,
              svg->mStroke, eStyleSVGPaintType_None, inherited);

  // stroke-dasharray: <dasharray>, none, inherit
  nsCSSValueList *list = SVGData.mStrokeDasharray;
  if (list) {
    if (eCSSUnit_Inherit == list->mValue.GetUnit()) {
      // only do the copy if weren't already set up by the copy constructor
      // FIXME Bug 389408: This is broken when aStartStruct is non-null!
      if (!svg->mStrokeDasharray) {
        inherited = PR_TRUE;
        svg->mStrokeDasharrayLength = parentSVG->mStrokeDasharrayLength;
        if (svg->mStrokeDasharrayLength) {
          svg->mStrokeDasharray = new nsStyleCoord[svg->mStrokeDasharrayLength];
          if (svg->mStrokeDasharray)
            memcpy(svg->mStrokeDasharray,
                   parentSVG->mStrokeDasharray,
                   svg->mStrokeDasharrayLength * sizeof(nsStyleCoord));
          else
            svg->mStrokeDasharrayLength = 0;
        }
      }
    } else {
      delete [] svg->mStrokeDasharray;
      svg->mStrokeDasharray = nsnull;
      svg->mStrokeDasharrayLength = 0;
      
      if (eCSSUnit_Initial != list->mValue.GetUnit() &&
          eCSSUnit_None    != list->mValue.GetUnit()) {
        // count number of values
        nsCSSValueList *value = SVGData.mStrokeDasharray;
        while (nsnull != value) {
          ++svg->mStrokeDasharrayLength;
          value = value->mNext;
        }
        
        NS_ASSERTION(svg->mStrokeDasharrayLength != 0, "no dasharray items");
        
        svg->mStrokeDasharray = new nsStyleCoord[svg->mStrokeDasharrayLength];

        if (svg->mStrokeDasharray) {
          value = SVGData.mStrokeDasharray;
          PRUint32 i = 0;
          while (nsnull != value) {
            SetCoord(value->mValue,
                     svg->mStrokeDasharray[i++], nsnull,
                     SETCOORD_LP | SETCOORD_FACTOR,
                     aContext, mPresContext, inherited);
            value = value->mNext;
          }
        } else
          svg->mStrokeDasharrayLength = 0;
      }
    }
  }

  // stroke-dashoffset: <dashoffset>, inherit
  SetCoord(SVGData.mStrokeDashoffset,
           svg->mStrokeDashoffset, parentSVG->mStrokeDashoffset,
           SETCOORD_LPH | SETCOORD_FACTOR | SETCOORD_INITIAL_ZERO,
           aContext, mPresContext, inherited);

  // stroke-linecap: enum, inherit, initial
  SetDiscrete(SVGData.mStrokeLinecap, svg->mStrokeLinecap, inherited,
              SETDSC_ENUMERATED, parentSVG->mStrokeLinecap,
              NS_STYLE_STROKE_LINECAP_BUTT, 0, 0, 0, 0);

  // stroke-linejoin: enum, inherit, initial
  SetDiscrete(SVGData.mStrokeLinejoin, svg->mStrokeLinejoin, inherited,
              SETDSC_ENUMERATED, parentSVG->mStrokeLinejoin,
              NS_STYLE_STROKE_LINEJOIN_MITER, 0, 0, 0, 0);

  // stroke-miterlimit: <miterlimit>, inherit
  SetFactor(SVGData.mStrokeMiterlimit, svg->mStrokeMiterlimit, inherited,
            parentSVG->mStrokeMiterlimit, 4.0f);

  // stroke-opacity:
  SetFactor(SVGData.mStrokeOpacity, svg->mStrokeOpacity, inherited,
            parentSVG->mStrokeOpacity, 1.0f, SETFCT_OPACITY);

  // stroke-width:
  if (eCSSUnit_Initial == SVGData.mStrokeWidth.GetUnit()) {
    svg->mStrokeWidth.SetCoordValue(nsPresContext::CSSPixelsToAppUnits(1));
  } else {
    SetCoord(SVGData.mStrokeWidth,
             svg->mStrokeWidth, parentSVG->mStrokeWidth,
             SETCOORD_LPH | SETCOORD_FACTOR,
             aContext, mPresContext, inherited);
  }

  // text-anchor: enum, inherit, initial
  SetDiscrete(SVGData.mTextAnchor, svg->mTextAnchor, inherited,
              SETDSC_ENUMERATED, parentSVG->mTextAnchor,
              NS_STYLE_TEXT_ANCHOR_START, 0, 0, 0, 0);
  
  // text-rendering: enum, auto, inherit, initial
  SetDiscrete(SVGData.mTextRendering, svg->mTextRendering, inherited,
              SETDSC_ENUMERATED | SETDSC_AUTO,
              parentSVG->mTextRendering,
              NS_STYLE_TEXT_RENDERING_AUTO,
              NS_STYLE_TEXT_RENDERING_AUTO, 0, 0, 0);

  COMPUTE_END_INHERITED(SVG, svg)
}

const void* 
nsRuleNode::ComputeSVGResetData(void* aStartStruct,
                                const nsRuleDataStruct& aData,
                                nsStyleContext* aContext, 
                                nsRuleNode* aHighestNode,
                                const RuleDetail aRuleDetail, PRBool aInherited)
{
  COMPUTE_START_RESET(SVGReset, (), svgReset, parentSVGReset, SVG, SVGData)

  // stop-color:
  if (eCSSUnit_Initial == SVGData.mStopColor.GetUnit()) {
    svgReset->mStopColor = NS_RGB(0, 0, 0);
  } else {
    SetColor(SVGData.mStopColor, parentSVGReset->mStopColor,
             mPresContext, aContext, svgReset->mStopColor, inherited);
  }

  // flood-color:
  if (eCSSUnit_Initial == SVGData.mFloodColor.GetUnit()) {
    svgReset->mFloodColor = NS_RGB(0, 0, 0);
  } else {
    SetColor(SVGData.mFloodColor, parentSVGReset->mFloodColor,
             mPresContext, aContext, svgReset->mFloodColor, inherited);
  }

  // lighting-color:
  if (eCSSUnit_Initial == SVGData.mLightingColor.GetUnit()) {
    svgReset->mLightingColor = NS_RGB(255, 255, 255);
  } else {
    SetColor(SVGData.mLightingColor, parentSVGReset->mLightingColor,
             mPresContext, aContext, svgReset->mLightingColor, inherited);
  }

  // clip-path: url, none, inherit
  if (eCSSUnit_URL == SVGData.mClipPath.GetUnit()) {
    svgReset->mClipPath = SVGData.mClipPath.GetURLValue();
  } else if (eCSSUnit_None == SVGData.mClipPath.GetUnit() ||
             eCSSUnit_Initial == SVGData.mClipPath.GetUnit()) {
    svgReset->mClipPath = nsnull;
  } else if (eCSSUnit_Inherit == SVGData.mClipPath.GetUnit()) {
    inherited = PR_TRUE;
    svgReset->mClipPath = parentSVGReset->mClipPath;
  }

  // stop-opacity:
  SetFactor(SVGData.mStopOpacity, svgReset->mStopOpacity, inherited,
            parentSVGReset->mStopOpacity, 1.0f, SETFCT_OPACITY);

  // flood-opacity:
  SetFactor(SVGData.mFloodOpacity, svgReset->mFloodOpacity, inherited,
            parentSVGReset->mFloodOpacity, 1.0f, SETFCT_OPACITY);

  // dominant-baseline: enum, auto, inherit, initial
  SetDiscrete(SVGData.mDominantBaseline, svgReset->mDominantBaseline,
              inherited,
              SETDSC_ENUMERATED | SETDSC_AUTO,
              parentSVGReset->mDominantBaseline,
              NS_STYLE_DOMINANT_BASELINE_AUTO,
              NS_STYLE_DOMINANT_BASELINE_AUTO, 0, 0, 0);

  // filter: url, none, inherit
  if (eCSSUnit_URL == SVGData.mFilter.GetUnit()) {
    svgReset->mFilter = SVGData.mFilter.GetURLValue();
  } else if (eCSSUnit_None == SVGData.mFilter.GetUnit() ||
             eCSSUnit_Initial == SVGData.mFilter.GetUnit()) {
    svgReset->mFilter = nsnull;
  } else if (eCSSUnit_Inherit == SVGData.mFilter.GetUnit()) {
    inherited = PR_TRUE;
    svgReset->mFilter = parentSVGReset->mFilter;
  }

  // mask: url, none, inherit
  if (eCSSUnit_URL == SVGData.mMask.GetUnit()) {
    svgReset->mMask = SVGData.mMask.GetURLValue();
  } else if (eCSSUnit_None == SVGData.mMask.GetUnit() ||
             eCSSUnit_Initial == SVGData.mMask.GetUnit()) {
    svgReset->mMask = nsnull;
  } else if (eCSSUnit_Inherit == SVGData.mMask.GetUnit()) {
    inherited = PR_TRUE;
    svgReset->mMask = parentSVGReset->mMask;
  }
  
  COMPUTE_END_RESET(SVGReset, svgReset)
}
#endif

inline const void* 
nsRuleNode::GetParentData(const nsStyleStructID aSID)
{
  NS_PRECONDITION(mDependentBits & nsCachedStyleData::GetBitForSID(aSID),
                  "should be called when node depends on parent data");
  NS_ASSERTION(mStyleData.GetStyleData(aSID) == nsnull,
               "both struct and dependent bits present");
  // Walk up the rule tree from this rule node (towards less specific
  // rules).
  PRUint32 bit = nsCachedStyleData::GetBitForSID(aSID);
  nsRuleNode *ruleNode = mParent;
  while (ruleNode->mDependentBits & bit) {
    NS_ASSERTION(ruleNode->mStyleData.GetStyleData(aSID) == nsnull,
                 "both struct and dependent bits present");
    ruleNode = ruleNode->mParent;
  }

  return ruleNode->mStyleData.GetStyleData(aSID);
}

#define STYLE_STRUCT(name_, checkdata_cb_, ctor_args_)                      \
inline const nsStyle##name_ *                                               \
nsRuleNode::GetParent##name_()                                              \
{                                                                           \
  NS_PRECONDITION(mDependentBits &                                          \
                  nsCachedStyleData::GetBitForSID(eStyleStruct_##name_),    \
                  "should be called when node depends on parent data");     \
  NS_ASSERTION(mStyleData.GetStyle##name_() == nsnull,                      \
               "both struct and dependent bits present");                   \
  /* Walk up the rule tree from this rule node (towards less specific */    \
  /* rules). */                                                             \
  PRUint32 bit = nsCachedStyleData::GetBitForSID(eStyleStruct_##name_);     \
  nsRuleNode *ruleNode = mParent;                                           \
  while (ruleNode->mDependentBits & bit) {                                  \
    NS_ASSERTION(ruleNode->mStyleData.GetStyle##name_() == nsnull,          \
                 "both struct and dependent bits present");                 \
    ruleNode = ruleNode->mParent;                                           \
  }                                                                         \
                                                                            \
  return ruleNode->mStyleData.GetStyle##name_();                            \
}
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

const void* 
nsRuleNode::GetStyleData(nsStyleStructID aSID, 
                         nsStyleContext* aContext,
                         PRBool aComputeData)
{
  const void *data;
  if (mDependentBits & nsCachedStyleData::GetBitForSID(aSID)) {
    // We depend on an ancestor for this struct since the cached struct
    // it has is also appropriate for this rule node.  Just go up the
    // rule tree and return the first cached struct we find.
    data = GetParentData(aSID);
    NS_ASSERTION(data, "dependent bits set but no cached struct present");
    return data;
  }

  data = mStyleData.GetStyleData(aSID);
  if (NS_LIKELY(data != nsnull))
    return data; // We have a fully specified struct. Just return it.

  if (NS_UNLIKELY(!aComputeData))
    return nsnull;

  // Nothing is cached.  We'll have to delve further and examine our rules.
#define STYLE_STRUCT_TEST aSID
#define STYLE_STRUCT(name, checkdata_cb, ctor_args) \
  data = Get##name##Data(aContext);
#include "nsStyleStructList.h"
#undef STYLE_STRUCT
#undef STYLE_STRUCT_TEST

  if (NS_LIKELY(data != nsnull))
    return data;

  NS_NOTREACHED("could not create style struct");
  // To ensure that |GetStyleData| never returns null (even when we're
  // out of memory), we'll get the style set and get a copy of the
  // default values for the given style struct from the set.  Note that
  // this works fine even if |this| is a rule node that has been
  // destroyed (leftover from a previous rule tree) but is somehow still
  // used.
  return mPresContext->PresShell()->StyleSet()->
    DefaultStyleData()->GetStyleData(aSID);
}

// See comments above in GetStyleData for an explanation of what the
// code below does.
#define STYLE_STRUCT(name_, checkdata_cb_, ctor_args_)                        \
const nsStyle##name_*                                                         \
nsRuleNode::GetStyle##name_(nsStyleContext* aContext, PRBool aComputeData)    \
{                                                                             \
  const nsStyle##name_ *data;                                                 \
  if (mDependentBits &                                                        \
      nsCachedStyleData::GetBitForSID(eStyleStruct_##name_)) {                \
    data = GetParent##name_();                                                \
    NS_ASSERTION(data, "dependent bits set but no cached struct present");    \
    return data;                                                              \
  }                                                                           \
                                                                              \
  data = mStyleData.GetStyle##name_();                                        \
  if (NS_LIKELY(data != nsnull))                                              \
    return data;                                                              \
                                                                              \
  if (NS_UNLIKELY(!aComputeData))                                             \
    return nsnull;                                                            \
                                                                              \
  data =                                                                      \
    static_cast<const nsStyle##name_ *>(Get##name_##Data(aContext));          \
                                                                              \
  if (NS_LIKELY(data != nsnull))                                              \
    return data;                                                              \
                                                                              \
  NS_NOTREACHED("could not create style struct");                             \
  return                                                                      \
    static_cast<const nsStyle##name_ *>(                                      \
                   mPresContext->PresShell()->StyleSet()->                    \
                     DefaultStyleData()->GetStyleData(eStyleStruct_##name_)); \
}
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

void
nsRuleNode::Mark()
{
  for (nsRuleNode *node = this;
       node && !(node->mDependentBits & NS_RULE_NODE_GC_MARK);
       node = node->mParent)
    node->mDependentBits |= NS_RULE_NODE_GC_MARK;
}

static PLDHashOperator
SweepRuleNodeChildren(PLDHashTable *table, PLDHashEntryHdr *hdr,
                      PRUint32 number, void *arg)
{
  ChildrenHashEntry *entry = static_cast<ChildrenHashEntry*>(hdr);
  if (entry->mRuleNode->Sweep())
    return PL_DHASH_REMOVE; // implies NEXT, unless |ed with STOP
  return PL_DHASH_NEXT;
}

PRBool
nsRuleNode::Sweep()
{
  // If we're not marked, then we have to delete ourself.
  // However, we never allow the root node to GC itself, because nsStyleSet
  // wants to hold onto the root node and not worry about re-creating a
  // rule walker if the root node is deleted.
  if (!(mDependentBits & NS_RULE_NODE_GC_MARK) && !IsRoot()) {
    Destroy();
    return PR_TRUE;
  }

  // Clear our mark, for the next time around.
  mDependentBits &= ~NS_RULE_NODE_GC_MARK;

  // Call sweep on the children, since some may not be marked, and
  // remove any deleted children from the child lists.
  if (HaveChildren()) {
    if (ChildrenAreHashed()) {
      PLDHashTable *children = ChildrenHash();
      PL_DHashTableEnumerate(children, SweepRuleNodeChildren, nsnull);
    } else {
      for (nsRuleNode **children = ChildrenListPtr(); *children; ) {
        nsRuleNode *next = (*children)->mNextSibling;
        if ((*children)->Sweep()) {
          // This rule node was destroyed, so implicitly advance by
          // making *children point to the next entry.
          *children = next;
        } else {
          // Advance.
          children = &(*children)->mNextSibling;
        }
      }
    }
  }
  return PR_FALSE;
}

/* static */ PRBool
nsRuleNode::HasAuthorSpecifiedRules(nsStyleContext* aStyleContext,
                                    PRUint32 ruleTypeMask)
{
  nsRuleDataColor colorData;
  nsRuleDataMargin marginData;
  PRUint32 nValues = 0;

  PRUint32 inheritBits = 0;
  if (ruleTypeMask & NS_AUTHOR_SPECIFIED_BACKGROUND)
    inheritBits |= NS_STYLE_INHERIT_BIT(Background);

  if (ruleTypeMask & NS_AUTHOR_SPECIFIED_BORDER)
    inheritBits |= NS_STYLE_INHERIT_BIT(Border);

  if (ruleTypeMask & NS_AUTHOR_SPECIFIED_PADDING)
    inheritBits |= NS_STYLE_INHERIT_BIT(Padding);

  /* We're relying on the use of |aStyleContext| not mutating it! */
  nsRuleData ruleData(inheritBits,
                      aStyleContext->PresContext(), aStyleContext);
  ruleData.mColorData = &colorData;
  ruleData.mMarginData = &marginData;

  nsCSSValue* backgroundValues[] = {
    &colorData.mBackColor,
    &colorData.mBackImage
  };

  nsCSSValue* borderValues[] = {
    &marginData.mBorderColor.mTop,
    &marginData.mBorderStyle.mTop,
    &marginData.mBorderWidth.mTop,
    &marginData.mBorderColor.mRight,
    &marginData.mBorderStyle.mRight,
    &marginData.mBorderWidth.mRight,
    &marginData.mBorderColor.mBottom,
    &marginData.mBorderStyle.mBottom,
    &marginData.mBorderWidth.mBottom,
    &marginData.mBorderColor.mLeft,
    &marginData.mBorderStyle.mLeft,
    &marginData.mBorderWidth.mLeft
    // XXX add &marginData.mBorder{Start,End}{Width,Color,Style}
  };

  nsCSSValue* paddingValues[] = {
    &marginData.mPadding.mTop,
    &marginData.mPadding.mRight,
    &marginData.mPadding.mBottom,
    &marginData.mPadding.mLeft,
    &marginData.mPaddingStart,
    &marginData.mPaddingEnd
  };

  nsCSSValue* values[NS_ARRAY_LENGTH(backgroundValues) +
                     NS_ARRAY_LENGTH(borderValues) +
                     NS_ARRAY_LENGTH(paddingValues)];

  if (ruleTypeMask & NS_AUTHOR_SPECIFIED_BACKGROUND) {
    memcpy(&values[nValues], backgroundValues, NS_ARRAY_LENGTH(backgroundValues) * sizeof(nsCSSValue*));
    nValues += NS_ARRAY_LENGTH(backgroundValues);
  }

  if (ruleTypeMask & NS_AUTHOR_SPECIFIED_BORDER) {
    memcpy(&values[nValues], borderValues, NS_ARRAY_LENGTH(borderValues) * sizeof(nsCSSValue*));
    nValues += NS_ARRAY_LENGTH(borderValues);
  }

  if (ruleTypeMask & NS_AUTHOR_SPECIFIED_PADDING) {
    memcpy(&values[nValues], paddingValues, NS_ARRAY_LENGTH(paddingValues) * sizeof(nsCSSValue*));
    nValues += NS_ARRAY_LENGTH(paddingValues);
  }

  nsStyleContext* styleContext = aStyleContext;

  // We need to be careful not to count styles covered up by user-important or
  // UA-important declarations.  But we do want to catch explicit inherit
  // styling in those and check our parent style context to see whether we have
  // user styling for those properties.  Note that we don't care here about
  // inheritance due to lack of a specified value, since all the properties we
  // care about are reset properties.
  PRBool haveExplicitUAInherit;
  do {
    haveExplicitUAInherit = PR_FALSE;
    for (nsRuleNode* ruleNode = styleContext->GetRuleNode(); ruleNode;
         ruleNode = ruleNode->GetParent()) {
      nsIStyleRule *rule = ruleNode->GetRule();
      if (rule) {
        ruleData.mLevel = ruleNode->GetLevel();
        ruleData.mIsImportantRule = ruleNode->IsImportantRule();
        rule->MapRuleInfoInto(&ruleData);
        // Do the same nulling out as in GetBorderData, GetBackgroundData
        // or GetPaddingData.
        // We are sharing with some style rule.  It really owns the data.
        marginData.mBoxShadow = nsnull;

        if (ruleData.mLevel == nsStyleSet::eAgentSheet ||
            ruleData.mLevel == nsStyleSet::eUserSheet) {
          // This is a rule whose effect we want to ignore, so if any of
          // the properties we care about were set, set them to the dummy
          // value that they'll never otherwise get.
          for (PRUint32 i = 0; i < nValues; ++i) {
            nsCSSUnit unit = values[i]->GetUnit();
            if (unit != eCSSUnit_Null &&
                unit != eCSSUnit_Dummy &&
                unit != eCSSUnit_DummyInherit) {
              if (unit == eCSSUnit_Inherit) {
                haveExplicitUAInherit = PR_TRUE;
                values[i]->SetDummyInheritValue();
              } else {
                values[i]->SetDummyValue();
              }
            }
          }
        } else {
          // If any of the values we care about was set by the above rule,
          // we have author style.
          for (PRUint32 i = 0; i < nValues; ++i)
            if (values[i]->GetUnit() != eCSSUnit_Null &&
                values[i]->GetUnit() != eCSSUnit_Dummy && // see above
                values[i]->GetUnit() != eCSSUnit_DummyInherit)
              return PR_TRUE;
        }
      }
    }

    if (haveExplicitUAInherit) {
      // reset all the eCSSUnit_Null values to eCSSUnit_Dummy (since they're
      // not styled by the author, or by anyone else), and then reset all the
      // eCSSUnit_DummyInherit values to eCSSUnit_Null (so we will be able to
      // detect them being styled by the author) and move up to our parent
      // style context.
      for (PRUint32 i = 0; i < nValues; ++i)
        if (values[i]->GetUnit() == eCSSUnit_Null)
          values[i]->SetDummyValue();
      for (PRUint32 i = 0; i < nValues; ++i)
        if (values[i]->GetUnit() == eCSSUnit_DummyInherit)
          values[i]->Reset();
      styleContext = styleContext->GetParent();
    }
  } while (haveExplicitUAInherit && styleContext);

  return PR_FALSE;
}
