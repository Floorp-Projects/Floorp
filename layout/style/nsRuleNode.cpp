/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *   Daniel Glazman <glazman@netscape.com>
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nscore.h"
#include "nsIServiceManager.h"
#include "nsRuleNode.h"
#include "nsIDeviceContext.h"
#include "nsILookAndFeel.h"
#include "nsIPresShell.h"
#include "nsIFontMetrics.h"
#include "nsIDocShellTreeItem.h"
#include "nsStyleUtil.h"
#include "nsCSSPseudoElements.h"
#include "nsThemeConstants.h"
#include "nsITheme.h"
#include "pldhash.h"

/*
 * For storage of an |nsRuleNode|'s children in a linked list.
 */
struct nsRuleList {
  nsRuleNode* mRuleNode;
  nsRuleList* mNext;
  
public:
  nsRuleList(nsRuleNode* aNode, nsRuleList* aNext= nsnull) {
    MOZ_COUNT_CTOR(nsRuleList);
    mRuleNode = aNode;
    mNext = aNext;
  }
 
  ~nsRuleList() {
    MOZ_COUNT_DTOR(nsRuleList);
    mRuleNode->Destroy();
    if (mNext)
      mNext->Destroy(mNext->mRuleNode->mPresContext);
  }

  void* operator new(size_t sz, nsIPresContext* aContext) CPP_THROW_NEW {
    void* result = nsnull;
    aContext->AllocateFromShell(sz, &result);
    return result;
  };
  void operator delete(void* aPtr) {} // Does nothing. The arena will free us up when the rule tree
                                      // dies.

  void Destroy(nsIPresContext* aContext) {
    this->~nsRuleList();
    aContext->FreeToShell(sizeof(nsRuleList), this);
  }

  // Destroy this node, but not its rule node or the rest of the list.
  nsRuleList* DestroySelf(nsIPresContext* aContext) {
    nsRuleList *next = mNext;
    MOZ_COUNT_DTOR(nsRuleList); // hack
    aContext->FreeToShell(sizeof(nsRuleList), this);
    return next;
  }
};

/*
 * For storage of an |nsRuleNode|'s children in a PLDHashTable.
 */

struct ChildrenHashEntry : public PLDHashEntryHdr {
  // key (the rule) is |mRuleNode->Rule()|
  nsRuleNode *mRuleNode;
};

PR_STATIC_CALLBACK(const void *)
ChildrenHashGetKey(PLDHashTable *table, PLDHashEntryHdr *hdr)
{
  ChildrenHashEntry *entry = NS_STATIC_CAST(ChildrenHashEntry*, hdr);
  return entry->mRuleNode->Rule();
}

PR_STATIC_CALLBACK(PRBool)
ChildrenHashMatchEntry(PLDHashTable *table, const PLDHashEntryHdr *hdr,
                         const void *key)
{
  const ChildrenHashEntry *entry =
    NS_STATIC_CAST(const ChildrenHashEntry*, hdr);
  return entry->mRuleNode->Rule() == key;
}

static PLDHashTableOps ChildrenHashOps = {
  // It's probably better to allocate the table itself using malloc and
  // free rather than the pres shell's arena because the table doesn't
  // grow very often and the pres shell's arena doesn't recycle very
  // large size allocations.
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  ChildrenHashGetKey,
  PL_DHashVoidPtrKeyStub,
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
    break;

  case NS_STYLE_DISPLAY_TABLE :
  case NS_STYLE_DISPLAY_BLOCK :
    // do not muck with these at all - already blocks
    break;

  case NS_STYLE_DISPLAY_LIST_ITEM :
    // do not change list items to blocks - retain the bullet/numbering
    break;

  case NS_STYLE_DISPLAY_TABLE_ROW_GROUP :
  case NS_STYLE_DISPLAY_TABLE_COLUMN :
  case NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP :
  case NS_STYLE_DISPLAY_TABLE_HEADER_GROUP :
  case NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP :
  case NS_STYLE_DISPLAY_TABLE_ROW :
  case NS_STYLE_DISPLAY_TABLE_CELL :
  case NS_STYLE_DISPLAY_TABLE_CAPTION :
    // special cases: don't do anything since these cannot really be floated anyway
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

nsString& Unquote(nsString& aString)
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

nscoord CalcLength(const nsCSSValue& aValue,
                   const nsFont* aFont, 
                   nsIStyleContext* aStyleContext,
                   nsIPresContext* aPresContext,
                   PRBool& aInherited)
{
  NS_ASSERTION(aValue.IsLengthUnit(), "not a length unit");
  if (aValue.IsFixedLengthUnit()) {
    return aValue.GetLengthTwips();
  }
  nsCSSUnit unit = aValue.GetUnit();
  switch (unit) {
    case eCSSUnit_EM:
    case eCSSUnit_Char: {
      aInherited = PR_TRUE;
      const nsFont* font = aStyleContext ? &(((nsStyleFont*)aStyleContext->GetStyleData(eStyleStruct_Font))->mFont) : aFont;
      return NSToCoordRound(aValue.GetFloatValue() * (float)font->size);
      // XXX scale against font metrics height instead?
    }
    case eCSSUnit_EN: {
      aInherited = PR_TRUE;
      const nsFont* font = aStyleContext ? &(((nsStyleFont*)aStyleContext->GetStyleData(eStyleStruct_Font))->mFont) : aFont;
      return NSToCoordRound((aValue.GetFloatValue() * (float)font->size) / 2.0f);
    }
    case eCSSUnit_XHeight: {
      aInherited = PR_TRUE;
      const nsFont* font = aStyleContext ? &(((nsStyleFont*)aStyleContext->GetStyleData(eStyleStruct_Font))->mFont) : aFont;
      nsCOMPtr<nsIFontMetrics> fm;
      aPresContext->GetMetricsFor(*font, getter_AddRefs(fm));
      nscoord xHeight;
      fm->GetXHeight(xHeight);
      return NSToCoordRound(aValue.GetFloatValue() * (float)xHeight);
    }
    case eCSSUnit_CapHeight: {
      NS_NOTYETIMPLEMENTED("cap height unit");
      aInherited = PR_TRUE;
      const nsFont* font = aStyleContext ? &(((nsStyleFont*)aStyleContext->GetStyleData(eStyleStruct_Font))->mFont) : aFont;
      nscoord capHeight = ((font->size / 3) * 2); // XXX HACK!
      return NSToCoordRound(aValue.GetFloatValue() * (float)capHeight);
    }
    case eCSSUnit_Pixel:
      float p2t;
      aPresContext->GetScaledPixelsToTwips(&p2t);
      return NSFloatPixelsToTwips(aValue.GetFloatValue(), p2t);
    default:
      NS_NOTREACHED("unexpected unit");
      break;
  }
  return 0;
}

#define SETCOORD_NORMAL       0x01   // N
#define SETCOORD_AUTO         0x02   // A
#define SETCOORD_INHERIT      0x04   // H
#define SETCOORD_PERCENT      0x08   // P
#define SETCOORD_FACTOR       0x10   // F
#define SETCOORD_LENGTH       0x20   // L
#define SETCOORD_INTEGER      0x40   // I
#define SETCOORD_ENUMERATED   0x80   // E

#define SETCOORD_LP     (SETCOORD_LENGTH | SETCOORD_PERCENT)
#define SETCOORD_LH     (SETCOORD_LENGTH | SETCOORD_INHERIT)
#define SETCOORD_AH     (SETCOORD_AUTO | SETCOORD_INHERIT)
#define SETCOORD_LPH    (SETCOORD_LP | SETCOORD_INHERIT)
#define SETCOORD_LPAH   (SETCOORD_LP | SETCOORD_AH)
#define SETCOORD_LPEH   (SETCOORD_LP | SETCOORD_ENUMERATED | SETCOORD_INHERIT)
#define SETCOORD_LE     (SETCOORD_LENGTH | SETCOORD_ENUMERATED)
#define SETCOORD_LEH    (SETCOORD_LE | SETCOORD_INHERIT)
#define SETCOORD_IA     (SETCOORD_INTEGER | SETCOORD_AUTO)
#define SETCOORD_LAE    (SETCOORD_LENGTH | SETCOORD_AUTO | SETCOORD_ENUMERATED)

static PRBool SetCoord(const nsCSSValue& aValue, nsStyleCoord& aCoord, 
                       const nsStyleCoord& aParentCoord,
                       PRInt32 aMask, nsIStyleContext* aStyleContext,
                       nsIPresContext* aPresContext, PRBool& aInherited)
{
  PRBool  result = PR_TRUE;
  if (aValue.GetUnit() == eCSSUnit_Null) {
    result = PR_FALSE;
  }
  else if (((aMask & SETCOORD_LENGTH) != 0) && 
           (aValue.GetUnit() == eCSSUnit_Char)) {
    aCoord.SetIntValue(NSToIntFloor(aValue.GetFloatValue()), eStyleUnit_Chars);
  } 
  else if (((aMask & SETCOORD_LENGTH) != 0) && 
           aValue.IsLengthUnit()) {
    aCoord.SetCoordValue(CalcLength(aValue, nsnull, aStyleContext, aPresContext, aInherited));
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
    nsStyleUnit unit = aParentCoord.GetUnit();
    if ((eStyleUnit_Null == unit) ||  // parent has explicit computed value
        (eStyleUnit_Factor == unit) ||
        (eStyleUnit_Coord == unit) ||
        (eStyleUnit_Integer == unit) ||
        (eStyleUnit_Enumerated == unit) ||
        (eStyleUnit_Normal == unit) ||
        (eStyleUnit_Chars == unit)) {
      aCoord = aParentCoord;  // just inherit value from parent
      aInherited = PR_TRUE;
    }
    else {
      aCoord.SetInheritValue(); // needs to be computed by client
                                // Since this works just like being
                                // specified and not inherited, that's
                                // how it's treated.
    }
  }
  else if (((aMask & SETCOORD_NORMAL) != 0) && 
           (aValue.GetUnit() == eCSSUnit_Normal)) {
    aCoord.SetNormalValue();
  }
  else if (((aMask & SETCOORD_FACTOR) != 0) && 
           (aValue.GetUnit() == eCSSUnit_Number)) {
    aCoord.SetFactorValue(aValue.GetFloatValue());
  }
  else {
    result = PR_FALSE;  // didn't set anything
  }
  return result;
}

static PRBool SetColor(const nsCSSValue& aValue, const nscolor aParentColor, 
                       nsIPresContext* aPresContext, nscolor& aResult, PRBool& aInherited)
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
  else if (eCSSUnit_Integer == unit) {
    PRInt32 intValue = aValue.GetIntValue();
    if (0 <= intValue) {
      nsILookAndFeel* look = nsnull;
      if (NS_SUCCEEDED(aPresContext->GetLookAndFeel(&look)) && look) {
        nsILookAndFeel::nsColorID colorID = (nsILookAndFeel::nsColorID) intValue;
        if (NS_SUCCEEDED(look->GetColor(colorID, aResult))) {
          result = PR_TRUE;
        }
        NS_RELEASE(look);
      }
    }
    else {
      switch (intValue) {
        case NS_COLOR_MOZ_HYPERLINKTEXT:
          if (NS_SUCCEEDED(aPresContext->GetDefaultLinkColor(&aResult))) {
            result = PR_TRUE;
          }
          break;
        case NS_COLOR_MOZ_VISITEDHYPERLINKTEXT:
          if (NS_SUCCEEDED(aPresContext->GetDefaultVisitedLinkColor(&aResult))) {
            result = PR_TRUE;
          }
          break;
        default:
          NS_NOTREACHED("Should never have an unknown negative colorID.");
          break;
      }
    }
  }
  else if (eCSSUnit_Inherit == unit) {
    aResult = aParentColor;
    result = PR_TRUE;
    aInherited = PR_TRUE;
  }
  return result;
}

// Overloaded new operator. Initializes the memory to 0 and relies on an arena
// (which comes from the presShell) to perform the allocation.
void* 
nsRuleNode::operator new(size_t sz, nsIPresContext* aPresContext) CPP_THROW_NEW
{
  // Check the recycle list first.
  void* result = nsnull;
  aPresContext->AllocateFromShell(sz, &result);
  return result;
}

// Overridden to prevent the global delete from being called, since the memory
// came out of an nsIArena instead of the global delete operator's heap.
void 
nsRuleNode::Destroy()
{
  // Destroy ourselves.
  this->~nsRuleNode();
  
  // Don't let the memory be freed, since it will be recycled
  // instead. Don't call the global operator delete.
  mPresContext->FreeToShell(sizeof(nsRuleNode), this);
}

void nsRuleNode::CreateRootNode(nsIPresContext* aPresContext, nsRuleNode** aResult)
{
  *aResult = new (aPresContext) nsRuleNode(aPresContext);
}

nsILanguageAtomService* nsRuleNode::gLangService = nsnull;

nsRuleNode::nsRuleNode(nsIPresContext* aContext, nsIStyleRule* aRule, nsRuleNode* aParent)
  : mPresContext(aContext),
    mParent(aParent),
    mRule(aRule),
    mChildrenTaggedPtr(nsnull),
    mDependentBits(0),
    mNoneBits(0)
{
  MOZ_COUNT_CTOR(nsRuleNode);
}

PR_STATIC_CALLBACK(PLDHashOperator)
DeleteRuleNodeChildren(PLDHashTable *table, PLDHashEntryHdr *hdr,
                       PRUint32 number, void *arg)
{
  ChildrenHashEntry *entry = NS_STATIC_CAST(ChildrenHashEntry*, hdr);
  entry->mRuleNode->Destroy();
  return PL_DHASH_NEXT;
}

nsRuleNode::~nsRuleNode()
{
  MOZ_COUNT_DTOR(nsRuleNode);
  if (mStyleData.mResetData || mStyleData.mInheritedData)
    mStyleData.Destroy(0, mPresContext);
  if (ChildrenAreHashed()) {
    PLDHashTable *children = ChildrenHash();
    PL_DHashTableEnumerate(children, DeleteRuleNodeChildren, nsnull);
    PL_DHashTableDestroy(children);
  } else if (HaveChildren())
    ChildrenList()->Destroy(mPresContext);
}

nsresult 
nsRuleNode::GetBits(PRInt32 aType, PRUint32* aResult)
{
  switch (aType) {
    case eNoneBits :      *aResult = mNoneBits;      break;
    case eDependentBits : *aResult = mDependentBits; break;
    default:
      NS_ERROR("invalid arg");
      return NS_ERROR_INVALID_ARG;
  }
  return NS_OK;
}

nsresult 
nsRuleNode::Transition(nsIStyleRule* aRule, PRBool aIsInlineStyle, nsRuleNode** aResult)
{
  nsRuleNode* next = nsnull;

  if (HaveChildren() && !ChildrenAreHashed()) {
    PRInt32 numKids = 0;
    nsRuleList* curr = ChildrenList();
    while (curr && curr->mRuleNode->mRule != aRule) {
      curr = curr->mNext;
      ++numKids;
    }
    if (curr)
      next = curr->mRuleNode;
    else if (numKids >= kMaxChildrenInList)
      ConvertChildrenToHash();
  }

  PRBool createdNode = PR_FALSE;
  if (ChildrenAreHashed()) {
    ChildrenHashEntry *entry = NS_STATIC_CAST(ChildrenHashEntry*,
        PL_DHashTableOperate(ChildrenHash(), aRule, PL_DHASH_ADD));
    if (entry->mRuleNode)
      next = entry->mRuleNode;
    else {
      next = entry->mRuleNode =
          new (mPresContext) nsRuleNode(mPresContext, aRule, this);
      if (!next) {
        PL_DHashTableRawRemove(ChildrenHash(), entry);
        *aResult = nsnull;
        return NS_ERROR_OUT_OF_MEMORY;
      }
      createdNode = PR_TRUE;
    }
  } else if (!next) {
    // Create the new entry in our list.
    next = new (mPresContext) nsRuleNode(mPresContext, aRule, this);
    if (!next) {
      *aResult = nsnull;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    SetChildrenList(new (mPresContext) nsRuleList(next, ChildrenList()));
    createdNode = PR_TRUE;
  }
    
  if (aIsInlineStyle && createdNode) {
    // We just made a new rule node for an inline style rule (e.g., for
    // the style attribute on an HTML, SVG, or XUL element).  In order to
    // fix bug 99344, we have to maintain a mapping from inline style rules
    // to all the rule nodes in a rule tree that correspond to the inline
    // style rule.  
    // Obtain our style set and then add this rule node to our mapping.
    nsCOMPtr<nsIPresShell> shell;
    mPresContext->GetShell(getter_AddRefs(shell));
    nsCOMPtr<nsIStyleSet> styleSet;
    shell->GetStyleSet(getter_AddRefs(styleSet));
    styleSet->AddRuleNodeMapping(next);
  }

  *aResult = next;
  return NS_OK;
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
  for (nsRuleList* curr = ChildrenList(); curr;
       curr = curr->DestroySelf(mPresContext)) {
    // This will never fail because of the initial size we gave the table.
    ChildrenHashEntry *entry = NS_STATIC_CAST(ChildrenHashEntry*,
        PL_DHashTableOperate(hash, curr->mRuleNode->mRule, PL_DHASH_ADD));
    NS_ASSERTION(!entry->mRuleNode, "duplicate entries in list");
    entry->mRuleNode = curr->mRuleNode;
  }
  SetChildrenHash(hash);
}

nsresult
nsRuleNode::PathContainsRule(nsIStyleRule* aRule, PRBool* aMatched)
{
  *aMatched = PR_FALSE;
  nsRuleNode* ruleDest = this;
  while (ruleDest) {
    if (ruleDest->mRule == aRule) {
      *aMatched = PR_TRUE;
      break;
    }
    ruleDest = ruleDest->mParent;
  }

  return NS_OK;
}

nsresult
nsRuleNode::ClearCachedData(nsIStyleRule* aRule)
{
  nsRuleNode* ruleDest = this;
  while (ruleDest) {
    if (ruleDest->mRule == aRule)
      break;
    ruleDest = ruleDest->mParent;
  }

  if (ruleDest) {
    // The rule was contained along our branch.  We need to blow away
    // all cached data along this path.  Note that, because of the definition
    // of inline style, all nodes along this path must have exactly one child.  This
    // is not a bushy subtree, and so we know that by clearing this path, we've
    // invalidated everything that we need to.
    // XXXldb What about !important :hover rules, and such?
    nsRuleNode* curr = this;
    while (curr) {
      curr->mNoneBits &= ~NS_STYLE_INHERIT_MASK;
      curr->mDependentBits &= ~NS_STYLE_INHERIT_MASK;
      if (curr->mStyleData.mResetData || curr->mStyleData.mInheritedData)
        curr->mStyleData.Destroy(0, mPresContext);

      if (curr == ruleDest)
        break;

      curr = curr->mParent;
    }
  }

  return NS_OK;
}

PR_STATIC_CALLBACK(PLDHashOperator)
ClearCachedDataInSubtreeHelper(PLDHashTable *table, PLDHashEntryHdr *hdr,
                               PRUint32 number, void *arg)
{
  ChildrenHashEntry *entry = NS_STATIC_CAST(ChildrenHashEntry*, hdr);
  nsIStyleRule* rule = NS_STATIC_CAST(nsIStyleRule*, arg);
  entry->mRuleNode->ClearCachedDataInSubtree(rule);
  return PL_DHASH_NEXT;
}

nsresult
nsRuleNode::ClearCachedDataInSubtree(nsIStyleRule* aRule)
{
  if (aRule == nsnull || mRule == aRule) {
    // We have a match.  Blow away all data stored at this node.
    if (mStyleData.mResetData || mStyleData.mInheritedData)
      mStyleData.Destroy(0, mPresContext);
    mNoneBits &= ~NS_STYLE_INHERIT_MASK;
    mDependentBits &= ~NS_STYLE_INHERIT_MASK;
    aRule = nsnull;  // Cause everything to be blown away in the descendants.
  }

  if (ChildrenAreHashed())
    PL_DHashTableEnumerate(ChildrenHash(),
                           ClearCachedDataInSubtreeHelper, aRule);
  else
    for (nsRuleList* curr = ChildrenList(); curr; curr = curr->mNext)
      curr->mRuleNode->ClearCachedDataInSubtree(aRule);

  return NS_OK;
}

nsresult
nsRuleNode::GetPresContext(nsIPresContext** aResult)
{
  *aResult = mPresContext;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
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
  if (mDependentBits & aBit)
    return; // Already set.

  nsRuleNode* curr = this;
  while (curr != aHighestNode) {
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
    curr = curr->mParent;
  }
}

/*
 * The following "Check" functions are used for determining what type of
 * sharing can be used for the data on this rule node.  MORE HERE...
 */

/* the information for a property (or in some cases, a rect group of
   properties) */

// for PropertyCheckData::type
// XXX Would bits be more efficient?
#define CHECKDATA_VALUE           0
#define CHECKDATA_RECT            1
#define CHECKDATA_VALUELIST       2
#define CHECKDATA_COUNTERDATA     3
#define CHECKDATA_QUOTES          4
#define CHECKDATA_SHADOW          5
#define CHECKDATA_VALUELIST_ARRAY 6

struct PropertyCheckData {
  size_t offset;
  PRUint16 type;
  PRPackedBool mayHaveExplicitInherit;
};

#define CHECKDATA_PROP(_datastruct, _member, _type, _iscoord) \
  { offsetof(_datastruct, _member), _type, _iscoord }

/* the information for all the properties in a style struct */

typedef nsRuleNode::RuleDetail
  (* PR_CALLBACK CheckCallbackFn)(const nsRuleDataStruct& aData);

struct StructCheckData {
  const PropertyCheckData* props;
  PRInt32 nprops;
  CheckCallbackFn callback;
};

static void
ExamineRectProperties(const nsCSSRect* aRect,
                      PRUint32& aSpecifiedCount, PRUint32& aInheritedCount)
{
  if (!aRect)
    return;

  if (eCSSUnit_Null != aRect->mLeft.GetUnit()) {
    aSpecifiedCount++;
    if (eCSSUnit_Inherit == aRect->mLeft.GetUnit())
      aInheritedCount++;
  }

  if (eCSSUnit_Null != aRect->mTop.GetUnit()) {
    aSpecifiedCount++;
    if (eCSSUnit_Inherit == aRect->mTop.GetUnit())
      aInheritedCount++;
  }

  if (eCSSUnit_Null != aRect->mRight.GetUnit()) {
    aSpecifiedCount++;
    if (eCSSUnit_Inherit == aRect->mRight.GetUnit())
      aInheritedCount++;
  }

  if (eCSSUnit_Null != aRect->mBottom.GetUnit()) {
    aSpecifiedCount++;
    if (eCSSUnit_Inherit == aRect->mBottom.GetUnit())
      aInheritedCount++;
  }
}

static void
ExamineRectCoordProperties(const nsCSSRect* aRect,
                           PRUint32& aSpecifiedCount,
                           PRUint32& aInheritedCount,
                           PRBool& aCanHaveExplicitInherit)
{
  if (!aRect)
    return;

  if (eCSSUnit_Null != aRect->mLeft.GetUnit()) {
    aSpecifiedCount++;
    if (eCSSUnit_Inherit == aRect->mLeft.GetUnit()) {
      aInheritedCount++;
      aCanHaveExplicitInherit = PR_TRUE;
    }
  }

  if (eCSSUnit_Null != aRect->mTop.GetUnit()) {
    aSpecifiedCount++;
    if (eCSSUnit_Inherit == aRect->mTop.GetUnit()) {
      aInheritedCount++;
      aCanHaveExplicitInherit = PR_TRUE;
    }
  }

  if (eCSSUnit_Null != aRect->mRight.GetUnit()) {
    aSpecifiedCount++;
    if (eCSSUnit_Inherit == aRect->mRight.GetUnit()) {
      aInheritedCount++;
      aCanHaveExplicitInherit = PR_TRUE;
    }
  }

  if (eCSSUnit_Null != aRect->mBottom.GetUnit()) {
    aSpecifiedCount++;
    if (eCSSUnit_Inherit == aRect->mBottom.GetUnit()) {
      aInheritedCount++;
      aCanHaveExplicitInherit = PR_TRUE;
    }
  }
}

PR_STATIC_CALLBACK(nsRuleNode::RuleDetail)
CheckFontCallback(const nsRuleDataStruct& aData)
{
  const nsRuleDataFont& fontData =
      NS_STATIC_CAST(const nsRuleDataFont&, aData);
  if (eCSSUnit_Enumerated == fontData.mFamily.GetUnit()) {
    // A special case. We treat this as a fully specified font,
    // since no other font props are legal with a system font.
    PRInt32 family = fontData.mFamily.GetIntValue();
    if ((family == NS_STYLE_FONT_CAPTION) ||
        (family == NS_STYLE_FONT_ICON) ||
        (family == NS_STYLE_FONT_MENU) ||
        (family == NS_STYLE_FONT_MESSAGE_BOX) ||
        (family == NS_STYLE_FONT_SMALL_CAPTION) ||
        (family == NS_STYLE_FONT_STATUS_BAR) ||
        (family == NS_STYLE_FONT_WINDOW) ||
        (family == NS_STYLE_FONT_DOCUMENT) ||
        (family == NS_STYLE_FONT_WORKSPACE) ||
        (family == NS_STYLE_FONT_DESKTOP) ||
        (family == NS_STYLE_FONT_INFO) ||
        (family == NS_STYLE_FONT_DIALOG) ||
        (family == NS_STYLE_FONT_BUTTON) ||
        (family == NS_STYLE_FONT_PULL_DOWN_MENU) ||
        (family == NS_STYLE_FONT_LIST) ||
        (family == NS_STYLE_FONT_FIELD))
      // Mixed rather than Reset in case another sub-property has
      // an explicit 'inherit'.   XXXperf Could check them.
      return nsRuleNode::eRuleFullMixed;
  }
  return nsRuleNode::eRuleUnknown;
}

static const PropertyCheckData FontCheckProperties[] = {
  CHECKDATA_PROP(nsRuleDataFont, mFamily, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataFont, mStyle, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataFont, mVariant, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataFont, mWeight, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataFont, mSize, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataFont, mSizeAdjust, CHECKDATA_VALUE, PR_FALSE)
};

static const PropertyCheckData DisplayCheckProperties[] = {
  CHECKDATA_PROP(nsRuleDataDisplay, mAppearance, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataDisplay, mClip, CHECKDATA_RECT, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataDisplay, mDisplay, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataDisplay, mBinding, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataDisplay, mPosition, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataDisplay, mFloat, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataDisplay, mClear, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataDisplay, mOverflow, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataDisplay, mBreakBefore, CHECKDATA_VALUE, PR_FALSE), // temp fix for bug 2400
  CHECKDATA_PROP(nsRuleDataDisplay, mBreakAfter, CHECKDATA_VALUE, PR_FALSE)   // temp fix for bug 2400
};

static const PropertyCheckData VisibilityCheckProperties[] = {
  CHECKDATA_PROP(nsRuleDataDisplay, mVisibility, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataDisplay, mDirection, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataDisplay, mLang, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataDisplay, mOpacity, CHECKDATA_VALUE, PR_FALSE)
};

static const PropertyCheckData MarginCheckProperties[] = {
  CHECKDATA_PROP(nsRuleDataMargin, mMargin, CHECKDATA_RECT, PR_TRUE)
};

static const PropertyCheckData BorderCheckProperties[] = {
  CHECKDATA_PROP(nsRuleDataMargin, mBorderWidth, CHECKDATA_RECT, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataMargin, mBorderStyle, CHECKDATA_RECT, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataMargin, mBorderColor, CHECKDATA_RECT, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataMargin, mBorderRadius, CHECKDATA_RECT, PR_TRUE),
  CHECKDATA_PROP(nsRuleDataMargin, mFloatEdge, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataMargin, mBorderColors, CHECKDATA_VALUELIST_ARRAY, PR_FALSE)
};

static const PropertyCheckData PaddingCheckProperties[] = {
  CHECKDATA_PROP(nsRuleDataMargin, mPadding, CHECKDATA_RECT, PR_TRUE)
};

static const PropertyCheckData OutlineCheckProperties[] = {
  CHECKDATA_PROP(nsRuleDataMargin, mOutlineColor, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataMargin, mOutlineWidth, CHECKDATA_VALUE, PR_TRUE),
  CHECKDATA_PROP(nsRuleDataMargin, mOutlineStyle, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataMargin, mOutlineRadius, CHECKDATA_RECT, PR_TRUE)
};

static const PropertyCheckData ListCheckProperties[] = {
  CHECKDATA_PROP(nsRuleDataList, mType, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataList, mImage, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataList, mPosition, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataList, mImageRegion, CHECKDATA_RECT, PR_TRUE)
};

static const PropertyCheckData ColorCheckProperties[] = {
  CHECKDATA_PROP(nsRuleDataColor, mColor, CHECKDATA_VALUE, PR_FALSE)
};

static const PropertyCheckData BackgroundCheckProperties[] = {
  CHECKDATA_PROP(nsRuleDataColor, mBackAttachment, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataColor, mBackRepeat, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataColor, mBackClip, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataColor, mBackColor, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataColor, mBackImage, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataColor, mBackOrigin, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataColor, mBackPositionX, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataColor, mBackPositionY, CHECKDATA_VALUE, PR_FALSE)
};

static const PropertyCheckData PositionCheckProperties[] = {
  CHECKDATA_PROP(nsRuleDataPosition, mOffset, CHECKDATA_RECT, PR_TRUE),
  CHECKDATA_PROP(nsRuleDataPosition, mWidth, CHECKDATA_VALUE, PR_TRUE),
  CHECKDATA_PROP(nsRuleDataPosition, mMinWidth, CHECKDATA_VALUE, PR_TRUE),
  CHECKDATA_PROP(nsRuleDataPosition, mMaxWidth, CHECKDATA_VALUE, PR_TRUE),
  CHECKDATA_PROP(nsRuleDataPosition, mHeight, CHECKDATA_VALUE, PR_TRUE),
  CHECKDATA_PROP(nsRuleDataPosition, mMinHeight, CHECKDATA_VALUE, PR_TRUE),
  CHECKDATA_PROP(nsRuleDataPosition, mMaxHeight, CHECKDATA_VALUE, PR_TRUE),
  CHECKDATA_PROP(nsRuleDataPosition, mBoxSizing, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataPosition, mZIndex, CHECKDATA_VALUE, PR_FALSE)
};

static const PropertyCheckData TableCheckProperties[] = {
  CHECKDATA_PROP(nsRuleDataTable, mLayout, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataTable, mFrame, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataTable, mRules, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataTable, mCols, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataTable, mSpan, CHECKDATA_VALUE, PR_FALSE)
};

static const PropertyCheckData TableBorderCheckProperties[] = {
  CHECKDATA_PROP(nsRuleDataTable, mBorderCollapse, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataTable, mBorderSpacingX, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataTable, mBorderSpacingY, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataTable, mCaptionSide, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataTable, mEmptyCells, CHECKDATA_VALUE, PR_FALSE)
};

static const PropertyCheckData ContentCheckProperties[] = {
  CHECKDATA_PROP(nsRuleDataContent, mContent, CHECKDATA_VALUELIST, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataContent, mMarkerOffset, CHECKDATA_VALUE, PR_TRUE),
  CHECKDATA_PROP(nsRuleDataContent, mCounterIncrement, CHECKDATA_COUNTERDATA, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataContent, mCounterReset, CHECKDATA_COUNTERDATA, PR_FALSE)
};

static const PropertyCheckData QuotesCheckProperties[] = {
  CHECKDATA_PROP(nsRuleDataContent, mQuotes, CHECKDATA_QUOTES, PR_FALSE)
};

static const PropertyCheckData TextCheckProperties[] = {
  CHECKDATA_PROP(nsRuleDataText, mLineHeight, CHECKDATA_VALUE, PR_TRUE),
  CHECKDATA_PROP(nsRuleDataText, mTextIndent, CHECKDATA_VALUE, PR_TRUE),
  CHECKDATA_PROP(nsRuleDataText, mWordSpacing, CHECKDATA_VALUE, PR_TRUE),
  CHECKDATA_PROP(nsRuleDataText, mLetterSpacing, CHECKDATA_VALUE, PR_TRUE),
  CHECKDATA_PROP(nsRuleDataText, mTextAlign, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataText, mTextTransform, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataText, mWhiteSpace, CHECKDATA_VALUE, PR_FALSE)
};

static const PropertyCheckData TextResetCheckProperties[] = {
  CHECKDATA_PROP(nsRuleDataText, mDecoration, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataText, mVerticalAlign, CHECKDATA_VALUE, PR_TRUE),
  CHECKDATA_PROP(nsRuleDataText, mUnicodeBidi, CHECKDATA_VALUE, PR_FALSE)
};

static const PropertyCheckData UserInterfaceCheckProperties[] = {
  CHECKDATA_PROP(nsRuleDataUserInterface, mUserInput, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataUserInterface, mUserModify, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataUserInterface, mUserFocus, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataUserInterface, mCursor, CHECKDATA_VALUELIST, PR_FALSE)
};

static const PropertyCheckData UIResetCheckProperties[] = {
  CHECKDATA_PROP(nsRuleDataUserInterface, mUserSelect, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataUserInterface, mResizer, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataUserInterface, mKeyEquivalent, CHECKDATA_VALUELIST, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataUserInterface, mForceBrokenImageIcon, CHECKDATA_VALUE, PR_FALSE)
};

#ifdef INCLUDE_XUL
static const PropertyCheckData XULCheckProperties[] = {
  CHECKDATA_PROP(nsRuleDataXUL, mBoxAlign, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataXUL, mBoxDirection, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataXUL, mBoxFlex, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataXUL, mBoxOrient, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataXUL, mBoxPack, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataXUL, mBoxOrdinal, CHECKDATA_VALUE, PR_FALSE)
};
#endif

#ifdef MOZ_SVG
static const PropertyCheckData SVGCheckProperties[] = {
  CHECKDATA_PROP(nsRuleDataSVG, mFill, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataSVG, mFillOpacity, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataSVG, mFillRule, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataSVG, mStroke, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataSVG, mStrokeDasharray, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataSVG, mStrokeDashoffset, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataSVG, mStrokeLinecap, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataSVG, mStrokeLinejoin, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataSVG, mStrokeMiterlimit, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataSVG, mStrokeOpacity, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsRuleDataSVG, mStrokeWidth, CHECKDATA_VALUE, PR_FALSE) 
};
#endif
  
static const StructCheckData gCheckProperties[] = {

#define STYLE_STRUCT(name, checkdata_cb) \
  {name##CheckProperties, \
   sizeof(name##CheckProperties)/sizeof(PropertyCheckData), \
   checkdata_cb},
#include "nsStyleStructList.h"
#undef STYLE_STRUCT
  {nsnull, 0, nsnull}

};



// XXXldb Taking the address of a reference is evil.

inline const nsCSSValue&
ValueAtOffset(const nsRuleDataStruct& aRuleDataStruct, size_t aOffset)
{
  return * NS_REINTERPRET_CAST(const nsCSSValue*,
                     NS_REINTERPRET_CAST(const char*, &aRuleDataStruct) + aOffset);
}

inline const nsCSSRect*
RectAtOffset(const nsRuleDataStruct& aRuleDataStruct, size_t aOffset)
{
  return * NS_REINTERPRET_CAST(const nsCSSRect*const*,
                     NS_REINTERPRET_CAST(const char*, &aRuleDataStruct) + aOffset);
}

inline const nsCSSValueList*
ValueListAtOffset(const nsRuleDataStruct& aRuleDataStruct, size_t aOffset)
{
  return * NS_REINTERPRET_CAST(const nsCSSValueList*const*,
                     NS_REINTERPRET_CAST(const char*, &aRuleDataStruct) + aOffset);
}

inline const nsCSSValueList**
ValueListArrayAtOffset(const nsRuleDataStruct& aRuleDataStruct, size_t aOffset)
{
  return * NS_REINTERPRET_CAST(const nsCSSValueList**const*,
                     NS_REINTERPRET_CAST(const char*, &aRuleDataStruct) + aOffset);
}

inline const nsCSSCounterData*
CounterDataAtOffset(const nsRuleDataStruct& aRuleDataStruct, size_t aOffset)
{
  return * NS_REINTERPRET_CAST(const nsCSSCounterData*const*,
                     NS_REINTERPRET_CAST(const char*, &aRuleDataStruct) + aOffset);
}

inline const nsCSSQuotes*
QuotesAtOffset(const nsRuleDataStruct& aRuleDataStruct, size_t aOffset)
{
  return * NS_REINTERPRET_CAST(const nsCSSQuotes*const*,
                     NS_REINTERPRET_CAST(const char*, &aRuleDataStruct) + aOffset);
}

inline nsRuleNode::RuleDetail
nsRuleNode::CheckSpecifiedProperties(const nsStyleStructID aSID,
                                     const nsRuleDataStruct& aRuleDataStruct)
{
  const StructCheckData *structData = gCheckProperties + aSID;
  if (structData->callback) {
    nsRuleNode::RuleDetail res = (*structData->callback)(aRuleDataStruct);
    if (res != eRuleUnknown)
      return res;
  }

  // Build a count of the:
  PRUint32 total = 0,      // total number of props in the struct
           specified = 0,  // number that were specified for this node
           inherited = 0;  // number that were 'inherit' (and not
                           //   eCSSUnit_Inherit) for this node
  PRBool canHaveExplicitInherit = PR_FALSE;

  for (const PropertyCheckData *prop = structData->props,
                           *prop_end = prop + structData->nprops;
       prop != prop_end;
       ++prop)
    switch (prop->type) {

      case CHECKDATA_VALUE:
        {
          ++total;
          const nsCSSValue& value = ValueAtOffset(aRuleDataStruct, prop->offset);
          if (eCSSUnit_Null != value.GetUnit()) {
            ++specified;
            if (eCSSUnit_Inherit == value.GetUnit()) {
              ++inherited;
              if (prop->mayHaveExplicitInherit)
                canHaveExplicitInherit = PR_TRUE;
            }
          }
        }
        break;

      case CHECKDATA_RECT:
        total += 4;
        if (prop->mayHaveExplicitInherit)
          ExamineRectCoordProperties(RectAtOffset(aRuleDataStruct, prop->offset),
                                     specified, inherited,
                                     canHaveExplicitInherit);
        else
          ExamineRectProperties(RectAtOffset(aRuleDataStruct, prop->offset),
                                specified, inherited);
        break;

      case CHECKDATA_VALUELIST:
        {
          ++total;
          const nsCSSValueList* valueList =
              ValueListAtOffset(aRuleDataStruct, prop->offset);
          if (valueList) {
            ++specified;
            if (eCSSUnit_Inherit == valueList->mValue.GetUnit()) {
              ++inherited;
              if (prop->mayHaveExplicitInherit)
                canHaveExplicitInherit = PR_TRUE;
            }
          }
        }
        break;

      case CHECKDATA_COUNTERDATA:
        {
          ++total;
          NS_ASSERTION(!prop->mayHaveExplicitInherit,
                       "counters can't be coordinates");
          const nsCSSCounterData* counterData =
              CounterDataAtOffset(aRuleDataStruct, prop->offset);
          if (counterData) {
            ++specified;
            if (eCSSUnit_Inherit == counterData->mCounter.GetUnit()) {
              ++inherited;
            }
          }
        }
        break;

      case CHECKDATA_QUOTES:
        {
          ++total;
          NS_ASSERTION(!prop->mayHaveExplicitInherit,
                       "quotes can't be coordinates");
          const nsCSSQuotes* quotes =
              QuotesAtOffset(aRuleDataStruct, prop->offset);
          if (quotes) {
            ++specified;
            if (eCSSUnit_Inherit == quotes->mOpen.GetUnit()) {
              ++inherited;
            }
          }
        }
        break;

      case CHECKDATA_VALUELIST_ARRAY:
        {
          total += 4;
          const nsCSSValueList** valueArray = 
            ValueListArrayAtOffset(aRuleDataStruct, prop->offset);
          if (valueArray) {
            for (PRInt32 i = 0; i < 4; i++) {
              const nsCSSValueList* valList = valueArray[i];
              if (valList) {
                ++specified;
                if (eCSSUnit_Inherit == valList->mValue.GetUnit()) {
                  ++inherited;
                  NS_ASSERTION(!prop->mayHaveExplicitInherit, "Value list arrays can't inherit!");
                }
              }
            }
          }
        }
        break;

      case CHECKDATA_SHADOW:
        NS_NOTYETIMPLEMENTED("nsCSSShadow not yet transferred to structs");
        break;

      default:
        NS_NOTREACHED("unknown type");
        break;

    }

#if 0
  printf("CheckSpecifiedProperties: SID=%d total=%d spec=%d inh=%d chei=%s.\n",
    aSID, total, specified, inherited, canHaveExplicitInherit?"true":"false");
#endif

  if (canHaveExplicitInherit) {
    if (specified == total)
      return eRuleFullMixed;
    return eRulePartialMixed;
  }
  if (inherited == total)
    return eRuleFullInherited;
  if (specified == total) {
    if (inherited == 0)
      return eRuleFullReset;
    return eRuleFullMixed;
  }
  if (specified == 0)
    return eRuleNone;
  if (specified == inherited)
    return eRulePartialInherited;
  if (inherited == 0)
    return eRulePartialReset;
  return eRulePartialMixed;
}

const nsStyleStruct*
nsRuleNode::GetDisplayData(nsIStyleContext* aContext, PRBool aComputeData)
{
  nsRuleDataDisplay displayData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_Display, mPresContext, aContext);
  ruleData.mDisplayData = &displayData;

  nsCSSRect clip;
  displayData.mClip = &clip;
  const nsStyleStruct* res = WalkRuleTree(eStyleStruct_Display, aContext, &ruleData, &displayData, aComputeData);
  displayData.mClip = nsnull;
  return res;
}

const nsStyleStruct*
nsRuleNode::GetVisibilityData(nsIStyleContext* aContext, PRBool aComputeData)
{
  nsRuleDataDisplay displayData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_Visibility, mPresContext, aContext);
  ruleData.mDisplayData = &displayData;

  return WalkRuleTree(eStyleStruct_Visibility, aContext, &ruleData, &displayData, aComputeData);
}

const nsStyleStruct*
nsRuleNode::GetTextData(nsIStyleContext* aContext, PRBool aComputeData)
{
  nsRuleDataText textData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_Text, mPresContext, aContext);
  ruleData.mTextData = &textData;

  return WalkRuleTree(eStyleStruct_Text, aContext, &ruleData, &textData, aComputeData);
}

const nsStyleStruct*
nsRuleNode::GetTextResetData(nsIStyleContext* aContext, PRBool aComputeData)
{
  nsRuleDataText textData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_TextReset, mPresContext, aContext);
  ruleData.mTextData = &textData;

  return WalkRuleTree(eStyleStruct_TextReset, aContext, &ruleData, &textData, aComputeData);
}

const nsStyleStruct*
nsRuleNode::GetUserInterfaceData(nsIStyleContext* aContext, PRBool aComputeData)
{
  nsRuleDataUserInterface uiData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_UserInterface, mPresContext, aContext);
  ruleData.mUIData = &uiData;

  const nsStyleStruct* res = WalkRuleTree(eStyleStruct_UserInterface, aContext, &ruleData, &uiData, aComputeData);
  uiData.mCursor = nsnull;
  return res;
}

const nsStyleStruct*
nsRuleNode::GetUIResetData(nsIStyleContext* aContext, PRBool aComputeData)
{
  nsRuleDataUserInterface uiData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_UIReset, mPresContext, aContext);
  ruleData.mUIData = &uiData;

  const nsStyleStruct* res = WalkRuleTree(eStyleStruct_UIReset, aContext, &ruleData, &uiData, aComputeData);
  uiData.mKeyEquivalent = nsnull;
  return res;
}

const nsStyleStruct*
nsRuleNode::GetFontData(nsIStyleContext* aContext, PRBool aComputeData)
{
  nsRuleDataFont fontData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_Font, mPresContext, aContext);
  ruleData.mFontData = &fontData;

  return WalkRuleTree(eStyleStruct_Font, aContext, &ruleData, &fontData, aComputeData);
}

const nsStyleStruct*
nsRuleNode::GetColorData(nsIStyleContext* aContext, PRBool aComputeData)
{
  nsRuleDataColor colorData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_Color, mPresContext, aContext);
  ruleData.mColorData = &colorData;

  return WalkRuleTree(eStyleStruct_Color, aContext, &ruleData, &colorData, aComputeData);
}

const nsStyleStruct*
nsRuleNode::GetBackgroundData(nsIStyleContext* aContext, PRBool aComputeData)
{
  nsRuleDataColor colorData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_Background, mPresContext, aContext);
  ruleData.mColorData = &colorData;

  return WalkRuleTree(eStyleStruct_Background, aContext, &ruleData, &colorData, aComputeData);
}

const nsStyleStruct*
nsRuleNode::GetMarginData(nsIStyleContext* aContext, PRBool aComputeData)
{
  nsRuleDataMargin marginData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_Margin, mPresContext, aContext);
  ruleData.mMarginData = &marginData;

  nsCSSRect margin;
  marginData.mMargin = &margin;
  
  const nsStyleStruct* res = WalkRuleTree(eStyleStruct_Margin, aContext, &ruleData, &marginData, aComputeData);
  
  marginData.mMargin = nsnull;
  return res;
}

const nsStyleStruct*
nsRuleNode::GetBorderData(nsIStyleContext* aContext, PRBool aComputeData)
{
  nsRuleDataMargin marginData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_Border, mPresContext, aContext);
  ruleData.mMarginData = &marginData;

  nsCSSRect borderWidth;
  nsCSSRect borderColor;
  nsCSSRect borderStyle;
  nsCSSRect borderRadius;
  
  nsCSSValueList* borderColors[4];
  for (PRInt32 i = 0; i < 4; i++)
    borderColors[i] = nsnull;

  marginData.mBorderWidth = &borderWidth;
  marginData.mBorderColor = &borderColor;
  marginData.mBorderStyle = &borderStyle;
  marginData.mBorderRadius = &borderRadius;
  marginData.mBorderColors = borderColors;

  const nsStyleStruct* res = WalkRuleTree(eStyleStruct_Border, aContext, &ruleData, &marginData, aComputeData);
  
  marginData.mBorderWidth = marginData.mBorderColor = marginData.mBorderStyle = marginData.mBorderRadius = nsnull;
  marginData.mBorderColors = nsnull;

  return res;
}

const nsStyleStruct*
nsRuleNode::GetPaddingData(nsIStyleContext* aContext, PRBool aComputeData)
{
  nsRuleDataMargin marginData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_Padding, mPresContext, aContext);
  ruleData.mMarginData = &marginData;

  nsCSSRect padding;
  marginData.mPadding = &padding;
  
  const nsStyleStruct* res = WalkRuleTree(eStyleStruct_Padding, aContext, &ruleData, &marginData, aComputeData);
  
  marginData.mPadding = nsnull;
  return res;
}

const nsStyleStruct*
nsRuleNode::GetOutlineData(nsIStyleContext* aContext, PRBool aComputeData)
{
  nsRuleDataMargin marginData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_Outline, mPresContext, aContext);
  ruleData.mMarginData = &marginData;

  nsCSSRect outlineRadius;
  marginData.mOutlineRadius = &outlineRadius;
  
  const nsStyleStruct* res = WalkRuleTree(eStyleStruct_Outline, aContext, &ruleData, &marginData, aComputeData);
  
  marginData.mOutlineRadius = nsnull;
  return res;
}

const nsStyleStruct*
nsRuleNode::GetListData(nsIStyleContext* aContext, PRBool aComputeData)
{
  nsRuleDataList listData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_List, mPresContext, aContext);
  ruleData.mListData = &listData;

  nsCSSRect imageRegion;
  listData.mImageRegion = &imageRegion;
  const nsStyleStruct* res = WalkRuleTree(eStyleStruct_List, aContext, &ruleData, &listData, aComputeData);
  listData.mImageRegion = nsnull;
  
  return res;
}

const nsStyleStruct*
nsRuleNode::GetPositionData(nsIStyleContext* aContext, PRBool aComputeData)
{
  nsRuleDataPosition posData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_Position, mPresContext, aContext);
  ruleData.mPositionData = &posData;

  nsCSSRect offset;
  posData.mOffset = &offset;
  const nsStyleStruct* res = WalkRuleTree(eStyleStruct_Position, aContext, &ruleData, &posData, aComputeData);
  posData.mOffset = nsnull;
  
  return res;
}

const nsStyleStruct*
nsRuleNode::GetTableData(nsIStyleContext* aContext, PRBool aComputeData)
{
  nsRuleDataTable tableData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_Table, mPresContext, aContext);
  ruleData.mTableData = &tableData;

  return WalkRuleTree(eStyleStruct_Table, aContext, &ruleData, &tableData, aComputeData);
}

const nsStyleStruct*
nsRuleNode::GetTableBorderData(nsIStyleContext* aContext, PRBool aComputeData)
{
  nsRuleDataTable tableData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_TableBorder, mPresContext, aContext);
  ruleData.mTableData = &tableData;

  return WalkRuleTree(eStyleStruct_TableBorder, aContext, &ruleData, &tableData, aComputeData);
}

const nsStyleStruct*
nsRuleNode::GetContentData(nsIStyleContext* aContext, PRBool aComputeData)
{
  nsRuleDataContent contentData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_Content, mPresContext, aContext);
  ruleData.mContentData = &contentData;

  const nsStyleStruct* res = WalkRuleTree(eStyleStruct_Content, aContext, &ruleData, &contentData, aComputeData);
  contentData.mCounterIncrement = contentData.mCounterReset = nsnull;
  contentData.mContent = nsnull; // We are sharing with some style rule.  It really owns the data.
  return res;
}

const nsStyleStruct*
nsRuleNode::GetQuotesData(nsIStyleContext* aContext, PRBool aComputeData)
{
  nsRuleDataContent contentData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_Quotes, mPresContext, aContext);
  ruleData.mContentData = &contentData;

  const nsStyleStruct* res = WalkRuleTree(eStyleStruct_Quotes, aContext, &ruleData, &contentData, aComputeData);
  contentData.mQuotes = nsnull; // We are sharing with some style rule.  It really owns the data.
  return res;
}

#ifdef INCLUDE_XUL
const nsStyleStruct*
nsRuleNode::GetXULData(nsIStyleContext* aContext, PRBool aComputeData)
{
  nsRuleDataXUL xulData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_XUL, mPresContext, aContext);
  ruleData.mXULData = &xulData;

  return WalkRuleTree(eStyleStruct_XUL, aContext, &ruleData, &xulData, aComputeData);
}
#endif

#ifdef MOZ_SVG
const nsStyleStruct*
nsRuleNode::GetSVGData(nsIStyleContext* aContext, PRBool aComputeData)
{
  nsRuleDataSVG svgData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_SVG, mPresContext, aContext);
  ruleData.mSVGData = &svgData;

  return WalkRuleTree(eStyleStruct_SVG, aContext, &ruleData, &svgData, aComputeData);
}
#endif

const nsStyleStruct*
nsRuleNode::WalkRuleTree(const nsStyleStructID aSID,
                         nsIStyleContext* aContext, 
                         nsRuleData* aRuleData,
                         nsRuleDataStruct* aSpecificData,
                         PRBool aComputeData)
{
  // We start at the most specific rule in the tree.  
  nsStyleStruct* startStruct = nsnull;
  
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
    if (rule)
      rule->MapRuleInfoInto(aRuleData);

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
  else if (!startStruct && ((!isReset && (detail == eRuleNone || detail == eRulePartialInherited)) 
                             || detail == eRuleFullInherited)) {
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
    nsCOMPtr<nsIStyleContext> parentContext = aContext->GetParent();
    if (parentContext) {
      // We have a parent, and so we should just inherit from the parent.
      // Set the inherit bits on our context.  These bits tell the style context that
      // it never has to go back to the rule tree for data.  Instead the style context tree
      // should be walked to find the data.
      const nsStyleStruct* parentStruct = parentContext->GetStyleData(aSID);
      aContext->AddStyleBit(bit); // makes const_cast OK.
      aContext->SetStyle(aSID, NS_CONST_CAST(nsStyleStruct*, parentStruct));
      return parentStruct;
    }
    else
      // We are the root.  In the case of fonts, the default values just
      // come from the pres context.
      return SetDefaultOnRoot(aSID, aContext);
  }

  // We need to compute the data from the information that the rules specified.
  if (!aComputeData)
    return nsnull;
  
  ComputeStyleDataFn fn = gComputeStyleDataFn[aSID];
  const nsStyleStruct* res = (this->*fn)(startStruct, *aSpecificData, aContext, highestNode, detail,
                                         !aRuleData->mCanStoreInRuleTree);

  // If we have a post-resolve callback, handle that now.
  if (aRuleData->mPostResolveCallback)
    (*aRuleData->mPostResolveCallback)((nsStyleStruct*)res, aRuleData);

  // Now return the result.
  return res;
}

static nscoord
ZoomFont(nsIPresContext* aPresContext, nscoord aInSize)
{
  nsCOMPtr<nsIDeviceContext> dc;
  aPresContext->GetDeviceContext(getter_AddRefs(dc));
  float textZoom;
  dc->GetTextZoom(textZoom);
  return nscoord(aInSize * textZoom);
}

static PRBool
IsChrome(nsIPresContext* aPresContext)
{
  PRBool isChrome = PR_FALSE;
  nsCOMPtr<nsISupports> container;
  nsresult result = aPresContext->GetContainer(getter_AddRefs(container));
  if (NS_SUCCEEDED(result) && container) {
    nsCOMPtr<nsIDocShellTreeItem> docShell(do_QueryInterface(container, &result));
    if (NS_SUCCEEDED(result) && docShell) {
      PRInt32 docShellType;
      result = docShell->GetItemType(&docShellType);
      if (NS_SUCCEEDED(result)) {
        isChrome = nsIDocShellTreeItem::typeChrome == docShellType;
      }
    }
  }
  return isChrome;
}

const nsStyleStruct*
nsRuleNode::SetDefaultOnRoot(const nsStyleStructID aSID, nsIStyleContext* aContext)
{
  switch (aSID) {
    case eStyleStruct_Font: 
    {
      const nsFont* defaultFont;
      mPresContext->GetDefaultFont(kPresContext_DefaultVariableFont_ID, &defaultFont);
      nsStyleFont* fontData = new (mPresContext) nsStyleFont(*defaultFont);
      fontData->mSize = ZoomFont(mPresContext, fontData->mFont.size);

      nscoord minimumFontSize = 0;
      mPresContext->GetCachedIntPref(kPresContext_MinimumFontSize, minimumFontSize);
      if (minimumFontSize > 0 && !IsChrome(mPresContext)) {
        fontData->mFont.size = PR_MAX(fontData->mSize, minimumFontSize);
      }
      else {
        fontData->mFont.size = fontData->mSize;
      }
      aContext->SetStyle(eStyleStruct_Font, fontData);
      return fontData;
    }
    case eStyleStruct_Display:
    {
      nsStyleDisplay* disp = new (mPresContext) nsStyleDisplay();
      aContext->SetStyle(eStyleStruct_Display, disp);
      return disp;
    }
    case eStyleStruct_Visibility:
    {
      nsStyleVisibility* vis = new (mPresContext) nsStyleVisibility(mPresContext);
      aContext->SetStyle(eStyleStruct_Visibility, vis);
      return vis;
    }
    case eStyleStruct_Text:
    {
      nsStyleText* text = new (mPresContext) nsStyleText();
      aContext->SetStyle(eStyleStruct_Text, text);
      return text;
    }
    case eStyleStruct_TextReset:
    {
      nsStyleTextReset* text = new (mPresContext) nsStyleTextReset();
      aContext->SetStyle(eStyleStruct_TextReset, text);
      return text;
    }
    case eStyleStruct_Color:
    {
      nsStyleColor* color = new (mPresContext) nsStyleColor(mPresContext);
      aContext->SetStyle(eStyleStruct_Color, color);
      return color;
    }
    case eStyleStruct_Background:
    {
      nsStyleBackground* bg = new (mPresContext) nsStyleBackground(mPresContext);
      aContext->SetStyle(eStyleStruct_Background, bg);
      return bg;
    }
    case eStyleStruct_Margin:
    {
      nsStyleMargin* margin = new (mPresContext) nsStyleMargin();
      aContext->SetStyle(eStyleStruct_Margin, margin);
      return margin;
    }
    case eStyleStruct_Border:
    {
      nsStyleBorder* border = new (mPresContext) nsStyleBorder(mPresContext);
      aContext->SetStyle(eStyleStruct_Border, border);
      return border;
    }
    case eStyleStruct_Padding:
    {
      nsStylePadding* padding = new (mPresContext) nsStylePadding();
      aContext->SetStyle(eStyleStruct_Padding, padding);
      return padding;
    }
    case eStyleStruct_Outline:
    {
      nsStyleOutline* outline = new (mPresContext) nsStyleOutline(mPresContext);
      aContext->SetStyle(eStyleStruct_Outline, outline);
      return outline;
    }
    case eStyleStruct_List:
    {
      nsStyleList* list = new (mPresContext) nsStyleList();
      aContext->SetStyle(eStyleStruct_List, list);
      return list;
    }
    case eStyleStruct_Position:
    {
      nsStylePosition* pos = new (mPresContext) nsStylePosition();
      aContext->SetStyle(eStyleStruct_Position, pos);
      return pos;
    }
    case eStyleStruct_Table:
    {
      nsStyleTable* table = new (mPresContext) nsStyleTable();
      aContext->SetStyle(eStyleStruct_Table, table);
      return table;
    }
    case eStyleStruct_TableBorder:
    {
      nsStyleTableBorder* table = new (mPresContext) nsStyleTableBorder(mPresContext);
      aContext->SetStyle(eStyleStruct_TableBorder, table);
      return table;
    }
    case eStyleStruct_Content:
    {
      nsStyleContent* content = new (mPresContext) nsStyleContent();
      aContext->SetStyle(eStyleStruct_Content, content);
      return content;
    }
    case eStyleStruct_Quotes:
    {
      nsStyleQuotes* quotes = new (mPresContext) nsStyleQuotes();
      aContext->SetStyle(eStyleStruct_Quotes, quotes);
      return quotes;
    }
    case eStyleStruct_UserInterface:
    {
      nsStyleUserInterface* ui = new (mPresContext) nsStyleUserInterface();
      aContext->SetStyle(eStyleStruct_UserInterface, ui);
      return ui;
    }
    case eStyleStruct_UIReset:
    {
      nsStyleUIReset* ui = new (mPresContext) nsStyleUIReset();
      aContext->SetStyle(eStyleStruct_UIReset, ui);
      return ui;
    }

#ifdef INCLUDE_XUL
    case eStyleStruct_XUL:
    {
      nsStyleXUL* xul = new (mPresContext) nsStyleXUL();
      aContext->SetStyle(eStyleStruct_XUL, xul);
      return xul;
    }
#endif

#ifdef MOZ_SVG
    case eStyleStruct_SVG:
    {
      nsStyleSVG* svg = new (mPresContext) nsStyleSVG();
      aContext->SetStyle(eStyleStruct_SVG, svg);
      return svg;
    }
#endif
  }
  return nsnull;
}

nsRuleNode::ComputeStyleDataFn
nsRuleNode::gComputeStyleDataFn[] = {

#define STYLE_STRUCT(name, checkdata_cb) \
  &nsRuleNode::Compute##name##Data,
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

  nsnull
};

static void
SetFont(nsIPresContext* aPresContext, nsIStyleContext* aContext,
        nscoord aMinFontSize, PRBool aUseDocumentFonts, PRBool aChromeOverride,
        PRBool aIsGeneric, const nsRuleDataFont& aFontData,
        const nsFont& aDefaultFont, const nsStyleFont* aParentFont,
        nsStyleFont* aFont, PRBool aZoom, PRBool& aInherited)
{
  const nsFont* defaultVariableFont;
  const nsFont* defaultFixedFont;
  aPresContext->GetDefaultFont(kPresContext_DefaultVariableFont_ID, &defaultVariableFont);
  aPresContext->GetDefaultFont(kPresContext_DefaultFixedFont_ID, &defaultFixedFont);

  // font-family: string list, enum, inherit
  if (eCSSUnit_String == aFontData.mFamily.GetUnit()) {
    // set the correct font if we are using DocumentFonts OR we are overriding for XUL
    // MJA: bug 31816
    if (aChromeOverride || aUseDocumentFonts) {
      if (!aIsGeneric) {
        // only bother appending fallback fonts if this isn't a fallback generic font itself
        aFont->mFont.name.Append((PRUnichar)',');
        aFont->mFont.name.Append(aDefaultFont.name);
      }
    }
    else {
      // now set to defaults
      aFont->mFont.name = aDefaultFont.name;
    }
    nsCompatibility compat;
    aPresContext->GetCompatibilityMode(&compat);
    aFont->mFont.familyNameQuirks =
        compat == eCompatibility_NavQuirks && aFontData.mFamilyFromHTML;
  }
  else if (eCSSUnit_Enumerated == aFontData.mFamily.GetUnit()) {
    nsSystemFontID sysID;
    switch (aFontData.mFamily.GetIntValue()) {
      // If you add fonts to this list, you need to also patch the list
      // in CheckFontCallback (also in this file).
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

    nsCOMPtr<nsIDeviceContext> dc;
    aPresContext->GetDeviceContext(getter_AddRefs(dc));
    if (dc) {
      // GetSystemFont sets the font face but not necessarily the size
      aFont->mFont.size = defaultVariableFont->size;
      if (NS_FAILED(dc->GetSystemFont(sysID, &aFont->mFont))) {
        aFont->mFont.name = defaultVariableFont->name;
      }
      aFont->mSize = aFont->mFont.size; // this becomes our cascading size
    }

    aFont->mFont.familyNameQuirks = PR_FALSE;

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
        aFont->mSize = PR_MAX(defaultVariableFont->size - NSIntPointsToTwips(2), 0);
        break;
    }
#endif
  }
  else if (eCSSUnit_Inherit == aFontData.mFamily.GetUnit()) {
    aInherited = PR_TRUE;
    aFont->mFont.name = aParentFont->mFont.name;
    aFont->mFont.familyNameQuirks = aParentFont->mFont.familyNameQuirks;
  }
  else if (eCSSUnit_Initial == aFontData.mFamily.GetUnit()) {
    aFont->mFont.name = aDefaultFont.name;
    aFont->mFont.familyNameQuirks = PR_FALSE;
  }

  // font-style: enum, normal, inherit
  if (eCSSUnit_Enumerated == aFontData.mStyle.GetUnit()) {
    aFont->mFont.style = aFontData.mStyle.GetIntValue();
  }
  else if (eCSSUnit_Normal == aFontData.mStyle.GetUnit()) {
    aFont->mFont.style = NS_STYLE_FONT_STYLE_NORMAL;
  }
  else if (eCSSUnit_Inherit == aFontData.mStyle.GetUnit()) {
    aInherited = PR_TRUE;
    aFont->mFont.style = aParentFont->mFont.style;
  }
  else if (eCSSUnit_Initial == aFontData.mStyle.GetUnit()) {
    aFont->mFont.style = aDefaultFont.style;
  }

  // font-variant: enum, normal, inherit
  if (eCSSUnit_Enumerated == aFontData.mVariant.GetUnit()) {
    aFont->mFont.variant = aFontData.mVariant.GetIntValue();
  }
  else if (eCSSUnit_Normal == aFontData.mVariant.GetUnit()) {
    aFont->mFont.variant = NS_STYLE_FONT_VARIANT_NORMAL;
  }
  else if (eCSSUnit_Inherit == aFontData.mVariant.GetUnit()) {
    aInherited = PR_TRUE;
    aFont->mFont.variant = aParentFont->mFont.variant;
  }
  else if (eCSSUnit_Initial == aFontData.mVariant.GetUnit()) {
    aFont->mFont.variant = aDefaultFont.variant;
  }

  // font-weight: int, enum, normal, inherit
  if (eCSSUnit_Integer == aFontData.mWeight.GetUnit()) {
    aFont->mFont.weight = aFontData.mWeight.GetIntValue();
  }
  else if (eCSSUnit_Enumerated == aFontData.mWeight.GetUnit()) {
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
  }
  else if (eCSSUnit_Normal == aFontData.mWeight.GetUnit()) {
    aFont->mFont.weight = NS_STYLE_FONT_WEIGHT_NORMAL;
  }
  else if (eCSSUnit_Inherit == aFontData.mWeight.GetUnit()) {
    aInherited = PR_TRUE;
    aFont->mFont.weight = aParentFont->mFont.weight;
  }
  else if (eCSSUnit_Initial == aFontData.mWeight.GetUnit()) {
    aFont->mFont.weight = aDefaultFont.weight;
  }

  // font-size: enum, length, percent, inherit
  PRBool zoom = aZoom;
  if (eCSSUnit_Enumerated == aFontData.mSize.GetUnit()) {
    PRInt32 value = aFontData.mSize.GetIntValue();
    PRInt32 scaler;
    aPresContext->GetFontScaler(&scaler);
    float scaleFactor = nsStyleUtil::GetScalingFactor(scaler);

    zoom = PR_TRUE;
    if ((NS_STYLE_FONT_SIZE_XXSMALL <= value) && 
        (value <= NS_STYLE_FONT_SIZE_XXLARGE)) {
      aFont->mSize = nsStyleUtil::CalcFontPointSize(value, (PRInt32)aDefaultFont.size, scaleFactor, aPresContext, eFontSize_CSS);
    }
    else if (NS_STYLE_FONT_SIZE_XXXLARGE == value) {
      // <font size="7"> is not specified in CSS, so we don't use eFontSize_CSS.
      aFont->mSize = nsStyleUtil::CalcFontPointSize(value, (PRInt32)aDefaultFont.size, scaleFactor, aPresContext);
    }
    else if (NS_STYLE_FONT_SIZE_LARGER      == value ||
             NS_STYLE_FONT_SIZE_SMALLER     == value) {

      aInherited = PR_TRUE;

      // Un-zoom so we use the tables correctly.  We'll then rezoom due
      // to the |zoom = PR_TRUE| above.
      nsCOMPtr<nsIDeviceContext> dc;
      aPresContext->GetDeviceContext(getter_AddRefs(dc));
      float textZoom;
      dc->GetTextZoom(textZoom);

      nscoord parentSize = nscoord(aParentFont->mSize / textZoom);

      if (NS_STYLE_FONT_SIZE_LARGER == value) {
        PRInt32 index = nsStyleUtil::FindNextLargerFontSize(parentSize, (PRInt32)aDefaultFont.size,
                                                            scaleFactor, aPresContext, eFontSize_CSS);
        nscoord largerSize = nsStyleUtil::CalcFontPointSize(index, (PRInt32)aDefaultFont.size,
                                                            scaleFactor, aPresContext, eFontSize_CSS);
        aFont->mSize = PR_MAX(largerSize, aParentFont->mSize);
        if (index == 6 &&
            aFont->mSize == aParentFont->mSize) {
          // 6 is the maximal answer from nsStyleUtil::FindNextLargerFontSize
          // when eFontSize_CSS parameter is passed; we reached the limit for
          // CSS named sizes, let's apply a %age.
          // 150% is more or less the ratio between xx-large and x-large in
          // all tables defined in nsStyleUtil.cpp
          aFont->mSize = (nscoord)((float)(aParentFont->mSize) * 1.5);
        }
        else if (index == 0) {
          largerSize = (nscoord)((float)(aParentFont->mSize) / 0.9);
          aFont->mSize = PR_MIN(largerSize, aFont->mSize);
        }
      } else {
        PRInt32 index = nsStyleUtil::FindNextSmallerFontSize(parentSize, (PRInt32)aDefaultFont.size,
                                                             scaleFactor, aPresContext, eFontSize_CSS);
        nscoord smallerSize = nsStyleUtil::CalcFontPointSize(index, (PRInt32)aDefaultFont.size,
                                                             scaleFactor, aPresContext, eFontSize_CSS);
        aFont->mSize = PR_MIN(smallerSize, aParentFont->mSize);
        if (index == 0 &&
            aFont->mSize == aParentFont->mSize) {
          // 0 is the miminal answer from nsStyleUtil::FindNextLargerFontSize
          // when eFontSize_CSS parameter is passed; we reached the limit for
          // CSS named sizes, let's apply a %age
          aFont->mSize = (nscoord)((float)(aParentFont->mSize) * 0.9);
        }
        else if (index == 6) {
          smallerSize = (nscoord)((float)(aParentFont->mSize) / 1.5);
          aFont->mSize = PR_MAX(smallerSize, aFont->mSize);
        }
      }
    } else {
      NS_NOTREACHED("unexpected value");
    }
  }
  else if (aFontData.mSize.IsLengthUnit()) {
    aFont->mSize = CalcLength(aFontData.mSize, &aParentFont->mFont, nsnull, aPresContext, aInherited);
    zoom = aFontData.mSize.IsFixedLengthUnit() ||
           aFontData.mSize.GetUnit() == eCSSUnit_Pixel;
  }
  else if (eCSSUnit_Percent == aFontData.mSize.GetUnit()) {
    aInherited = PR_TRUE;
    aFont->mSize = (nscoord)((float)(aParentFont->mSize) * aFontData.mSize.GetPercentValue());
    zoom = PR_FALSE;
  }
  else if (eCSSUnit_Inherit == aFontData.mSize.GetUnit()) {
    aInherited = PR_TRUE;
    aFont->mSize = aParentFont->mSize;
    zoom = PR_FALSE;
  }
  else if (eCSSUnit_Initial == aFontData.mSize.GetUnit()) {
    aFont->mSize = aDefaultFont.size;
    zoom = PR_TRUE;
  }

  // We want to zoom the cascaded size so that em-based measurements,
  // line-heights, etc., work.
  if (zoom)
    aFont->mSize = ZoomFont(aPresContext, aFont->mSize);

  // enforce the user' specified minimum font-size on the value that we expose
  if (aChromeOverride) {
    // the chrome is unconstrained, it always uses our cascading size
    aFont->mFont.size = aFont->mSize;
  }
  else {
    aFont->mFont.size = PR_MAX(aFont->mSize, aMinFontSize);
  }

  // font-size-adjust: number, none, inherit
  if (eCSSUnit_Number == aFontData.mSizeAdjust.GetUnit()) {
    aFont->mFont.sizeAdjust = aFontData.mSizeAdjust.GetFloatValue();
  }
  else if (eCSSUnit_None == aFontData.mSizeAdjust.GetUnit()) {
    aFont->mFont.sizeAdjust = 0.0f;
  }
  else if (eCSSUnit_Inherit == aFontData.mSizeAdjust.GetUnit()) {
    aInherited = PR_TRUE;
    aFont->mFont.sizeAdjust = aParentFont->mFont.sizeAdjust;
  }
  else if (eCSSUnit_Initial == aFontData.mSizeAdjust.GetUnit()) {
    aFont->mFont.sizeAdjust = 0.0f;
  }
}

// SetGenericFont:
//  - backtrack to an ancestor with the same generic font name (possibly
//    up to the root where default values come from the presentation context)
//  - re-apply cascading rules from there without caching intermediate values
static void
SetGenericFont(nsIPresContext* aPresContext, nsIStyleContext* aContext,
               const nsRuleDataFont& aFontData, PRUint8 aGenericFontID,
               nscoord aMinFontSize, PRBool aUseDocumentFonts,
               PRBool aChromeOverride, nsStyleFont* aFont)
{
  // walk up the contexts until a context with the desired generic font
  nsAutoVoidArray contextPath;
  nsCOMPtr<nsIStyleContext> higherContext = aContext->GetParent();
  while (higherContext) {
    contextPath.AppendElement(higherContext);
    const nsStyleFont* higherFont = (const nsStyleFont*)higherContext->GetStyleData(eStyleStruct_Font);
    if (higherFont && higherFont->mFlags & aGenericFontID) {
      // done walking up the higher contexts
      break;
    }
    higherContext = higherContext->GetParent();
  }

  // re-apply the cascading rules, starting from the higher context

  // If we stopped earlier because we reached the root of the style tree,
  // we will start with the default generic font from the presentation
  // context. Otherwise we start with the higher context.
  const nsFont* defaultFont;
  aPresContext->GetDefaultFont(aGenericFontID, &defaultFont);
  nsStyleFont parentFont(*defaultFont);
  PRInt32 i = contextPath.Count() - 1;
  PRBool zoom = PR_TRUE;
  if (higherContext) {
    nsIStyleContext* context = (nsIStyleContext*)contextPath[i];
    nsStyleFont* tmpFont = (nsStyleFont*)context->GetStyleData(eStyleStruct_Font);
    parentFont.mFlags = tmpFont->mFlags;
    parentFont.mFont = tmpFont->mFont;
    parentFont.mSize = tmpFont->mSize;
    --i;
    zoom = PR_FALSE;
  }
  aFont->mFlags = parentFont.mFlags;
  aFont->mFont = parentFont.mFont;
  aFont->mSize = parentFont.mSize;

  PRBool dummy;
  PRUint32 noneBits;
  PRUint32 fontBit = nsCachedStyleData::GetBitForSID(eStyleStruct_Font);
  nsRuleNode* ruleNode = nsnull;
  
  nsCOMPtr<nsIStyleRule> rule;

  for (; i >= 0; --i) {
    nsIStyleContext* context = (nsIStyleContext*)contextPath[i];
    nsRuleDataFont fontData; // Declare a struct with null CSS values.
    nsRuleData ruleData(eStyleStruct_Font, aPresContext, context);
    ruleData.mFontData = &fontData;

    // Trimmed down version of ::WalkRuleTree() to re-apply the style rules
    context->GetRuleNode(&ruleNode);
    while (ruleNode) {
      ruleNode->GetBits(nsRuleNode::eNoneBits, &noneBits);
      if (noneBits & fontBit) // no more font rules on this branch, get out
        break;

      ruleNode->GetRule(getter_AddRefs(rule));
      if (rule)
        rule->MapRuleInfoInto(&ruleData);

      ruleNode = ruleNode->GetParent();
    }

    // Compute the delta from the information that the rules specified
    fontData.mFamily.Reset(); // avoid unnecessary operations in SetFont()

    SetFont(aPresContext, context, aMinFontSize,
            aUseDocumentFonts, aChromeOverride, PR_TRUE,
            fontData, *defaultFont, &parentFont, aFont, zoom, dummy);

    // XXX Not sure if we need to do this here
    // If we have a post-resolve callback, handle that now.
    if (ruleData.mPostResolveCallback)
      (ruleData.mPostResolveCallback)((nsStyleStruct*)aFont, &ruleData);

    parentFont.mFlags = aFont->mFlags;
    parentFont.mFont = aFont->mFont;
    parentFont.mSize = aFont->mSize;
    zoom = PR_FALSE;
  }

  // Finish off by applying our own rules. In this case, aFontData
  // already has the current cascading information that we want. We
  // can just compute the delta from the parent.
  SetFont(aPresContext, aContext, aMinFontSize,
          aUseDocumentFonts, aChromeOverride, PR_TRUE,
          aFontData, *defaultFont, &parentFont, aFont, zoom, dummy);
}

const nsStyleStruct* 
nsRuleNode::ComputeFontData(nsStyleStruct* aStartStruct,
                            const nsRuleDataStruct& aData, 
                            nsIStyleContext* aContext, 
                            nsRuleNode* aHighestNode,
                            const RuleDetail& aRuleDetail, PRBool aInherited)
{
  nsCOMPtr<nsIStyleContext> parentContext = aContext->GetParent();

  const nsRuleDataFont& fontData = NS_STATIC_CAST(const nsRuleDataFont&, aData);
  nsStyleFont* font = nsnull;
  const nsStyleFont* parentFont = nsnull;
  PRBool inherited = aInherited;

  // This optimization is a little weaker here since 'em', etc., for
  // 'font-size' require inheritance.
  if (parentContext &&
      (aRuleDetail != eRuleFullReset ||
       (fontData.mSize.IsRelativeLengthUnit() &&
        fontData.mSize.GetUnit() != eCSSUnit_Pixel) ||
       fontData.mSize.GetUnit() == eCSSUnit_Percent))
    parentFont = NS_STATIC_CAST(const nsStyleFont*,
                               parentContext->GetStyleData(eStyleStruct_Font));
  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    font = new (mPresContext) nsStyleFont(*NS_STATIC_CAST(nsStyleFont*, aStartStruct));
  else {
    // XXXldb What about eRuleFullInherited?  Which path is faster?
    if (aRuleDetail != eRuleFullMixed && aRuleDetail != eRuleFullReset) {
      // No question. We will have to inherit. Go ahead and init
      // with inherited vals from parent.
      inherited = PR_TRUE;
      if (parentFont)
        font = new (mPresContext) nsStyleFont(*parentFont);
    }
  }

  PRBool zoom = PR_FALSE;
  const nsFont* defaultFont;
  if (!font) {
    mPresContext->GetDefaultFont(kPresContext_DefaultVariableFont_ID, &defaultFont);
    font = new (mPresContext) nsStyleFont(*defaultFont);
    zoom = PR_TRUE;
  }
  if (!parentFont)
    parentFont = font;

  // See if there is a minimum font-size constraint to honor
  nscoord minimumFontSize = 0; // unconstrained by default
  mPresContext->GetCachedIntPref(kPresContext_MinimumFontSize, minimumFontSize);

  PRBool chromeOverride = PR_FALSE;
  PRBool useDocumentFonts = PR_TRUE;

  // Figure out if we are a generic font
  PRUint8 generic = kGenericFont_NONE;
  if (eCSSUnit_String == fontData.mFamily.GetUnit()) {
    fontData.mFamily.GetStringValue(font->mFont.name);
    nsFont::GetGenericID(font->mFont.name, &generic);

    // MJA: bug 31816
    // if we are not using document fonts, but this is a XUL document,
    // then we set the chromeOverride flag to use the document fonts anyway
    mPresContext->GetCachedBoolPref(kPresContext_UseDocumentFonts, useDocumentFonts);
    if (!useDocumentFonts) {
      // check if the prefs have been disabled for this shell
      // - if prefs are disabled then we use the document fonts anyway (yet another override)
      PRBool prefsEnabled = PR_TRUE;
      nsCOMPtr<nsIPresShell> shell;
      mPresContext->GetShell(getter_AddRefs(shell));
      if (shell)
        shell->ArePrefStyleRulesEnabled(prefsEnabled);
      if (!prefsEnabled)
        useDocumentFonts = PR_TRUE;
    }
  }

  // See if we are in the chrome
  // We only need to know this to determine if we have to use the
  // document fonts (overriding the useDocumentFonts flag), or to
  // determine if we have to override the minimum font-size constraint.
  if (!useDocumentFonts || minimumFontSize > 0) {
    chromeOverride = IsChrome(mPresContext);
  }

  // If we don't have to use document fonts, then we are only entitled
  // to use the user's default variable-width font and fixed-width font
  if (!useDocumentFonts && !chromeOverride) {
    if (generic != kGenericFont_moz_fixed)
      generic = kGenericFont_NONE;
  }

  // Now compute our font struct
  if (generic == kGenericFont_NONE) {
    // continue the normal processing
    // our default font is the most recent generic font
    generic = parentFont->mFlags & NS_STYLE_FONT_FACE_MASK;
    mPresContext->GetDefaultFont(generic, &defaultFont);
    SetFont(mPresContext, aContext, minimumFontSize,
            useDocumentFonts, chromeOverride, PR_FALSE,
            fontData, *defaultFont, parentFont, font, zoom, inherited);
  }
  else {
    // re-calculate the font as a generic font
    inherited = PR_TRUE;
    SetGenericFont(mPresContext, aContext, fontData,
                   generic, minimumFontSize, useDocumentFonts,
                   chromeOverride, font);
  }
  // Set our generic font's bit to inform our descendants
  font->mFlags &= ~NS_STYLE_FONT_FACE_MASK;
  font->mFlags |= generic;

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_Font, font);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mInheritedData)
      aHighestNode->mStyleData.mInheritedData = new (mPresContext) nsInheritedStyleData;
    aHighestNode->mStyleData.mInheritedData->mFontData = font;
    // Propagate the bit down.
    PropagateDependentBit(NS_STYLE_INHERIT_BIT(Font), aHighestNode);
  }

  return font;
}

const nsStyleStruct*
nsRuleNode::ComputeTextData(nsStyleStruct* aStartStruct,
                            const nsRuleDataStruct& aData, 
                            nsIStyleContext* aContext, 
                            nsRuleNode* aHighestNode,
                            const RuleDetail& aRuleDetail, PRBool aInherited)
{
  nsCOMPtr<nsIStyleContext> parentContext = aContext->GetParent();

  const nsRuleDataText& textData = NS_STATIC_CAST(const nsRuleDataText&, aData);
  nsStyleText* text = nsnull;
  const nsStyleText* parentText = nsnull;
  PRBool inherited = aInherited;

  if (parentContext && aRuleDetail != eRuleFullReset)
    parentText = NS_STATIC_CAST(const nsStyleText*,
                               parentContext->GetStyleData(eStyleStruct_Text));
  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    text = new (mPresContext) nsStyleText(*NS_STATIC_CAST(nsStyleText*, aStartStruct));
  else {
    // XXXldb What about eRuleFullInherited?  Which path is faster?
    if (aRuleDetail != eRuleFullMixed && aRuleDetail != eRuleFullReset) {
      // No question. We will have to inherit. Go ahead and init
      // with inherited vals from parent.
      inherited = PR_TRUE;
      if (parentText)
        text = new (mPresContext) nsStyleText(*parentText);
    }
  }

  if (!text)
    text = new (mPresContext) nsStyleText();
  if (!parentText)
    parentText = text;

    // letter-spacing: normal, length, inherit
  SetCoord(textData.mLetterSpacing, text->mLetterSpacing, parentText->mLetterSpacing,
           SETCOORD_LH | SETCOORD_NORMAL, aContext, mPresContext, inherited);

  // line-height: normal, number, length, percent, inherit
  if (eCSSUnit_Percent == textData.mLineHeight.GetUnit()) {
    aInherited = PR_TRUE;
    const nsStyleFont* font = NS_STATIC_CAST(const nsStyleFont*,
                                    aContext->GetStyleData(eStyleStruct_Font));
    text->mLineHeight.SetCoordValue((nscoord)((float)(font->mSize) *
                                     textData.mLineHeight.GetPercentValue()));
  } else {
    SetCoord(textData.mLineHeight, text->mLineHeight, parentText->mLineHeight,
             SETCOORD_LH | SETCOORD_FACTOR | SETCOORD_NORMAL,
             aContext, mPresContext, inherited);
    if (textData.mLineHeight.IsFixedLengthUnit() ||
        textData.mLineHeight.GetUnit() == eCSSUnit_Pixel)
      text->mLineHeight.SetCoordValue(
                    ZoomFont(mPresContext, text->mLineHeight.GetCoordValue()));
  }


  // text-align: enum, string, inherit
  if (eCSSUnit_Enumerated == textData.mTextAlign.GetUnit()) {
    text->mTextAlign = textData.mTextAlign.GetIntValue();
  }
  else if (eCSSUnit_String == textData.mTextAlign.GetUnit()) {
    NS_NOTYETIMPLEMENTED("align string");
  }
  else if (eCSSUnit_Inherit == textData.mTextAlign.GetUnit()) {
    inherited = PR_TRUE;
    text->mTextAlign = parentText->mTextAlign;
  }
  else if (eCSSUnit_Initial == textData.mTextAlign.GetUnit())
    text->mTextAlign = NS_STYLE_TEXT_ALIGN_DEFAULT;

  // text-indent: length, percent, inherit
  SetCoord(textData.mTextIndent, text->mTextIndent, parentText->mTextIndent,
           SETCOORD_LPH, aContext, mPresContext, inherited);

  // text-transform: enum, none, inherit
  if (eCSSUnit_Enumerated == textData.mTextTransform.GetUnit()) {
    text->mTextTransform = textData.mTextTransform.GetIntValue();
  }
  else if (eCSSUnit_None == textData.mTextTransform.GetUnit()) {
    text->mTextTransform = NS_STYLE_TEXT_TRANSFORM_NONE;
  }
  else if (eCSSUnit_Inherit == textData.mTextTransform.GetUnit()) {
    inherited = PR_TRUE;
    text->mTextTransform = parentText->mTextTransform;
  }

  // white-space: enum, normal, inherit
  if (eCSSUnit_Enumerated == textData.mWhiteSpace.GetUnit()) {
    text->mWhiteSpace = textData.mWhiteSpace.GetIntValue();
  }
  else if (eCSSUnit_Normal == textData.mWhiteSpace.GetUnit()) {
    text->mWhiteSpace = NS_STYLE_WHITESPACE_NORMAL;
  }
  else if (eCSSUnit_Inherit == textData.mWhiteSpace.GetUnit()) {
    inherited = PR_TRUE;
    text->mWhiteSpace = parentText->mWhiteSpace;
  }

  // word-spacing: normal, length, inherit
  SetCoord(textData.mWordSpacing, text->mWordSpacing, parentText->mWordSpacing,
           SETCOORD_LH | SETCOORD_NORMAL, aContext, mPresContext, inherited);

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_Text, text);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mInheritedData)
      aHighestNode->mStyleData.mInheritedData = new (mPresContext) nsInheritedStyleData;
    aHighestNode->mStyleData.mInheritedData->mTextData = text;
    // Propagate the bit down.
    PropagateDependentBit(NS_STYLE_INHERIT_BIT(Text), aHighestNode);
  }

  return text;
}

const nsStyleStruct*
nsRuleNode::ComputeTextResetData(nsStyleStruct* aStartData,
                                 const nsRuleDataStruct& aData, 
                                 nsIStyleContext* aContext, 
                                 nsRuleNode* aHighestNode,
                                 const RuleDetail& aRuleDetail, PRBool aInherited)
{
  nsCOMPtr<nsIStyleContext> parentContext = aContext->GetParent();

  const nsRuleDataText& textData = NS_STATIC_CAST(const nsRuleDataText&, aData);
  nsStyleTextReset* text;
  if (aStartData)
    // We only need to compute the delta between this computed data and our
    // computed data.
    text = new (mPresContext) nsStyleTextReset(*NS_STATIC_CAST(nsStyleTextReset*, aStartData));
  else
    text = new (mPresContext) nsStyleTextReset();
  nsStyleTextReset* parentText = text;

  if (parentContext && 
      aRuleDetail != eRuleFullReset &&
      aRuleDetail != eRulePartialReset &&
      aRuleDetail != eRuleNone)
    parentText = (nsStyleTextReset*)parentContext->GetStyleData(eStyleStruct_TextReset);
  PRBool inherited = aInherited;
  
  // vertical-align: enum, length, percent, inherit
  SetCoord(textData.mVerticalAlign, text->mVerticalAlign, parentText->mVerticalAlign,
           SETCOORD_LPH | SETCOORD_ENUMERATED, aContext, mPresContext, inherited);

  // text-decoration: none, enum (bit field), inherit
  if (eCSSUnit_Enumerated == textData.mDecoration.GetUnit()) {
    PRInt32 td = textData.mDecoration.GetIntValue();
    text->mTextDecoration = td;
    if (td & NS_STYLE_TEXT_DECORATION_PREF_ANCHORS) {
      PRBool underlineLinks = PR_TRUE;
      nsresult res = mPresContext->GetCachedBoolPref(kPresContext_UnderlineLinks, underlineLinks);
      if (NS_SUCCEEDED(res)) {
        if (underlineLinks) {
          text->mTextDecoration |= NS_STYLE_TEXT_DECORATION_UNDERLINE;
        }
        else {
          text->mTextDecoration &= ~NS_STYLE_TEXT_DECORATION_UNDERLINE;
        }
      }
    }
  }
  else if (eCSSUnit_None == textData.mDecoration.GetUnit()) {
    text->mTextDecoration = NS_STYLE_TEXT_DECORATION_NONE;
  }
  else if (eCSSUnit_Inherit == textData.mDecoration.GetUnit()) {
    inherited = PR_TRUE;
    text->mTextDecoration = parentText->mTextDecoration;
  }

#ifdef IBMBIDI
  // unicode-bidi: enum, normal, inherit
  if (eCSSUnit_Normal == textData.mUnicodeBidi.GetUnit() ) {
    text->mUnicodeBidi = NS_STYLE_UNICODE_BIDI_NORMAL;
  }
  else if (eCSSUnit_Enumerated == textData.mUnicodeBidi.GetUnit() ) {
    text->mUnicodeBidi = textData.mUnicodeBidi.GetIntValue();
  }
  else if (eCSSUnit_Inherit == textData.mUnicodeBidi.GetUnit() ) {
    inherited = PR_TRUE;
    text->mUnicodeBidi = parentText->mUnicodeBidi;
  }
#endif // IBMBIDI

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_TextReset, text);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mResetData)
      aHighestNode->mStyleData.mResetData = new (mPresContext) nsResetStyleData;
    aHighestNode->mStyleData.mResetData->mTextResetData = text;
    // Propagate the bit down.
    PropagateDependentBit(NS_STYLE_INHERIT_BIT(TextReset), aHighestNode);
  }

  return text;
}

const nsStyleStruct*
nsRuleNode::ComputeUserInterfaceData(nsStyleStruct* aStartData,
                                     const nsRuleDataStruct& aData, 
                                     nsIStyleContext* aContext, 
                                     nsRuleNode* aHighestNode,
                                     const RuleDetail& aRuleDetail,
                                     PRBool aInherited)
{
  nsCOMPtr<nsIStyleContext> parentContext = aContext->GetParent();
  
  const nsRuleDataUserInterface& uiData = NS_STATIC_CAST(const nsRuleDataUserInterface&, aData);
  nsStyleUserInterface* ui = nsnull;
  const nsStyleUserInterface* parentUI = nsnull;
  PRBool inherited = aInherited;

  if (parentContext && aRuleDetail != eRuleFullReset)
    parentUI = NS_STATIC_CAST(const nsStyleUserInterface*,
                      parentContext->GetStyleData(eStyleStruct_UserInterface));
  if (aStartData)
    // We only need to compute the delta between this computed data and our
    // computed data.
    ui = new (mPresContext) nsStyleUserInterface(*NS_STATIC_CAST(nsStyleUserInterface*, aStartData));
  else {
    // XXXldb What about eRuleFullInherited?  Which path is faster?
    if (aRuleDetail != eRuleFullMixed && aRuleDetail != eRuleFullReset) {
      // No question. We will have to inherit. Go ahead and init
      // with inherited vals from parent.
      inherited = PR_TRUE;
      if (parentUI)
        ui = new (mPresContext) nsStyleUserInterface(*parentUI);
    }
  }

  if (!ui)
    ui = new (mPresContext) nsStyleUserInterface();
  if (!parentUI)
    parentUI = ui;

  // cursor: enum, auto, url, inherit
  nsCSSValueList*  list = uiData.mCursor;
  if (nsnull != list) {
    // XXX need to deal with multiple URL values
    if (eCSSUnit_Enumerated == list->mValue.GetUnit()) {
      ui->mCursor = list->mValue.GetIntValue();
    }
    else if (eCSSUnit_Auto == list->mValue.GetUnit()) {
      ui->mCursor = NS_STYLE_CURSOR_AUTO;
    }
    else if (eCSSUnit_URL == list->mValue.GetUnit()) {
      list->mValue.GetStringValue(ui->mCursorImage);
    }
    else if (eCSSUnit_Inherit == list->mValue.GetUnit()) {
      inherited = PR_TRUE;
      ui->mCursor = parentUI->mCursor;
    }
  }

  // user-input: auto, none, enum, inherit
  if (eCSSUnit_Enumerated == uiData.mUserInput.GetUnit()) {
    ui->mUserInput = uiData.mUserInput.GetIntValue();
  }
  else if (eCSSUnit_Auto == uiData.mUserInput.GetUnit()) {
    ui->mUserInput = NS_STYLE_USER_INPUT_AUTO;
  }
  else if (eCSSUnit_None == uiData.mUserInput.GetUnit()) {
    ui->mUserInput = NS_STYLE_USER_INPUT_NONE;
  }
  else if (eCSSUnit_Inherit == uiData.mUserInput.GetUnit()) {
    inherited = PR_TRUE;
    ui->mUserInput = parentUI->mUserInput;
  }

  // user-modify: enum, inherit
  if (eCSSUnit_Enumerated == uiData.mUserModify.GetUnit()) {
    ui->mUserModify = uiData.mUserModify.GetIntValue();
  }
  else if (eCSSUnit_Inherit == uiData.mUserModify.GetUnit()) {
    inherited = PR_TRUE;
    ui->mUserModify = parentUI->mUserModify;
  }

  // user-focus: none, normal, enum, inherit
  if (eCSSUnit_Enumerated == uiData.mUserFocus.GetUnit()) {
    ui->mUserFocus = uiData.mUserFocus.GetIntValue();
  }
  else if (eCSSUnit_None == uiData.mUserFocus.GetUnit()) {
    ui->mUserFocus = NS_STYLE_USER_FOCUS_NONE;
  }
  else if (eCSSUnit_Normal == uiData.mUserFocus.GetUnit()) {
    ui->mUserFocus = NS_STYLE_USER_FOCUS_NORMAL;
  }
  else if (eCSSUnit_Inherit == uiData.mUserFocus.GetUnit()) {
    inherited = PR_TRUE;
    ui->mUserFocus = parentUI->mUserFocus;
  }

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_UserInterface, ui);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mInheritedData)
      aHighestNode->mStyleData.mInheritedData = new (mPresContext) nsInheritedStyleData;
    aHighestNode->mStyleData.mInheritedData->mUserInterfaceData = ui;
    // Propagate the bit down.
    PropagateDependentBit(NS_STYLE_INHERIT_BIT(UserInterface), aHighestNode);
  }

  return ui;
}

const nsStyleStruct*
nsRuleNode::ComputeUIResetData(nsStyleStruct* aStartData,
                               const nsRuleDataStruct& aData, 
                               nsIStyleContext* aContext, 
                               nsRuleNode* aHighestNode,
                               const RuleDetail& aRuleDetail, PRBool aInherited)
{
  nsCOMPtr<nsIStyleContext> parentContext = aContext->GetParent();
  
  const nsRuleDataUserInterface& uiData = NS_STATIC_CAST(const nsRuleDataUserInterface&, aData);
  nsStyleUIReset* ui;
  if (aStartData)
    // We only need to compute the delta between this computed data and our
    // computed data.
    ui = new (mPresContext) nsStyleUIReset(*NS_STATIC_CAST(nsStyleUIReset*, aStartData));
  else
    ui = new (mPresContext) nsStyleUIReset();
  const nsStyleUIReset* parentUI = ui;

  if (parentContext && 
      aRuleDetail != eRuleFullReset &&
      aRuleDetail != eRulePartialReset &&
      aRuleDetail != eRuleNone)
    parentUI = NS_STATIC_CAST(const nsStyleUIReset*,
                            parentContext->GetStyleData(eStyleStruct_UIReset));
  PRBool inherited = aInherited;
  
  // user-select: none, enum, inherit
  if (eCSSUnit_Enumerated == uiData.mUserSelect.GetUnit()) {
    ui->mUserSelect = uiData.mUserSelect.GetIntValue();
  }
  else if (eCSSUnit_None == uiData.mUserSelect.GetUnit()) {
    ui->mUserSelect = NS_STYLE_USER_SELECT_NONE;
  }
  else if (eCSSUnit_Inherit == uiData.mUserSelect.GetUnit()) {
    inherited = PR_TRUE;
    ui->mUserSelect = parentUI->mUserSelect;
  }

  // key-equivalent: none, enum XXX, inherit
  nsCSSValueList*  keyEquiv = uiData.mKeyEquivalent;
  if (keyEquiv) {
    // XXX need to deal with multiple values
    if (eCSSUnit_Enumerated == keyEquiv->mValue.GetUnit()) {
      ui->mKeyEquivalent = PRUnichar(0);  // XXX To be implemented
    }
    else if (eCSSUnit_None == keyEquiv->mValue.GetUnit()) {
      ui->mKeyEquivalent = PRUnichar(0);
    }
    else if (eCSSUnit_Inherit == keyEquiv->mValue.GetUnit()) {
      inherited = PR_TRUE;
      ui->mKeyEquivalent = parentUI->mKeyEquivalent;
    }
  }
  // resizer: auto, none, enum, inherit
  if (eCSSUnit_Enumerated == uiData.mResizer.GetUnit()) {
    ui->mResizer = uiData.mResizer.GetIntValue();
  }
  else if (eCSSUnit_Auto == uiData.mResizer.GetUnit()) {
    ui->mResizer = NS_STYLE_RESIZER_AUTO;
  }
  else if (eCSSUnit_None == uiData.mResizer.GetUnit()) {
    ui->mResizer = NS_STYLE_RESIZER_NONE;
  }
  else if (eCSSUnit_Inherit == uiData.mResizer.GetUnit()) {
    inherited = PR_TRUE;
    ui->mResizer = parentUI->mResizer;
  }

  // force-broken-image-icons: integer
  if (eCSSUnit_Integer == uiData.mForceBrokenImageIcon.GetUnit()) {
    ui->mForceBrokenImageIcon = uiData.mForceBrokenImageIcon.GetIntValue();
  }
  
  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_UIReset, ui);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mResetData)
      aHighestNode->mStyleData.mResetData = new (mPresContext) nsResetStyleData;
    aHighestNode->mStyleData.mResetData->mUIResetData = ui;
    // Propagate the bit down.
    PropagateDependentBit(NS_STYLE_INHERIT_BIT(UIReset), aHighestNode);
  }

  return ui;
}

const nsStyleStruct*
nsRuleNode::ComputeDisplayData(nsStyleStruct* aStartStruct,
                               const nsRuleDataStruct& aData, 
                               nsIStyleContext* aContext, 
                               nsRuleNode* aHighestNode,
                               const RuleDetail& aRuleDetail, PRBool aInherited)
{
  nsCOMPtr<nsIStyleContext> parentContext = aContext->GetParent();
  
  const nsRuleDataDisplay& displayData = NS_STATIC_CAST(const nsRuleDataDisplay&, aData);
  nsStyleDisplay* display;
  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    display = new (mPresContext) nsStyleDisplay(*NS_STATIC_CAST(nsStyleDisplay*, aStartStruct));
  else
    display = new (mPresContext) nsStyleDisplay();
  const nsStyleDisplay* parentDisplay = display;

  nsCOMPtr<nsIAtom> pseudoTag;
  aContext->GetPseudoType(*getter_AddRefs(pseudoTag));
  PRBool generatedContent = (pseudoTag == nsCSSPseudoElements::before || 
                             pseudoTag == nsCSSPseudoElements::after);

  if (parentContext && 
      ((aRuleDetail != eRuleFullReset &&
        aRuleDetail != eRulePartialReset &&
        aRuleDetail != eRuleNone) ||
       generatedContent))
    parentDisplay = NS_STATIC_CAST(const nsStyleDisplay*,
                            parentContext->GetStyleData(eStyleStruct_Display));
  PRBool inherited = aInherited;

  // display: enum, none, inherit
  if (eCSSUnit_Enumerated == displayData.mDisplay.GetUnit()) {
    display->mDisplay = displayData.mDisplay.GetIntValue();
  }
  else if (eCSSUnit_None == displayData.mDisplay.GetUnit()) {
    display->mDisplay = NS_STYLE_DISPLAY_NONE;
  }
  else if (eCSSUnit_Inherit == displayData.mDisplay.GetUnit()) {
    inherited = PR_TRUE;
    display->mDisplay = parentDisplay->mDisplay;
  }

  // appearance: enum, none, inherit
  if (eCSSUnit_Enumerated == displayData.mAppearance.GetUnit()) {
    display->mAppearance = displayData.mAppearance.GetIntValue();
  }
  else if (eCSSUnit_None == displayData.mAppearance.GetUnit()) {
    display->mAppearance = NS_THEME_NONE;
  }
  else if (eCSSUnit_Inherit == displayData.mAppearance.GetUnit()) {
    inherited = PR_TRUE;
    display->mAppearance = parentDisplay->mAppearance;
  }

  // binding: url, none, inherit
  if (eCSSUnit_URL == displayData.mBinding.GetUnit()) {
    displayData.mBinding.GetStringValue(display->mBinding);
  }
  else if (eCSSUnit_None == displayData.mBinding.GetUnit()) {
    display->mBinding.Truncate();
  }
  else if (eCSSUnit_Inherit == displayData.mBinding.GetUnit()) {
    inherited = PR_TRUE;
    display->mBinding = parentDisplay->mBinding;
  }

  // position: enum, inherit
  if (eCSSUnit_Enumerated == displayData.mPosition.GetUnit()) {
    display->mPosition = displayData.mPosition.GetIntValue();
  }
  else if (eCSSUnit_Inherit == displayData.mPosition.GetUnit()) {
    inherited = PR_TRUE;
    display->mPosition = parentDisplay->mPosition;
  }

  // clear: enum, none, inherit
  if (eCSSUnit_Enumerated == displayData.mClear.GetUnit()) {
    display->mBreakType = displayData.mClear.GetIntValue();
  }
  else if (eCSSUnit_None == displayData.mClear.GetUnit()) {
    display->mBreakType = NS_STYLE_CLEAR_NONE;
  }
  else if (eCSSUnit_Inherit == displayData.mClear.GetUnit()) {
    inherited = PR_TRUE;
    display->mBreakType = parentDisplay->mBreakType;
  }

  // temp fix for bug 24000
  if (eCSSUnit_Enumerated == displayData.mBreakBefore.GetUnit()) {
    display->mBreakBefore = (NS_STYLE_PAGE_BREAK_ALWAYS == displayData.mBreakBefore.GetIntValue());
  }
  if (eCSSUnit_Enumerated == displayData.mBreakAfter.GetUnit()) {
    display->mBreakAfter = (NS_STYLE_PAGE_BREAK_ALWAYS == displayData.mBreakAfter.GetIntValue());
  }
  // end temp fix

  // float: enum, none, inherit
  if (eCSSUnit_Enumerated == displayData.mFloat.GetUnit()) {
    display->mFloats = displayData.mFloat.GetIntValue();
  }
  else if (eCSSUnit_None == displayData.mFloat.GetUnit()) {
    display->mFloats = NS_STYLE_FLOAT_NONE;
  }
  else if (eCSSUnit_Inherit == displayData.mFloat.GetUnit()) {
    inherited = PR_TRUE;
    display->mFloats = parentDisplay->mFloats;
  }

  // overflow: enum, auto, inherit
  if (eCSSUnit_Enumerated == displayData.mOverflow.GetUnit()) {
    display->mOverflow = displayData.mOverflow.GetIntValue();
  }
  else if (eCSSUnit_Auto == displayData.mOverflow.GetUnit()) {
    display->mOverflow = NS_STYLE_OVERFLOW_AUTO;
  }
  else if (eCSSUnit_Inherit == displayData.mOverflow.GetUnit()) {
    inherited = PR_TRUE;
    display->mOverflow = parentDisplay->mOverflow;
  }

  // clip property: length, auto, inherit
  if (nsnull != displayData.mClip) {
    if (eCSSUnit_Inherit == displayData.mClip->mTop.GetUnit()) { // if one is inherit, they all are
      inherited = PR_TRUE;
      display->mClipFlags = parentDisplay->mClipFlags;
      display->mClip = parentDisplay->mClip;
    }
    else {
      PRBool  fullAuto = PR_TRUE;

      display->mClipFlags = 0; // clear it

      if (eCSSUnit_Auto == displayData.mClip->mTop.GetUnit()) {
        display->mClip.y = 0;
        display->mClipFlags |= NS_STYLE_CLIP_TOP_AUTO;
      } 
      else if (displayData.mClip->mTop.IsLengthUnit()) {
        display->mClip.y = CalcLength(displayData.mClip->mTop, nsnull, aContext, mPresContext, inherited);
        fullAuto = PR_FALSE;
      }
      if (eCSSUnit_Auto == displayData.mClip->mBottom.GetUnit()) {
        display->mClip.height = 0;
        display->mClipFlags |= NS_STYLE_CLIP_BOTTOM_AUTO;
      } 
      else if (displayData.mClip->mBottom.IsLengthUnit()) {
        display->mClip.height = CalcLength(displayData.mClip->mBottom, nsnull, aContext, mPresContext, inherited) -
                                display->mClip.y;
        fullAuto = PR_FALSE;
      }
      if (eCSSUnit_Auto == displayData.mClip->mLeft.GetUnit()) {
        display->mClip.x = 0;
        display->mClipFlags |= NS_STYLE_CLIP_LEFT_AUTO;
      } 
      else if (displayData.mClip->mLeft.IsLengthUnit()) {
        display->mClip.x = CalcLength(displayData.mClip->mLeft, nsnull, aContext, mPresContext, inherited);
        fullAuto = PR_FALSE;
      }
      if (eCSSUnit_Auto == displayData.mClip->mRight.GetUnit()) {
        display->mClip.width = 0;
        display->mClipFlags |= NS_STYLE_CLIP_RIGHT_AUTO;
      } 
      else if (displayData.mClip->mRight.IsLengthUnit()) {
        display->mClip.width = CalcLength(displayData.mClip->mRight, nsnull, aContext, mPresContext, inherited) -
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
  }

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_Display, display);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mResetData)
      aHighestNode->mStyleData.mResetData = new (mPresContext) nsResetStyleData;
    aHighestNode->mStyleData.mResetData->mDisplayData = display;
    // Propagate the bit down.
    PropagateDependentBit(NS_STYLE_INHERIT_BIT(Display), aHighestNode);
  }

  // CSS2 specified fixups:
  if (generatedContent) {
    // According to CSS2 section 12.1, :before and :after
    // pseudo-elements must not be positioned or floated (CSS2 12.1) and
    // must be limited to certain display types (depending on the
    // display type of the element to which they are attached).

    if (display->mPosition != NS_STYLE_POSITION_STATIC)
      display->mPosition = NS_STYLE_POSITION_STATIC;
    if (display->mFloats != NS_STYLE_FLOAT_NONE)
      display->mFloats = NS_STYLE_FLOAT_NONE;

    PRUint8 displayValue = display->mDisplay;
    if (displayValue != NS_STYLE_DISPLAY_NONE &&
        displayValue != NS_STYLE_DISPLAY_INLINE) {
      if (parentDisplay->IsBlockLevel()) {
        // If the subject of the selector is a block-level element,
        // allowed values are 'none', 'inline', 'block', and 'marker'.
        // If the value of the 'display' has any other value, the
        // pseudo-element will behave as if the value were 'block'.
        if (displayValue != NS_STYLE_DISPLAY_BLOCK &&
            displayValue != NS_STYLE_DISPLAY_MARKER)
          display->mDisplay = NS_STYLE_DISPLAY_BLOCK;
      } else {
        // If the subject of the selector is an inline-level element,
        // allowed values are 'none' and 'inline'. If the value of the
        // 'display' has any other value, the pseudo-element will behave
        // as if the value were 'inline'.
        display->mDisplay = NS_STYLE_DISPLAY_INLINE;
      }
    }
  } 
  else if (display->mDisplay != NS_STYLE_DISPLAY_NONE) {
    // CSS2 9.7 specifies display type corrections dealing with 'float'
    // and 'position'.  Since generated content can't be floated or
    // positioned, we can deal with it here.

    // 1) if float is not none, and display is not none, then we must
    // set display to block
    //    XXX - there are problems with following the spec here: what we
    //          will do instead of following the letter of the spec is
    //          to make sure that floated elements are some kind of
    //          block, not strictly 'block' - see EnsureBlockDisplay
    //          method
    if (display->mFloats != NS_STYLE_FLOAT_NONE)
      EnsureBlockDisplay(display->mDisplay);
    else if (nsCSSPseudoElements::firstLetter == pseudoTag) 
      // a non-floating first-letter must be inline
      // XXX this fix can go away once bug 103189 is fixed correctly
      display->mDisplay = NS_STYLE_DISPLAY_INLINE;
    
    // 2) if position is 'absolute' or 'fixed' then display must be
    // 'block and float must be 'none'
    //    XXX - see note for fixup 1) above...
    if (display->IsAbsolutelyPositioned()) {
      // Backup original display value for calculation of a hypothetical
      // box (CSS2 10.6.4/10.6.5).
      // See nsHTMLReflowState::CalculateHypotheticalBox
      display->mOriginalDisplay = display->mDisplay;
      EnsureBlockDisplay(display->mDisplay);
      display->mFloats = NS_STYLE_FLOAT_NONE;
    }
  }

  return display;
}

const nsStyleStruct*
nsRuleNode::ComputeVisibilityData(nsStyleStruct* aStartStruct,
                                  const nsRuleDataStruct& aData, 
                                  nsIStyleContext* aContext, 
                                  nsRuleNode* aHighestNode,
                                  const RuleDetail& aRuleDetail, PRBool aInherited)
{
  nsCOMPtr<nsIStyleContext> parentContext = aContext->GetParent();
  
  const nsRuleDataDisplay& displayData = NS_STATIC_CAST(const nsRuleDataDisplay&, aData);
  nsStyleVisibility* visibility = nsnull;
  const nsStyleVisibility* parentVisibility = nsnull;
  PRBool inherited = aInherited;

  if (parentContext && aRuleDetail != eRuleFullReset)
    parentVisibility = NS_STATIC_CAST(const nsStyleVisibility*,
                         parentContext->GetStyleData(eStyleStruct_Visibility));
  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    visibility = new (mPresContext) nsStyleVisibility(*NS_STATIC_CAST(nsStyleVisibility*, aStartStruct));
  else {
    // XXXldb What about eRuleFullInherited?  Which path is faster?
    if (aRuleDetail != eRuleFullMixed && aRuleDetail != eRuleFullReset) {
      // No question. We will have to inherit. Go ahead and init
      // with inherited vals from parent.
      inherited = PR_TRUE;
      if (parentVisibility)
        visibility = new (mPresContext) nsStyleVisibility(*parentVisibility);
    }
  }

  if (!visibility)
    visibility = new (mPresContext) nsStyleVisibility(mPresContext);
  if (!parentVisibility)
    parentVisibility = visibility;

  // opacity: factor, percent, inherit
  if (eCSSUnit_Percent == displayData.mOpacity.GetUnit()) {
    inherited = PR_TRUE;
    float opacity = parentVisibility->mOpacity * displayData.mOpacity.GetPercentValue();
    if (opacity < 0.0f) {
      visibility->mOpacity = 0.0f;
    } else if (1.0 < opacity) {
      visibility->mOpacity = 1.0f;
    }
    else {
      visibility->mOpacity = opacity;
    }
  }
  else if (eCSSUnit_Number == displayData.mOpacity.GetUnit()) {
    visibility->mOpacity = displayData.mOpacity.GetFloatValue();
  }
  else if (eCSSUnit_Inherit == displayData.mOpacity.GetUnit()) {
    inherited = PR_TRUE;
    visibility->mOpacity = parentVisibility->mOpacity;
  }

  // direction: enum, inherit
  if (eCSSUnit_Enumerated == displayData.mDirection.GetUnit()) {
    visibility->mDirection = displayData.mDirection.GetIntValue();
#ifdef IBMBIDI    
    if (NS_STYLE_DIRECTION_RTL == visibility->mDirection)
      mPresContext->SetBidiEnabled(PR_TRUE);
#endif // IBMBIDI
  }
  else if (eCSSUnit_Inherit == displayData.mDirection.GetUnit()) {
    inherited = PR_TRUE;
    visibility->mDirection = parentVisibility->mDirection;
  }

  // visibility: enum, inherit
  if (eCSSUnit_Enumerated == displayData.mVisibility.GetUnit()) {
    visibility->mVisible = displayData.mVisibility.GetIntValue();
  }
  else if (eCSSUnit_Inherit == displayData.mVisibility.GetUnit()) {
    inherited = PR_TRUE;
    visibility->mVisible = parentVisibility->mVisible;
  }

  // lang: string, inherit
  // this is not a real CSS property, it is a html attribute mapped to CSS struture
  if (eCSSUnit_String == displayData.mLang.GetUnit()) {
    if (!gLangService) {
      CallGetService(NS_LANGUAGEATOMSERVICE_CONTRACTID, &gLangService);
    }

    if (gLangService) {
      nsAutoString lang;
      displayData.mLang.GetStringValue(lang);
      gLangService->LookupLanguage(lang.get(),
                        getter_AddRefs(visibility->mLanguage));
    }
  } 

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_Visibility, visibility);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mInheritedData)
      aHighestNode->mStyleData.mInheritedData = new (mPresContext) nsInheritedStyleData;
    aHighestNode->mStyleData.mInheritedData->mVisibilityData = visibility;
    // Propagate the bit down.
    PropagateDependentBit(NS_STYLE_INHERIT_BIT(Visibility), aHighestNode);
  }

  return visibility;
}

const nsStyleStruct*
nsRuleNode::ComputeColorData(nsStyleStruct* aStartStruct,
                             const nsRuleDataStruct& aData, 
                             nsIStyleContext* aContext, 
                             nsRuleNode* aHighestNode,
                             const RuleDetail& aRuleDetail, PRBool aInherited)
{
  nsCOMPtr<nsIStyleContext> parentContext = aContext->GetParent();
  
  const nsRuleDataColor& colorData = NS_STATIC_CAST(const nsRuleDataColor&, aData);
  nsStyleColor* color = nsnull;
  const nsStyleColor* parentColor = nsnull;
  PRBool inherited = aInherited;

  if (parentContext && aRuleDetail != eRuleFullReset)
    parentColor = NS_STATIC_CAST(const nsStyleColor*,
                              parentContext->GetStyleData(eStyleStruct_Color));
  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    color = new (mPresContext) nsStyleColor(*NS_STATIC_CAST(nsStyleColor*, aStartStruct));
  else {
    // XXXldb What about eRuleFullInherited?  Which path is faster?
    if (aRuleDetail != eRuleFullMixed && aRuleDetail != eRuleFullReset) {
      // No question. We will have to inherit. Go ahead and init
      // with inherited vals from parent.
      inherited = PR_TRUE;
      if (parentColor)
        color = new (mPresContext) nsStyleColor(*parentColor);
    }
  }

  if (!color)
    color = new (mPresContext) nsStyleColor(mPresContext);
  if (!parentColor)
    parentColor = color;

  // color: color, string, inherit
  SetColor(colorData.mColor, parentColor->mColor, mPresContext, color->mColor, 
           inherited);

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_Color, color);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mInheritedData)
      aHighestNode->mStyleData.mInheritedData = new (mPresContext) nsInheritedStyleData;
    aHighestNode->mStyleData.mInheritedData->mColorData = color;
    // Propagate the bit down.
    PropagateDependentBit(NS_STYLE_INHERIT_BIT(Color), aHighestNode);
  }

  return color;
}

const nsStyleStruct*
nsRuleNode::ComputeBackgroundData(nsStyleStruct* aStartStruct,
                                  const nsRuleDataStruct& aData, 
                                  nsIStyleContext* aContext, 
                                  nsRuleNode* aHighestNode,
                                  const RuleDetail& aRuleDetail, PRBool aInherited)
{
  nsCOMPtr<nsIStyleContext> parentContext = aContext->GetParent();
  
  const nsRuleDataColor& colorData = NS_STATIC_CAST(const nsRuleDataColor&, aData);
  nsStyleBackground* bg;
  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    bg = new (mPresContext) nsStyleBackground(*NS_STATIC_CAST(nsStyleBackground*, aStartStruct));
  else
    bg = new (mPresContext) nsStyleBackground(mPresContext);
  const nsStyleBackground* parentBG = bg;

  if (parentContext && 
      aRuleDetail != eRuleFullReset &&
      aRuleDetail != eRulePartialReset &&
      aRuleDetail != eRuleNone)
    parentBG = NS_STATIC_CAST(const nsStyleBackground*,
                         parentContext->GetStyleData(eStyleStruct_Background));
  PRBool inherited = aInherited;
  // save parentFlags in case bg == parentBG and we clobber them later
  PRUint8 parentFlags = parentBG->mBackgroundFlags;

  // background-color: color, string, enum (flags), inherit
  if (eCSSUnit_Inherit == colorData.mBackColor.GetUnit()) { // do inherit first, so SetColor doesn't do it
    bg->mBackgroundColor = parentBG->mBackgroundColor;
    bg->mBackgroundFlags &= ~NS_STYLE_BG_COLOR_TRANSPARENT;
    bg->mBackgroundFlags |= (parentFlags & NS_STYLE_BG_COLOR_TRANSPARENT);
    inherited = PR_TRUE;
  }
  else if (SetColor(colorData.mBackColor, parentBG->mBackgroundColor, 
                    mPresContext, bg->mBackgroundColor, inherited)) {
    bg->mBackgroundFlags &= ~NS_STYLE_BG_COLOR_TRANSPARENT;
  }
  else if (eCSSUnit_Enumerated == colorData.mBackColor.GetUnit()) {
    //bg->mBackgroundColor = parentBG->mBackgroundColor; XXXwdh crap crap crap!
    bg->mBackgroundFlags |= NS_STYLE_BG_COLOR_TRANSPARENT;
  }

  // background-image: url, none, inherit
  if (eCSSUnit_URL == colorData.mBackImage.GetUnit()) {
    colorData.mBackImage.GetStringValue(bg->mBackgroundImage);
    bg->mBackgroundFlags &= ~NS_STYLE_BG_IMAGE_NONE;
  }
  else if (eCSSUnit_None == colorData.mBackImage.GetUnit()) {
    bg->mBackgroundImage.Truncate();
    bg->mBackgroundFlags |= NS_STYLE_BG_IMAGE_NONE;
  }
  else if (eCSSUnit_Inherit == colorData.mBackImage.GetUnit()) {
    inherited = PR_TRUE;
    bg->mBackgroundImage = parentBG->mBackgroundImage;
    bg->mBackgroundFlags &= ~NS_STYLE_BG_IMAGE_NONE;
    bg->mBackgroundFlags |= (parentFlags & NS_STYLE_BG_IMAGE_NONE);
  }

  // background-repeat: enum, inherit
  if (eCSSUnit_Enumerated == colorData.mBackRepeat.GetUnit()) {
    bg->mBackgroundRepeat = colorData.mBackRepeat.GetIntValue();
  }
  else if (eCSSUnit_Inherit == colorData.mBackRepeat.GetUnit()) {
    inherited = PR_TRUE;
    bg->mBackgroundRepeat = parentBG->mBackgroundRepeat;
  }

  // background-attachment: enum, inherit
  if (eCSSUnit_Enumerated == colorData.mBackAttachment.GetUnit()) {
    bg->mBackgroundAttachment = colorData.mBackAttachment.GetIntValue();
  }
  else if (eCSSUnit_Inherit == colorData.mBackAttachment.GetUnit()) {
    inherited = PR_TRUE;
    bg->mBackgroundAttachment = parentBG->mBackgroundAttachment;
  }

  // background-clip: enum, inherit, initial
  if (eCSSUnit_Enumerated == colorData.mBackClip.GetUnit()) {
    bg->mBackgroundClip = colorData.mBackClip.GetIntValue();
  }
  else if (eCSSUnit_Inherit == colorData.mBackClip.GetUnit()) {
    bg->mBackgroundClip = parentBG->mBackgroundClip;
  }
  else if (eCSSUnit_Initial == colorData.mBackClip.GetUnit()) {
    bg->mBackgroundClip = NS_STYLE_BG_CLIP_BORDER;
  }

  // background-origin: enum, inherit, initial
  if (eCSSUnit_Enumerated == colorData.mBackOrigin.GetUnit()) {
    bg->mBackgroundOrigin = colorData.mBackOrigin.GetIntValue();
  }
  else if (eCSSUnit_Inherit == colorData.mBackOrigin.GetUnit()) {
    bg->mBackgroundOrigin = parentBG->mBackgroundOrigin;
  }
  else if (eCSSUnit_Initial == colorData.mBackOrigin.GetUnit()) {
    bg->mBackgroundOrigin = NS_STYLE_BG_ORIGIN_PADDING;
  }

  // background-position: enum, length, percent (flags), inherit
  if (eCSSUnit_Percent == colorData.mBackPositionX.GetUnit()) {
    bg->mBackgroundXPosition = (nscoord)(100.0f * colorData.mBackPositionX.GetPercentValue());
    bg->mBackgroundFlags |= NS_STYLE_BG_X_POSITION_PERCENT;
    bg->mBackgroundFlags &= ~NS_STYLE_BG_X_POSITION_LENGTH;
  }
  else if (colorData.mBackPositionX.IsLengthUnit()) {
    bg->mBackgroundXPosition = CalcLength(colorData.mBackPositionX, nsnull, 
                                          aContext, mPresContext, inherited);
    bg->mBackgroundFlags |= NS_STYLE_BG_X_POSITION_LENGTH;
    bg->mBackgroundFlags &= ~NS_STYLE_BG_X_POSITION_PERCENT;
  }
  else if (eCSSUnit_Enumerated == colorData.mBackPositionX.GetUnit()) {
    bg->mBackgroundXPosition = (nscoord)colorData.mBackPositionX.GetIntValue();
    bg->mBackgroundFlags |= NS_STYLE_BG_X_POSITION_PERCENT;
    bg->mBackgroundFlags &= ~NS_STYLE_BG_X_POSITION_LENGTH;
  }
  else if (eCSSUnit_Inherit == colorData.mBackPositionX.GetUnit()) {
    inherited = PR_TRUE;
    bg->mBackgroundXPosition = parentBG->mBackgroundXPosition;
    bg->mBackgroundFlags &= ~(NS_STYLE_BG_X_POSITION_LENGTH | NS_STYLE_BG_X_POSITION_PERCENT);
    bg->mBackgroundFlags |= (parentFlags & (NS_STYLE_BG_X_POSITION_LENGTH | NS_STYLE_BG_X_POSITION_PERCENT));
  }

  if (eCSSUnit_Percent == colorData.mBackPositionY.GetUnit()) {
    bg->mBackgroundYPosition = (nscoord)(100.0f * colorData.mBackPositionY.GetPercentValue());
    bg->mBackgroundFlags |= NS_STYLE_BG_Y_POSITION_PERCENT;
    bg->mBackgroundFlags &= ~NS_STYLE_BG_Y_POSITION_LENGTH;
  }
  else if (colorData.mBackPositionY.IsLengthUnit()) {
    bg->mBackgroundYPosition = CalcLength(colorData.mBackPositionY, nsnull,
                                          aContext, mPresContext, inherited);
    bg->mBackgroundFlags |= NS_STYLE_BG_Y_POSITION_LENGTH;
    bg->mBackgroundFlags &= ~NS_STYLE_BG_Y_POSITION_PERCENT;
  }
  else if (eCSSUnit_Enumerated == colorData.mBackPositionY.GetUnit()) {
    bg->mBackgroundYPosition = (nscoord)colorData.mBackPositionY.GetIntValue();
    bg->mBackgroundFlags |= NS_STYLE_BG_Y_POSITION_PERCENT;
    bg->mBackgroundFlags &= ~NS_STYLE_BG_Y_POSITION_LENGTH;
  }
  else if (eCSSUnit_Inherit == colorData.mBackPositionY.GetUnit()) {
    inherited = PR_TRUE;
    bg->mBackgroundYPosition = parentBG->mBackgroundYPosition;
    bg->mBackgroundFlags &= ~(NS_STYLE_BG_Y_POSITION_LENGTH | NS_STYLE_BG_Y_POSITION_PERCENT);
    bg->mBackgroundFlags |= (parentFlags & (NS_STYLE_BG_Y_POSITION_LENGTH | NS_STYLE_BG_Y_POSITION_PERCENT));
  }

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_Background, bg);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mResetData)
      aHighestNode->mStyleData.mResetData = new (mPresContext) nsResetStyleData;
    aHighestNode->mStyleData.mResetData->mBackgroundData = bg;
    // Propagate the bit down.
    PropagateDependentBit(NS_STYLE_INHERIT_BIT(Background), aHighestNode);
  }

  return bg;
}

const nsStyleStruct*
nsRuleNode::ComputeMarginData(nsStyleStruct* aStartStruct,
                              const nsRuleDataStruct& aData, 
                              nsIStyleContext* aContext, 
                              nsRuleNode* aHighestNode,
                              const RuleDetail& aRuleDetail, PRBool aInherited)
{
  nsCOMPtr<nsIStyleContext> parentContext = aContext->GetParent();
  
  const nsRuleDataMargin& marginData = NS_STATIC_CAST(const nsRuleDataMargin&, aData);
  nsStyleMargin* margin;
  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    margin = new (mPresContext) nsStyleMargin(*NS_STATIC_CAST(nsStyleMargin*, aStartStruct));
  else
    margin = new (mPresContext) nsStyleMargin();
  const nsStyleMargin* parentMargin = margin;

  if (parentContext && 
      aRuleDetail != eRuleFullReset &&
      aRuleDetail != eRulePartialReset &&
      aRuleDetail != eRuleNone)
    parentMargin = NS_STATIC_CAST(const nsStyleMargin*,
                             parentContext->GetStyleData(eStyleStruct_Margin));
  PRBool inherited = aInherited;

  // margin: length, percent, auto, inherit
  if (marginData.mMargin) {
    nsStyleCoord  coord;
    nsStyleCoord  parentCoord;
    parentMargin->mMargin.GetLeft(parentCoord);
    if (SetCoord(marginData.mMargin->mLeft, coord, parentCoord, SETCOORD_LPAH, aContext, mPresContext, inherited)) {
      margin->mMargin.SetLeft(coord);
    }
    parentMargin->mMargin.GetTop(parentCoord);
    if (SetCoord(marginData.mMargin->mTop, coord, parentCoord, SETCOORD_LPAH, aContext, mPresContext, inherited)) {
      margin->mMargin.SetTop(coord);
    }
    parentMargin->mMargin.GetRight(parentCoord);
    if (SetCoord(marginData.mMargin->mRight, coord, parentCoord, SETCOORD_LPAH, aContext, mPresContext, inherited)) {
      margin->mMargin.SetRight(coord);
    }
    parentMargin->mMargin.GetBottom(parentCoord);
    if (SetCoord(marginData.mMargin->mBottom, coord, parentCoord, SETCOORD_LPAH, aContext, mPresContext, inherited)) {
      margin->mMargin.SetBottom(coord);
    }
  }

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_Margin, margin);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mResetData)
      aHighestNode->mStyleData.mResetData = new (mPresContext) nsResetStyleData;
    aHighestNode->mStyleData.mResetData->mMarginData = margin;
    // Propagate the bit down.
    PropagateDependentBit(NS_STYLE_INHERIT_BIT(Margin), aHighestNode);
  }

  margin->RecalcData();
  return margin;
}

const nsStyleStruct* 
nsRuleNode::ComputeBorderData(nsStyleStruct* aStartStruct,
                              const nsRuleDataStruct& aData, 
                              nsIStyleContext* aContext, 
                              nsRuleNode* aHighestNode,
                              const RuleDetail& aRuleDetail, PRBool aInherited)
{
  nsCOMPtr<nsIStyleContext> parentContext = aContext->GetParent();
  
  const nsRuleDataMargin& marginData = NS_STATIC_CAST(const nsRuleDataMargin&, aData);
  nsStyleBorder* border;
  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    border = new (mPresContext) nsStyleBorder(*NS_STATIC_CAST(nsStyleBorder*, aStartStruct));
  else
    border = new (mPresContext) nsStyleBorder(mPresContext);
  
  const nsStyleBorder* parentBorder = border;
  if (parentContext && 
      aRuleDetail != eRuleFullReset &&
      aRuleDetail != eRulePartialReset &&
      aRuleDetail != eRuleNone)
    parentBorder = NS_STATIC_CAST(const nsStyleBorder*,
                             parentContext->GetStyleData(eStyleStruct_Border));
  PRBool inherited = aInherited;

  // border-size: length, enum, inherit
  if (marginData.mBorderWidth) {
    nsStyleCoord  coord;
    nsStyleCoord  parentCoord;
    if (SetCoord(marginData.mBorderWidth->mLeft, coord, parentCoord, SETCOORD_LE, aContext, mPresContext, inherited))
      border->mBorder.SetLeft(coord);
    else if (eCSSUnit_Inherit == marginData.mBorderWidth->mLeft.GetUnit()) {
      inherited = PR_TRUE;
      border->mBorder.SetLeft(parentBorder->mBorder.GetLeft(coord));
    }

    if (SetCoord(marginData.mBorderWidth->mTop, coord, parentCoord, SETCOORD_LE, aContext, mPresContext, inherited))
      border->mBorder.SetTop(coord);
    else if (eCSSUnit_Inherit == marginData.mBorderWidth->mTop.GetUnit()) {
      inherited = PR_TRUE;
      border->mBorder.SetTop(parentBorder->mBorder.GetTop(coord));
    }

    if (SetCoord(marginData.mBorderWidth->mRight, coord, parentCoord, SETCOORD_LE, aContext, mPresContext, inherited))
      border->mBorder.SetRight(coord);
    else if (eCSSUnit_Inherit == marginData.mBorderWidth->mRight.GetUnit()) {
      inherited = PR_TRUE;
      border->mBorder.SetRight(parentBorder->mBorder.GetRight(coord));
    }

    if (SetCoord(marginData.mBorderWidth->mBottom, coord, parentCoord, SETCOORD_LE, aContext, mPresContext, inherited))
      border->mBorder.SetBottom(coord);
    else if (eCSSUnit_Inherit == marginData.mBorderWidth->mBottom.GetUnit()) {
      inherited = PR_TRUE;
      border->mBorder.SetBottom(parentBorder->mBorder.GetBottom(coord));
    }
  }

  // border-style: enum, none, inhert
  if (nsnull != marginData.mBorderStyle) {
    nsCSSRect* ourStyle = marginData.mBorderStyle;
    if (eCSSUnit_Enumerated == ourStyle->mTop.GetUnit()) {
      border->SetBorderStyle(NS_SIDE_TOP, ourStyle->mTop.GetIntValue());
    }
    else if (eCSSUnit_None == ourStyle->mTop.GetUnit()) {
      border->SetBorderStyle(NS_SIDE_TOP, NS_STYLE_BORDER_STYLE_NONE);
    }
    else if (eCSSUnit_Inherit == ourStyle->mTop.GetUnit()) {
      inherited = PR_TRUE;
      border->SetBorderStyle(NS_SIDE_TOP, parentBorder->GetBorderStyle(NS_SIDE_TOP));
    }

    if (eCSSUnit_Enumerated == ourStyle->mRight.GetUnit()) {
      border->SetBorderStyle(NS_SIDE_RIGHT, ourStyle->mRight.GetIntValue());
    }
    else if (eCSSUnit_None == ourStyle->mRight.GetUnit()) {
      border->SetBorderStyle(NS_SIDE_RIGHT, NS_STYLE_BORDER_STYLE_NONE);
    }
    else if (eCSSUnit_Inherit == ourStyle->mRight.GetUnit()) {
      inherited = PR_TRUE;
      border->SetBorderStyle(NS_SIDE_RIGHT, parentBorder->GetBorderStyle(NS_SIDE_RIGHT));
    }

    if (eCSSUnit_Enumerated == ourStyle->mBottom.GetUnit()) {
      border->SetBorderStyle(NS_SIDE_BOTTOM, ourStyle->mBottom.GetIntValue());
    }
    else if (eCSSUnit_None == ourStyle->mBottom.GetUnit()) {
      border->SetBorderStyle(NS_SIDE_BOTTOM, NS_STYLE_BORDER_STYLE_NONE);
    }
    else if (eCSSUnit_Inherit == ourStyle->mBottom.GetUnit()) { 
      inherited = PR_TRUE;
      border->SetBorderStyle(NS_SIDE_BOTTOM, parentBorder->GetBorderStyle(NS_SIDE_BOTTOM));
    }

    if (eCSSUnit_Enumerated == ourStyle->mLeft.GetUnit()) {
      border->SetBorderStyle(NS_SIDE_LEFT, ourStyle->mLeft.GetIntValue());
    }
    else if (eCSSUnit_None == ourStyle->mLeft.GetUnit()) {
     border->SetBorderStyle(NS_SIDE_LEFT, NS_STYLE_BORDER_STYLE_NONE);
    }
    else if (eCSSUnit_Inherit == ourStyle->mLeft.GetUnit()) {
      inherited = PR_TRUE;
      border->SetBorderStyle(NS_SIDE_LEFT, parentBorder->GetBorderStyle(NS_SIDE_LEFT));
    }
  }

  // border-colors: color, string, enum
  if (marginData.mBorderColors) {
    nscolor borderColor;
    nscolor unused = NS_RGB(0,0,0);
    
    for (PRInt32 i = 0; i < 4; i++) {
      if (marginData.mBorderColors[i]) {
        // Some composite border color information has been specified for this
        // border side.
        border->EnsureBorderColors();
        border->ClearBorderColors(i);
        nsCSSValueList* list = marginData.mBorderColors[i];
        while (list) {
          if (SetColor(list->mValue, unused, mPresContext, borderColor, inherited))
            border->AppendBorderColor(i, borderColor, PR_FALSE);
          else if (eCSSUnit_Enumerated == list->mValue.GetUnit() &&
                   NS_STYLE_COLOR_TRANSPARENT == list->mValue.GetIntValue())
            border->AppendBorderColor(i, nsnull, PR_TRUE);
          list = list->mNext;
        }
      }
    }
  }

  // border-color: color, string, enum, inherit
  if (nsnull != marginData.mBorderColor) {
    nsCSSRect* ourBorderColor = marginData.mBorderColor;
    nscolor borderColor;
    nscolor unused = NS_RGB(0,0,0);
    PRBool transparent;
    PRBool foreground;

    // top
    if (eCSSUnit_Inherit == ourBorderColor->mTop.GetUnit()) {
      inherited = PR_TRUE;
      parentBorder->GetBorderColor(NS_SIDE_TOP, borderColor, transparent, foreground);      
      if (transparent)
        border->SetBorderTransparent(NS_SIDE_TOP);
      else if (foreground)
        border->SetBorderToForeground(NS_SIDE_TOP);
      else
        border->SetBorderColor(NS_SIDE_TOP, borderColor);
    }
    else if (SetColor(ourBorderColor->mTop, unused, mPresContext, borderColor, inherited)) {
      border->SetBorderColor(NS_SIDE_TOP, borderColor);
    }
    else if (eCSSUnit_Enumerated == ourBorderColor->mTop.GetUnit()) {
      switch (ourBorderColor->mTop.GetIntValue()) {
        case NS_STYLE_COLOR_TRANSPARENT:
          border->SetBorderTransparent(NS_SIDE_TOP);
          break;
        case NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR:
          border->SetBorderToForeground(NS_SIDE_TOP);
          break;
      }
    }
    // right
    if (eCSSUnit_Inherit == ourBorderColor->mRight.GetUnit()) {
      inherited = PR_TRUE;
      parentBorder->GetBorderColor(NS_SIDE_RIGHT, borderColor, transparent, foreground);      
      if (transparent)
        border->SetBorderTransparent(NS_SIDE_RIGHT);
      else if (foreground)
        border->SetBorderToForeground(NS_SIDE_RIGHT);
      else
        border->SetBorderColor(NS_SIDE_RIGHT, borderColor);
    }
    else if (SetColor(ourBorderColor->mRight, unused, mPresContext, borderColor, inherited)) {
      border->SetBorderColor(NS_SIDE_RIGHT, borderColor);
    }
    else if (eCSSUnit_Enumerated == ourBorderColor->mRight.GetUnit()) {
      switch (ourBorderColor->mRight.GetIntValue()) {
        case NS_STYLE_COLOR_TRANSPARENT:
          border->SetBorderTransparent(NS_SIDE_RIGHT);
          break;
        case NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR:
          border->SetBorderToForeground(NS_SIDE_RIGHT);
          break;
      }
    }
    // bottom
    if (eCSSUnit_Inherit == ourBorderColor->mBottom.GetUnit()) {
      inherited = PR_TRUE;
      parentBorder->GetBorderColor(NS_SIDE_BOTTOM, borderColor, transparent, foreground);      
      if (transparent)
        border->SetBorderTransparent(NS_SIDE_BOTTOM);
      else if (foreground)
        border->SetBorderToForeground(NS_SIDE_BOTTOM);
      else
        border->SetBorderColor(NS_SIDE_BOTTOM, borderColor);
    }
    else if (SetColor(ourBorderColor->mBottom, unused, mPresContext, borderColor, inherited)) {
      border->SetBorderColor(NS_SIDE_BOTTOM, borderColor);
    }
    else if (eCSSUnit_Enumerated == ourBorderColor->mBottom.GetUnit()) {
      switch (ourBorderColor->mBottom.GetIntValue()) {
        case NS_STYLE_COLOR_TRANSPARENT:
          border->SetBorderTransparent(NS_SIDE_BOTTOM);
          break;
        case NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR:
          border->SetBorderToForeground(NS_SIDE_BOTTOM);
          break;
      }
    }
    // left
    if (eCSSUnit_Inherit == ourBorderColor->mLeft.GetUnit()) {
      inherited = PR_TRUE;
      parentBorder->GetBorderColor(NS_SIDE_LEFT, borderColor, transparent, foreground);      
      if (transparent)
        border->SetBorderTransparent(NS_SIDE_LEFT);
      else if (foreground)
        border->SetBorderToForeground(NS_SIDE_LEFT);
      else
        border->SetBorderColor(NS_SIDE_LEFT, borderColor);
    }
    else if (SetColor(ourBorderColor->mLeft, unused, mPresContext, borderColor, inherited)) {
      border->SetBorderColor(NS_SIDE_LEFT, borderColor);
    }
    else if (eCSSUnit_Enumerated == ourBorderColor->mLeft.GetUnit()) {
      switch (ourBorderColor->mLeft.GetIntValue()) {
        case NS_STYLE_COLOR_TRANSPARENT:
          border->SetBorderTransparent(NS_SIDE_LEFT);
          break;
        case NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR:
          border->SetBorderToForeground(NS_SIDE_LEFT);
          break;
      }
    }
  }

  // -moz-border-radius: length, percent, inherit
  if (marginData.mBorderRadius) {
    nsStyleCoord  coord;
    nsStyleCoord  parentCoord;
    parentBorder->mBorderRadius.GetLeft(parentCoord);
    if (SetCoord(marginData.mBorderRadius->mLeft, coord, parentCoord, SETCOORD_LPH, aContext, mPresContext, inherited))
      border->mBorderRadius.SetLeft(coord);
    parentBorder->mBorderRadius.GetTop(parentCoord);
    if (SetCoord(marginData.mBorderRadius->mTop, coord, parentCoord, SETCOORD_LPH, aContext, mPresContext, inherited))
      border->mBorderRadius.SetTop(coord);
    parentBorder->mBorderRadius.GetRight(parentCoord);
    if (SetCoord(marginData.mBorderRadius->mRight, coord, parentCoord, SETCOORD_LPH, aContext, mPresContext, inherited))
      border->mBorderRadius.SetRight(coord);
    parentBorder->mBorderRadius.GetBottom(parentCoord);
    if (SetCoord(marginData.mBorderRadius->mBottom, coord, parentCoord, SETCOORD_LPH, aContext, mPresContext, inherited))
      border->mBorderRadius.SetBottom(coord);
  }

  // float-edge: enum, inherit
  if (eCSSUnit_Enumerated == marginData.mFloatEdge.GetUnit())
    border->mFloatEdge = marginData.mFloatEdge.GetIntValue();
  else if (eCSSUnit_Inherit == marginData.mFloatEdge.GetUnit()) {
    inherited = PR_TRUE;
    border->mFloatEdge = parentBorder->mFloatEdge;
  }

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_Border, border);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mResetData)
      aHighestNode->mStyleData.mResetData = new (mPresContext) nsResetStyleData;
    aHighestNode->mStyleData.mResetData->mBorderData = border;
    // Propagate the bit down.
    PropagateDependentBit(NS_STYLE_INHERIT_BIT(Border), aHighestNode);
  }

  border->RecalcData();
  return border;
}
  
const nsStyleStruct*
nsRuleNode::ComputePaddingData(nsStyleStruct* aStartStruct,
                               const nsRuleDataStruct& aData, 
                               nsIStyleContext* aContext, 
                               nsRuleNode* aHighestNode,
                               const RuleDetail& aRuleDetail, PRBool aInherited)
{
  nsCOMPtr<nsIStyleContext> parentContext = aContext->GetParent();
  
  const nsRuleDataMargin& marginData = NS_STATIC_CAST(const nsRuleDataMargin&, aData);
  nsStylePadding* padding;
  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    padding = new (mPresContext) nsStylePadding(*NS_STATIC_CAST(nsStylePadding*, aStartStruct));
  else
    padding = new (mPresContext) nsStylePadding();
  
  const nsStylePadding* parentPadding = padding;
  if (parentContext && 
      aRuleDetail != eRuleFullReset &&
      aRuleDetail != eRulePartialReset &&
      aRuleDetail != eRuleNone)
    parentPadding = NS_STATIC_CAST(const nsStylePadding*,
                            parentContext->GetStyleData(eStyleStruct_Padding));
  PRBool inherited = aInherited;

  // padding: length, percent, inherit
  if (marginData.mPadding) {
    nsStyleCoord  coord;
    nsStyleCoord  parentCoord;
    parentPadding->mPadding.GetLeft(parentCoord);
    if (SetCoord(marginData.mPadding->mLeft, coord, parentCoord, SETCOORD_LPH, aContext, mPresContext, inherited)) {
      padding->mPadding.SetLeft(coord);
    }
    parentPadding->mPadding.GetTop(parentCoord);
    if (SetCoord(marginData.mPadding->mTop, coord, parentCoord, SETCOORD_LPH, aContext, mPresContext, inherited)) {
      padding->mPadding.SetTop(coord);
    }
    parentPadding->mPadding.GetRight(parentCoord);
    if (SetCoord(marginData.mPadding->mRight, coord, parentCoord, SETCOORD_LPH, aContext, mPresContext, inherited)) {
      padding->mPadding.SetRight(coord);
    }
    parentPadding->mPadding.GetBottom(parentCoord);
    if (SetCoord(marginData.mPadding->mBottom, coord, parentCoord, SETCOORD_LPH, aContext, mPresContext, inherited)) {
      padding->mPadding.SetBottom(coord);
    }
  }

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_Padding, padding);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mResetData)
      aHighestNode->mStyleData.mResetData = new (mPresContext) nsResetStyleData;
    aHighestNode->mStyleData.mResetData->mPaddingData = padding;
    // Propagate the bit down.
    PropagateDependentBit(NS_STYLE_INHERIT_BIT(Padding), aHighestNode);
  }

  padding->RecalcData();
  return padding;
}

const nsStyleStruct*
nsRuleNode::ComputeOutlineData(nsStyleStruct* aStartStruct,
                               const nsRuleDataStruct& aData, 
                               nsIStyleContext* aContext, 
                               nsRuleNode* aHighestNode,
                               const RuleDetail& aRuleDetail, PRBool aInherited)
{
  nsCOMPtr<nsIStyleContext> parentContext = aContext->GetParent();
  
  const nsRuleDataMargin& marginData = NS_STATIC_CAST(const nsRuleDataMargin&, aData);
  nsStyleOutline* outline;
  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    outline = new (mPresContext) nsStyleOutline(*NS_STATIC_CAST(nsStyleOutline*, aStartStruct));
  else
    outline = new (mPresContext) nsStyleOutline(mPresContext);
  
  const nsStyleOutline* parentOutline = outline;
  if (parentContext && 
      aRuleDetail != eRuleFullReset &&
      aRuleDetail != eRulePartialReset &&
      aRuleDetail != eRuleNone)
    parentOutline = NS_STATIC_CAST(const nsStyleOutline*,
                            parentContext->GetStyleData(eStyleStruct_Outline));
  PRBool inherited = aInherited;

  // outline-width: length, enum, inherit
  SetCoord(marginData.mOutlineWidth, outline->mOutlineWidth, parentOutline->mOutlineWidth,
           SETCOORD_LEH, aContext, mPresContext, inherited);
  
  // outline-color: color, string, enum, inherit
  nscolor outlineColor;
  nscolor unused = NS_RGB(0,0,0);
  if (eCSSUnit_Inherit == marginData.mOutlineColor.GetUnit()) {
    inherited = PR_TRUE;
    if (parentOutline->GetOutlineColor(outlineColor))
      outline->SetOutlineColor(outlineColor);
    else
      outline->SetOutlineInvert();
  }
  else if (SetColor(marginData.mOutlineColor, unused, mPresContext, outlineColor, inherited))
    outline->SetOutlineColor(outlineColor);
  else if (eCSSUnit_Enumerated == marginData.mOutlineColor.GetUnit())
    outline->SetOutlineInvert();

  // outline-style: enum, none, inherit
  if (eCSSUnit_Enumerated == marginData.mOutlineStyle.GetUnit())
    outline->SetOutlineStyle(marginData.mOutlineStyle.GetIntValue());
  else if (eCSSUnit_None == marginData.mOutlineStyle.GetUnit())
    outline->SetOutlineStyle(NS_STYLE_BORDER_STYLE_NONE);
  else if (eCSSUnit_Inherit == marginData.mOutlineStyle.GetUnit()) {
    inherited = PR_TRUE;
    outline->SetOutlineStyle(parentOutline->GetOutlineStyle());
  }

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_Outline, outline);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mResetData)
      aHighestNode->mStyleData.mResetData = new (mPresContext) nsResetStyleData;
    aHighestNode->mStyleData.mResetData->mOutlineData = outline;
    // Propagate the bit down.
    PropagateDependentBit(NS_STYLE_INHERIT_BIT(Outline), aHighestNode);
  }

  outline->RecalcData();
  return outline;
}

const nsStyleStruct* 
nsRuleNode::ComputeListData(nsStyleStruct* aStartStruct,
                            const nsRuleDataStruct& aData, 
                            nsIStyleContext* aContext, 
                            nsRuleNode* aHighestNode,
                            const RuleDetail& aRuleDetail, PRBool aInherited)
{
  nsCOMPtr<nsIStyleContext> parentContext = aContext->GetParent();
  
  const nsRuleDataList& listData = NS_STATIC_CAST(const nsRuleDataList&, aData);
  nsStyleList* list = nsnull;
  const nsStyleList* parentList = nsnull;
  PRBool inherited = aInherited;

  if (parentContext && aRuleDetail != eRuleFullReset)
    parentList = NS_STATIC_CAST(const nsStyleList*,
                               parentContext->GetStyleData(eStyleStruct_List));
  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    list = new (mPresContext) nsStyleList(*NS_STATIC_CAST(nsStyleList*, aStartStruct));
  else {
    // XXXldb What about eRuleFullInherited?  Which path is faster?
    if (aRuleDetail != eRuleFullMixed && aRuleDetail != eRuleFullReset) {
      // No question. We will have to inherit. Go ahead and init
      // with inherited vals from parent.
      inherited = PR_TRUE;
      if (parentList)
        list = new (mPresContext) nsStyleList(*parentList);
    }
  }

  if (!list)
    list = new (mPresContext) nsStyleList();
  if (!parentList)
    parentList = list;

  // list-style-type: enum, none, inherit
  if (eCSSUnit_Enumerated == listData.mType.GetUnit()) {
    list->mListStyleType = listData.mType.GetIntValue();
  }
  else if (eCSSUnit_None == listData.mType.GetUnit()) {
    list->mListStyleType = NS_STYLE_LIST_STYLE_NONE;
  }
  else if (eCSSUnit_Inherit == listData.mType.GetUnit()) {
    inherited = PR_TRUE;
    list->mListStyleType = parentList->mListStyleType;
  }

  // list-style-image: url, none, inherit
  if (eCSSUnit_URL == listData.mImage.GetUnit()) {
    listData.mImage.GetStringValue(list->mListStyleImage);
  }
  else if (eCSSUnit_None == listData.mImage.GetUnit()) {
    list->mListStyleImage.Truncate();
  }
  else if (eCSSUnit_Inherit == listData.mImage.GetUnit()) {
    inherited = PR_TRUE;
    list->mListStyleImage = parentList->mListStyleImage;
  }

  // list-style-position: enum, inherit
  if (eCSSUnit_Enumerated == listData.mPosition.GetUnit()) {
    list->mListStylePosition = listData.mPosition.GetIntValue();
  }
  else if (eCSSUnit_Inherit == listData.mPosition.GetUnit()) {
    inherited = PR_TRUE;
    list->mListStylePosition = parentList->mListStylePosition;
  }

  // image region property: length, auto, inherit
  if (listData.mImageRegion) {
    if (eCSSUnit_Inherit == listData.mImageRegion->mTop.GetUnit()) { // if one is inherit, they all are
      inherited = PR_TRUE;
      list->mImageRegion = parentList->mImageRegion;
    }
    else {
      if (eCSSUnit_Auto == listData.mImageRegion->mTop.GetUnit())
        list->mImageRegion.y = 0;
      else if (listData.mImageRegion->mTop.IsLengthUnit())
        list->mImageRegion.y = CalcLength(listData.mImageRegion->mTop, nsnull, aContext, mPresContext, inherited);
        
      if (eCSSUnit_Auto == listData.mImageRegion->mBottom.GetUnit())
        list->mImageRegion.height = 0;
      else if (listData.mImageRegion->mBottom.IsLengthUnit())
        list->mImageRegion.height = CalcLength(listData.mImageRegion->mBottom, nsnull, aContext, 
                                              mPresContext, inherited) - list->mImageRegion.y;
    
      if (eCSSUnit_Auto == listData.mImageRegion->mLeft.GetUnit())
        list->mImageRegion.x = 0;
      else if (listData.mImageRegion->mLeft.IsLengthUnit())
        list->mImageRegion.x = CalcLength(listData.mImageRegion->mLeft, nsnull, aContext, mPresContext, inherited);
        
      if (eCSSUnit_Auto == listData.mImageRegion->mRight.GetUnit())
        list->mImageRegion.width = 0;
      else if (listData.mImageRegion->mRight.IsLengthUnit())
        list->mImageRegion.width = CalcLength(listData.mImageRegion->mRight, nsnull, aContext, mPresContext, inherited) -
                                  list->mImageRegion.x;
    }
  }

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_List, list);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mInheritedData)
      aHighestNode->mStyleData.mInheritedData = new (mPresContext) nsInheritedStyleData;
    aHighestNode->mStyleData.mInheritedData->mListData = list;
    // Propagate the bit down.
    PropagateDependentBit(NS_STYLE_INHERIT_BIT(List), aHighestNode);
  }

  return list;
}

const nsStyleStruct* 
nsRuleNode::ComputePositionData(nsStyleStruct* aStartStruct,
                                const nsRuleDataStruct& aData, 
                                nsIStyleContext* aContext, 
                                nsRuleNode* aHighestNode,
                                const RuleDetail& aRuleDetail, PRBool aInherited)
{
  nsCOMPtr<nsIStyleContext> parentContext = aContext->GetParent();
  
  const nsRuleDataPosition& posData = NS_STATIC_CAST(const nsRuleDataPosition&, aData);
  nsStylePosition* pos;
  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    pos = new (mPresContext) nsStylePosition(*NS_STATIC_CAST(nsStylePosition*, aStartStruct));
  else
    pos = new (mPresContext) nsStylePosition();
  
  const nsStylePosition* parentPos = pos;
  if (parentContext && 
      aRuleDetail != eRuleFullReset &&
      aRuleDetail != eRulePartialReset &&
      aRuleDetail != eRuleNone)
    parentPos = NS_STATIC_CAST(const nsStylePosition*,
                           parentContext->GetStyleData(eStyleStruct_Position));
  PRBool inherited = aInherited;

  // box offsets: length, percent, auto, inherit
  if (posData.mOffset) {
    nsStyleCoord  coord;
    nsStyleCoord  parentCoord;
    parentPos->mOffset.GetTop(parentCoord);
    if (SetCoord(posData.mOffset->mTop, coord, parentCoord, SETCOORD_LPAH, aContext, mPresContext, inherited)) {
      pos->mOffset.SetTop(coord);            
    }
    parentPos->mOffset.GetRight(parentCoord);
    if (SetCoord(posData.mOffset->mRight, coord, parentCoord, SETCOORD_LPAH, aContext, mPresContext, inherited)) {
      pos->mOffset.SetRight(coord);            
    }
    parentPos->mOffset.GetBottom(parentCoord);
    if (SetCoord(posData.mOffset->mBottom, coord, parentCoord, SETCOORD_LPAH, aContext, mPresContext, inherited)) {
      pos->mOffset.SetBottom(coord);
    }
    parentPos->mOffset.GetLeft(parentCoord);
    if (SetCoord(posData.mOffset->mLeft, coord, parentCoord, SETCOORD_LPAH, aContext, mPresContext, inherited)) {
      pos->mOffset.SetLeft(coord);
    }
  }

  if (posData.mWidth.GetUnit() == eCSSUnit_Proportional)
    pos->mWidth.SetIntValue((PRInt32)(posData.mWidth.GetFloatValue()), eStyleUnit_Proportional);
  else 
    SetCoord(posData.mWidth, pos->mWidth, parentPos->mWidth,
             SETCOORD_LPAH, aContext, mPresContext, inherited);
  SetCoord(posData.mMinWidth, pos->mMinWidth, parentPos->mMinWidth,
           SETCOORD_LPH, aContext, mPresContext, inherited);
  if (! SetCoord(posData.mMaxWidth, pos->mMaxWidth, parentPos->mMaxWidth,
                 SETCOORD_LPH, aContext, mPresContext, inherited)) {
    if (eCSSUnit_None == posData.mMaxWidth.GetUnit()) {
      pos->mMaxWidth.Reset();
    }
  }

  SetCoord(posData.mHeight, pos->mHeight, parentPos->mHeight,
           SETCOORD_LPAH, aContext, mPresContext, inherited);
  SetCoord(posData.mMinHeight, pos->mMinHeight, parentPos->mMinHeight,
           SETCOORD_LPH, aContext, mPresContext, inherited);
  if (! SetCoord(posData.mMaxHeight, pos->mMaxHeight, parentPos->mMaxHeight,
                 SETCOORD_LPH, aContext, mPresContext, inherited)) {
    if (eCSSUnit_None == posData.mMaxHeight.GetUnit()) {
      pos->mMaxHeight.Reset();
    }
  }

  // box-sizing: enum, inherit
  if (eCSSUnit_Enumerated == posData.mBoxSizing.GetUnit()) {
    pos->mBoxSizing = posData.mBoxSizing.GetIntValue();
  }
  else if (eCSSUnit_Inherit == posData.mBoxSizing.GetUnit()) {
    inherited = PR_TRUE;
    pos->mBoxSizing = parentPos->mBoxSizing;
  }

  // z-index
  if (! SetCoord(posData.mZIndex, pos->mZIndex, parentPos->mZIndex,
                 SETCOORD_IA, aContext, nsnull, inherited)) {
    if (eCSSUnit_Inherit == posData.mZIndex.GetUnit()) {
      // handle inherit, because it's ok to inherit 'auto' here
      inherited = PR_TRUE;
      pos->mZIndex = parentPos->mZIndex;
    }
  }

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_Position, pos);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mResetData)
      aHighestNode->mStyleData.mResetData = new (mPresContext) nsResetStyleData;
    aHighestNode->mStyleData.mResetData->mPositionData = pos;
    // Propagate the bit down.
    PropagateDependentBit(NS_STYLE_INHERIT_BIT(Position), aHighestNode);
  }

  return pos;
}

const nsStyleStruct* 
nsRuleNode::ComputeTableData(nsStyleStruct* aStartStruct,
                             const nsRuleDataStruct& aData, 
                             nsIStyleContext* aContext, 
                             nsRuleNode* aHighestNode,
                             const RuleDetail& aRuleDetail, PRBool aInherited)
{
  nsCOMPtr<nsIStyleContext> parentContext = aContext->GetParent();
  
  const nsRuleDataTable& tableData = NS_STATIC_CAST(const nsRuleDataTable&, aData);
  nsStyleTable* table;
  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    table = new (mPresContext) nsStyleTable(*NS_STATIC_CAST(nsStyleTable*, aStartStruct));
  else
    table = new (mPresContext) nsStyleTable();
  
  const nsStyleTable* parentTable = table;
  if (parentContext && 
      aRuleDetail != eRuleFullReset &&
      aRuleDetail != eRulePartialReset &&
      aRuleDetail != eRuleNone)
    parentTable = NS_STATIC_CAST(const nsStyleTable*,
                              parentContext->GetStyleData(eStyleStruct_Table));
  PRBool inherited = aInherited;

  // table-layout: auto, enum, inherit
  if (eCSSUnit_Enumerated == tableData.mLayout.GetUnit())
    table->mLayoutStrategy = tableData.mLayout.GetIntValue();
  else if (eCSSUnit_Auto == tableData.mLayout.GetUnit())
    table->mLayoutStrategy = NS_STYLE_TABLE_LAYOUT_AUTO;
  else if (eCSSUnit_Inherit == tableData.mLayout.GetUnit()) {
    inherited = PR_TRUE;
    table->mLayoutStrategy = parentTable->mLayoutStrategy;
  }

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
    
  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_Table, table);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mResetData)
      aHighestNode->mStyleData.mResetData = new (mPresContext) nsResetStyleData;
    aHighestNode->mStyleData.mResetData->mTableData = table;
    // Propagate the bit down.
    PropagateDependentBit(NS_STYLE_INHERIT_BIT(Table), aHighestNode);
  }

  return table;
}

const nsStyleStruct* 
nsRuleNode::ComputeTableBorderData(nsStyleStruct* aStartStruct,
                                   const nsRuleDataStruct& aData, 
                                   nsIStyleContext* aContext, 
                                   nsRuleNode* aHighestNode,
                                   const RuleDetail& aRuleDetail, PRBool aInherited)
{
  nsCOMPtr<nsIStyleContext> parentContext = aContext->GetParent();
  
  const nsRuleDataTable& tableData = NS_STATIC_CAST(const nsRuleDataTable&, aData);
  nsStyleTableBorder* table = nsnull;
  const nsStyleTableBorder* parentTable = nsnull;
  PRBool inherited = aInherited;

  if (parentContext && aRuleDetail != eRuleFullReset)
    parentTable = NS_STATIC_CAST(const nsStyleTableBorder*,
                        parentContext->GetStyleData(eStyleStruct_TableBorder));
  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    table = new (mPresContext) nsStyleTableBorder(*NS_STATIC_CAST(nsStyleTableBorder*, aStartStruct));
  else {
    // XXXldb What about eRuleFullInherited?  Which path is faster?
    if (aRuleDetail != eRuleFullMixed && aRuleDetail != eRuleFullReset) {
      // No question. We will have to inherit. Go ahead and init
      // with inherited vals from parent.
      inherited = PR_TRUE;
      if (parentTable)
        table = new (mPresContext) nsStyleTableBorder(*parentTable);
    }
  }

  if (!table)
    table = new (mPresContext) nsStyleTableBorder(mPresContext);
  if (!parentTable)
    parentTable = table;

  // border-collapse: enum, inherit
  if (eCSSUnit_Enumerated == tableData.mBorderCollapse.GetUnit()) {
    table->mBorderCollapse = tableData.mBorderCollapse.GetIntValue();
  }
  else if (eCSSUnit_Inherit == tableData.mBorderCollapse.GetUnit()) {
    inherited = PR_TRUE;
    table->mBorderCollapse = parentTable->mBorderCollapse;
  }

  nsStyleCoord coord;

  // border-spacing-x: length, inherit
  if (SetCoord(tableData.mBorderSpacingX, coord, coord, SETCOORD_LENGTH, aContext, mPresContext, inherited)) {
    table->mBorderSpacingX = coord.GetCoordValue();
  }
  else if (eCSSUnit_Inherit == tableData.mBorderSpacingX.GetUnit()) {
    inherited = PR_TRUE;
    table->mBorderSpacingX = parentTable->mBorderSpacingX;
  }
  // border-spacing-y: length, inherit
  if (SetCoord(tableData.mBorderSpacingY, coord, coord, SETCOORD_LENGTH, aContext, mPresContext, inherited)) {
    table->mBorderSpacingY = coord.GetCoordValue();
  }
  else if (eCSSUnit_Inherit == tableData.mBorderSpacingY.GetUnit()) {
    inherited = PR_TRUE;
    table->mBorderSpacingY = parentTable->mBorderSpacingY;
  }

  // caption-side: enum, inherit
  if (eCSSUnit_Enumerated == tableData.mCaptionSide.GetUnit()) {
    table->mCaptionSide = tableData.mCaptionSide.GetIntValue();
  }
  else if (eCSSUnit_Inherit == tableData.mCaptionSide.GetUnit()) {
    inherited = PR_TRUE;
    table->mCaptionSide = parentTable->mCaptionSide;
  }

  // empty-cells: enum, inherit
  if (eCSSUnit_Enumerated == tableData.mEmptyCells.GetUnit()) {
    table->mEmptyCells = tableData.mEmptyCells.GetIntValue();
  }
  else if (eCSSUnit_Inherit == tableData.mEmptyCells.GetUnit()) {
    inherited = PR_TRUE;
    table->mEmptyCells = parentTable->mEmptyCells;
  }

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_TableBorder, table);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mInheritedData)
      aHighestNode->mStyleData.mInheritedData = new (mPresContext) nsInheritedStyleData;
    aHighestNode->mStyleData.mInheritedData->mTableBorderData = table;
    // Propagate the bit down.
    PropagateDependentBit(NS_STYLE_INHERIT_BIT(TableBorder), aHighestNode);
  }

  return table;
}

const nsStyleStruct* 
nsRuleNode::ComputeContentData(nsStyleStruct* aStartStruct,
                               const nsRuleDataStruct& aData, 
                               nsIStyleContext* aContext, 
                               nsRuleNode* aHighestNode,
                               const RuleDetail& aRuleDetail, PRBool aInherited)
{
  nsCOMPtr<nsIStyleContext> parentContext = aContext->GetParent();
  
  const nsRuleDataContent& contentData = NS_STATIC_CAST(const nsRuleDataContent&, aData);
  nsStyleContent* content;
  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    content = new (mPresContext) nsStyleContent(*NS_STATIC_CAST(nsStyleContent*, aStartStruct));
  else
    content = new (mPresContext) nsStyleContent();
  
  const nsStyleContent* parentContent = content;
  if (parentContext && 
      aRuleDetail != eRuleFullReset &&
      aRuleDetail != eRulePartialReset &&
      aRuleDetail != eRuleNone)
    parentContent = NS_STATIC_CAST(const nsStyleContent*,
                            parentContext->GetStyleData(eStyleStruct_Content));
  PRBool inherited = aInherited;

  // content: [string, url, counter, attr, enum]+, inherit
  PRUint32 count;
  nsAutoString  buffer;
  nsCSSValueList* contentValue = contentData.mContent;
  if (contentValue) {
    if (eCSSUnit_Inherit == contentValue->mValue.GetUnit()) {
      inherited = PR_TRUE;
      count = parentContent->ContentCount();
      if (NS_SUCCEEDED(content->AllocateContents(count))) {
        nsStyleContentType type;
        while (0 < count--) {
          parentContent->GetContentAt(count, type, buffer);
          content->SetContentAt(count, type, buffer);
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
          switch (unit) {
            case eCSSUnit_String:   type = eStyleContentType_String;    break;
            case eCSSUnit_URL:      type = eStyleContentType_URL;       break;
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
                default:
                  NS_ERROR("bad content value");
              }
              break;
            default:
              NS_ERROR("bad content type");
          }
          if (type < eStyleContentType_OpenQuote) {
            value.GetStringValue(buffer);
            Unquote(buffer);
            content->SetContentAt(count++, type, buffer);
          }
          else {
            content->SetContentAt(count++, type, nullStr);
          }
          contentValue = contentValue->mNext;
        }
      } 
    }
  }

  // counter-increment: [string [int]]+, none, inherit
  nsCSSCounterData* ourIncrement = contentData.mCounterIncrement;
  if (ourIncrement) {
    PRInt32 increment;
    if (eCSSUnit_Inherit == ourIncrement->mCounter.GetUnit()) {
      inherited = PR_TRUE;
      count = parentContent->CounterIncrementCount();
      if (NS_SUCCEEDED(content->AllocateCounterIncrements(count))) {
        while (0 < count--) {
          parentContent->GetCounterIncrementAt(count, buffer, increment);
          content->SetCounterIncrementAt(count, buffer, increment);
        }
      }
    }
    else if (eCSSUnit_None == ourIncrement->mCounter.GetUnit()) {
      content->AllocateCounterIncrements(0);
    }
    else if (eCSSUnit_String == ourIncrement->mCounter.GetUnit()) {
      count = 0;
      while (ourIncrement) {
        count++;
        ourIncrement = ourIncrement->mNext;
      }
      if (NS_SUCCEEDED(content->AllocateCounterIncrements(count))) {
        count = 0;
        ourIncrement = contentData.mCounterIncrement;
        while (ourIncrement) {
          if (eCSSUnit_Integer == ourIncrement->mValue.GetUnit()) {
            increment = ourIncrement->mValue.GetIntValue();
          }
          else {
            increment = 1;
          }
          ourIncrement->mCounter.GetStringValue(buffer);
          content->SetCounterIncrementAt(count++, buffer, increment);
          ourIncrement = ourIncrement->mNext;
        }
      }
    }
  }

  // counter-reset: [string [int]]+, none, inherit
  nsCSSCounterData* ourReset = contentData.mCounterReset;
  if (ourReset) {
    PRInt32 reset;
    if (eCSSUnit_Inherit == ourReset->mCounter.GetUnit()) {
      inherited = PR_TRUE;
      count = parentContent->CounterResetCount();
      if (NS_SUCCEEDED(content->AllocateCounterResets(count))) {
        while (0 < count--) {
          parentContent->GetCounterResetAt(count, buffer, reset);
          content->SetCounterResetAt(count, buffer, reset);
        }
      }
    }
    else if (eCSSUnit_None == ourReset->mCounter.GetUnit()) {
      content->AllocateCounterResets(0);
    }
    else if (eCSSUnit_String == ourReset->mCounter.GetUnit()) {
      count = 0;
      while (ourReset) {
        count++;
        ourReset = ourReset->mNext;
      }
      if (NS_SUCCEEDED(content->AllocateCounterResets(count))) {
        count = 0;
        ourReset = contentData.mCounterReset;
        while (ourReset) {
          if (eCSSUnit_Integer == ourReset->mValue.GetUnit()) {
            reset = ourReset->mValue.GetIntValue();
          }
          else {
            reset = 0;
          }
          ourReset->mCounter.GetStringValue(buffer);
          content->SetCounterResetAt(count++, buffer, reset);
          ourReset = ourReset->mNext;
        }
      }
    }
  }

  // marker-offset: length, auto, inherit
  SetCoord(contentData.mMarkerOffset, content->mMarkerOffset, parentContent->mMarkerOffset,
           SETCOORD_LH | SETCOORD_AUTO, aContext, mPresContext, inherited);
    
  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_Content, content);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mResetData)
      aHighestNode->mStyleData.mResetData = new (mPresContext) nsResetStyleData;
    aHighestNode->mStyleData.mResetData->mContentData = content;
    // Propagate the bit down.
    PropagateDependentBit(NS_STYLE_INHERIT_BIT(Content), aHighestNode);
  }

  return content;
}

const nsStyleStruct* 
nsRuleNode::ComputeQuotesData(nsStyleStruct* aStartStruct,
                              const nsRuleDataStruct& aData, 
                              nsIStyleContext* aContext, 
                              nsRuleNode* aHighestNode,
                              const RuleDetail& aRuleDetail, PRBool aInherited)
{
  nsCOMPtr<nsIStyleContext> parentContext = aContext->GetParent();
  
  const nsRuleDataContent& contentData = NS_STATIC_CAST(const nsRuleDataContent&, aData);
  nsStyleQuotes* quotes = nsnull;
  const nsStyleQuotes* parentQuotes = nsnull;
  PRBool inherited = aInherited;

  if (parentContext && aRuleDetail != eRuleFullReset)
    parentQuotes = NS_STATIC_CAST(const nsStyleQuotes*,
                             parentContext->GetStyleData(eStyleStruct_Quotes));
  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    quotes = new (mPresContext) nsStyleQuotes(*NS_STATIC_CAST(nsStyleQuotes*, aStartStruct));
  else {
    // XXXldb What about eRuleFullInherited?  Which path is faster?
    if (aRuleDetail != eRuleFullMixed && aRuleDetail != eRuleFullReset) {
      // No question. We will have to inherit. Go ahead and init
      // with inherited vals from parent.
      inherited = PR_TRUE;
      if (parentQuotes)
        quotes = new (mPresContext) nsStyleQuotes(*parentQuotes);
    }
  }

  if (!quotes)
    quotes = new (mPresContext) nsStyleQuotes();
  if (!parentQuotes)
    parentQuotes = quotes;

  // quotes: [string string]+, none, inherit
  PRUint32 count;
  nsAutoString  buffer;
  nsCSSQuotes* ourQuotes = contentData.mQuotes;
  if (ourQuotes) {
    nsAutoString  closeBuffer;
    if (eCSSUnit_Inherit == ourQuotes->mOpen.GetUnit()) {
      inherited = PR_TRUE;
      count = parentQuotes->QuotesCount();
      if (NS_SUCCEEDED(quotes->AllocateQuotes(count))) {
        while (0 < count--) {
          parentQuotes->GetQuotesAt(count, buffer, closeBuffer);
          quotes->SetQuotesAt(count, buffer, closeBuffer);
        }
      }
    }
    else if (eCSSUnit_None == ourQuotes->mOpen.GetUnit()) {
      quotes->AllocateQuotes(0);
    }
    else if (eCSSUnit_String == ourQuotes->mOpen.GetUnit()) {
      count = 0;
      while (ourQuotes) {
        count++;
        ourQuotes = ourQuotes->mNext;
      }
      if (NS_SUCCEEDED(quotes->AllocateQuotes(count))) {
        count = 0;
        ourQuotes = contentData.mQuotes;
        while (ourQuotes) {
          ourQuotes->mOpen.GetStringValue(buffer);
          ourQuotes->mClose.GetStringValue(closeBuffer);
          Unquote(buffer);
          Unquote(closeBuffer);
          quotes->SetQuotesAt(count++, buffer, closeBuffer);
          ourQuotes = ourQuotes->mNext;
        }
      }
    }
  }

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_Quotes, quotes);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mInheritedData)
      aHighestNode->mStyleData.mInheritedData = new (mPresContext) nsInheritedStyleData;
    aHighestNode->mStyleData.mInheritedData->mQuotesData = quotes;
    // Propagate the bit down.
    PropagateDependentBit(NS_STYLE_INHERIT_BIT(Quotes), aHighestNode);
  }

  return quotes;
}

#ifdef INCLUDE_XUL
const nsStyleStruct* 
nsRuleNode::ComputeXULData(nsStyleStruct* aStartStruct,
                           const nsRuleDataStruct& aData, 
                           nsIStyleContext* aContext, 
                           nsRuleNode* aHighestNode,
                           const RuleDetail& aRuleDetail, PRBool aInherited)
{
  nsCOMPtr<nsIStyleContext> parentContext = aContext->GetParent();
  
  const nsRuleDataXUL& xulData = NS_STATIC_CAST(const nsRuleDataXUL&, aData);
  nsStyleXUL* xul = nsnull;
  
  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    xul = new (mPresContext) nsStyleXUL(*NS_STATIC_CAST(nsStyleXUL*, aStartStruct));
  else
    xul = new (mPresContext) nsStyleXUL();

  const nsStyleXUL* parentXUL = xul;
  if (parentContext && 
      aRuleDetail != eRuleFullReset &&
      aRuleDetail != eRulePartialReset &&
      aRuleDetail != eRuleNone)
    parentXUL = NS_STATIC_CAST(const nsStyleXUL*,
                                parentContext->GetStyleData(eStyleStruct_XUL));

  PRBool inherited = aInherited;

  // box-align: enum, inherit
  if (eCSSUnit_Enumerated == xulData.mBoxAlign.GetUnit()) {
    xul->mBoxAlign = xulData.mBoxAlign.GetIntValue();
  }
  else if (eCSSUnit_Inherit == xulData.mBoxAlign.GetUnit()) {
    inherited = PR_TRUE;
    xul->mBoxAlign = parentXUL->mBoxAlign;
  }

  // box-direction: enum, inherit
  if (eCSSUnit_Enumerated == xulData.mBoxDirection.GetUnit()) {
    xul->mBoxDirection = xulData.mBoxDirection.GetIntValue();
  }
  else if (eCSSUnit_Inherit == xulData.mBoxDirection.GetUnit()) {
    inherited = PR_TRUE;
    xul->mBoxDirection = parentXUL->mBoxDirection;
  }

  // box-flex: factor, inherit
  if (eCSSUnit_Number == xulData.mBoxFlex.GetUnit()) {
    xul->mBoxFlex = xulData.mBoxFlex.GetFloatValue();
  }
  else if (eCSSUnit_Inherit == xulData.mBoxOrient.GetUnit()) {
    inherited = PR_TRUE;
    xul->mBoxFlex = parentXUL->mBoxFlex;
  }

  // box-orient: enum, inherit
  if (eCSSUnit_Enumerated == xulData.mBoxOrient.GetUnit()) {
    xul->mBoxOrient = xulData.mBoxOrient.GetIntValue();
  }
  else if (eCSSUnit_Inherit == xulData.mBoxOrient.GetUnit()) {
    inherited = PR_TRUE;
    xul->mBoxOrient = parentXUL->mBoxOrient;
  }

  // box-pack: enum, inherit
  if (eCSSUnit_Enumerated == xulData.mBoxPack.GetUnit()) {
    xul->mBoxPack = xulData.mBoxPack.GetIntValue();
  }
  else if (eCSSUnit_Inherit == xulData.mBoxPack.GetUnit()) {
    inherited = PR_TRUE;
    xul->mBoxPack = parentXUL->mBoxPack;
  }

  // box-ordinal-group: integer
  if (eCSSUnit_Integer == xulData.mBoxOrdinal.GetUnit()) {
    xul->mBoxOrdinal = xulData.mBoxOrdinal.GetIntValue();
  }

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_XUL, xul);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mResetData)
      aHighestNode->mStyleData.mResetData = new (mPresContext) nsResetStyleData;
    aHighestNode->mStyleData.mResetData->mXULData = xul;
    // Propagate the bit down.
    PropagateDependentBit(NS_STYLE_INHERIT_BIT(XUL), aHighestNode);
  }

  return xul;
}
#endif

#ifdef MOZ_SVG
static void
SetSVGPaint(const nsCSSValue& aValue, const nsStyleSVGPaint& parentPaint,
            nsIPresContext* aPresContext, nsStyleSVGPaint& aResult, PRBool& aInherited)
{
  if (aValue.GetUnit() == eCSSUnit_Inherit) {
    aResult = parentPaint;
    aInherited = PR_TRUE;
  } else if (aValue.GetUnit() == eCSSUnit_None) {
    aResult.mType = eStyleSVGPaintType_None;
  } else if (SetColor(aValue, parentPaint.mColor, aPresContext, aResult.mColor, aInherited)) {
    aResult.mType = eStyleSVGPaintType_Color;
  }
}

static void
SetSVGOpacity(const nsCSSValue& aValue, float parentOpacity, float& opacity, PRBool& aInherited)
{
  if (aValue.GetUnit() == eCSSUnit_Inherit) {
    opacity = parentOpacity;
    aInherited = PR_TRUE;
  }
  else if (aValue.GetUnit() == eCSSUnit_Number) {
    opacity = aValue.GetFloatValue();
  }
}

static void
SetSVGLength(const nsCSSValue& aValue, float parentLength, float& length,
             nsIStyleContext* aContext, nsIPresContext* aPresContext, PRBool& aInherited)
{
  nsStyleCoord coord;
  PRBool dummy;
  if (SetCoord(aValue, coord, coord,
               SETCOORD_LP | SETCOORD_FACTOR,
               aContext, aPresContext, dummy)) {
    if (coord.GetUnit() == eStyleUnit_Factor) { // user units
      length = (float) coord.GetFactorValue();
    }
    else {
      length = (float) coord.GetCoordValue();
      float twipsPerPix;
      aPresContext->GetScaledPixelsToTwips(&twipsPerPix);
      if (twipsPerPix == 0.0f)
        twipsPerPix = 1e-20f;
      length /= twipsPerPix;
    }
  }
  else if (aValue.GetUnit() == eCSSUnit_Inherit) {
    length = parentLength;
    aInherited = PR_TRUE;
  }
}  

const nsStyleStruct* 
nsRuleNode::ComputeSVGData(nsStyleStruct* aStartStruct,
                           const nsRuleDataStruct& aData, 
                           nsIStyleContext* aContext, 
                           nsRuleNode* aHighestNode,
                           const RuleDetail& aRuleDetail, PRBool aInherited)
{
  nsCOMPtr<nsIStyleContext> parentContext = aContext->GetParent();

  nsStyleSVG* svg = nsnull;
  nsStyleSVG* parentSVG = svg;
  PRBool inherited = aInherited;
  const nsRuleDataSVG& SVGData = NS_STATIC_CAST(const nsRuleDataSVG&, aData);

  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    svg = new (mPresContext) nsStyleSVG(*NS_STATIC_CAST(nsStyleSVG*,aStartStruct));
  else {
    if (aRuleDetail != eRuleFullMixed) {
      // No question. We will have to inherit. Go ahead and init
      // with inherited vals from parent.
      inherited = PR_TRUE;
      if (parentContext)
        parentSVG = (nsStyleSVG*)parentContext->GetStyleData(eStyleStruct_SVG);
      if (parentSVG)
        svg = new (mPresContext) nsStyleSVG(*parentSVG);
    }
  }

  if (!svg)
    svg = parentSVG = new (mPresContext) nsStyleSVG();

  // fill: 
  SetSVGPaint(SVGData.mFill, parentSVG->mFill, mPresContext, svg->mFill, inherited);

  // fill-opacity:
  SetSVGOpacity(SVGData.mFillOpacity, parentSVG->mFillOpacity, svg->mFillOpacity, inherited);

  // fill-rule: enum, inherit
  if (eCSSUnit_Enumerated == SVGData.mFillRule.GetUnit()) {
    svg->mFillRule = SVGData.mFillRule.GetIntValue();
  }
  else if (eCSSUnit_Inherit == SVGData.mFillRule.GetUnit()) {
    inherited = PR_TRUE;
    svg->mFillRule = parentSVG->mFillRule;
  }
  
  // stroke: 
  SetSVGPaint(SVGData.mStroke, parentSVG->mStroke, mPresContext, svg->mStroke, inherited);

  // stroke-dasharray: <dasharray>, none, inherit
  if (eCSSUnit_String == SVGData.mStrokeDasharray.GetUnit()) {
    SVGData.mStrokeDasharray.GetStringValue(svg->mStrokeDasharray);
  }
  else if (eCSSUnit_None == SVGData.mStrokeDasharray.GetUnit()) {
    svg->mStrokeDasharray.Truncate();
  }
  else if (eCSSUnit_Inherit == SVGData.mStrokeDasharray.GetUnit()) {
    inherited = PR_TRUE;
    svg->mStrokeDasharray = parentSVG->mStrokeDasharray;
  }

  // stroke-dashoffset: <dashoffset>, inherit
  SetSVGLength(SVGData.mStrokeDashoffset, parentSVG->mStrokeDashoffset,
               svg->mStrokeDashoffset, aContext, mPresContext, inherited);
  
  // stroke-linecap: enum, inherit
  if (eCSSUnit_Enumerated == SVGData.mStrokeLinecap.GetUnit()) {
    svg->mStrokeLinecap = SVGData.mStrokeLinecap.GetIntValue();
  }
  else if (eCSSUnit_Inherit == SVGData.mStrokeLinecap.GetUnit()) {
    inherited = PR_TRUE;
    svg->mStrokeLinecap = parentSVG->mStrokeLinecap;
  }

  // stroke-linejoin: enum, inherit
  if (eCSSUnit_Enumerated == SVGData.mStrokeLinejoin.GetUnit()) {
    svg->mStrokeLinejoin = SVGData.mStrokeLinejoin.GetIntValue();
  }
  else if (eCSSUnit_Inherit == SVGData.mStrokeLinejoin.GetUnit()) {
    inherited = PR_TRUE;
    svg->mStrokeLinejoin = parentSVG->mStrokeLinejoin;
  }

  // stroke-miterlimit: <miterlimit>, inherit
  if (eCSSUnit_Number == SVGData.mStrokeMiterlimit.GetUnit()) {
    float value = SVGData.mStrokeMiterlimit.GetFloatValue();
    if (value > 1) { // XXX this check should probably be in nsCSSParser
      svg->mStrokeMiterlimit = value;
    }
  }
  else if (eCSSUnit_Inherit == SVGData.mStrokeMiterlimit.GetUnit()) {
    svg->mStrokeMiterlimit = parentSVG->mStrokeMiterlimit;
    inherited = PR_TRUE;
  }

  // stroke-opacity:
  SetSVGOpacity(SVGData.mStrokeOpacity, parentSVG->mStrokeOpacity, svg->mStrokeOpacity, inherited);  

  // stroke-width:
  SetSVGLength(SVGData.mStrokeWidth, parentSVG->mStrokeWidth,
               svg->mStrokeWidth, aContext, mPresContext, inherited);

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_SVG, svg);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mInheritedData)
      aHighestNode->mStyleData.mInheritedData = new (mPresContext) nsInheritedStyleData;
    aHighestNode->mStyleData.mInheritedData->mSVGData = svg;
    // Propagate the bit down.
    PropagateDependentBit(NS_STYLE_INHERIT_BIT(SVG), aHighestNode);
  }

  return svg;
}
#endif

inline const nsStyleStruct* 
nsRuleNode::GetParentData(const nsStyleStructID aSID)
{
  nsRuleNode* ruleNode = mParent;
  nsStyleStruct* currStruct = nsnull;
  while (ruleNode) {
    currStruct = ruleNode->mStyleData.GetStyleData(aSID);
    if (currStruct)
      break; // We found a rule with fully specified data.  We don't need to go up
             // the tree any further, since the remainder of this branch has already
             // been computed.
    ruleNode = ruleNode->mParent; // Climb up to the next rule in the tree (a less specific rule).
  }  

  return currStruct; // Just return whatever we found.
}

nsRuleNode::GetStyleDataFn
nsRuleNode::gGetStyleDataFn[] = {

#define STYLE_STRUCT(name, checkdata_cb) &nsRuleNode::Get##name##Data,
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

  nsnull
};

const nsStyleStruct* 
nsRuleNode::GetStyleData(nsStyleStructID aSID, 
                         nsIStyleContext* aContext,
                         PRBool aComputeData)
{
  const nsStyleStruct* cachedData = mStyleData.GetStyleData(aSID);
  if (cachedData)
    return cachedData; // We have a fully specified struct. Just return it.

  if (mDependentBits & nsCachedStyleData::GetBitForSID(aSID))
    return GetParentData(aSID); // We depend on an ancestor for this
                                // struct since the cached struct it has
                                // is also appropriate for this rule
                                // node.  Just go up the rule tree and
                                // return the first cached struct we
                                // find.

  // Nothing is cached.  We'll have to delve further and examine our rules.
  GetStyleDataFn fn = gGetStyleDataFn[aSID];
  return fn ? (this->*fn)(aContext, aComputeData) : nsnull;
}
