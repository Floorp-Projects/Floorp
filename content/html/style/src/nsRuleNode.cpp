/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

#include "nsRuleNode.h"
#include "nsIDeviceContext.h"
#include "nsILookAndFeel.h"
#include "nsIPresShell.h"
#include "nsIFontMetrics.h"
#include "nsIDocShellTreeItem.h"
#include "nsStyleUtil.h"
#include "nsCSSAtoms.h"

class nsShellISupportsKey : public nsHashKey {
public:
  nsISupports* mKey;
  
public:
  nsShellISupportsKey(nsISupports* key) {
#ifdef DEBUG
    mKeyType = SupportsKey;
#endif
    mKey = key;
  }
  
  ~nsShellISupportsKey(void) {
  }
  
  void* operator new(size_t sz, nsIPresContext* aContext) {
    void* result = nsnull;
    aContext->AllocateFromShell(sz, &result);
    return result;
  };
  void operator delete(void* aPtr) {} // Does nothing. The arena will free us up when the rule tree
                                      // dies.

  void Destroy(nsIPresContext* aContext) {
    this->~nsShellISupportsKey();
    aContext->FreeToShell(sizeof(nsShellISupportsKey), this);
  }

  PRUint32 HashCode(void) const {
    return (PRUint32)mKey;
  }

  PRBool Equals(const nsHashKey *aKey) const {
    NS_ASSERTION(aKey->GetKeyType() == SupportsKey, "mismatched key types");
    return (mKey == ((nsShellISupportsKey *) aKey)->mKey);
  }

  nsHashKey* Clone() const {
    return (nsHashKey*)this; // Just return ourselves.
  }
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
                   nsFont* aFont, 
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
      nsFont* font = aStyleContext ? &(((nsStyleFont*)aStyleContext->GetStyleData(eStyleStruct_Font))->mFont) : aFont;
      return NSToCoordRound(aValue.GetFloatValue() * (float)font->size);
      // XXX scale against font metrics height instead?
    }
    case eCSSUnit_EN: {
      aInherited = PR_TRUE;
      nsFont* font = aStyleContext ? &(((nsStyleFont*)aStyleContext->GetStyleData(eStyleStruct_Font))->mFont) : aFont;
      return NSToCoordRound((aValue.GetFloatValue() * (float)font->size) / 2.0f);
    }
    case eCSSUnit_XHeight: {
      aInherited = PR_TRUE;
      nsFont* font = aStyleContext ? &(((nsStyleFont*)aStyleContext->GetStyleData(eStyleStruct_Font))->mFont) : aFont;
      nsIFontMetrics* fm;
      aPresContext->GetMetricsFor(*font, &fm);
      NS_ASSERTION(nsnull != fm, "can't get font metrics");
      nscoord xHeight;
      if (nsnull != fm) {
        fm->GetXHeight(xHeight);
        NS_RELEASE(fm);
      }
      else {
        xHeight = ((font->size * 2) / 3);
      }
      return NSToCoordRound(aValue.GetFloatValue() * (float)xHeight);
    }
    case eCSSUnit_CapHeight: {
      NS_NOTYETIMPLEMENTED("cap height unit");
      aInherited = PR_TRUE;
      nsFont* font = aStyleContext ? &(((nsStyleFont*)aStyleContext->GetStyleData(eStyleStruct_Font))->mFont) : aFont;
      nscoord capHeight = ((font->size / 3) * 2); // XXX HACK!
      return NSToCoordRound(aValue.GetFloatValue() * (float)capHeight);
    }
    case eCSSUnit_Pixel:
      float p2t;
      aPresContext->GetScaledPixelsToTwips(&p2t);
      return NSFloatPixelsToTwips(aValue.GetFloatValue(), p2t);
    default:
      break;
  }
  return 0;
}

#define SETCOORD_NORMAL       0x01
#define SETCOORD_AUTO         0x02
#define SETCOORD_INHERIT      0x04
#define SETCOORD_PERCENT      0x08
#define SETCOORD_FACTOR       0x10
#define SETCOORD_LENGTH       0x20
#define SETCOORD_INTEGER      0x40
#define SETCOORD_ENUMERATED   0x80

#define SETCOORD_LP     (SETCOORD_LENGTH | SETCOORD_PERCENT)
#define SETCOORD_LH     (SETCOORD_LENGTH | SETCOORD_INHERIT)
#define SETCOORD_AH     (SETCOORD_AUTO | SETCOORD_INHERIT)
#define SETCOORD_LPH    (SETCOORD_LP | SETCOORD_INHERIT)
#define SETCOORD_LPFHN  (SETCOORD_LPH | SETCOORD_FACTOR | SETCOORD_NORMAL)
#define SETCOORD_LPAH   (SETCOORD_LP | SETCOORD_AH)
#define SETCOORD_LPEH   (SETCOORD_LP | SETCOORD_ENUMERATED | SETCOORD_INHERIT)
#define SETCOORD_LE     (SETCOORD_LENGTH | SETCOORD_ENUMERATED)
#define SETCOORD_LEH    (SETCOORD_LE | SETCOORD_INHERIT)
#define SETCOORD_IA     (SETCOORD_INTEGER | SETCOORD_AUTO)
#define SETCOORD_LAE		(SETCOORD_LENGTH | SETCOORD_AUTO | SETCOORD_ENUMERATED)

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
    }
    else {
      aCoord.SetInheritValue(); // needs to be computed by client
    }
    aInherited = PR_TRUE;
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
    nsILookAndFeel* look = nsnull;
    if (NS_SUCCEEDED(aPresContext->GetLookAndFeel(&look)) && look) {
      nsILookAndFeel::nsColorID colorID = (nsILookAndFeel::nsColorID)aValue.GetIntValue();
      if (NS_SUCCEEDED(look->GetColor(colorID, aResult))) {
        result = PR_TRUE;
      }
      NS_RELEASE(look);
    }
  }
  else if (eCSSUnit_Inherit == unit) {
    aResult = aParentColor;
    result = PR_TRUE;
    aInherited = PR_TRUE;
  }
  return result;
}

// nsRuleNode globals
PRUint32 nsRuleNode::gRefCnt = 0;

NS_IMPL_ADDREF(nsRuleNode)
NS_IMPL_RELEASE_WITH_DESTROY(nsRuleNode, Destroy())
NS_IMPL_QUERY_INTERFACE1(nsRuleNode, nsIRuleNode)

// Overloaded new operator. Initializes the memory to 0 and relies on an arena
// (which comes from the presShell) to perform the allocation.
void* 
nsRuleNode::operator new(size_t sz, nsIPresContext* aPresContext)
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

void nsRuleNode::CreateRootNode(nsIPresContext* aPresContext, nsIRuleNode** aResult)
{
  *aResult = new (aPresContext) nsRuleNode(aPresContext);
  NS_IF_ADDREF(*aResult);
}

nsRuleNode::nsRuleNode(nsIPresContext* aContext, nsIStyleRule* aRule, nsRuleNode* aParent)
    :mPresContext(aContext), mParent(aParent), mChildren(nsnull), mInheritBits(0), mNoneBits(0)
{
  NS_INIT_REFCNT();
  gRefCnt++;
  mRule = aRule;
}

nsRuleNode::~nsRuleNode()
{
  if (mStyleData.mResetData || mStyleData.mInheritedData)
    mStyleData.Destroy(0, mPresContext);
  delete mChildren;
  gRefCnt--;
}

NS_IMETHODIMP 
nsRuleNode::Transition(nsIStyleRule* aRule, nsIRuleNode** aResult)
{
  nsCOMPtr<nsIRuleNode> next;
  nsShellISupportsKey key(aRule);
  if (mChildren)
    next = getter_AddRefs(NS_STATIC_CAST(nsIRuleNode*, mChildren->Get(&key)));
  
  if (!next) {
    // Create the new entry in our table.
    nsRuleNode* newNode = new (mPresContext) nsRuleNode(mPresContext, aRule, this);
    next = newNode;

    if (!mChildren)
      mChildren = new nsSupportsHashtable(4);

    // Clone the key ourselves by allocating it from the shell's arena.
    nsShellISupportsKey* clonedKey = new (mPresContext) nsShellISupportsKey(aRule);
    mChildren->Put(clonedKey, next);
  }

  *aResult = next;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsRuleNode::GetParent(nsIRuleNode** aResult)
{
  *aResult = mParent;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsRuleNode::IsRoot(PRBool* aResult)
{
  *aResult = (mParent == nsnull);
  return NS_OK;
}

NS_IMETHODIMP
nsRuleNode::GetRule(nsIStyleRule** aResult)
{
  *aResult = mRule;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
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
    // all cached data along this path.
    nsRuleNode* curr = this;
    while (curr) {
      curr->mNoneBits &= ~NS_STYLE_INHERIT_MASK;
      curr->mInheritBits &= ~NS_STYLE_INHERIT_MASK;
      if (curr->mStyleData.mResetData || curr->mStyleData.mInheritedData)
        curr->mStyleData.Destroy(0, mPresContext);

      if (curr == ruleDest)
        break;

      curr = curr->mParent;
    }
  }

  return NS_OK;
}

PRBool PR_CALLBACK ClearCachedDataHelper(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsIRuleNode* ruleNode = (nsIRuleNode*)aData;
  nsIStyleRule* rule = (nsIStyleRule*)aClosure;
  ruleNode->ClearCachedDataInSubtree(rule);
  return PR_TRUE;
}

NS_IMETHODIMP
nsRuleNode::ClearCachedDataInSubtree(nsIStyleRule* aRule)
{
  if (mRule == aRule) {
    // We have a match.  Blow away all data stored at this node.
    if (mStyleData.mResetData || mStyleData.mInheritedData)
      mStyleData.Destroy(0, mPresContext);
    mNoneBits &= ~NS_STYLE_INHERIT_MASK;
    mInheritBits &= ~NS_STYLE_INHERIT_MASK;  // XXXdwh need to clear all data in descendants!
  }

  if (mChildren)
    mChildren->Enumerate(ClearCachedDataHelper, (void*)aRule);

  return NS_OK;
}

NS_IMETHODIMP
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
  while (curr && curr != aHighestNode) {
    if (curr->mNoneBits & aBit)
      break;

    curr->mNoneBits |= aBit;
    curr = curr->mParent;
  }
}

inline void
nsRuleNode::PropagateInheritBit(PRUint32 aBit, nsRuleNode* aHighestNode)
{
  if (mInheritBits & aBit)
    return; // Already set.

  nsRuleNode* curr = this;
  while (curr && curr != aHighestNode) {
    curr->mInheritBits |= aBit;
    curr = curr->mParent;
  }
}

inline PRBool 
nsRuleNode::InheritsFromParentRule(const nsStyleStructID& aSID)
{
  return mInheritBits & nsCachedStyleData::GetBitForSID(aSID);
}

inline nsRuleNode::RuleDetail
nsRuleNode::CheckSpecifiedProperties(const nsStyleStructID& aSID, const nsCSSStruct& aCSSStruct)
{
  switch (aSID) {
  case eStyleStruct_Display:
    return CheckDisplayProperties((const nsCSSDisplay&)aCSSStruct);
  case eStyleStruct_Text:
    return CheckTextProperties((const nsCSSText&)aCSSStruct);
  case eStyleStruct_TextReset:
    return CheckTextResetProperties((const nsCSSText&)aCSSStruct);
  case eStyleStruct_UserInterface:
    return CheckUIProperties((const nsCSSUserInterface&)aCSSStruct);
  case eStyleStruct_UIReset:
    return CheckUIResetProperties((const nsCSSUserInterface&)aCSSStruct);
  case eStyleStruct_Visibility:
    return CheckVisibilityProperties((const nsCSSDisplay&)aCSSStruct);
  case eStyleStruct_Font:
    return CheckFontProperties((const nsCSSFont&)aCSSStruct);
  case eStyleStruct_Color:
    return CheckColorProperties((const nsCSSColor&)aCSSStruct);
  case eStyleStruct_Background:
    return CheckBackgroundProperties((const nsCSSColor&)aCSSStruct);
  case eStyleStruct_Margin:
      return CheckMarginProperties((const nsCSSMargin&)aCSSStruct);
  case eStyleStruct_Border:
    return CheckBorderProperties((const nsCSSMargin&)aCSSStruct);
  case eStyleStruct_Padding:
    return CheckPaddingProperties((const nsCSSMargin&)aCSSStruct);
  case eStyleStruct_Outline:
    return CheckOutlineProperties((const nsCSSMargin&)aCSSStruct);
  case eStyleStruct_List:
    return CheckListProperties((const nsCSSList&)aCSSStruct);
  case eStyleStruct_Position:
    return CheckPositionProperties((const nsCSSPosition&)aCSSStruct);
  case eStyleStruct_Table:
    return CheckTableProperties((const nsCSSTable&)aCSSStruct);
  case eStyleStruct_TableBorder:
    return CheckTableBorderProperties((const nsCSSTable&)aCSSStruct);
  case eStyleStruct_Content:
    return CheckContentProperties((const nsCSSContent&)aCSSStruct);
  case eStyleStruct_Quotes:
    return CheckQuotesProperties((const nsCSSContent&)aCSSStruct);
#ifdef INCLUDE_XUL
  case eStyleStruct_XUL:
    return CheckXULProperties((const nsCSSXUL&)aCSSStruct);
#endif
  }

  return eRuleNone;
}

const nsStyleStruct*
nsRuleNode::GetDisplayData(nsIStyleContext* aContext)
{
  nsCSSDisplay displayData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_Display, mPresContext, aContext);
  ruleData.mDisplayData = &displayData;

  nsCSSRect clip;
  displayData.mClip = &clip;
  const nsStyleStruct* res = WalkRuleTree(eStyleStruct_Display, aContext, &ruleData, &displayData);
  displayData.mClip = nsnull;
  return res;
}

const nsStyleStruct*
nsRuleNode::GetVisibilityData(nsIStyleContext* aContext)
{
  nsCSSDisplay displayData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_Visibility, mPresContext, aContext);
  ruleData.mDisplayData = &displayData;

  return WalkRuleTree(eStyleStruct_Visibility, aContext, &ruleData, &displayData);
}

const nsStyleStruct*
nsRuleNode::GetTextData(nsIStyleContext* aContext)
{
  nsCSSText textData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_Text, mPresContext, aContext);
  ruleData.mTextData = &textData;

  return WalkRuleTree(eStyleStruct_Text, aContext, &ruleData, &textData);
}

const nsStyleStruct*
nsRuleNode::GetTextResetData(nsIStyleContext* aContext)
{
  nsCSSText textData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_TextReset, mPresContext, aContext);
  ruleData.mTextData = &textData;

  return WalkRuleTree(eStyleStruct_TextReset, aContext, &ruleData, &textData);
}

const nsStyleStruct*
nsRuleNode::GetUIData(nsIStyleContext* aContext)
{
  nsCSSUserInterface uiData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_UserInterface, mPresContext, aContext);
  ruleData.mUIData = &uiData;

  const nsStyleStruct* res = WalkRuleTree(eStyleStruct_UserInterface, aContext, &ruleData, &uiData);
  uiData.mCursor = nsnull;
  return res;
}

const nsStyleStruct*
nsRuleNode::GetUIResetData(nsIStyleContext* aContext)
{
  nsCSSUserInterface uiData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_UIReset, mPresContext, aContext);
  ruleData.mUIData = &uiData;

  const nsStyleStruct* res = WalkRuleTree(eStyleStruct_UIReset, aContext, &ruleData, &uiData);
  uiData.mKeyEquivalent = nsnull;
  return res;
}

const nsStyleStruct*
nsRuleNode::GetFontData(nsIStyleContext* aContext)
{
  nsCSSFont fontData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_Font, mPresContext, aContext);
  ruleData.mFontData = &fontData;

  return WalkRuleTree(eStyleStruct_Font, aContext, &ruleData, &fontData);
}

const nsStyleStruct*
nsRuleNode::GetColorData(nsIStyleContext* aContext)
{
  nsCSSColor colorData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_Color, mPresContext, aContext);
  ruleData.mColorData = &colorData;

  return WalkRuleTree(eStyleStruct_Color, aContext, &ruleData, &colorData);
}

const nsStyleStruct*
nsRuleNode::GetBackgroundData(nsIStyleContext* aContext)
{
  nsCSSColor colorData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_Background, mPresContext, aContext);
  ruleData.mColorData = &colorData;

  return WalkRuleTree(eStyleStruct_Background, aContext, &ruleData, &colorData);
}

const nsStyleStruct*
nsRuleNode::GetMarginData(nsIStyleContext* aContext)
{
  nsCSSMargin marginData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_Margin, mPresContext, aContext);
  ruleData.mMarginData = &marginData;

  nsCSSRect margin;
  marginData.mMargin = &margin;
  
  const nsStyleStruct* res = WalkRuleTree(eStyleStruct_Margin, aContext, &ruleData, &marginData);
  
  marginData.mMargin = nsnull;
  return res;
}

const nsStyleStruct*
nsRuleNode::GetBorderData(nsIStyleContext* aContext)
{
  nsCSSMargin marginData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_Border, mPresContext, aContext);
  ruleData.mMarginData = &marginData;

  nsCSSRect borderWidth;
  nsCSSRect borderColor;
  nsCSSRect borderStyle;
  nsCSSRect borderRadius;
  marginData.mBorderWidth = &borderWidth;
  marginData.mBorderColor = &borderColor;
  marginData.mBorderStyle = &borderStyle;
  marginData.mBorderRadius = &borderRadius;
  
  const nsStyleStruct* res = WalkRuleTree(eStyleStruct_Border, aContext, &ruleData, &marginData);
  
  marginData.mBorderWidth = marginData.mBorderColor = marginData.mBorderStyle = marginData.mBorderRadius = nsnull;
  return res;
}

const nsStyleStruct*
nsRuleNode::GetPaddingData(nsIStyleContext* aContext)
{
  nsCSSMargin marginData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_Padding, mPresContext, aContext);
  ruleData.mMarginData = &marginData;

  nsCSSRect padding;
  marginData.mPadding = &padding;
  
  const nsStyleStruct* res = WalkRuleTree(eStyleStruct_Padding, aContext, &ruleData, &marginData);
  
  marginData.mPadding = nsnull;
  return res;
}

const nsStyleStruct*
nsRuleNode::GetOutlineData(nsIStyleContext* aContext)
{
  nsCSSMargin marginData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_Outline, mPresContext, aContext);
  ruleData.mMarginData = &marginData;

  nsCSSRect outlineRadius;
  marginData.mOutlineRadius = &outlineRadius;
  
  const nsStyleStruct* res = WalkRuleTree(eStyleStruct_Outline, aContext, &ruleData, &marginData);
  
  marginData.mOutlineRadius = nsnull;
  return res;
}

const nsStyleStruct*
nsRuleNode::GetListData(nsIStyleContext* aContext)
{
  nsCSSList listData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_List, mPresContext, aContext);
  ruleData.mListData = &listData;

  return WalkRuleTree(eStyleStruct_List, aContext, &ruleData, &listData);
}

const nsStyleStruct*
nsRuleNode::GetPositionData(nsIStyleContext* aContext)
{
  nsCSSPosition posData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_Position, mPresContext, aContext);
  ruleData.mPositionData = &posData;

  nsCSSRect offset;
  posData.mOffset = &offset;
  
  const nsStyleStruct* res = WalkRuleTree(eStyleStruct_Position, aContext, &ruleData, &posData);
  
  posData.mOffset = nsnull;
  return res;
}

const nsStyleStruct*
nsRuleNode::GetTableData(nsIStyleContext* aContext)
{
  nsCSSTable tableData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_Table, mPresContext, aContext);
  ruleData.mTableData = &tableData;

  return WalkRuleTree(eStyleStruct_Table, aContext, &ruleData, &tableData);
}

const nsStyleStruct*
nsRuleNode::GetTableBorderData(nsIStyleContext* aContext)
{
  nsCSSTable tableData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_TableBorder, mPresContext, aContext);
  ruleData.mTableData = &tableData;

  return WalkRuleTree(eStyleStruct_TableBorder, aContext, &ruleData, &tableData);
}

const nsStyleStruct*
nsRuleNode::GetContentData(nsIStyleContext* aContext)
{
  nsCSSContent contentData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_Content, mPresContext, aContext);
  ruleData.mContentData = &contentData;

  const nsStyleStruct* res = WalkRuleTree(eStyleStruct_Content, aContext, &ruleData, &contentData);
  contentData.mCounterIncrement = contentData.mCounterReset = nsnull;
  contentData.mContent = nsnull; // We are sharing with some style rule.  It really owns the data.
  return res;
}

const nsStyleStruct*
nsRuleNode::GetQuotesData(nsIStyleContext* aContext)
{
  nsCSSContent contentData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_Quotes, mPresContext, aContext);
  ruleData.mContentData = &contentData;

  const nsStyleStruct* res = WalkRuleTree(eStyleStruct_Quotes, aContext, &ruleData, &contentData);
  contentData.mQuotes = nsnull; // We are sharing with some style rule.  It really owns the data.
  return res;
}

#ifdef INCLUDE_XUL
const nsStyleStruct*
nsRuleNode::GetXULData(nsIStyleContext* aContext)
{
  nsCSSXUL xulData; // Declare a struct with null CSS values.
  nsRuleData ruleData(eStyleStruct_XUL, mPresContext, aContext);
  ruleData.mXULData = &xulData;

  return WalkRuleTree(eStyleStruct_XUL, aContext, &ruleData, &xulData);
}
#endif

const nsStyleStruct*
nsRuleNode::WalkRuleTree(const nsStyleStructID& aSID, nsIStyleContext* aContext, 
                         nsRuleData* aRuleData,
                         nsCSSStruct* aSpecificData)
{
  // We start at the most specific rule in the tree.  
  nsStyleStruct* startStruct = nsnull;
  
  nsCOMPtr<nsIStyleRule> rule = mRule;
  nsRuleNode* ruleNode = this;
  nsRuleNode* highestNode = nsnull;
  nsRuleNode* rootNode = this;
  RuleDetail detail = eRuleNone;
  PRUint32 bit = nsCachedStyleData::GetBitForSID(aSID);

  while (ruleNode) {
    startStruct = ruleNode->mStyleData.GetStyleData(aSID);
    if (startStruct)
      break; // We found a rule with fully specified data.  We don't need to go up
             // the tree any further, since the remainder of this branch has already
             // been computed.

    // See if this rule node has cached the fact that the remaining nodes along this
    // path specify no data whatsoever.
    if (ruleNode->mNoneBits & bit)
      break;

    // Failing the following test mean that we have specified no rule information yet 
    // along this branch, but some ancestor in the rule tree actually has the data.  We 
    // continue walking up the rule tree without asking the style rules for any information 
    // (since the bit being set tells us that the rules aren't going to supply any info anyway. 
    if (!(detail == eRuleNone && ruleNode->mInheritBits & bit)) {
      // Ask the rule to fill in the properties that it specifies.
      ruleNode->GetRule(getter_AddRefs(rule));
      if (rule)
        rule->MapRuleInfoInto(aRuleData);

      // Now we check to see how many properties have been specified by the rules
      // we've examined so far.
      RuleDetail oldDetail = detail;
      detail = CheckSpecifiedProperties(aSID, *aSpecificData);
    
      if (oldDetail == eRuleNone && detail != oldDetail)
        highestNode = ruleNode;

      if (detail == eRuleFullMixed || detail == eRuleFullInherited)
        break; // We don't need to examine any more rules.  All properties have been fully specified.
    }

    rootNode = ruleNode;
    ruleNode = ruleNode->mParent; // Climb up to the next rule in the tree (a less specific rule).
  }

  PRBool isReset = nsCachedStyleData::IsReset(aSID);
  if (!highestNode)
    highestNode = rootNode;

  if (!aRuleData->mCanStoreInRuleTree)
    detail = eRulePartialMixed; // Treat as though some data is specified to avoid
                                // the optimizations and force data computation.

  if (detail == eRuleNone && startStruct) {
    // We specified absolutely no rule information, but a parent rule in the tree
    // specified all the rule information.  We set a bit along the branch from our
    // node in the tree to the node that specified the data that tells nodes on that
    // branch that they never need to examine their rules for this particular struct type
    // ever again.
    PropagateInheritBit(bit, ruleNode);
    return startStruct;
  }
  else if (!startStruct && ((!isReset && (detail == eRuleNone || detail == eRulePartialInherited)) 
                             || detail == eRuleFullInherited)) {
    // We specified no non-inherited information and neither did any of our parent rules.  We set a bit
    // along the branch from the highest node down to our node indicating that no non-inherited data
    // was specified.
    if (detail == eRuleNone)
      PropagateNoneBit(bit, ruleNode);
    
    // All information must necessarily be inherited from our parent style context.
    // In the absence of any computed data in the rule tree and with
    // no rules specified that didn't have values of 'inherit', we should check our parent.
    nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
    if (parentContext) {
      // We have a parent, and so we should just inherit from the parent.
      // Set the inherit bits on our context.  These bits tell the style context that
      // it never has to go back to the rule tree for data.  Instead the style context tree
      // should be walked to find the data.
      const nsStyleStruct* parentStruct = parentContext->GetStyleData(aSID);
      aContext->AddStyleBit(bit);
      aContext->SetStyle(aSID, *parentStruct);
      return parentStruct;
    }
    else
      // We are the root.  In the case of fonts, the default values just
      // come from the pres context.
      return SetDefaultOnRoot(aSID, aContext);
  }

  // We need to compute the data from the information that the rules specified.
  const nsStyleStruct* res = ComputeStyleData(aSID, startStruct, *aSpecificData, aContext, highestNode, detail,
                                              !aRuleData->mCanStoreInRuleTree);

  // If we have a post-resolve callback, handle that now.
  if (aRuleData->mPostResolveCallback)
    (*aRuleData->mPostResolveCallback)((nsStyleStruct*)res, aRuleData);

  // Now return the result.
  return res;
}

const nsStyleStruct*
nsRuleNode::SetDefaultOnRoot(const nsStyleStructID& aSID, nsIStyleContext* aContext)
{
  switch (aSID) {
    case eStyleStruct_Font: 
    {
      const nsFont& defaultFont = mPresContext->GetDefaultFontDeprecated();
      const nsFont& defaultFixedFont = mPresContext->GetDefaultFixedFontDeprecated();
      nsStyleFont* fontData = new (mPresContext) nsStyleFont(defaultFont, defaultFixedFont);
      aContext->SetStyle(eStyleStruct_Font, *fontData);
      return fontData;
    }
    case eStyleStruct_Display:
    {
      nsStyleDisplay* disp = new (mPresContext) nsStyleDisplay();
      aContext->SetStyle(eStyleStruct_Display, *disp);
      return disp;
    }
    case eStyleStruct_Visibility:
    {
      nsStyleVisibility* vis = new (mPresContext) nsStyleVisibility(mPresContext);
      aContext->SetStyle(eStyleStruct_Visibility, *vis);
      return vis;
    }
    case eStyleStruct_Text:
    {
      nsStyleText* text = new (mPresContext) nsStyleText();
      aContext->SetStyle(eStyleStruct_Text, *text);
      return text;
    }
    case eStyleStruct_TextReset:
    {
      nsStyleTextReset* text = new (mPresContext) nsStyleTextReset();
      aContext->SetStyle(eStyleStruct_TextReset, *text);
      return text;
    }
    case eStyleStruct_Color:
    {
      nsStyleColor* color = new (mPresContext) nsStyleColor(mPresContext);
      aContext->SetStyle(eStyleStruct_Color, *color);
      return color;
    }
    case eStyleStruct_Background:
    {
      nsStyleBackground* bg = new (mPresContext) nsStyleBackground(mPresContext);
      aContext->SetStyle(eStyleStruct_Background, *bg);
      return bg;
    }
    case eStyleStruct_Margin:
    {
      nsStyleMargin* margin = new (mPresContext) nsStyleMargin();
      aContext->SetStyle(eStyleStruct_Margin, *margin);
      return margin;
    }
    case eStyleStruct_Border:
    {
      nsStyleBorder* border = new (mPresContext) nsStyleBorder(mPresContext);
      aContext->SetStyle(eStyleStruct_Border, *border);
      return border;
    }
    case eStyleStruct_Padding:
    {
      nsStylePadding* padding = new (mPresContext) nsStylePadding();
      aContext->SetStyle(eStyleStruct_Padding, *padding);
      return padding;
    }
    case eStyleStruct_Outline:
    {
      nsStyleOutline* outline = new (mPresContext) nsStyleOutline(mPresContext);
      aContext->SetStyle(eStyleStruct_Outline, *outline);
      return outline;
    }
    case eStyleStruct_List:
    {
      nsStyleList* list = new (mPresContext) nsStyleList();
      aContext->SetStyle(eStyleStruct_List, *list);
      return list;
    }
    case eStyleStruct_Position:
    {
      nsStylePosition* pos = new (mPresContext) nsStylePosition();
      aContext->SetStyle(eStyleStruct_Position, *pos);
      return pos;
    }
    case eStyleStruct_Table:
    {
      nsStyleTable* table = new (mPresContext) nsStyleTable();
      aContext->SetStyle(eStyleStruct_Table, *table);
      return table;
    }
    case eStyleStruct_TableBorder:
    {
      nsStyleTableBorder* table = new (mPresContext) nsStyleTableBorder(mPresContext);
      aContext->SetStyle(eStyleStruct_TableBorder, *table);
      return table;
    }
    case eStyleStruct_Content:
    {
      nsStyleContent* content = new (mPresContext) nsStyleContent();
      aContext->SetStyle(eStyleStruct_Content, *content);
      return content;
    }
    case eStyleStruct_Quotes:
    {
      nsStyleQuotes* quotes = new (mPresContext) nsStyleQuotes();
      aContext->SetStyle(eStyleStruct_Quotes, *quotes);
      return quotes;
    }
    case eStyleStruct_UserInterface:
    {
      nsStyleUserInterface* ui = new (mPresContext) nsStyleUserInterface();
      aContext->SetStyle(eStyleStruct_UserInterface, *ui);
      return ui;
    }
    case eStyleStruct_UIReset:
    {
      nsStyleUIReset* ui = new (mPresContext) nsStyleUIReset();
      aContext->SetStyle(eStyleStruct_UIReset, *ui);
      return ui;
    }

#ifdef INCLUDE_XUL
    case eStyleStruct_XUL:
    {
      nsStyleXUL* xul = new (mPresContext) nsStyleXUL();
      aContext->SetStyle(eStyleStruct_XUL, *xul);
      return xul;
    }
#endif
  }
  return nsnull;
}

const nsStyleStruct* 
nsRuleNode::ComputeStyleData(const nsStyleStructID& aSID, nsStyleStruct* aStartStruct, const nsCSSStruct& aStartData, 
                             nsIStyleContext* aContext, 
                             nsRuleNode* aHighestNode,
                             const RuleDetail& aRuleDetail, PRBool aInherited)
{

  switch (aSID) {
  case eStyleStruct_Font:
    return ComputeFontData((nsStyleFont*)aStartStruct, (const nsCSSFont&)aStartData,
                           aContext, aHighestNode, aRuleDetail, aInherited);
  case eStyleStruct_Display:
    return ComputeDisplayData((nsStyleDisplay*)aStartStruct, (const nsCSSDisplay&)aStartData,
                              aContext, aHighestNode, aRuleDetail, aInherited);
  case eStyleStruct_Visibility:
    return ComputeVisibilityData((nsStyleVisibility*)aStartStruct, (const nsCSSDisplay&)aStartData,
                                 aContext, aHighestNode, aRuleDetail, aInherited);
  case eStyleStruct_Color:
    return ComputeColorData((nsStyleColor*)aStartStruct, (const nsCSSColor&)aStartData,
                            aContext, aHighestNode, aRuleDetail, aInherited);
  case eStyleStruct_Background:
    return ComputeBackgroundData((nsStyleBackground*)aStartStruct, (const nsCSSColor&)aStartData,
                                 aContext, aHighestNode, aRuleDetail, aInherited);
  case eStyleStruct_Margin:
    return ComputeMarginData((nsStyleMargin*)aStartStruct, (const nsCSSMargin&)aStartData, 
                             aContext, aHighestNode, aRuleDetail, aInherited);
  case eStyleStruct_Border:
    return ComputeBorderData((nsStyleBorder*)aStartStruct, (const nsCSSMargin&)aStartData, 
                             aContext, aHighestNode, aRuleDetail, aInherited);
  case eStyleStruct_Padding:
    return ComputePaddingData((nsStylePadding*)aStartStruct, (const nsCSSMargin&)aStartData, 
                              aContext, aHighestNode, aRuleDetail, aInherited);
  case eStyleStruct_Outline:
    return ComputeOutlineData((nsStyleOutline*)aStartStruct, (const nsCSSMargin&)aStartData, 
                              aContext, aHighestNode, aRuleDetail, aInherited);
  case eStyleStruct_List:
    return ComputeListData((nsStyleList*)aStartStruct, (const nsCSSList&)aStartData, 
                           aContext, aHighestNode, aRuleDetail, aInherited);
  case eStyleStruct_Position:
    return ComputePositionData((nsStylePosition*)aStartStruct, (const nsCSSPosition&)aStartData, 
                               aContext, aHighestNode, aRuleDetail, aInherited);
  case eStyleStruct_Table:
    return ComputeTableData((nsStyleTable*)aStartStruct, (const nsCSSTable&)aStartData, 
                            aContext, aHighestNode, aRuleDetail, aInherited);
  case eStyleStruct_TableBorder:
    return ComputeTableBorderData((nsStyleTableBorder*)aStartStruct, (const nsCSSTable&)aStartData, 
                                  aContext, aHighestNode, aRuleDetail, aInherited);
  case eStyleStruct_Content:
    return ComputeContentData((nsStyleContent*)aStartStruct, (const nsCSSContent&)aStartData, 
                              aContext, aHighestNode, aRuleDetail, aInherited);
  case eStyleStruct_Quotes:
    return ComputeQuotesData((nsStyleQuotes*)aStartStruct, (const nsCSSContent&)aStartData, 
                              aContext, aHighestNode, aRuleDetail, aInherited);
  case eStyleStruct_Text:
    return ComputeTextData((nsStyleText*)aStartStruct, (const nsCSSText&)aStartData,
                           aContext, aHighestNode, aRuleDetail, aInherited);
  case eStyleStruct_TextReset:
    return ComputeTextResetData((nsStyleTextReset*)aStartStruct, (const nsCSSText&)aStartData,
                                aContext, aHighestNode, aRuleDetail, aInherited);
  case eStyleStruct_UserInterface:
    return ComputeUIData((nsStyleUserInterface*)aStartStruct, (const nsCSSUserInterface&)aStartData,
                         aContext, aHighestNode, aRuleDetail, aInherited);
  case eStyleStruct_UIReset:
    return ComputeUIResetData((nsStyleUIReset*)aStartStruct, (const nsCSSUserInterface&)aStartData,
                              aContext, aHighestNode, aRuleDetail, aInherited);
#ifdef INCLUDE_XUL
  case eStyleStruct_XUL:
    return ComputeXULData((nsStyleXUL*)aStartStruct, (const nsCSSXUL&)aStartData, 
                           aContext, aHighestNode, aRuleDetail, aInherited);
#endif
  }

  return nsnull;
}

const nsStyleStruct* 
nsRuleNode::ComputeFontData(nsStyleFont* aStartFont, const nsCSSFont& aFontData, 
                            nsIStyleContext* aContext, 
                            nsRuleNode* aHighestNode,
                            const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW FONT CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  const nsFont& defaultFont = mPresContext->GetDefaultFontDeprecated();
  const nsFont& defaultFixedFont = mPresContext->GetDefaultFixedFontDeprecated();

  nsStyleFont* font = nsnull;
  nsStyleFont* parentFont = font;
  PRBool inherited = aInherited;

  if (aStartFont)
    // We only need to compute the delta between this computed data and our
    // computed data.
    font = new (mPresContext) nsStyleFont(*aStartFont);
  else {
    if (aRuleDetail != eRuleFullMixed) {
      // No question. We will have to inherit. Go ahead and init
      // with inherited vals from parent.
      inherited = PR_TRUE;
      if (parentContext)
        parentFont = (nsStyleFont*)parentContext->GetStyleData(eStyleStruct_Font);
      if (parentFont)
        font = new (mPresContext) nsStyleFont(*parentFont);
    }
  }

  if (!font)
    font = parentFont = new (mPresContext) nsStyleFont(defaultFont, defaultFixedFont);

  // font-family: string list, enum, inherit
  if (eCSSUnit_String == aFontData.mFamily.GetUnit()) {
    nsCOMPtr<nsIDeviceContext> dc;
    mPresContext->GetDeviceContext(getter_AddRefs(dc));
    if (dc) {
      nsAutoString  familyList;
      aFontData.mFamily.GetStringValue(familyList);
      font->mFont.name = familyList;
      nsAutoString  face;

      // MJA: bug 31816
      // if we are not using document fonts, but this is a xul document,
      // then we set the chromeOverride bit so we use the document fonts anyway
      PRBool chromeOverride = PR_FALSE;
      PRBool useDocumentFonts = PR_TRUE;
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
        else {
          // see if we are in the chrome, if so, use the document fonts (override the useDocFonts setting)
          nsresult result = NS_OK;
          nsCOMPtr<nsISupports> container;
          result = mPresContext->GetContainer(getter_AddRefs(container));
          if (NS_SUCCEEDED(result) && container) {
            nsCOMPtr<nsIDocShellTreeItem> docShell(do_QueryInterface(container, &result));
            if (NS_SUCCEEDED(result) && docShell){
              PRInt32 docShellType;
              result = docShell->GetItemType(&docShellType);
              if (NS_SUCCEEDED(result)){
                if (nsIDocShellTreeItem::typeChrome == docShellType){
                  chromeOverride = PR_TRUE;
                }
              }      
            }
          }
        }
      }

      // find the correct font if we are usingDocumentFonts OR we are overriding for XUL
      // MJA: bug 31816
      PRBool fontFaceOK = PR_TRUE;
      PRBool isMozFixed = font->mFont.name.EqualsIgnoreCase("-moz-fixed");
      if (chromeOverride || useDocumentFonts) {
        font->mFont.name += nsAutoString(NS_LITERAL_STRING(",")) + defaultFont.name;
        font->mFixedFont.name += nsAutoString(NS_LITERAL_STRING(",")) + defaultFixedFont.name;
      }
      else {
        // now set to defaults
        font->mFont.name = defaultFont.name;
        font->mFixedFont.name = defaultFixedFont.name;
      }

      // set to monospace if using moz-fixed
      if (isMozFixed)
        font->mFlags |= NS_STYLE_FONT_USE_FIXED;
      else
        font->mFlags &= ~NS_STYLE_FONT_USE_FIXED;
      font->mFlags |= NS_STYLE_FONT_FACE_EXPLICIT;
    }
  }
  else if (eCSSUnit_Enumerated == aFontData.mFamily.GetUnit()) {
    nsSystemAttrID sysID;
    switch (aFontData.mFamily.GetIntValue()) {
      // If you add fonts to this list, you need to also patch the list
      // in CheckFontProperties (also in this file above).
      case NS_STYLE_FONT_CAPTION:       sysID = eSystemAttr_Font_Caption;       break;    // css2
      case NS_STYLE_FONT_ICON:          sysID = eSystemAttr_Font_Icon;          break;
      case NS_STYLE_FONT_MENU:          sysID = eSystemAttr_Font_Menu;          break;
      case NS_STYLE_FONT_MESSAGE_BOX:   sysID = eSystemAttr_Font_MessageBox;    break;
      case NS_STYLE_FONT_SMALL_CAPTION: sysID = eSystemAttr_Font_SmallCaption;  break;
      case NS_STYLE_FONT_STATUS_BAR:    sysID = eSystemAttr_Font_StatusBar;     break;
      case NS_STYLE_FONT_WINDOW:        sysID = eSystemAttr_Font_Window;        break;    // css3
      case NS_STYLE_FONT_DOCUMENT:      sysID = eSystemAttr_Font_Document;      break;
      case NS_STYLE_FONT_WORKSPACE:     sysID = eSystemAttr_Font_Workspace;     break;
      case NS_STYLE_FONT_DESKTOP:       sysID = eSystemAttr_Font_Desktop;       break;
      case NS_STYLE_FONT_INFO:          sysID = eSystemAttr_Font_Info;          break;
      case NS_STYLE_FONT_DIALOG:        sysID = eSystemAttr_Font_Dialog;        break;
      case NS_STYLE_FONT_BUTTON:        sysID = eSystemAttr_Font_Button;        break;
      case NS_STYLE_FONT_PULL_DOWN_MENU:sysID = eSystemAttr_Font_PullDownMenu;  break;
      case NS_STYLE_FONT_LIST:          sysID = eSystemAttr_Font_List;          break;
      case NS_STYLE_FONT_FIELD:         sysID = eSystemAttr_Font_Field;         break;
    }

    nsCompatibility mode;
    mPresContext->GetCompatibilityMode(&mode);
		nsCOMPtr<nsIDeviceContext> dc;
    mPresContext->GetDeviceContext(getter_AddRefs(dc));
    if (dc) {
      SystemAttrStruct sysInfo;
      sysInfo.mFont = &font->mFont;
      font->mFont.size = defaultFont.size; // GetSystemAttribute sets the font face but not necessarily the size
      if (NS_FAILED(dc->GetSystemAttribute(sysID, &sysInfo))) {
        font->mFont.name = defaultFont.name;
        font->mFixedFont.name = defaultFixedFont.name;
      }
      font->mFlags |= NS_STYLE_FONT_FACE_EXPLICIT;
    }

    // NavQuirks uses sans-serif instead of whatever the native font is
#ifdef XP_MAC
    if (eCompatibility_NavQuirks == mode) {
      switch (sysID) {
        case eSystemAttr_Font_Field:
        case eSystemAttr_Font_List:
          font->mFont.name.AssignWithConversion("monospace");
          font->mFont.size = defaultFixedFont.size;
          break;
        case eSystemAttr_Font_Button:
          font->mFont.name.AssignWithConversion("serif");
          font->mFont.size = defaultFont.size;
          break;
      }
    }
#endif

#ifdef XP_PC
    //
    // As far as I can tell the system default fonts and sizes for
    // on MS-Windows for Buttons, Listboxes/Comboxes and Text Fields are 
    // all pre-determined and cannot be changed by either the control panel 
    // or programmtically.
    //
    switch (sysID) {
      // Fields (text fields)
      //
      // For NavQuirks:
      //   We always use the monospace font and whatever
      //   the "fixed" font size this that is set by the browser
      //
      // For Standard/Strict Mode:
      //    We use whatever font is defined by the system. Which it appears
      //    (and the assumption is) it is always a proportional font. Then we
      //    always use 2 points smaller than what the browser has defined as
      //    the default proportional font.
      case eSystemAttr_Font_Field:
        if (eCompatibility_NavQuirks == mode) {
          font->mFont.name.AssignWithConversion("monospace");
          font->mFont.size = defaultFixedFont.size;
        } else {
          // Assumption: system defined font is proportional
          font->mFont.size = PR_MAX(defaultFont.size - NSIntPointsToTwips(2), 0);
        }
        break;
      //
      // Button and Selects (listboxes/comboboxes)
      //
      // For NavQuirks:
      //   We always use the sans-serif font and 2 point point sizes smaller
      //   that whatever the "proportional" font size this that is set by the browser
      //
      // For Standard/Strict Mode:
      //    We use whatever font is defined by the system. Which it appears
      //    (and the assumption is) it is always a proportional font. Then we
      //    always use 2 points smaller than what the browser has defined as
      //    the default proportional font.
      case eSystemAttr_Font_Button:
      case eSystemAttr_Font_List:
        if (eCompatibility_NavQuirks == mode) {
          font->mFont.name.AssignWithConversion("sans-serif");
        }
        // Assumption: system defined font is proportional
        font->mFont.size = PR_MAX(defaultFont.size - NSIntPointsToTwips(2), 0);
        break;
    }
#endif

#ifdef XP_UNIX
    if (eCompatibility_NavQuirks == mode) {
      switch (sysID) {
        case eSystemAttr_Font_Field:
          font->mFont.name.AssignWithConversion("monospace");
          font->mFont.size = defaultFixedFont.size;
          break;
        case eSystemAttr_Font_Button:
        case eSystemAttr_Font_List:
          font->mFont.name.AssignWithConversion("serif");
          font->mFont.size = defaultFont.size;
          break;
      }
    }
#endif
  }
  else if (eCSSUnit_Inherit == aFontData.mFamily.GetUnit()) {
    if (parentContext && !inherited) {
      inherited = PR_TRUE;
      parentFont = (nsStyleFont*)parentContext->GetStyleData(eStyleStruct_Font);
    }

    font->mFont.name = parentFont->mFont.name;
    font->mFixedFont.name = parentFont->mFixedFont.name;
    font->mFlags &= ~(NS_STYLE_FONT_FACE_EXPLICIT | NS_STYLE_FONT_USE_FIXED);
    font->mFlags |= (parentFont->mFlags & (NS_STYLE_FONT_FACE_EXPLICIT | NS_STYLE_FONT_USE_FIXED));
  }
  else if (eCSSUnit_Initial == aFontData.mFamily.GetUnit()) {
    font->mFont.name = defaultFont.name;
    font->mFixedFont.name = defaultFixedFont.name;
  }

  // font-style: enum, normal, inherit
  if (eCSSUnit_Enumerated == aFontData.mStyle.GetUnit()) {
    font->mFont.style = aFontData.mStyle.GetIntValue();
    font->mFixedFont.style = aFontData.mStyle.GetIntValue();
  }
  else if (eCSSUnit_Normal == aFontData.mStyle.GetUnit()) {
    font->mFont.style = NS_STYLE_FONT_STYLE_NORMAL;
    font->mFixedFont.style = NS_STYLE_FONT_STYLE_NORMAL;
  }
  else if (eCSSUnit_Inherit == aFontData.mStyle.GetUnit()) {
    if (parentContext && !inherited) {
      inherited = PR_TRUE;
      parentFont = (nsStyleFont*)parentContext->GetStyleData(eStyleStruct_Font);
    }

    font->mFont.style = parentFont->mFont.style;
    font->mFixedFont.style = parentFont->mFixedFont.style;
  }
  else if (eCSSUnit_Initial == aFontData.mStyle.GetUnit()) {
    font->mFont.style = defaultFont.style;
    font->mFixedFont.style = defaultFixedFont.style;
  }

  // font-variant: enum, normal, inherit
  if (eCSSUnit_Enumerated == aFontData.mVariant.GetUnit()) {
    font->mFont.variant = aFontData.mVariant.GetIntValue();
    font->mFixedFont.variant = aFontData.mVariant.GetIntValue();
  }
  else if (eCSSUnit_Normal == aFontData.mVariant.GetUnit()) {
    font->mFont.variant = NS_STYLE_FONT_VARIANT_NORMAL;
    font->mFixedFont.variant = NS_STYLE_FONT_VARIANT_NORMAL;
  }
  else if (eCSSUnit_Inherit == aFontData.mVariant.GetUnit()) {
    if (parentContext && !inherited) {
      inherited = PR_TRUE;
      parentFont = (nsStyleFont*)parentContext->GetStyleData(eStyleStruct_Font);
    }

    font->mFont.variant = parentFont->mFont.variant;
    font->mFixedFont.variant = parentFont->mFixedFont.variant;
  }
  else if (eCSSUnit_Initial == aFontData.mVariant.GetUnit()) {
    font->mFont.variant = defaultFont.variant;
    font->mFixedFont.variant = defaultFixedFont.variant;
  }

  // font-weight: int, enum, normal, inherit
  if (eCSSUnit_Integer == aFontData.mWeight.GetUnit()) {
    font->mFont.weight = aFontData.mWeight.GetIntValue();
    font->mFixedFont.weight = aFontData.mWeight.GetIntValue();
  }
  else if (eCSSUnit_Enumerated == aFontData.mWeight.GetUnit()) {
    PRInt32 value = aFontData.mWeight.GetIntValue();
    switch (value) {
      case NS_STYLE_FONT_WEIGHT_NORMAL:
      case NS_STYLE_FONT_WEIGHT_BOLD:
        font->mFont.weight = value;
        font->mFixedFont.weight = value;
        break;
      case NS_STYLE_FONT_WEIGHT_BOLDER:
      case NS_STYLE_FONT_WEIGHT_LIGHTER:
        if (parentContext && !inherited) {
          inherited = PR_TRUE;
          parentFont = (nsStyleFont*)parentContext->GetStyleData(eStyleStruct_Font);
        }
        font->mFont.weight = nsStyleUtil::ConstrainFontWeight(parentFont->mFont.weight + value);
        font->mFixedFont.weight = nsStyleUtil::ConstrainFontWeight(parentFont->mFixedFont.weight + value);
        break;
    }
  }
  else if (eCSSUnit_Normal == aFontData.mWeight.GetUnit()) {
    font->mFont.weight = NS_STYLE_FONT_WEIGHT_NORMAL;
    font->mFixedFont.weight = NS_STYLE_FONT_WEIGHT_NORMAL;
  }
  else if (eCSSUnit_Inherit == aFontData.mWeight.GetUnit()) {
    if (parentContext && !inherited) {
      inherited = PR_TRUE;
      parentFont = (nsStyleFont*)parentContext->GetStyleData(eStyleStruct_Font);
    }

    font->mFont.weight = parentFont->mFont.weight;
    font->mFixedFont.weight = parentFont->mFixedFont.weight;
  }
  else if (eCSSUnit_Initial == aFontData.mWeight.GetUnit()) {
    font->mFont.weight = defaultFont.weight;
    font->mFixedFont.weight = defaultFixedFont.weight;
  }
  

  // font-size: enum, length, percent, inherit
  if (eCSSUnit_Enumerated == aFontData.mSize.GetUnit()) {
    PRInt32 value = aFontData.mSize.GetIntValue();
    PRInt32 scaler;
    mPresContext->GetFontScaler(&scaler);
    float scaleFactor = nsStyleUtil::GetScalingFactor(scaler);

    if (parentContext && !inherited) {
      inherited = PR_TRUE;
      parentFont = (nsStyleFont*)parentContext->GetStyleData(eStyleStruct_Font);
    }

    if ((NS_STYLE_FONT_SIZE_XXSMALL <= value) && 
        (value <= NS_STYLE_FONT_SIZE_XXXLARGE)) {
      font->mFont.size = nsStyleUtil::CalcFontPointSize(value, (PRInt32)defaultFont.size, scaleFactor, mPresContext, eFontSize_CSS);
      font->mFixedFont.size = nsStyleUtil::CalcFontPointSize(value, (PRInt32)defaultFixedFont.size, scaleFactor, mPresContext, eFontSize_CSS);
    }
    else if (NS_STYLE_FONT_SIZE_LARGER == value) {
      PRInt32 index = nsStyleUtil::FindNextLargerFontSize(parentFont->mFont.size, (PRInt32)defaultFont.size, scaleFactor, mPresContext, eFontSize_CSS);
      nscoord largerSize = nsStyleUtil::CalcFontPointSize(index, (PRInt32)defaultFont.size, scaleFactor, mPresContext, eFontSize_CSS);
      nscoord largerFixedSize = nsStyleUtil::CalcFontPointSize(index, (PRInt32)defaultFixedFont.size, scaleFactor, mPresContext, eFontSize_CSS);
      font->mFont.size = PR_MAX(largerSize, parentFont->mFont.size);
      font->mFixedFont.size = PR_MAX(largerFixedSize, parentFont->mFixedFont.size);
    }
    else if (NS_STYLE_FONT_SIZE_SMALLER == value) {
      PRInt32 index = nsStyleUtil::FindNextSmallerFontSize(parentFont->mFont.size, (PRInt32)defaultFont.size, scaleFactor, mPresContext, eFontSize_CSS);
      nscoord smallerSize = nsStyleUtil::CalcFontPointSize(index, (PRInt32)defaultFont.size, scaleFactor, mPresContext, eFontSize_CSS);
      nscoord smallerFixedSize = nsStyleUtil::CalcFontPointSize(index, (PRInt32)defaultFixedFont.size, scaleFactor, mPresContext, eFontSize_CSS);
      font->mFont.size = PR_MIN(smallerSize, parentFont->mFont.size);
      font->mFixedFont.size = PR_MIN(smallerFixedSize, parentFont->mFixedFont.size);
    }
    // this does NOT explicitly set font size
    font->mFlags &= ~NS_STYLE_FONT_SIZE_EXPLICIT;
  }
  else if (aFontData.mSize.IsLengthUnit()) {
    if (parentContext && !inherited)
      parentFont = (nsStyleFont*)parentContext->GetStyleData(eStyleStruct_Font);

    font->mFont.size = CalcLength(aFontData.mSize, &parentFont->mFont, nsnull, mPresContext, inherited);
    font->mFixedFont.size = CalcLength(aFontData.mSize, &parentFont->mFixedFont, nsnull, mPresContext, inherited);
    font->mFlags |= NS_STYLE_FONT_SIZE_EXPLICIT;
  }
  else if (eCSSUnit_Percent == aFontData.mSize.GetUnit()) {
    if (parentContext && !inherited) {
      inherited = PR_TRUE;
      parentFont = (nsStyleFont*)parentContext->GetStyleData(eStyleStruct_Font);
    }

    font->mFont.size = (nscoord)((float)(parentFont->mFont.size) * aFontData.mSize.GetPercentValue());
    font->mFixedFont.size = (nscoord)((float)(parentFont->mFixedFont.size) * aFontData.mSize.GetPercentValue());
    font->mFlags |= NS_STYLE_FONT_SIZE_EXPLICIT;
  }
  else if (eCSSUnit_Inherit == aFontData.mSize.GetUnit()) {
    if (parentContext && !inherited) {
      inherited = PR_TRUE;
      parentFont = (nsStyleFont*)parentContext->GetStyleData(eStyleStruct_Font);
    }

    font->mFont.size = parentFont->mFont.size;
    font->mFixedFont.size = parentFont->mFixedFont.size;
    font->mFlags &= ~NS_STYLE_FONT_SIZE_EXPLICIT;
    font->mFlags |= (parentFont->mFlags & NS_STYLE_FONT_SIZE_EXPLICIT);
  }
  else if (eCSSUnit_Initial == aFontData.mSize.GetUnit()) {
    font->mFont.size = defaultFont.size;
    font->mFixedFont.size = defaultFixedFont.size;
  }

  if (font->mFlags & NS_STYLE_FONT_USE_FIXED)
    font->mFont = font->mFixedFont;

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_Font, *font);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mInheritedData)
      aHighestNode->mStyleData.mInheritedData = new (mPresContext) nsInheritedStyleData;
    aHighestNode->mStyleData.mInheritedData->mFontData = font;
    // Propagate the bit down.
    PropagateInheritBit(NS_STYLE_INHERIT_FONT, aHighestNode);
  }

  return font;
}

const nsStyleStruct*
nsRuleNode::ComputeTextData(nsStyleText* aStartData, const nsCSSText& aTextData, 
                            nsIStyleContext* aContext, 
                            nsRuleNode* aHighestNode,
                            const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW TEXT CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  nsStyleText* text = nsnull;
  nsStyleText* parentText = text;
  PRBool inherited = aInherited;

  if (aStartData)
    // We only need to compute the delta between this computed data and our
    // computed data.
    text = new (mPresContext) nsStyleText(*aStartData);
  else {
    if (aRuleDetail != eRuleFullMixed) {
      // No question. We will have to inherit. Go ahead and init
      // with inherited vals from parent.
      inherited = PR_TRUE;
      if (parentContext)
        parentText = (nsStyleText*)parentContext->GetStyleData(eStyleStruct_Text);
      if (parentText) {
        text = new (mPresContext) nsStyleText(*parentText);

        // Percentage line heights are not inherited.  We need to change the value
        // of line-height to inherit.
        nsStyleUnit unit = text->mLineHeight.GetUnit();
        if (unit != eStyleUnit_Normal && unit != eStyleUnit_Factor && unit != eStyleUnit_Coord)
          text->mLineHeight.SetInheritValue();
      }
    }
  }

  if (!text)
    text = parentText = new (mPresContext) nsStyleText();

    // letter-spacing: normal, length, inherit
  SetCoord(aTextData.mLetterSpacing, text->mLetterSpacing, parentText->mLetterSpacing,
           SETCOORD_LH | SETCOORD_NORMAL, aContext, mPresContext, inherited);

  // line-height: normal, number, length, percent, inherit
  SetCoord(aTextData.mLineHeight, text->mLineHeight, parentText->mLineHeight,
           SETCOORD_LPFHN, aContext, mPresContext, inherited);

  // text-align: enum, string, inherit
  if (eCSSUnit_Enumerated == aTextData.mTextAlign.GetUnit()) {
    text->mTextAlign = aTextData.mTextAlign.GetIntValue();
  }
  else if (eCSSUnit_String == aTextData.mTextAlign.GetUnit()) {
    NS_NOTYETIMPLEMENTED("align string");
  }
  else if (eCSSUnit_Inherit == aTextData.mTextAlign.GetUnit()) {
    inherited = PR_TRUE;
    text->mTextAlign = parentText->mTextAlign;
  }
  else if (eCSSUnit_Initial == aTextData.mTextAlign.GetUnit())
    text->mTextAlign = NS_STYLE_TEXT_ALIGN_DEFAULT;

  // text-indent: length, percent, inherit
  SetCoord(aTextData.mTextIndent, text->mTextIndent, parentText->mTextIndent,
           SETCOORD_LPH, aContext, mPresContext, inherited);

  // text-transform: enum, none, inherit
  if (eCSSUnit_Enumerated == aTextData.mTextTransform.GetUnit()) {
    text->mTextTransform = aTextData.mTextTransform.GetIntValue();
  }
  else if (eCSSUnit_None == aTextData.mTextTransform.GetUnit()) {
    text->mTextTransform = NS_STYLE_TEXT_TRANSFORM_NONE;
  }
  else if (eCSSUnit_Inherit == aTextData.mTextTransform.GetUnit()) {
    inherited = PR_TRUE;
    text->mTextTransform = parentText->mTextTransform;
  }

  // white-space: enum, normal, inherit
  if (eCSSUnit_Enumerated == aTextData.mWhiteSpace.GetUnit()) {
    text->mWhiteSpace = aTextData.mWhiteSpace.GetIntValue();
  }
  else if (eCSSUnit_Normal == aTextData.mWhiteSpace.GetUnit()) {
    text->mWhiteSpace = NS_STYLE_WHITESPACE_NORMAL;
  }
  else if (eCSSUnit_Inherit == aTextData.mWhiteSpace.GetUnit()) {
    inherited = PR_TRUE;
    text->mWhiteSpace = parentText->mWhiteSpace;
  }

  // word-spacing: normal, length, inherit
  SetCoord(aTextData.mWordSpacing, text->mWordSpacing, parentText->mWordSpacing,
           SETCOORD_LH | SETCOORD_NORMAL, aContext, mPresContext, inherited);

#ifdef IBMBIDI
  // unicode-bidi: enum, normal, inherit
  // normal means that override prohibited
  if (eCSSUnit_Normal == aTextData.mUnicodeBidi.GetUnit() ) {
    text->mUnicodeBidi = NS_STYLE_UNICODE_BIDI_NORMAL;
  }
  else {
    if (eCSSUnit_Enumerated == aTextData.mUnicodeBidi.GetUnit() ) {
      text->mUnicodeBidi = aTextData.mUnicodeBidi.GetIntValue();
    }
    if (NS_STYLE_UNICODE_BIDI_INHERIT == text->mUnicodeBidi) {
      inherited = PR_TRUE;
      text->mUnicodeBidi = parentText->mUnicodeBidi;
    }
  }
#endif // IBMBIDI

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_Text, *text);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mInheritedData)
      aHighestNode->mStyleData.mInheritedData = new (mPresContext) nsInheritedStyleData;
    aHighestNode->mStyleData.mInheritedData->mTextData = text;
    // Propagate the bit down.
    PropagateInheritBit(NS_STYLE_INHERIT_TEXT, aHighestNode);
  }

  return text;
}

const nsStyleStruct*
nsRuleNode::ComputeTextResetData(nsStyleTextReset* aStartData, const nsCSSText& aTextData, 
                                 nsIStyleContext* aContext, 
                                 nsRuleNode* aHighestNode,
                                 const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW TEXT CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  nsStyleTextReset* text;
  if (aStartData)
    // We only need to compute the delta between this computed data and our
    // computed data.
    text = new (mPresContext) nsStyleTextReset(*aStartData);
  else
    text = new (mPresContext) nsStyleTextReset();
  nsStyleTextReset* parentText = text;

  if (parentContext)
    parentText = (nsStyleTextReset*)parentContext->GetStyleData(eStyleStruct_TextReset);
  PRBool inherited = aInherited;
  
  // vertical-align: enum, length, percent, inherit
  SetCoord(aTextData.mVerticalAlign, text->mVerticalAlign, parentText->mVerticalAlign,
           SETCOORD_LPH | SETCOORD_ENUMERATED, aContext, mPresContext, inherited);

  // text-decoration: none, enum (bit field), inherit
  if (eCSSUnit_Enumerated == aTextData.mDecoration.GetUnit()) {
    PRInt32 td = aTextData.mDecoration.GetIntValue();
    text->mTextDecoration = td;
  }
  else if (eCSSUnit_None == aTextData.mDecoration.GetUnit()) {
    text->mTextDecoration = NS_STYLE_TEXT_DECORATION_NONE;
  }
  else if (eCSSUnit_Inherit == aTextData.mDecoration.GetUnit()) {
    inherited = PR_TRUE;
    text->mTextDecoration = parentText->mTextDecoration;
  }

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_TextReset, *text);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mResetData)
      aHighestNode->mStyleData.mResetData = new (mPresContext) nsResetStyleData;
    aHighestNode->mStyleData.mResetData->mTextData = text;
    // Propagate the bit down.
    PropagateInheritBit(NS_STYLE_INHERIT_TEXT_RESET, aHighestNode);
  }

  return text;
}

const nsStyleStruct*
nsRuleNode::ComputeUIData(nsStyleUserInterface* aStartData, const nsCSSUserInterface& aUIData, 
                          nsIStyleContext* aContext, 
                          nsRuleNode* aHighestNode,
                          const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW TEXT CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  nsStyleUserInterface* ui = nsnull;
  nsStyleUserInterface* parentUI = ui;
  PRBool inherited = aInherited;

  if (aStartData)
    // We only need to compute the delta between this computed data and our
    // computed data.
    ui = new (mPresContext) nsStyleUserInterface(*aStartData);
  else {
    if (aRuleDetail != eRuleFullMixed) {
      // No question. We will have to inherit. Go ahead and init
      // with inherited vals from parent.
      inherited = PR_TRUE;
      if (parentContext)
        parentUI = (nsStyleUserInterface*)parentContext->GetStyleData(eStyleStruct_UserInterface);
      if (parentUI)
        ui = new (mPresContext) nsStyleUserInterface(*parentUI);
    }
  }

  if (!ui)
    ui = parentUI = new (mPresContext) nsStyleUserInterface();

  // cursor: enum, auto, url, inherit
  nsCSSValueList*  list = aUIData.mCursor;
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
  if (eCSSUnit_Enumerated == aUIData.mUserInput.GetUnit()) {
    ui->mUserInput = aUIData.mUserInput.GetIntValue();
  }
  else if (eCSSUnit_Auto == aUIData.mUserInput.GetUnit()) {
    ui->mUserInput = NS_STYLE_USER_INPUT_AUTO;
  }
  else if (eCSSUnit_None == aUIData.mUserInput.GetUnit()) {
    ui->mUserInput = NS_STYLE_USER_INPUT_NONE;
  }
  else if (eCSSUnit_Inherit == aUIData.mUserInput.GetUnit()) {
    inherited = PR_TRUE;
    ui->mUserInput = parentUI->mUserInput;
  }

  // user-modify: enum, inherit
  if (eCSSUnit_Enumerated == aUIData.mUserModify.GetUnit()) {
    ui->mUserModify = aUIData.mUserModify.GetIntValue();
  }
  else if (eCSSUnit_Inherit == aUIData.mUserModify.GetUnit()) {
    inherited = PR_TRUE;
    ui->mUserModify = parentUI->mUserModify;
  }

  // user-focus: none, normal, enum, inherit
  if (eCSSUnit_Enumerated == aUIData.mUserFocus.GetUnit()) {
    ui->mUserFocus = aUIData.mUserFocus.GetIntValue();
  }
  else if (eCSSUnit_None == aUIData.mUserFocus.GetUnit()) {
    ui->mUserFocus = NS_STYLE_USER_FOCUS_NONE;
  }
  else if (eCSSUnit_Normal == aUIData.mUserFocus.GetUnit()) {
    ui->mUserFocus = NS_STYLE_USER_FOCUS_NORMAL;
  }
  else if (eCSSUnit_Inherit == aUIData.mUserFocus.GetUnit()) {
    inherited = PR_TRUE;
    ui->mUserFocus = parentUI->mUserFocus;
  }

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_UserInterface, *ui);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mInheritedData)
      aHighestNode->mStyleData.mInheritedData = new (mPresContext) nsInheritedStyleData;
    aHighestNode->mStyleData.mInheritedData->mUIData = ui;
    // Propagate the bit down.
    PropagateInheritBit(NS_STYLE_INHERIT_UI, aHighestNode);
  }

  return ui;
}

const nsStyleStruct*
nsRuleNode::ComputeUIResetData(nsStyleUIReset* aStartData, const nsCSSUserInterface& aUIData, 
                               nsIStyleContext* aContext, 
                               nsRuleNode* aHighestNode,
                               const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW TEXT CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  nsStyleUIReset* ui;
  if (aStartData)
    // We only need to compute the delta between this computed data and our
    // computed data.
    ui = new (mPresContext) nsStyleUIReset(*aStartData);
  else
    ui = new (mPresContext) nsStyleUIReset();
  nsStyleUIReset* parentUI = ui;

  if (parentContext)
    parentUI = (nsStyleUIReset*)parentContext->GetStyleData(eStyleStruct_UIReset);
  PRBool inherited = aInherited;
  
  // user-select: none, enum, inherit
  if (eCSSUnit_Enumerated == aUIData.mUserSelect.GetUnit()) {
    ui->mUserSelect = aUIData.mUserSelect.GetIntValue();
  }
  else if (eCSSUnit_None == aUIData.mUserSelect.GetUnit()) {
    ui->mUserSelect = NS_STYLE_USER_SELECT_NONE;
  }
  else if (eCSSUnit_Inherit == aUIData.mUserSelect.GetUnit()) {
    inherited = PR_TRUE;
    ui->mUserSelect = parentUI->mUserSelect;
  }

  // key-equivalent: none, enum XXX, inherit
  nsCSSValueList*  keyEquiv = aUIData.mKeyEquivalent;
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
  if (eCSSUnit_Enumerated == aUIData.mResizer.GetUnit()) {
    ui->mResizer = aUIData.mResizer.GetIntValue();
  }
  else if (eCSSUnit_Auto == aUIData.mResizer.GetUnit()) {
    ui->mResizer = NS_STYLE_RESIZER_AUTO;
  }
  else if (eCSSUnit_None == aUIData.mResizer.GetUnit()) {
    ui->mResizer = NS_STYLE_RESIZER_NONE;
  }
  else if (eCSSUnit_Inherit == aUIData.mResizer.GetUnit()) {
    inherited = PR_TRUE;
    ui->mResizer = parentUI->mResizer;
  }

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_UIReset, *ui);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mResetData)
      aHighestNode->mStyleData.mResetData = new (mPresContext) nsResetStyleData;
    aHighestNode->mStyleData.mResetData->mUIData = ui;
    // Propagate the bit down.
    PropagateInheritBit(NS_STYLE_INHERIT_UI_RESET, aHighestNode);
  }

  return ui;
}

const nsStyleStruct*
nsRuleNode::ComputeDisplayData(nsStyleDisplay* aStartDisplay, const nsCSSDisplay& aDisplayData, 
                               nsIStyleContext* aContext, 
                               nsRuleNode* aHighestNode,
                               const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW DISPLAY CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  nsStyleDisplay* display;
  if (aStartDisplay)
    // We only need to compute the delta between this computed data and our
    // computed data.
    display = new (mPresContext) nsStyleDisplay(*aStartDisplay);
  else
    display = new (mPresContext) nsStyleDisplay();
  nsStyleDisplay* parentDisplay = display;

  if (parentContext)
    parentDisplay = (nsStyleDisplay*)parentContext->GetStyleData(eStyleStruct_Display);
  PRBool inherited = aInherited;

  // display: enum, none, inherit
  if (eCSSUnit_Enumerated == aDisplayData.mDisplay.GetUnit()) {
    display->mDisplay = aDisplayData.mDisplay.GetIntValue();
  }
  else if (eCSSUnit_None == aDisplayData.mDisplay.GetUnit()) {
    display->mDisplay = NS_STYLE_DISPLAY_NONE;
  }
  else if (eCSSUnit_Inherit == aDisplayData.mDisplay.GetUnit()) {
    inherited = PR_TRUE;
    display->mDisplay = parentDisplay->mDisplay;
  }

  // binding: url, none, inherit
  if (eCSSUnit_URL == aDisplayData.mBinding.GetUnit()) {
    aDisplayData.mBinding.GetStringValue(display->mBinding);
  }
  else if (eCSSUnit_None == aDisplayData.mBinding.GetUnit()) {
    display->mBinding.Truncate();
  }
  else if (eCSSUnit_Inherit == aDisplayData.mBinding.GetUnit()) {
    inherited = PR_TRUE;
    display->mBinding = parentDisplay->mBinding;
  }

  // position: enum, inherit
  if (eCSSUnit_Enumerated == aDisplayData.mPosition.GetUnit()) {
    display->mPosition = aDisplayData.mPosition.GetIntValue();
    if (display->mPosition != NS_STYLE_POSITION_NORMAL) {
      // :before and :after elts cannot be positioned.  We need to check for this
      // case.
      nsCOMPtr<nsIAtom> tag;
      aContext->GetPseudoType(*getter_AddRefs(tag));
      if (tag && tag.get() == nsCSSAtoms::beforePseudo || tag.get() == nsCSSAtoms::afterPseudo)
        display->mPosition = NS_STYLE_POSITION_NORMAL;
    }
  }
  else if (eCSSUnit_Inherit == aDisplayData.mPosition.GetUnit()) {
    inherited = PR_TRUE;
    display->mPosition = parentDisplay->mPosition;
  }

  // clear: enum, none, inherit
  if (eCSSUnit_Enumerated == aDisplayData.mClear.GetUnit()) {
    display->mBreakType = aDisplayData.mClear.GetIntValue();
  }
  else if (eCSSUnit_None == aDisplayData.mClear.GetUnit()) {
    display->mBreakType = NS_STYLE_CLEAR_NONE;
  }
  else if (eCSSUnit_Inherit == aDisplayData.mClear.GetUnit()) {
    inherited = PR_TRUE;
    display->mBreakType = parentDisplay->mBreakType;
  }

  // float: enum, none, inherit
  if (eCSSUnit_Enumerated == aDisplayData.mFloat.GetUnit()) {
    display->mFloats = aDisplayData.mFloat.GetIntValue();
  }
  else if (eCSSUnit_None == aDisplayData.mFloat.GetUnit()) {
    display->mFloats = NS_STYLE_FLOAT_NONE;
  }
  else if (eCSSUnit_Inherit == aDisplayData.mFloat.GetUnit()) {
    inherited = PR_TRUE;
    display->mFloats = parentDisplay->mFloats;
  }

  // overflow: enum, auto, inherit
  if (eCSSUnit_Enumerated == aDisplayData.mOverflow.GetUnit()) {
    display->mOverflow = aDisplayData.mOverflow.GetIntValue();
  }
  else if (eCSSUnit_Auto == aDisplayData.mOverflow.GetUnit()) {
    display->mOverflow = NS_STYLE_OVERFLOW_AUTO;
  }
  else if (eCSSUnit_Inherit == aDisplayData.mOverflow.GetUnit()) {
    display->mOverflow = parentDisplay->mOverflow;
  }

  // clip property: length, auto, inherit
  if (nsnull != aDisplayData.mClip) {
    if (eCSSUnit_Inherit == aDisplayData.mClip->mTop.GetUnit()) { // if one is inherit, they all are
      inherited = PR_TRUE;
      display->mClipFlags = NS_STYLE_CLIP_INHERIT;
    }
    else {
      PRBool  fullAuto = PR_TRUE;

      display->mClipFlags = 0; // clear it

      if (eCSSUnit_Auto == aDisplayData.mClip->mTop.GetUnit()) {
        display->mClip.y = 0;
        display->mClipFlags |= NS_STYLE_CLIP_TOP_AUTO;
      } 
      else if (aDisplayData.mClip->mTop.IsLengthUnit()) {
        display->mClip.y = CalcLength(aDisplayData.mClip->mTop, nsnull, aContext, mPresContext, inherited);
        fullAuto = PR_FALSE;
      }
      if (eCSSUnit_Auto == aDisplayData.mClip->mBottom.GetUnit()) {
        display->mClip.height = 0;
        display->mClipFlags |= NS_STYLE_CLIP_BOTTOM_AUTO;
      } 
      else if (aDisplayData.mClip->mBottom.IsLengthUnit()) {
        display->mClip.height = CalcLength(aDisplayData.mClip->mBottom, nsnull, aContext, mPresContext, inherited) -
                                display->mClip.y;
        fullAuto = PR_FALSE;
      }
      if (eCSSUnit_Auto == aDisplayData.mClip->mLeft.GetUnit()) {
        display->mClip.x = 0;
        display->mClipFlags |= NS_STYLE_CLIP_LEFT_AUTO;
      } 
      else if (aDisplayData.mClip->mLeft.IsLengthUnit()) {
        display->mClip.x = CalcLength(aDisplayData.mClip->mLeft, nsnull, aContext, mPresContext, inherited);
        fullAuto = PR_FALSE;
      }
      if (eCSSUnit_Auto == aDisplayData.mClip->mRight.GetUnit()) {
        display->mClip.width = 0;
        display->mClipFlags |= NS_STYLE_CLIP_RIGHT_AUTO;
      } 
      else if (aDisplayData.mClip->mRight.IsLengthUnit()) {
        display->mClip.width = CalcLength(aDisplayData.mClip->mRight, nsnull, aContext, mPresContext, inherited) -
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
    aContext->SetStyle(eStyleStruct_Display, *display);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mResetData)
      aHighestNode->mStyleData.mResetData = new (mPresContext) nsResetStyleData;
    aHighestNode->mStyleData.mResetData->mDisplayData = display;
    // Propagate the bit down.
    PropagateInheritBit(NS_STYLE_INHERIT_DISPLAY, aHighestNode);
  }

  // CSS2 specified fixups:
  // 1) if float is not none, and display is not none, then we must set display to block
  //    XXX - there are problems with following the spec here: what we will do instead of
  //          following the letter of the spec is to make sure that floated elements are
  //          some kind of block, not strictly 'block' - see EnsureBlockDisplay method
  if (display->mDisplay != NS_STYLE_DISPLAY_NONE &&
      display->mFloats != NS_STYLE_FLOAT_NONE )
    EnsureBlockDisplay(display->mDisplay);
  
  // 2) if position is 'absolute' or 'fixed' then display must be 'block and float must be 'none'
  //    XXX - see note for fixup 1) above...
  if (display->IsAbsolutelyPositioned() && display->mDisplay != NS_STYLE_DISPLAY_NONE) {
    EnsureBlockDisplay(display->mDisplay);
    display->mFloats = NS_STYLE_FLOAT_NONE;
  }

  nsCOMPtr<nsIAtom> tag;
  aContext->GetPseudoType(*getter_AddRefs(tag));
  if (tag && tag.get() == nsCSSAtoms::beforePseudo || tag.get() == nsCSSAtoms::afterPseudo) {
    PRUint8 displayValue = display->mDisplay;
    if (parentDisplay->IsBlockLevel()) {
      // For block-level elements the only allowed 'display' values are:
      // 'none', 'inline', 'block', and 'marker'
      if ((NS_STYLE_DISPLAY_INLINE != displayValue) &&
          (NS_STYLE_DISPLAY_BLOCK != displayValue) &&
          (NS_STYLE_DISPLAY_MARKER != displayValue)) {
        // Pseudo-element behaves as if the value were 'block'
        displayValue = NS_STYLE_DISPLAY_BLOCK;
      }

    } else {
      // For inline-level elements the only allowed 'display' values are
      // 'none' and 'inline'
      displayValue = NS_STYLE_DISPLAY_INLINE;
    }
  
    if (display->mDisplay != displayValue) {
      // Reset the value
      display->mDisplay = displayValue;
    }
  }

  return display;
}

const nsStyleStruct*
nsRuleNode::ComputeVisibilityData(nsStyleVisibility* aStartVisibility, const nsCSSDisplay& aDisplayData, 
                                  nsIStyleContext* aContext, 
                                  nsRuleNode* aHighestNode,
                                  const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW VISIBILITY CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  nsStyleVisibility* visibility = nsnull;
  nsStyleVisibility* parentVisibility = visibility;
  PRBool inherited = aInherited;

  if (aStartVisibility)
    // We only need to compute the delta between this computed data and our
    // computed data.
    visibility = new (mPresContext) nsStyleVisibility(*aStartVisibility);
  else {
    if (aRuleDetail != eRuleFullMixed) {
      // No question. We will have to inherit. Go ahead and init
      // with inherited vals from parent.
      inherited = PR_TRUE;
      if (parentContext)
        parentVisibility = (nsStyleVisibility*)parentContext->GetStyleData(eStyleStruct_Visibility);
      if (parentVisibility)
        visibility = new (mPresContext) nsStyleVisibility(*parentVisibility);
    }
  }

  if (!visibility)
    visibility = parentVisibility = new (mPresContext) nsStyleVisibility(mPresContext);

  // opacity: factor, percent, inherit
  if (eCSSUnit_Percent == aDisplayData.mOpacity.GetUnit()) {
    inherited = PR_TRUE;
    float opacity = parentVisibility->mOpacity * aDisplayData.mOpacity.GetPercentValue();
    if (opacity < 0.0f) {
      visibility->mOpacity = 0.0f;
    } else if (1.0 < opacity) {
      visibility->mOpacity = 1.0f;
    }
    else {
      visibility->mOpacity = opacity;
    }
  }
  else if (eCSSUnit_Number == aDisplayData.mOpacity.GetUnit()) {
    visibility->mOpacity = aDisplayData.mOpacity.GetFloatValue();
  }
  else if (eCSSUnit_Inherit == aDisplayData.mOpacity.GetUnit()) {
    inherited = PR_TRUE;
    visibility->mOpacity = parentVisibility->mOpacity;
  }

  // direction: enum, inherit
  if (eCSSUnit_Enumerated == aDisplayData.mDirection.GetUnit()) {
    visibility->mDirection = aDisplayData.mDirection.GetIntValue();
#ifdef IBMBIDI    
    if (NS_STYLE_DIRECTION_RTL == visibility->mDirection)
      mPresContext->SetBidiEnabled(PR_TRUE);
#endif // IBMBIDI
  }
  else if (eCSSUnit_Inherit == aDisplayData.mDirection.GetUnit()) {
    inherited = PR_TRUE;
    visibility->mDirection = parentVisibility->mDirection;
  }

  // visibility: enum, inherit
  if (eCSSUnit_Enumerated == aDisplayData.mVisibility.GetUnit()) {
    visibility->mVisible = aDisplayData.mVisibility.GetIntValue();
  }
  else if (eCSSUnit_Inherit == aDisplayData.mVisibility.GetUnit()) {
    inherited = PR_TRUE;
    visibility->mVisible = parentVisibility->mVisible;
  }

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_Visibility, *visibility);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mInheritedData)
      aHighestNode->mStyleData.mInheritedData = new (mPresContext) nsInheritedStyleData;
    aHighestNode->mStyleData.mInheritedData->mVisibilityData = visibility;
    // Propagate the bit down.
    PropagateInheritBit(NS_STYLE_INHERIT_VISIBILITY, aHighestNode);
  }

  return visibility;
}

const nsStyleStruct*
nsRuleNode::ComputeColorData(nsStyleColor* aStartColor, const nsCSSColor& aColorData, 
                             nsIStyleContext* aContext, 
                             nsRuleNode* aHighestNode,
                             const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW COLOR CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  nsStyleColor* color = nsnull;
  nsStyleColor* parentColor = color;
  PRBool inherited = aInherited;

  if (aStartColor)
    // We only need to compute the delta between this computed data and our
    // computed data.
    color = new (mPresContext) nsStyleColor(*aStartColor);
  else {
    if (aRuleDetail != eRuleFullMixed) {
      // No question. We will have to inherit. Go ahead and init
      // with inherited vals from parent.
      inherited = PR_TRUE;
      if (parentContext)
        parentColor = (nsStyleColor*)parentContext->GetStyleData(eStyleStruct_Color);
      if (parentColor)
        color = new (mPresContext) nsStyleColor(*parentColor);
    }
  }

  if (!color)
    color = parentColor = new (mPresContext) nsStyleColor(mPresContext);

  // color: color, string, inherit
  SetColor(aColorData.mColor, parentColor->mColor, mPresContext, color->mColor, inherited);

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_Color, *color);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mInheritedData)
      aHighestNode->mStyleData.mInheritedData = new (mPresContext) nsInheritedStyleData;
    aHighestNode->mStyleData.mInheritedData->mColorData = color;
    // Propagate the bit down.
    PropagateInheritBit(NS_STYLE_INHERIT_COLOR, aHighestNode);
  }

  return color;
}

const nsStyleStruct*
nsRuleNode::ComputeBackgroundData(nsStyleBackground* aStartBG, const nsCSSColor& aColorData, 
                                  nsIStyleContext* aContext, 
                                  nsRuleNode* aHighestNode,
                                  const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW BACKGROUND CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  nsStyleBackground* bg;
  if (aStartBG)
    // We only need to compute the delta between this computed data and our
    // computed data.
    bg = new (mPresContext) nsStyleBackground(*aStartBG);
  else
    bg = new (mPresContext) nsStyleBackground(mPresContext);
  nsStyleBackground* parentBG = bg;

  if (parentContext)
    parentBG = (nsStyleBackground*)parentContext->GetStyleData(eStyleStruct_Background);
  PRBool inherited = aInherited;

  // background-color: color, string, enum (flags), inherit
  if (eCSSUnit_Inherit == aColorData.mBackColor.GetUnit()) { // do inherit first, so SetColor doesn't do it
    const nsStyleBackground* inheritBG = parentBG;
    if (inheritBG->mBackgroundFlags & NS_STYLE_BG_PROPAGATED_TO_PARENT) {
      // walk up the contexts until we get to a context that does not have its
      // background propagated to its parent (or a context that has had its background
      // propagated from its child)
      if (nsnull != parentContext) {
        nsCOMPtr<nsIStyleContext> higherContext = getter_AddRefs(parentContext->GetParent());
        do {
          if (higherContext) {
            inheritBG = (const nsStyleBackground*)higherContext->GetStyleData(eStyleStruct_Background);
            if (inheritBG && 
                (!(inheritBG->mBackgroundFlags & NS_STYLE_BG_PROPAGATED_TO_PARENT)) ||
                (inheritBG->mBackgroundFlags & NS_STYLE_BG_PROPAGATED_FROM_CHILD)) {
              // done walking up the higher contexts
              break;
            }
            higherContext = getter_AddRefs(higherContext->GetParent());
          }
        } while (higherContext);
      }
    }
    bg->mBackgroundColor = inheritBG->mBackgroundColor;
    bg->mBackgroundFlags &= ~NS_STYLE_BG_COLOR_TRANSPARENT;
    bg->mBackgroundFlags |= (inheritBG->mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT);
    inherited = PR_TRUE;
  }
  else if (SetColor(aColorData.mBackColor, parentBG->mBackgroundColor, 
                    mPresContext, bg->mBackgroundColor, inherited)) {
    bg->mBackgroundFlags &= ~NS_STYLE_BG_COLOR_TRANSPARENT;
  }
  else if (eCSSUnit_Enumerated == aColorData.mBackColor.GetUnit()) {
    //bg->mBackgroundColor = parentBG->mBackgroundColor; XXXwdh crap crap crap!
    bg->mBackgroundFlags |= NS_STYLE_BG_COLOR_TRANSPARENT;
  }

  // background-image: url, none, inherit
  if (eCSSUnit_URL == aColorData.mBackImage.GetUnit()) {
    aColorData.mBackImage.GetStringValue(bg->mBackgroundImage);
    bg->mBackgroundFlags &= ~NS_STYLE_BG_IMAGE_NONE;
  }
  else if (eCSSUnit_None == aColorData.mBackImage.GetUnit()) {
    bg->mBackgroundImage.Truncate();
    bg->mBackgroundFlags |= NS_STYLE_BG_IMAGE_NONE;
  }
  else if (eCSSUnit_Inherit == aColorData.mBackImage.GetUnit()) {
    inherited = PR_TRUE;
    bg->mBackgroundImage = parentBG->mBackgroundImage;
    bg->mBackgroundFlags &= ~NS_STYLE_BG_IMAGE_NONE;
    bg->mBackgroundFlags |= (parentBG->mBackgroundFlags & NS_STYLE_BG_IMAGE_NONE);
  }

  // background-repeat: enum, inherit
  if (eCSSUnit_Enumerated == aColorData.mBackRepeat.GetUnit()) {
    bg->mBackgroundRepeat = aColorData.mBackRepeat.GetIntValue();
  }
  else if (eCSSUnit_Inherit == aColorData.mBackRepeat.GetUnit()) {
    bg->mBackgroundRepeat = parentBG->mBackgroundRepeat;
  }

  // background-attachment: enum, inherit
  if (eCSSUnit_Enumerated == aColorData.mBackAttachment.GetUnit()) {
    bg->mBackgroundAttachment = aColorData.mBackAttachment.GetIntValue();
  }
  else if (eCSSUnit_Inherit == aColorData.mBackAttachment.GetUnit()) {
    inherited = PR_TRUE;
    bg->mBackgroundAttachment = parentBG->mBackgroundAttachment;
  }

  // background-position: enum, length, percent (flags), inherit
  if (eCSSUnit_Percent == aColorData.mBackPositionX.GetUnit()) {
    bg->mBackgroundXPosition = (nscoord)(100.0f * aColorData.mBackPositionX.GetPercentValue());
    bg->mBackgroundFlags |= NS_STYLE_BG_X_POSITION_PERCENT;
    bg->mBackgroundFlags &= ~NS_STYLE_BG_X_POSITION_LENGTH;
  }
  else if (aColorData.mBackPositionX.IsLengthUnit()) {
    bg->mBackgroundXPosition = CalcLength(aColorData.mBackPositionX, nsnull, 
                                          aContext, mPresContext, inherited);
    bg->mBackgroundFlags |= NS_STYLE_BG_X_POSITION_LENGTH;
    bg->mBackgroundFlags &= ~NS_STYLE_BG_X_POSITION_PERCENT;
  }
  else if (eCSSUnit_Enumerated == aColorData.mBackPositionX.GetUnit()) {
    bg->mBackgroundXPosition = (nscoord)aColorData.mBackPositionX.GetIntValue();
    bg->mBackgroundFlags |= NS_STYLE_BG_X_POSITION_PERCENT;
    bg->mBackgroundFlags &= ~NS_STYLE_BG_X_POSITION_LENGTH;
  }
  else if (eCSSUnit_Inherit == aColorData.mBackPositionX.GetUnit()) {
    inherited = PR_TRUE;
    bg->mBackgroundXPosition = parentBG->mBackgroundXPosition;
    bg->mBackgroundFlags &= ~(NS_STYLE_BG_X_POSITION_LENGTH | NS_STYLE_BG_X_POSITION_PERCENT);
    bg->mBackgroundFlags |= (parentBG->mBackgroundFlags & (NS_STYLE_BG_X_POSITION_LENGTH | NS_STYLE_BG_X_POSITION_PERCENT));
  }

  if (eCSSUnit_Percent == aColorData.mBackPositionY.GetUnit()) {
    bg->mBackgroundYPosition = (nscoord)(100.0f * aColorData.mBackPositionY.GetPercentValue());
    bg->mBackgroundFlags |= NS_STYLE_BG_Y_POSITION_PERCENT;
    bg->mBackgroundFlags &= ~NS_STYLE_BG_Y_POSITION_LENGTH;
  }
  else if (aColorData.mBackPositionY.IsLengthUnit()) {
    bg->mBackgroundYPosition = CalcLength(aColorData.mBackPositionY, nsnull,
                                          aContext, mPresContext, inherited);
    bg->mBackgroundFlags |= NS_STYLE_BG_Y_POSITION_LENGTH;
    bg->mBackgroundFlags &= ~NS_STYLE_BG_Y_POSITION_PERCENT;
  }
  else if (eCSSUnit_Enumerated == aColorData.mBackPositionY.GetUnit()) {
    bg->mBackgroundYPosition = (nscoord)aColorData.mBackPositionY.GetIntValue();
    bg->mBackgroundFlags |= NS_STYLE_BG_Y_POSITION_PERCENT;
    bg->mBackgroundFlags &= ~NS_STYLE_BG_Y_POSITION_LENGTH;
  }
  else if (eCSSUnit_Inherit == aColorData.mBackPositionY.GetUnit()) {
    inherited = PR_TRUE;
    bg->mBackgroundYPosition = parentBG->mBackgroundYPosition;
    bg->mBackgroundFlags &= ~(NS_STYLE_BG_Y_POSITION_LENGTH | NS_STYLE_BG_Y_POSITION_PERCENT);
    bg->mBackgroundFlags |= (parentBG->mBackgroundFlags & (NS_STYLE_BG_Y_POSITION_LENGTH | NS_STYLE_BG_Y_POSITION_PERCENT));
  }

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_Background, *bg);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mResetData)
      aHighestNode->mStyleData.mResetData = new (mPresContext) nsResetStyleData;
    aHighestNode->mStyleData.mResetData->mBackgroundData = bg;
    // Propagate the bit down.
    PropagateInheritBit(NS_STYLE_INHERIT_BACKGROUND, aHighestNode);
  }

  return bg;
}

const nsStyleStruct*
nsRuleNode::ComputeMarginData(nsStyleMargin* aStartMargin, const nsCSSMargin& aMarginData, 
                              nsIStyleContext* aContext, 
                              nsRuleNode* aHighestNode,
                              const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW MARGIN CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  nsStyleMargin* margin;
  if (aStartMargin)
    // We only need to compute the delta between this computed data and our
    // computed data.
    margin = new (mPresContext) nsStyleMargin(*aStartMargin);
  else
    margin = new (mPresContext) nsStyleMargin();
  nsStyleMargin* parentMargin = margin;

  if (parentContext)
    parentMargin = (nsStyleMargin*)parentContext->GetStyleData(eStyleStruct_Margin);
  PRBool inherited = aInherited;

  // margin: length, percent, auto, inherit
  if (aMarginData.mMargin) {
    nsStyleCoord  coord;
    nsStyleCoord  parentCoord;
    parentMargin->mMargin.GetLeft(parentCoord);
    if (SetCoord(aMarginData.mMargin->mLeft, coord, parentCoord, SETCOORD_LPAH, aContext, mPresContext, inherited)) {
      margin->mMargin.SetLeft(coord);
    }
    parentMargin->mMargin.GetTop(parentCoord);
    if (SetCoord(aMarginData.mMargin->mTop, coord, parentCoord, SETCOORD_LPAH, aContext, mPresContext, inherited)) {
      margin->mMargin.SetTop(coord);
    }
    parentMargin->mMargin.GetRight(parentCoord);
    if (SetCoord(aMarginData.mMargin->mRight, coord, parentCoord, SETCOORD_LPAH, aContext, mPresContext, inherited)) {
      margin->mMargin.SetRight(coord);
    }
    parentMargin->mMargin.GetBottom(parentCoord);
    if (SetCoord(aMarginData.mMargin->mBottom, coord, parentCoord, SETCOORD_LPAH, aContext, mPresContext, inherited)) {
      margin->mMargin.SetBottom(coord);
    }
  }

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_Margin, *margin);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mResetData)
      aHighestNode->mStyleData.mResetData = new (mPresContext) nsResetStyleData;
    aHighestNode->mStyleData.mResetData->mMarginData = margin;
    // Propagate the bit down.
    PropagateInheritBit(NS_STYLE_INHERIT_MARGIN, aHighestNode);
  }

  margin->RecalcData();
  return margin;
}

const nsStyleStruct* 
nsRuleNode::ComputeBorderData(nsStyleBorder* aStartBorder, const nsCSSMargin& aMarginData, 
                              nsIStyleContext* aContext, 
                              nsRuleNode* aHighestNode,
                              const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW BORDER CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  nsStyleBorder* border;
  if (aStartBorder)
    // We only need to compute the delta between this computed data and our
    // computed data.
    border = new (mPresContext) nsStyleBorder(*aStartBorder);
  else
    border = new (mPresContext) nsStyleBorder(mPresContext);
  
  nsStyleBorder* parentBorder = border;
  if (parentContext)
    parentBorder = (nsStyleBorder*)parentContext->GetStyleData(eStyleStruct_Border);
  PRBool inherited = aInherited;

  // border-size: length, enum, inherit
  if (aMarginData.mBorderWidth) {
    nsStyleCoord  coord;
    nsStyleCoord  parentCoord;
    if (SetCoord(aMarginData.mBorderWidth->mLeft, coord, parentCoord, SETCOORD_LE, aContext, mPresContext, inherited))
      border->mBorder.SetLeft(coord);
    else if (eCSSUnit_Inherit == aMarginData.mBorderWidth->mLeft.GetUnit())
      border->mBorder.SetLeft(parentBorder->mBorder.GetLeft(coord));

    if (SetCoord(aMarginData.mBorderWidth->mTop, coord, parentCoord, SETCOORD_LE, aContext, mPresContext, inherited))
      border->mBorder.SetTop(coord);
    else if (eCSSUnit_Inherit == aMarginData.mBorderWidth->mTop.GetUnit())
      border->mBorder.SetTop(parentBorder->mBorder.GetTop(coord));

    if (SetCoord(aMarginData.mBorderWidth->mRight, coord, parentCoord, SETCOORD_LE, aContext, mPresContext, inherited))
      border->mBorder.SetRight(coord);
    else if (eCSSUnit_Inherit == aMarginData.mBorderWidth->mRight.GetUnit())
      border->mBorder.SetRight(parentBorder->mBorder.GetRight(coord));

    if (SetCoord(aMarginData.mBorderWidth->mBottom, coord, parentCoord, SETCOORD_LE, aContext, mPresContext, inherited))
      border->mBorder.SetBottom(coord);
    else if (eCSSUnit_Inherit == aMarginData.mBorderWidth->mBottom.GetUnit())
      border->mBorder.SetBottom(parentBorder->mBorder.GetBottom(coord));
  }

  // border-style: enum, none, inhert
  if (nsnull != aMarginData.mBorderStyle) {
    nsCSSRect* ourStyle = aMarginData.mBorderStyle;
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

  // border-color: color, string, enum, inherit
  if (nsnull != aMarginData.mBorderColor) {
    nsCSSRect* ourBorderColor = aMarginData.mBorderColor;
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
  if (aMarginData.mBorderRadius) {
    nsStyleCoord  coord;
    nsStyleCoord  parentCoord;
    parentBorder->mBorderRadius.GetLeft(parentCoord);
    if (SetCoord(aMarginData.mBorderRadius->mLeft, coord, parentCoord, SETCOORD_LPH, aContext, mPresContext, inherited))
      border->mBorderRadius.SetLeft(coord);
    parentBorder->mBorderRadius.GetTop(parentCoord);
    if (SetCoord(aMarginData.mBorderRadius->mTop, coord, parentCoord, SETCOORD_LPH, aContext, mPresContext, inherited))
      border->mBorderRadius.SetTop(coord);
    parentBorder->mBorderRadius.GetRight(parentCoord);
    if (SetCoord(aMarginData.mBorderRadius->mRight, coord, parentCoord, SETCOORD_LPH, aContext, mPresContext, inherited))
      border->mBorderRadius.SetRight(coord);
    parentBorder->mBorderRadius.GetBottom(parentCoord);
    if (SetCoord(aMarginData.mBorderRadius->mBottom, coord, parentCoord, SETCOORD_LPH, aContext, mPresContext, inherited))
      border->mBorderRadius.SetBottom(coord);
  }

  // float-edge: enum, inherit
  if (eCSSUnit_Enumerated == aMarginData.mFloatEdge.GetUnit())
    border->mFloatEdge = aMarginData.mFloatEdge.GetIntValue();
  else if (eCSSUnit_Inherit == aMarginData.mFloatEdge.GetUnit()) {
    inherited = PR_TRUE;
    border->mFloatEdge = parentBorder->mFloatEdge;
  }

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_Border, *border);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mResetData)
      aHighestNode->mStyleData.mResetData = new (mPresContext) nsResetStyleData;
    aHighestNode->mStyleData.mResetData->mBorderData = border;
    // Propagate the bit down.
    PropagateInheritBit(NS_STYLE_INHERIT_BORDER, aHighestNode);
  }

  border->RecalcData();
  return border;
}
  
const nsStyleStruct*
nsRuleNode::ComputePaddingData(nsStylePadding* aStartPadding, const nsCSSMargin& aMarginData, 
                               nsIStyleContext* aContext, 
                               nsRuleNode* aHighestNode,
                               const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW PADDING CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  nsStylePadding* padding;
  if (aStartPadding)
    // We only need to compute the delta between this computed data and our
    // computed data.
    padding = new (mPresContext) nsStylePadding(*aStartPadding);
  else
    padding = new (mPresContext) nsStylePadding();
  
  nsStylePadding* parentPadding = padding;
  if (parentContext)
    parentPadding = (nsStylePadding*)parentContext->GetStyleData(eStyleStruct_Padding);
  PRBool inherited = aInherited;

  // padding: length, percent, inherit
  if (aMarginData.mPadding) {
    nsStyleCoord  coord;
    nsStyleCoord  parentCoord;
    parentPadding->mPadding.GetLeft(parentCoord);
    if (SetCoord(aMarginData.mPadding->mLeft, coord, parentCoord, SETCOORD_LPH, aContext, mPresContext, inherited)) {
      padding->mPadding.SetLeft(coord);
    }
    parentPadding->mPadding.GetTop(parentCoord);
    if (SetCoord(aMarginData.mPadding->mTop, coord, parentCoord, SETCOORD_LPH, aContext, mPresContext, inherited)) {
      padding->mPadding.SetTop(coord);
    }
    parentPadding->mPadding.GetRight(parentCoord);
    if (SetCoord(aMarginData.mPadding->mRight, coord, parentCoord, SETCOORD_LPH, aContext, mPresContext, inherited)) {
      padding->mPadding.SetRight(coord);
    }
    parentPadding->mPadding.GetBottom(parentCoord);
    if (SetCoord(aMarginData.mPadding->mBottom, coord, parentCoord, SETCOORD_LPH, aContext, mPresContext, inherited)) {
      padding->mPadding.SetBottom(coord);
    }
  }

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_Padding, *padding);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mResetData)
      aHighestNode->mStyleData.mResetData = new (mPresContext) nsResetStyleData;
    aHighestNode->mStyleData.mResetData->mPaddingData = padding;
    // Propagate the bit down.
    PropagateInheritBit(NS_STYLE_INHERIT_PADDING, aHighestNode);
  }

  padding->RecalcData();
  return padding;
}

const nsStyleStruct*
nsRuleNode::ComputeOutlineData(nsStyleOutline* aStartOutline, const nsCSSMargin& aMarginData, 
                               nsIStyleContext* aContext, 
                               nsRuleNode* aHighestNode,
                               const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW OUTLINE CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  nsStyleOutline* outline;
  if (aStartOutline)
    // We only need to compute the delta between this computed data and our
    // computed data.
    outline = new (mPresContext) nsStyleOutline(*aStartOutline);
  else
    outline = new (mPresContext) nsStyleOutline(mPresContext);
  
  nsStyleOutline* parentOutline = outline;
  if (parentContext)
    parentOutline = (nsStyleOutline*)parentContext->GetStyleData(eStyleStruct_Outline);
  PRBool inherited = aInherited;

  // outline-width: length, enum, inherit
  SetCoord(aMarginData.mOutlineWidth, outline->mOutlineWidth, parentOutline->mOutlineWidth,
           SETCOORD_LEH, aContext, mPresContext, inherited);
  
  // outline-color: color, string, enum, inherit
  nscolor outlineColor;
  nscolor unused = NS_RGB(0,0,0);
  if (eCSSUnit_Inherit == aMarginData.mOutlineColor.GetUnit()) {
    inherited = PR_TRUE;
    if (parentOutline->GetOutlineColor(outlineColor))
      outline->SetOutlineColor(outlineColor);
    else
      outline->SetOutlineInvert();
  }
  else if (SetColor(aMarginData.mOutlineColor, unused, mPresContext, outlineColor, inherited))
    outline->SetOutlineColor(outlineColor);
  else if (eCSSUnit_Enumerated == aMarginData.mOutlineColor.GetUnit())
    outline->SetOutlineInvert();

  // outline-style: enum, none, inherit
  if (eCSSUnit_Enumerated == aMarginData.mOutlineStyle.GetUnit())
    outline->SetOutlineStyle(aMarginData.mOutlineStyle.GetIntValue());
  else if (eCSSUnit_None == aMarginData.mOutlineStyle.GetUnit())
    outline->SetOutlineStyle(NS_STYLE_BORDER_STYLE_NONE);
  else if (eCSSUnit_Inherit == aMarginData.mOutlineStyle.GetUnit()) {
    inherited = PR_TRUE;
    outline->SetOutlineStyle(parentOutline->GetOutlineStyle());
  }

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_Outline, *outline);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mResetData)
      aHighestNode->mStyleData.mResetData = new (mPresContext) nsResetStyleData;
    aHighestNode->mStyleData.mResetData->mOutlineData = outline;
    // Propagate the bit down.
    PropagateInheritBit(NS_STYLE_INHERIT_OUTLINE, aHighestNode);
  }

  outline->RecalcData();
  return outline;
}

const nsStyleStruct* 
nsRuleNode::ComputeListData(nsStyleList* aStartList, const nsCSSList& aListData, 
                            nsIStyleContext* aContext, 
                            nsRuleNode* aHighestNode,
                            const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW LIST CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  nsStyleList* list = nsnull;
  nsStyleList* parentList = list;
  PRBool inherited = aInherited;

  if (aStartList)
    // We only need to compute the delta between this computed data and our
    // computed data.
    list = new (mPresContext) nsStyleList(*aStartList);
  else {
    if (aRuleDetail != eRuleFullMixed) {
      // No question. We will have to inherit. Go ahead and init
      // with inherited vals from parent.
      inherited = PR_TRUE;
      if (parentContext)
        parentList = (nsStyleList*)parentContext->GetStyleData(eStyleStruct_List);
      if (parentList)
        list = new (mPresContext) nsStyleList(*parentList);
    }
  }

  if (!list)
    list = parentList = new (mPresContext) nsStyleList();

  // list-style-type: enum, none, inherit
  if (eCSSUnit_Enumerated == aListData.mType.GetUnit()) {
    list->mListStyleType = aListData.mType.GetIntValue();
  }
  else if (eCSSUnit_None == aListData.mType.GetUnit()) {
    list->mListStyleType = NS_STYLE_LIST_STYLE_NONE;
  }
  else if (eCSSUnit_Inherit == aListData.mType.GetUnit()) {
    inherited = PR_TRUE;
    list->mListStyleType = parentList->mListStyleType;
  }

  // list-style-image: url, none, inherit
  if (eCSSUnit_URL == aListData.mImage.GetUnit()) {
    aListData.mImage.GetStringValue(list->mListStyleImage);
  }
  else if (eCSSUnit_None == aListData.mImage.GetUnit()) {
    list->mListStyleImage.Truncate();
  }
  else if (eCSSUnit_Inherit == aListData.mImage.GetUnit()) {
    inherited = PR_TRUE;
    list->mListStyleImage = parentList->mListStyleImage;
  }

  // list-style-position: enum, inherit
  if (eCSSUnit_Enumerated == aListData.mPosition.GetUnit()) {
    list->mListStylePosition = aListData.mPosition.GetIntValue();
  }
  else if (eCSSUnit_Inherit == aListData.mPosition.GetUnit()) {
    inherited = PR_TRUE;
    list->mListStylePosition = parentList->mListStylePosition;
  }

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_List, *list);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mInheritedData)
      aHighestNode->mStyleData.mInheritedData = new (mPresContext) nsInheritedStyleData;
    aHighestNode->mStyleData.mInheritedData->mListData = list;
    // Propagate the bit down.
    PropagateInheritBit(NS_STYLE_INHERIT_LIST, aHighestNode);
  }

  return list;
}

const nsStyleStruct* 
nsRuleNode::ComputePositionData(nsStylePosition* aStartPos, const nsCSSPosition& aPosData, 
                                nsIStyleContext* aContext, 
                                nsRuleNode* aHighestNode,
                                const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW POSITION CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  nsStylePosition* pos;
  if (aStartPos)
    // We only need to compute the delta between this computed data and our
    // computed data.
    pos = new (mPresContext) nsStylePosition(*aStartPos);
  else
    pos = new (mPresContext) nsStylePosition();
  
  nsStylePosition* parentPos = pos;
  if (parentContext)
    parentPos = (nsStylePosition*)parentContext->GetStyleData(eStyleStruct_Position);
  PRBool inherited = aInherited;

  // box offsets: length, percent, auto, inherit
  if (aPosData.mOffset) {
    nsStyleCoord  coord;
    nsStyleCoord  parentCoord;
    parentPos->mOffset.GetTop(parentCoord);
    if (SetCoord(aPosData.mOffset->mTop, coord, parentCoord, SETCOORD_LPAH, aContext, mPresContext, inherited)) {
      pos->mOffset.SetTop(coord);            
    }
    parentPos->mOffset.GetRight(parentCoord);
    if (SetCoord(aPosData.mOffset->mRight, coord, parentCoord, SETCOORD_LPAH, aContext, mPresContext, inherited)) {
      pos->mOffset.SetRight(coord);            
    }
    parentPos->mOffset.GetBottom(parentCoord);
    if (SetCoord(aPosData.mOffset->mBottom, coord, parentCoord, SETCOORD_LPAH, aContext, mPresContext, inherited)) {
      pos->mOffset.SetBottom(coord);
    }
    parentPos->mOffset.GetLeft(parentCoord);
    if (SetCoord(aPosData.mOffset->mLeft, coord, parentCoord, SETCOORD_LPAH, aContext, mPresContext, inherited)) {
      pos->mOffset.SetLeft(coord);
    }
  }

  if (aPosData.mWidth.GetUnit() == eCSSUnit_Proportional)
    pos->mWidth.SetIntValue((PRInt32)(aPosData.mWidth.GetFloatValue()), eStyleUnit_Proportional);
  else 
    SetCoord(aPosData.mWidth, pos->mWidth, parentPos->mWidth,
             SETCOORD_LPAH, aContext, mPresContext, inherited);
  SetCoord(aPosData.mMinWidth, pos->mMinWidth, parentPos->mMinWidth,
           SETCOORD_LPH, aContext, mPresContext, inherited);
  if (! SetCoord(aPosData.mMaxWidth, pos->mMaxWidth, parentPos->mMaxWidth,
                 SETCOORD_LPH, aContext, mPresContext, inherited)) {
    if (eCSSUnit_None == aPosData.mMaxWidth.GetUnit()) {
      pos->mMaxWidth.Reset();
    }
  }

  SetCoord(aPosData.mHeight, pos->mHeight, parentPos->mHeight,
           SETCOORD_LPAH, aContext, mPresContext, inherited);
  SetCoord(aPosData.mMinHeight, pos->mMinHeight, parentPos->mMinHeight,
           SETCOORD_LPH, aContext, mPresContext, inherited);
  if (! SetCoord(aPosData.mMaxHeight, pos->mMaxHeight, parentPos->mMaxHeight,
                 SETCOORD_LPH, aContext, mPresContext, inherited)) {
    if (eCSSUnit_None == aPosData.mMaxHeight.GetUnit()) {
      pos->mMaxHeight.Reset();
    }
  }

  // box-sizing: enum, inherit
  if (eCSSUnit_Enumerated == aPosData.mBoxSizing.GetUnit()) {
    pos->mBoxSizing = aPosData.mBoxSizing.GetIntValue();
  }
  else if (eCSSUnit_Inherit == aPosData.mBoxSizing.GetUnit()) {
    inherited = PR_TRUE;
    pos->mBoxSizing = parentPos->mBoxSizing;
  }

  // z-index
  if (! SetCoord(aPosData.mZIndex, pos->mZIndex, parentPos->mZIndex,
                 SETCOORD_IA, aContext, nsnull, inherited)) {
    if (eCSSUnit_Inherit == aPosData.mZIndex.GetUnit()) {
      // handle inherit, because it's ok to inherit 'auto' here
      inherited = PR_TRUE;
      pos->mZIndex = parentPos->mZIndex;
    }
  }

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_Position, *pos);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mResetData)
      aHighestNode->mStyleData.mResetData = new (mPresContext) nsResetStyleData;
    aHighestNode->mStyleData.mResetData->mPositionData = pos;
    // Propagate the bit down.
    PropagateInheritBit(NS_STYLE_INHERIT_POSITION, aHighestNode);
  }

  return pos;
}

const nsStyleStruct* 
nsRuleNode::ComputeTableData(nsStyleTable* aStartTable, const nsCSSTable& aTableData, 
                             nsIStyleContext* aContext, 
                             nsRuleNode* aHighestNode,
                             const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW TABLE CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  nsStyleTable* table;
  if (aStartTable)
    // We only need to compute the delta between this computed data and our
    // computed data.
    table = new (mPresContext) nsStyleTable(*aStartTable);
  else
    table = new (mPresContext) nsStyleTable();
  
  nsStyleTable* parentTable = table;
  if (parentContext)
    parentTable = (nsStyleTable*)parentContext->GetStyleData(eStyleStruct_Table);
  PRBool inherited = aInherited;

  // table-layout: auto, enum, inherit
  if (eCSSUnit_Enumerated == aTableData.mLayout.GetUnit())
    table->mLayoutStrategy = aTableData.mLayout.GetIntValue();
  else if (eCSSUnit_Auto == aTableData.mLayout.GetUnit())
    table->mLayoutStrategy = NS_STYLE_TABLE_LAYOUT_AUTO;
  else if (eCSSUnit_Inherit == aTableData.mLayout.GetUnit()) {
    inherited = PR_TRUE;
    table->mLayoutStrategy = parentTable->mLayoutStrategy;
  }

  // rules: enum (not a real CSS prop)
  if (eCSSUnit_Enumerated == aTableData.mRules.GetUnit())
    table->mRules = aTableData.mRules.GetIntValue();

  // frame: enum (not a real CSS prop)
  if (eCSSUnit_Enumerated == aTableData.mFrame.GetUnit())
    table->mFrame = aTableData.mFrame.GetIntValue();

  // cols: enum, int (not a real CSS prop)
  if (eCSSUnit_Enumerated == aTableData.mCols.GetUnit() ||
      eCSSUnit_Integer == aTableData.mCols.GetUnit())
    table->mCols = aTableData.mCols.GetIntValue();

  // span: pixels (not a real CSS prop)
  if (eCSSUnit_Enumerated == aTableData.mSpan.GetUnit() ||
      eCSSUnit_Integer == aTableData.mSpan.GetUnit())
    table->mSpan = aTableData.mSpan.GetIntValue();
    
  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_Table, *table);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mResetData)
      aHighestNode->mStyleData.mResetData = new (mPresContext) nsResetStyleData;
    aHighestNode->mStyleData.mResetData->mTableData = table;
    // Propagate the bit down.
    PropagateInheritBit(NS_STYLE_INHERIT_TABLE, aHighestNode);
  }

  return table;
}

const nsStyleStruct* 
nsRuleNode::ComputeTableBorderData(nsStyleTableBorder* aStartTable, const nsCSSTable& aTableData, 
                                   nsIStyleContext* aContext, 
                                   nsRuleNode* aHighestNode,
                                   const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW TABLE BORDER CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  nsStyleTableBorder* table = nsnull;
  nsStyleTableBorder* parentTable = table;
  PRBool inherited = aInherited;

  if (aStartTable)
    // We only need to compute the delta between this computed data and our
    // computed data.
    table = new (mPresContext) nsStyleTableBorder(*aStartTable);
  else {
    if (aRuleDetail != eRuleFullMixed) {
      // No question. We will have to inherit. Go ahead and init
      // with inherited vals from parent.
      inherited = PR_TRUE;
      if (parentContext)
        parentTable = (nsStyleTableBorder*)parentContext->GetStyleData(eStyleStruct_TableBorder);
      if (parentTable)
        table = new (mPresContext) nsStyleTableBorder(*parentTable);
    }
  }

  if (!table)
    table = parentTable = new (mPresContext) nsStyleTableBorder(mPresContext);

  // border-collapse: enum, inherit
  if (eCSSUnit_Enumerated == aTableData.mBorderCollapse.GetUnit()) {
    table->mBorderCollapse = aTableData.mBorderCollapse.GetIntValue();
  }
  else if (eCSSUnit_Inherit == aTableData.mBorderCollapse.GetUnit()) {
    inherited = PR_TRUE;
    table->mBorderCollapse = parentTable->mBorderCollapse;
  }

  nsStyleCoord coord;

  // border-spacing-x: length, inherit
  if (SetCoord(aTableData.mBorderSpacingX, coord, coord, SETCOORD_LENGTH, aContext, mPresContext, inherited)) {
    table->mBorderSpacingX = coord.GetCoordValue();
  }
  else if (eCSSUnit_Inherit == aTableData.mBorderSpacingX.GetUnit()) {
    inherited = PR_TRUE;
    table->mBorderSpacingX = parentTable->mBorderSpacingX;
  }
  // border-spacing-y: length, inherit
  if (SetCoord(aTableData.mBorderSpacingY, coord, coord, SETCOORD_LENGTH, aContext, mPresContext, inherited)) {
    table->mBorderSpacingY = coord.GetCoordValue();
  }
  else if (eCSSUnit_Inherit == aTableData.mBorderSpacingY.GetUnit()) {
    inherited = PR_TRUE;
    table->mBorderSpacingY = parentTable->mBorderSpacingY;
  }

  // caption-side: enum, inherit
  if (eCSSUnit_Enumerated == aTableData.mCaptionSide.GetUnit()) {
    table->mCaptionSide = aTableData.mCaptionSide.GetIntValue();
  }
  else if (eCSSUnit_Inherit == aTableData.mCaptionSide.GetUnit()) {
    inherited = PR_TRUE;
    table->mCaptionSide = parentTable->mCaptionSide;
  }

  // empty-cells: enum, inherit
  if (eCSSUnit_Enumerated == aTableData.mEmptyCells.GetUnit()) {
    table->mEmptyCells = aTableData.mEmptyCells.GetIntValue();
  }
  else if (eCSSUnit_Inherit == aTableData.mEmptyCells.GetUnit()) {
    inherited = PR_TRUE;
    table->mEmptyCells = parentTable->mEmptyCells;
  }

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_TableBorder, *table);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mInheritedData)
      aHighestNode->mStyleData.mInheritedData = new (mPresContext) nsInheritedStyleData;
    aHighestNode->mStyleData.mInheritedData->mTableData = table;
    // Propagate the bit down.
    PropagateInheritBit(NS_STYLE_INHERIT_TABLE_BORDER, aHighestNode);
  }

  return table;
}

const nsStyleStruct* 
nsRuleNode::ComputeContentData(nsStyleContent* aStartContent, const nsCSSContent& aContentData, 
                               nsIStyleContext* aContext, 
                               nsRuleNode* aHighestNode,
                               const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW CONTENT CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  nsStyleContent* content;
  if (aStartContent)
    // We only need to compute the delta between this computed data and our
    // computed data.
    content = new (mPresContext) nsStyleContent(*aStartContent);
  else
    content = new (mPresContext) nsStyleContent();
  
  nsStyleContent* parentContent = content;
  if (parentContext)
    parentContent = (nsStyleContent*)parentContext->GetStyleData(eStyleStruct_Content);
  PRBool inherited = aInherited;

  // content: [string, url, counter, attr, enum]+, inherit
  PRUint32 count;
  nsAutoString  buffer;
  nsCSSValueList* contentValue = aContentData.mContent;
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
        contentValue = aContentData.mContent;
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
  nsCSSCounterData* ourIncrement = aContentData.mCounterIncrement;
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
        ourIncrement = aContentData.mCounterIncrement;
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
  nsCSSCounterData* ourReset = aContentData.mCounterReset;
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
        ourReset = aContentData.mCounterReset;
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
  SetCoord(aContentData.mMarkerOffset, content->mMarkerOffset, parentContent->mMarkerOffset,
           SETCOORD_LH | SETCOORD_AUTO, aContext, mPresContext, inherited);
    
  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_Content, *content);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mResetData)
      aHighestNode->mStyleData.mResetData = new (mPresContext) nsResetStyleData;
    aHighestNode->mStyleData.mResetData->mContentData = content;
    // Propagate the bit down.
    PropagateInheritBit(NS_STYLE_INHERIT_CONTENT, aHighestNode);
  }

  return content;
}

const nsStyleStruct* 
nsRuleNode::ComputeQuotesData(nsStyleQuotes* aStartQuotes, const nsCSSContent& aContentData, 
                                   nsIStyleContext* aContext, 
                                   nsRuleNode* aHighestNode,
                                   const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW QUOTES CREATED!\n");
#endif

  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  nsStyleQuotes* quotes = nsnull;
  nsStyleQuotes* parentQuotes = quotes;
  PRBool inherited = aInherited;

  if (aStartQuotes)
    // We only need to compute the delta between this computed data and our
    // computed data.
    quotes = new (mPresContext) nsStyleQuotes(*aStartQuotes);
  else {
    if (aRuleDetail != eRuleFullMixed) {
      // No question. We will have to inherit. Go ahead and init
      // with inherited vals from parent.
      inherited = PR_TRUE;
      if (parentContext)
        parentQuotes = (nsStyleQuotes*)parentContext->GetStyleData(eStyleStruct_Quotes);
      if (parentQuotes)
        quotes = new (mPresContext) nsStyleQuotes(*parentQuotes);
    }
  }

  if (!quotes)
    quotes = parentQuotes = new (mPresContext) nsStyleQuotes();

  // quotes: [string string]+, none, inherit
  PRUint32 count;
  nsAutoString  buffer;
  nsCSSQuotes* ourQuotes = aContentData.mQuotes;
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
        ourQuotes = aContentData.mQuotes;
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
    aContext->SetStyle(eStyleStruct_Quotes, *quotes);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mInheritedData)
      aHighestNode->mStyleData.mInheritedData = new (mPresContext) nsInheritedStyleData;
    aHighestNode->mStyleData.mInheritedData->mQuotesData = quotes;
    // Propagate the bit down.
    PropagateInheritBit(NS_STYLE_INHERIT_QUOTES, aHighestNode);
  }

  return quotes;
}

#ifdef INCLUDE_XUL
const nsStyleStruct* 
nsRuleNode::ComputeXULData(nsStyleXUL* aStartXUL, const nsCSSXUL& aXULData, 
                           nsIStyleContext* aContext, 
                           nsRuleNode* aHighestNode,
                           const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW XUL CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  nsStyleXUL* xul = nsnull;
  
  if (aStartXUL)
    // We only need to compute the delta between this computed data and our
    // computed data.
    xul = new (mPresContext) nsStyleXUL(*aStartXUL);
  else
    xul = new (mPresContext) nsStyleXUL();

  nsStyleXUL* parentXUL = xul;

  if (parentContext)
    parentXUL = (nsStyleXUL*)parentContext->GetStyleData(eStyleStruct_XUL);

  PRBool inherited = aInherited;

  // box-orient: enum, inherit
  if (eCSSUnit_Enumerated == aXULData.mBoxOrient.GetUnit()) {
    xul->mBoxOrient = aXULData.mBoxOrient.GetIntValue();
  }
  else if (eCSSUnit_Inherit == aXULData.mBoxOrient.GetUnit()) {
    inherited = PR_TRUE;
    xul->mBoxOrient = parentXUL->mBoxOrient;
  }

  if (inherited)
    // We inherited, and therefore can't be cached in the rule node.  We have to be put right on the
    // style context.
    aContext->SetStyle(eStyleStruct_XUL, *xul);
  else {
    // We were fully specified and can therefore be cached right on the rule node.
    if (!aHighestNode->mStyleData.mResetData)
      aHighestNode->mStyleData.mResetData = new (mPresContext) nsResetStyleData;
    aHighestNode->mStyleData.mResetData->mXULData = xul;
    // Propagate the bit down.
    PropagateInheritBit(NS_STYLE_INHERIT_XUL, aHighestNode);
  }

  return xul;
}
#endif

static void
ExamineRectProperties(const nsCSSRect* aRect, PRUint32& aTotalCount, PRUint32& aInheritedCount)
{
  if (!aRect)
    return;

  if (eCSSUnit_Null != aRect->mLeft.GetUnit()) {
    aTotalCount++;
    if (eCSSUnit_Inherit == aRect->mLeft.GetUnit())
      aInheritedCount++;
  }

  if (eCSSUnit_Null != aRect->mTop.GetUnit()) {
    aTotalCount++;
    if (eCSSUnit_Inherit == aRect->mTop.GetUnit())
      aInheritedCount++;
  }

  if (eCSSUnit_Null != aRect->mRight.GetUnit()) {
    aTotalCount++;
    if (eCSSUnit_Inherit == aRect->mRight.GetUnit())
      aInheritedCount++;
  }

  if (eCSSUnit_Null != aRect->mBottom.GetUnit()) {
    aTotalCount++;
    if (eCSSUnit_Inherit == aRect->mBottom.GetUnit())
      aInheritedCount++;
  }
}

inline nsRuleNode::RuleDetail 
nsRuleNode::CheckFontProperties(const nsCSSFont& aFontData)
{
  const PRUint32 numFontProps = 5;
  PRUint32 totalCount=0;
  PRUint32 inheritCount=0;

  if (eCSSUnit_Null != aFontData.mFamily.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aFontData.mFamily.GetUnit())
      inheritCount++;
    if (eCSSUnit_Enumerated == aFontData.mFamily.GetUnit()) {
      // A special case. We treat this as a fully specified font,
      // since no other font props are legal with a system font.
      switch (aFontData.mFamily.GetIntValue()) {
        case NS_STYLE_FONT_CAPTION:
        case NS_STYLE_FONT_ICON:
        case NS_STYLE_FONT_MENU:
        case NS_STYLE_FONT_MESSAGE_BOX:
        case NS_STYLE_FONT_SMALL_CAPTION:
        case NS_STYLE_FONT_STATUS_BAR:
        case NS_STYLE_FONT_WINDOW:
        case NS_STYLE_FONT_DOCUMENT:
        case NS_STYLE_FONT_WORKSPACE:
        case NS_STYLE_FONT_DESKTOP:
        case NS_STYLE_FONT_INFO:
        case NS_STYLE_FONT_DIALOG:
        case NS_STYLE_FONT_BUTTON:
        case NS_STYLE_FONT_PULL_DOWN_MENU:
        case NS_STYLE_FONT_LIST:
        case NS_STYLE_FONT_FIELD:
          return eRuleFullMixed;
      }
    }
  }

  if (eCSSUnit_Null != aFontData.mStyle.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aFontData.mStyle.GetUnit())
      inheritCount++;
  }
  if (eCSSUnit_Null != aFontData.mVariant.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aFontData.mVariant.GetUnit())
      inheritCount++;
  }
  if (eCSSUnit_Null != aFontData.mWeight.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aFontData.mWeight.GetUnit())
      inheritCount++;
  }
  if (eCSSUnit_Null != aFontData.mSize.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aFontData.mSize.GetUnit())
      inheritCount++;
  }
  
  if (inheritCount == numFontProps)
    return eRuleFullInherited;
  else if (totalCount == numFontProps) 
    return eRuleFullMixed;

  if (totalCount == 0)
    return eRuleNone;
  else if (totalCount == inheritCount)
    return eRulePartialInherited;

  return eRulePartialMixed;
}

inline nsRuleNode::RuleDetail 
nsRuleNode::CheckDisplayProperties(const nsCSSDisplay& aData)
{
  // Left/Top/Right/Bottom are the four props we have to check.
  const PRUint32 numProps = 10;
  PRUint32 totalCount=0;
  PRUint32 inheritCount=0;

  ExamineRectProperties(aData.mClip, totalCount, inheritCount);

  if (eCSSUnit_Null != aData.mDisplay.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aData.mDisplay.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aData.mBinding.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aData.mBinding.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aData.mPosition.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aData.mPosition.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aData.mFloat.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aData.mFloat.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aData.mClear.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aData.mClear.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aData.mOverflow.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aData.mOverflow.GetUnit())
      inheritCount++;
  }

  if (inheritCount == numProps)
    return eRuleFullInherited;
  else if (totalCount == numProps) 
    return eRuleFullMixed;

  if (totalCount == 0)
    return eRuleNone;
  else if (totalCount == inheritCount)
    return eRulePartialInherited;

  return eRulePartialMixed;
}

inline nsRuleNode::RuleDetail 
nsRuleNode::CheckVisibilityProperties(const nsCSSDisplay& aData)
{
  // Left/Top/Right/Bottom are the four props we have to check.
  const PRUint32 numProps = 3;
  PRUint32 totalCount=0;
  PRUint32 inheritCount=0;

  if (eCSSUnit_Null != aData.mVisibility.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aData.mVisibility.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aData.mDirection.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aData.mDirection.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aData.mOpacity.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aData.mOpacity.GetUnit())
      inheritCount++;
  }

  if (inheritCount == numProps)
    return eRuleFullInherited;
  else if (totalCount == numProps) 
    return eRuleFullMixed;

  if (totalCount == 0)
    return eRuleNone;
  else if (totalCount == inheritCount)
    return eRulePartialInherited;

  return eRulePartialMixed;
}

inline nsRuleNode::RuleDetail 
nsRuleNode::CheckMarginProperties(const nsCSSMargin& aMarginData)
{
  // Left/Top/Right/Bottom are the four props we have to check.
  const PRUint32 numMarginProps = 4;
  PRUint32 totalCount=0;
  PRUint32 inheritCount=0;

  ExamineRectProperties(aMarginData.mMargin, totalCount, inheritCount);

  if (inheritCount == numMarginProps)
    return eRuleFullInherited;
  else if (totalCount == numMarginProps) 
    return eRuleFullMixed;

  if (totalCount == 0)
    return eRuleNone;
  else if (totalCount == inheritCount)
    return eRulePartialInherited;

  return eRulePartialMixed;
}

inline nsRuleNode::RuleDetail 
nsRuleNode::CheckBorderProperties(const nsCSSMargin& aBorderData)
{
  // Left/Top/Right/Bottom are the four props we have to check.
  const PRUint32 numBorderProps = 17;
  PRUint32 totalCount=0;
  PRUint32 inheritCount=0;

  ExamineRectProperties(aBorderData.mBorderWidth, totalCount, inheritCount);
  ExamineRectProperties(aBorderData.mBorderStyle, totalCount, inheritCount);
  ExamineRectProperties(aBorderData.mBorderColor, totalCount, inheritCount);
  ExamineRectProperties(aBorderData.mBorderRadius, totalCount, inheritCount);
  if (eCSSUnit_Null != aBorderData.mFloatEdge.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aBorderData.mFloatEdge.GetUnit())
      inheritCount++;
  }

  if (inheritCount == numBorderProps)
    return eRuleFullInherited;
  else if (totalCount == numBorderProps)
    return eRuleFullMixed;

  if (totalCount == 0)
    return eRuleNone;
  else if (totalCount == inheritCount)
    return eRulePartialInherited;

  return eRulePartialMixed;
}
  
inline nsRuleNode::RuleDetail 
nsRuleNode::CheckPaddingProperties(const nsCSSMargin& aPaddingData)
{
  // Left/Top/Right/Bottom are the four props we have to check.
  const PRUint32 numPaddingProps = 4;
  PRUint32 totalCount=0;
  PRUint32 inheritCount=0;

  ExamineRectProperties(aPaddingData.mPadding, totalCount, inheritCount);

  if (inheritCount == numPaddingProps)
    return eRuleFullInherited;
  else if (totalCount == numPaddingProps) 
    return eRuleFullMixed;

  if (totalCount == 0)
    return eRuleNone;
  else if (totalCount == inheritCount)
    return eRulePartialInherited;

  return eRulePartialMixed;
}
  
inline nsRuleNode::RuleDetail 
nsRuleNode::CheckOutlineProperties(const nsCSSMargin& aMargin)
{
  const PRUint32 numProps = 7;
  PRUint32 totalCount=0;
  PRUint32 inheritCount=0;
  if (eCSSUnit_Null != aMargin.mOutlineColor.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aMargin.mOutlineColor.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aMargin.mOutlineWidth.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aMargin.mOutlineWidth.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aMargin.mOutlineStyle.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aMargin.mOutlineStyle.GetUnit())
      inheritCount++;
  }

  ExamineRectProperties(aMargin.mOutlineRadius, totalCount, inheritCount);
  
  if (inheritCount == numProps)
    return eRuleFullInherited;
  else if (totalCount == numProps) 
    return eRuleFullMixed;

  if (totalCount == 0)
    return eRuleNone;
  else if (totalCount == inheritCount)
    return eRulePartialInherited;

  return eRulePartialMixed;
}

inline nsRuleNode::RuleDetail 
nsRuleNode::CheckListProperties(const nsCSSList& aListData)
{
  const PRUint32 numListProps = 3;
  PRUint32 totalCount=0;
  PRUint32 inheritCount=0;
  if (eCSSUnit_Null != aListData.mType.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aListData.mType.GetUnit())
      inheritCount++;
  }
  if (eCSSUnit_Null != aListData.mImage.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aListData.mImage.GetUnit())
      inheritCount++;
  }
  if (eCSSUnit_Null != aListData.mPosition.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aListData.mPosition.GetUnit())
      inheritCount++;
  }
  
  if (inheritCount == numListProps)
    return eRuleFullInherited;
  else if (totalCount == numListProps)
    return eRuleFullMixed;

  if (totalCount == 0)
    return eRuleNone;
  else if (totalCount == inheritCount)
    return eRulePartialInherited;

  return eRulePartialMixed;
}

inline nsRuleNode::RuleDetail 
nsRuleNode::CheckColorProperties(const nsCSSColor& aColorData)
{
  const PRUint32 numColorProps = 1;
  PRUint32 totalCount=0;
  PRUint32 inheritCount=0;
  if (eCSSUnit_Null != aColorData.mColor.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aColorData.mColor.GetUnit())
      inheritCount++;
  }
  
  if (inheritCount == numColorProps)
    return eRuleFullInherited;
  else if (totalCount == numColorProps)
    return eRuleFullMixed;

  if (totalCount == 0)
    return eRuleNone;
  else if (totalCount == inheritCount)
    return eRulePartialInherited;

  return eRulePartialMixed;
}

inline nsRuleNode::RuleDetail 
nsRuleNode::CheckBackgroundProperties(const nsCSSColor& aColorData)
{
  const PRUint32 numBGProps = 6;
  PRUint32 totalCount=0;
  PRUint32 inheritCount=0;
  
  if (eCSSUnit_Null != aColorData.mBackAttachment.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aColorData.mBackAttachment.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aColorData.mBackRepeat.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aColorData.mBackRepeat.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aColorData.mBackColor.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aColorData.mBackColor.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aColorData.mBackImage.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aColorData.mBackImage.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aColorData.mBackPositionX.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aColorData.mBackPositionX.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aColorData.mBackPositionY.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aColorData.mBackPositionY.GetUnit())
      inheritCount++;
  }

  if (inheritCount == numBGProps)
    return eRuleFullInherited;
  else if (totalCount == numBGProps)
    return eRuleFullMixed;

  if (totalCount == 0)
    return eRuleNone;
  else if (totalCount == inheritCount)
    return eRulePartialInherited;

  return eRulePartialMixed;
}

inline nsRuleNode::RuleDetail 
nsRuleNode::CheckPositionProperties(const nsCSSPosition& aPosData)
{
  const PRUint32 numPosProps = 12;
  PRUint32 totalCount=0;
  PRUint32 inheritCount=0;

  ExamineRectProperties(aPosData.mOffset, totalCount, inheritCount);
  
  if (eCSSUnit_Null != aPosData.mWidth.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aPosData.mWidth.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aPosData.mMinWidth.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aPosData.mMinWidth.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aPosData.mMaxWidth.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aPosData.mMaxWidth.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aPosData.mHeight.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aPosData.mHeight.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aPosData.mMinHeight.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aPosData.mMinHeight.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aPosData.mMaxHeight.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aPosData.mMaxHeight.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aPosData.mBoxSizing.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aPosData.mBoxSizing.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aPosData.mZIndex.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aPosData.mZIndex.GetUnit())
      inheritCount++;
  }

  if (inheritCount == numPosProps)
    return eRuleFullInherited;
  else if (totalCount == numPosProps)
    return eRuleFullMixed;

  if (totalCount == 0)
    return eRuleNone;
  else if (totalCount == inheritCount)
    return eRulePartialInherited;

  return eRulePartialMixed;
}

inline nsRuleNode::RuleDetail 
nsRuleNode::CheckTableProperties(const nsCSSTable& aTableData)
{
  const PRUint32 numTableProps = 5;
  PRUint32 totalCount=0;
  PRUint32 inheritCount=0;

  if (eCSSUnit_Null != aTableData.mLayout.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aTableData.mLayout.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aTableData.mFrame.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aTableData.mFrame.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aTableData.mRules.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aTableData.mRules.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aTableData.mCols.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aTableData.mCols.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aTableData.mSpan.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aTableData.mSpan.GetUnit())
      inheritCount++;
  }

  if (inheritCount == numTableProps)
    return eRuleFullInherited;
  else if (totalCount == numTableProps)
    return eRuleFullMixed;

  if (totalCount == 0)
    return eRuleNone;
  else if (totalCount == inheritCount)
    return eRulePartialInherited;

  return eRulePartialMixed;
}

inline nsRuleNode::RuleDetail 
nsRuleNode::CheckTableBorderProperties(const nsCSSTable& aTableData)
{
  const PRUint32 numTableProps = 5;
  PRUint32 totalCount=0;
  PRUint32 inheritCount=0;

  if (eCSSUnit_Null != aTableData.mBorderCollapse.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aTableData.mBorderCollapse.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aTableData.mBorderSpacingX.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aTableData.mBorderSpacingX.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aTableData.mBorderSpacingY.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aTableData.mBorderSpacingY.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aTableData.mCaptionSide.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aTableData.mCaptionSide.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aTableData.mEmptyCells.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aTableData.mEmptyCells.GetUnit())
      inheritCount++;
  }

  if (inheritCount == numTableProps)
    return eRuleFullInherited;
  else if (totalCount == numTableProps)
    return eRuleFullMixed;

  if (totalCount == 0)
    return eRuleNone;
  else if (totalCount == inheritCount)
    return eRulePartialInherited;

  return eRulePartialMixed;
}

inline nsRuleNode::RuleDetail 
nsRuleNode::CheckContentProperties(const nsCSSContent& aData)
{
  const PRUint32 numProps = 4;
  PRUint32 totalCount=0;
  PRUint32 inheritCount=0;

  if (aData.mContent) {
    totalCount++;
    if (eCSSUnit_Inherit == aData.mContent->mValue.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aData.mMarkerOffset.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aData.mMarkerOffset.GetUnit())
      inheritCount++;
  }

  if (aData.mCounterIncrement) {
    totalCount++;
    if (eCSSUnit_Inherit == aData.mCounterIncrement->mCounter.GetUnit())
      inheritCount++;
  }

  if (aData.mCounterReset) {
    totalCount++;
    if (eCSSUnit_Inherit == aData.mCounterReset->mCounter.GetUnit())
      inheritCount++;
  }

  if (inheritCount == numProps)
    return eRuleFullInherited;
  else if (totalCount == numProps)
    return eRuleFullMixed;

  if (totalCount == 0)
    return eRuleNone;
  else if (totalCount == inheritCount)
    return eRulePartialInherited;

  return eRulePartialMixed;
}

inline nsRuleNode::RuleDetail 
nsRuleNode::CheckQuotesProperties(const nsCSSContent& aData)
{
  const PRUint32 numProps = 1;
  PRUint32 totalCount=0;
  PRUint32 inheritCount=0;

  if (aData.mQuotes) {
    totalCount++;
    if (eCSSUnit_Inherit == aData.mQuotes->mOpen.GetUnit())
      inheritCount++;
  }

  if (inheritCount == numProps)
    return eRuleFullInherited;
  else if (totalCount == numProps)
    return eRuleFullMixed;

  if (totalCount == 0)
    return eRuleNone;
  else if (totalCount == inheritCount)
    return eRulePartialInherited;

  return eRulePartialMixed;
}

inline nsRuleNode::RuleDetail 
nsRuleNode::CheckTextProperties(const nsCSSText& aData)
{
  const PRUint32 numProps = 8;
  PRUint32 totalCount=0;
  PRUint32 inheritCount=0;

  if (eCSSUnit_Null != aData.mLineHeight.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aData.mLineHeight.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aData.mUnicodeBidi.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aData.mUnicodeBidi.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aData.mTextIndent.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aData.mTextIndent.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aData.mTextAlign.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aData.mTextAlign.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aData.mTextTransform.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aData.mTextTransform.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aData.mWordSpacing.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aData.mWordSpacing.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aData.mLetterSpacing.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aData.mLetterSpacing.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aData.mWhiteSpace.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aData.mWhiteSpace.GetUnit())
      inheritCount++;
  }

  if (inheritCount == numProps)
    return eRuleFullInherited;
  else if (totalCount == numProps)
    return eRuleFullMixed;

  if (totalCount == 0)
    return eRuleNone;
  else if (totalCount == inheritCount)
    return eRulePartialInherited;

  return eRulePartialMixed;
}

inline nsRuleNode::RuleDetail 
nsRuleNode::CheckTextResetProperties(const nsCSSText& aData)
{
  const PRUint32 numProps = 2;
  PRUint32 totalCount=0;
  PRUint32 inheritCount=0;

  if (eCSSUnit_Null != aData.mDecoration.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aData.mDecoration.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aData.mVerticalAlign.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aData.mVerticalAlign.GetUnit())
      inheritCount++;
  }

  if (inheritCount == numProps)
    return eRuleFullInherited;
  else if (totalCount == numProps)
    return eRuleFullMixed;

  if (totalCount == 0)
    return eRuleNone;
  else if (totalCount == inheritCount)
    return eRulePartialInherited;

  return eRulePartialMixed;
}

inline nsRuleNode::RuleDetail 
nsRuleNode::CheckUIProperties(const nsCSSUserInterface& aData)
{
  const PRUint32 numProps = 4;
  PRUint32 totalCount=0;
  PRUint32 inheritCount=0;

  if (eCSSUnit_Null != aData.mUserInput.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aData.mUserInput.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aData.mUserModify.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aData.mUserModify.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aData.mUserFocus.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aData.mUserFocus.GetUnit())
      inheritCount++;
  }

  if (aData.mCursor) {
    totalCount++;
    if (aData.mCursor->mValue.GetUnit() == eCSSUnit_Inherit)
      inheritCount++;
  }

  if (inheritCount == numProps)
    return eRuleFullInherited;
  else if (totalCount == numProps)
    return eRuleFullMixed;

  if (totalCount == 0)
    return eRuleNone;
  else if (totalCount == inheritCount)
    return eRulePartialInherited;

  return eRulePartialMixed;
}

inline nsRuleNode::RuleDetail 
nsRuleNode::CheckUIResetProperties(const nsCSSUserInterface& aData)
{
  const PRUint32 numProps = 3;
  PRUint32 totalCount=0;
  PRUint32 inheritCount=0;

  if (eCSSUnit_Null != aData.mUserSelect.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aData.mUserSelect.GetUnit())
      inheritCount++;
  }

  if (eCSSUnit_Null != aData.mResizer.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aData.mResizer.GetUnit())
      inheritCount++;
  }

  if (aData.mKeyEquivalent) {
    totalCount++;
    if (aData.mKeyEquivalent->mValue.GetUnit() == eCSSUnit_Inherit)
      inheritCount++;
  }

  if (inheritCount == numProps)
    return eRuleFullInherited;
  else if (totalCount == numProps)
    return eRuleFullMixed;

  if (totalCount == 0)
    return eRuleNone;
  else if (totalCount == inheritCount)
    return eRulePartialInherited;

  return eRulePartialMixed;
}

#ifdef INCLUDE_XUL
inline nsRuleNode::RuleDetail 
nsRuleNode::CheckXULProperties(const nsCSSXUL& aXULData)
{
  const PRUint32 numXULProps = 1;
  PRUint32 totalCount=0;
  PRUint32 inheritCount=0;

  if (eCSSUnit_Null != aXULData.mBoxOrient.GetUnit()) {
    totalCount++;
    if (eCSSUnit_Inherit == aXULData.mBoxOrient.GetUnit())
      inheritCount++;
  }

  if (inheritCount == numXULProps)
    return eRuleFullInherited;
  else if (totalCount == numXULProps) 
    return eRuleFullMixed;

  if (totalCount == 0)
    return eRuleNone;
  else if (totalCount == inheritCount)
    return eRulePartialInherited;

  return eRulePartialMixed;
}
#endif

inline const nsStyleStruct* 
nsRuleNode::GetParentData(const nsStyleStructID& aSID)
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

inline const nsStyleStruct* 
nsRuleNode::GetStyleData(nsStyleStructID aSID, 
                         nsIStyleContext* aContext)
{
  const nsStyleStruct* cachedData = mStyleData.GetStyleData(aSID);
  if (cachedData)
    return cachedData; // We have a fully specified struct. Just return it.

  if (InheritsFromParentRule(aSID))
    return GetParentData(aSID); // We inherit. Just go up the rule tree and return the first
                                // cached struct we find.

  switch (aSID) {
    // Nothing is cached.  We'll have to delve further and examine our rules.
    case eStyleStruct_Font:
      return GetFontData(aContext);
    case eStyleStruct_Display:
      return GetDisplayData(aContext);
    case eStyleStruct_Visibility:
      return GetVisibilityData(aContext);
    case eStyleStruct_Color:
      return GetColorData(aContext);
    case eStyleStruct_Background:
      return GetBackgroundData(aContext);
    case eStyleStruct_Margin:
      return GetMarginData(aContext);
    case eStyleStruct_Border:
      return GetBorderData(aContext);
    case eStyleStruct_Padding:
      return GetPaddingData(aContext);
    case eStyleStruct_Outline:
      return GetOutlineData(aContext);
    case eStyleStruct_List:
      return GetListData(aContext);
    case eStyleStruct_Position:
      return GetPositionData(aContext);
    case eStyleStruct_Table:
      return GetTableData(aContext);
    case eStyleStruct_TableBorder:
      return GetTableBorderData(aContext);
    case eStyleStruct_Content:
      return GetContentData(aContext);
    case eStyleStruct_Quotes:
      return GetQuotesData(aContext);
    case eStyleStruct_Text:
      return GetTextData(aContext);
    case eStyleStruct_TextReset:
      return GetTextResetData(aContext);
    case eStyleStruct_UserInterface:
      return GetUIData(aContext);
    case eStyleStruct_UIReset:
      return GetUIResetData(aContext);
#ifdef INCLUDE_XUL
    case eStyleStruct_XUL:
      return GetXULData(aContext);
#endif
  }

  return nsnull;
}
