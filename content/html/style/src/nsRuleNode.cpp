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
#include "nsRuleNode.h"
#include "nsIDeviceContext.h"
#include "nsILookAndFeel.h"
#include "nsIPresShell.h"
#include "nsIFontMetrics.h"
#include "nsIDocShellTreeItem.h"
#include "nsStyleUtil.h"
#include "nsCSSAtoms.h"

// Temporary - Test of full time Standard mode for forms
#include "nsIPref.h"
#include "nsIServiceManager.h"

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
    return NS_PTR_TO_INT32(mKey);
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
#define SETCOORD_LPFHN  (SETCOORD_LPH | SETCOORD_FACTOR | SETCOORD_NORMAL)
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
    // all cached data along this path.  Note that, because of the definition
    // of inline style, all nodes along this path must have exactly one child.  This
    // is not a bushy subtree, and so we know that by clearing this path, we've
    // invalidated everything that we need to.
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

PRBool PR_CALLBACK ClearCachedDataInSubtreeHelper(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsIRuleNode* ruleNode = (nsIRuleNode*)aData;
  nsIStyleRule* rule = (nsIStyleRule*)aClosure;
  ruleNode->ClearCachedDataInSubtree(rule);
  return PR_TRUE;
}

NS_IMETHODIMP
nsRuleNode::ClearCachedDataInSubtree(nsIStyleRule* aRule)
{
  if (aRule == nsnull || mRule == aRule) {
    // We have a match.  Blow away all data stored at this node.
    if (mStyleData.mResetData || mStyleData.mInheritedData)
      mStyleData.Destroy(0, mPresContext);
    mNoneBits &= ~NS_STYLE_INHERIT_MASK;
    mInheritBits &= ~NS_STYLE_INHERIT_MASK;
    aRule = nsnull;
  }

  if (mChildren)
    mChildren->Enumerate(ClearCachedDataInSubtreeHelper, (void*)aRule);

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
nsRuleNode::InheritsFromParentRule(const nsStyleStructID aSID)
{
  return mInheritBits & nsCachedStyleData::GetBitForSID(aSID);
}

/*
 * The following "Check" functions are used for determining what type of
 * sharing can be used for the data on this rule node.  MORE HERE...
 */

/* the information for a property (or in some cases, a rect group of
   properties) */

// for PropertyCheckData::type
// XXX Would bits be more efficient?
#define CHECKDATA_VALUE       0
#define CHECKDATA_RECT        1
#define CHECKDATA_VALUELIST   2
#define CHECKDATA_COUNTERDATA 3
#define CHECKDATA_QUOTES      4
#define CHECKDATA_SHADOW      5

struct PropertyCheckData {
  size_t offset;
  PRUint16 type;
  PRPackedBool mayHaveExplicitInherit;
};

#define CHECKDATA_PROP(_datastruct, _member, _type, _iscoord) \
  { offsetof(_datastruct, _member), _type, _iscoord }

/* the information for all the properties in a style struct */

typedef nsRuleNode::RuleDetail
  (* PR_CALLBACK CheckCallbackFn)(const nsCSSStruct& aData);

struct StructCheckData {
  const PropertyCheckData* props;
  PRInt32 nprops;
  CheckCallbackFn callback;
};

#define CHECKDATA_STRUCT(_props) \
  {_props, sizeof(_props)/sizeof(PropertyCheckData), nsnull}
#define CHECKDATA_STRUCT_WITH_CALLBACK(_props, _cb) \
  {_props, sizeof(_props)/sizeof(PropertyCheckData), _cb}

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
CheckFontCallback(const nsCSSStruct& aData)
{
  const nsCSSFont& fontData = NS_STATIC_CAST(const nsCSSFont&, aData);
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
      return nsRuleNode::eRuleFullMixed;
  }
  return nsRuleNode::eRuleUnknown;
}

static const PropertyCheckData FontCheckProperties[] = {
  CHECKDATA_PROP(nsCSSFont, mFamily, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSFont, mStyle, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSFont, mVariant, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSFont, mWeight, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSFont, mSize, CHECKDATA_VALUE, PR_FALSE)
};

static const PropertyCheckData DisplayCheckProperties[] = {
  CHECKDATA_PROP(nsCSSDisplay, mClip, CHECKDATA_RECT, PR_FALSE),
  CHECKDATA_PROP(nsCSSDisplay, mDisplay, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSDisplay, mBinding, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSDisplay, mPosition, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSDisplay, mFloat, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSDisplay, mClear, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSDisplay, mOverflow, CHECKDATA_VALUE, PR_FALSE)
};

static const PropertyCheckData VisibilityCheckProperties[] = {
  CHECKDATA_PROP(nsCSSDisplay, mVisibility, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSDisplay, mDirection, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSDisplay, mOpacity, CHECKDATA_VALUE, PR_FALSE)
};

static const PropertyCheckData MarginCheckProperties[] = {
  CHECKDATA_PROP(nsCSSMargin, mMargin, CHECKDATA_RECT, PR_TRUE)
};

static const PropertyCheckData BorderCheckProperties[] = {
  CHECKDATA_PROP(nsCSSMargin, mBorderWidth, CHECKDATA_RECT, PR_FALSE),
  CHECKDATA_PROP(nsCSSMargin, mBorderStyle, CHECKDATA_RECT, PR_FALSE),
  CHECKDATA_PROP(nsCSSMargin, mBorderColor, CHECKDATA_RECT, PR_FALSE),
  CHECKDATA_PROP(nsCSSMargin, mBorderRadius, CHECKDATA_RECT, PR_TRUE),
  CHECKDATA_PROP(nsCSSMargin, mFloatEdge, CHECKDATA_VALUE, PR_FALSE)
};

static const PropertyCheckData PaddingCheckProperties[] = {
  CHECKDATA_PROP(nsCSSMargin, mPadding, CHECKDATA_RECT, PR_TRUE)
};

static const PropertyCheckData OutlineCheckProperties[] = {
  CHECKDATA_PROP(nsCSSMargin, mOutlineColor, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSMargin, mOutlineWidth, CHECKDATA_VALUE, PR_TRUE),
  CHECKDATA_PROP(nsCSSMargin, mOutlineStyle, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSMargin, mOutlineRadius, CHECKDATA_RECT, PR_TRUE)
};

static const PropertyCheckData ListCheckProperties[] = {
  CHECKDATA_PROP(nsCSSList, mType, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSList, mImage, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSList, mPosition, CHECKDATA_VALUE, PR_FALSE)
};

static const PropertyCheckData ColorCheckProperties[] = {
  CHECKDATA_PROP(nsCSSColor, mColor, CHECKDATA_VALUE, PR_FALSE)
};

static const PropertyCheckData BackgroundCheckProperties[] = {
  CHECKDATA_PROP(nsCSSColor, mBackAttachment, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSColor, mBackRepeat, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSColor, mBackColor, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSColor, mBackImage, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSColor, mBackPositionX, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSColor, mBackPositionY, CHECKDATA_VALUE, PR_FALSE)
};

static const PropertyCheckData PositionCheckProperties[] = {
  CHECKDATA_PROP(nsCSSPosition, mOffset, CHECKDATA_RECT, PR_TRUE),
  CHECKDATA_PROP(nsCSSPosition, mWidth, CHECKDATA_VALUE, PR_TRUE),
  CHECKDATA_PROP(nsCSSPosition, mMinWidth, CHECKDATA_VALUE, PR_TRUE),
  CHECKDATA_PROP(nsCSSPosition, mMaxWidth, CHECKDATA_VALUE, PR_TRUE),
  CHECKDATA_PROP(nsCSSPosition, mHeight, CHECKDATA_VALUE, PR_TRUE),
  CHECKDATA_PROP(nsCSSPosition, mMinHeight, CHECKDATA_VALUE, PR_TRUE),
  CHECKDATA_PROP(nsCSSPosition, mMaxHeight, CHECKDATA_VALUE, PR_TRUE),
  CHECKDATA_PROP(nsCSSPosition, mBoxSizing, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSPosition, mZIndex, CHECKDATA_VALUE, PR_FALSE)
};

static const PropertyCheckData TableCheckProperties[] = {
  CHECKDATA_PROP(nsCSSTable, mLayout, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSTable, mFrame, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSTable, mRules, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSTable, mCols, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSTable, mSpan, CHECKDATA_VALUE, PR_FALSE)
};

static const PropertyCheckData TableBorderCheckProperties[] = {
  CHECKDATA_PROP(nsCSSTable, mBorderCollapse, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSTable, mBorderSpacingX, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSTable, mBorderSpacingY, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSTable, mCaptionSide, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSTable, mEmptyCells, CHECKDATA_VALUE, PR_FALSE)
};

static const PropertyCheckData ContentCheckProperties[] = {
  CHECKDATA_PROP(nsCSSContent, mContent, CHECKDATA_VALUELIST, PR_FALSE),
  CHECKDATA_PROP(nsCSSContent, mMarkerOffset, CHECKDATA_VALUE, PR_TRUE),
  CHECKDATA_PROP(nsCSSContent, mCounterIncrement, CHECKDATA_COUNTERDATA, PR_FALSE),
  CHECKDATA_PROP(nsCSSContent, mCounterReset, CHECKDATA_COUNTERDATA, PR_FALSE)
};

static const PropertyCheckData QuotesCheckProperties[] = {
  CHECKDATA_PROP(nsCSSContent, mQuotes, CHECKDATA_QUOTES, PR_FALSE)
};

static const PropertyCheckData TextCheckProperties[] = {
  CHECKDATA_PROP(nsCSSText, mLineHeight, CHECKDATA_VALUE, PR_TRUE),
  CHECKDATA_PROP(nsCSSText, mTextIndent, CHECKDATA_VALUE, PR_TRUE),
  CHECKDATA_PROP(nsCSSText, mWordSpacing, CHECKDATA_VALUE, PR_TRUE),
  CHECKDATA_PROP(nsCSSText, mLetterSpacing, CHECKDATA_VALUE, PR_TRUE),
  CHECKDATA_PROP(nsCSSText, mTextAlign, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSText, mTextTransform, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSText, mWhiteSpace, CHECKDATA_VALUE, PR_FALSE)
};

static const PropertyCheckData TextResetCheckProperties[] = {
  CHECKDATA_PROP(nsCSSText, mDecoration, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSText, mVerticalAlign, CHECKDATA_VALUE, PR_TRUE),
  CHECKDATA_PROP(nsCSSText, mUnicodeBidi, CHECKDATA_VALUE, PR_FALSE)
};

static const PropertyCheckData UserInterfaceCheckProperties[] = {
  CHECKDATA_PROP(nsCSSUserInterface, mUserInput, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSUserInterface, mUserModify, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSUserInterface, mUserFocus, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSUserInterface, mCursor, CHECKDATA_VALUELIST, PR_FALSE)
};

static const PropertyCheckData UIResetCheckProperties[] = {
  CHECKDATA_PROP(nsCSSUserInterface, mUserSelect, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSUserInterface, mResizer, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSUserInterface, mKeyEquivalent, CHECKDATA_VALUELIST, PR_FALSE)
};

#ifdef INCLUDE_XUL
static const PropertyCheckData XULCheckProperties[] = {
  CHECKDATA_PROP(nsCSSXUL, mBoxAlign, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSXUL, mBoxDirection, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSXUL, mBoxFlex, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSXUL, mBoxOrient, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSXUL, mBoxPack, CHECKDATA_VALUE, PR_FALSE),
  CHECKDATA_PROP(nsCSSXUL, mBoxOrdinal, CHECKDATA_VALUE, PR_FALSE)
};
#endif

// These are indexed by style struct ID and must stay in order!
static const StructCheckData gCheckProperties[] = {
  { nsnull, 0, nsnull}, /* empty, since no 0th SID */
  CHECKDATA_STRUCT_WITH_CALLBACK(FontCheckProperties, CheckFontCallback),
  CHECKDATA_STRUCT(ColorCheckProperties),
  CHECKDATA_STRUCT(BackgroundCheckProperties),
  CHECKDATA_STRUCT(ListCheckProperties),
  CHECKDATA_STRUCT(PositionCheckProperties),
  CHECKDATA_STRUCT(TextCheckProperties),
  CHECKDATA_STRUCT(TextResetCheckProperties),
  CHECKDATA_STRUCT(DisplayCheckProperties),
  CHECKDATA_STRUCT(VisibilityCheckProperties),
  CHECKDATA_STRUCT(ContentCheckProperties),
  CHECKDATA_STRUCT(QuotesCheckProperties),
  CHECKDATA_STRUCT(UserInterfaceCheckProperties),
  CHECKDATA_STRUCT(UIResetCheckProperties),
  CHECKDATA_STRUCT(TableCheckProperties),
  CHECKDATA_STRUCT(TableBorderCheckProperties),
  CHECKDATA_STRUCT(MarginCheckProperties),
  CHECKDATA_STRUCT(PaddingCheckProperties),
  CHECKDATA_STRUCT(BorderCheckProperties),
  CHECKDATA_STRUCT(OutlineCheckProperties),
#ifdef INCLUDE_XUL
  CHECKDATA_STRUCT(XULCheckProperties),
#endif
  { nsnull, 0, nsnull} /* empty, so at least we crash reliably if someone
                          passes in the BorderPaddingShortcut ID */
};



// XXXldb Taking the address of a reference is evil.

inline const nsCSSValue&
ValueAtOffset(const nsCSSStruct& aCSSStruct, size_t aOffset)
{
  return * NS_REINTERPRET_CAST(const nsCSSValue*,
                     NS_REINTERPRET_CAST(const char*, &aCSSStruct) + aOffset);
}

inline const nsCSSRect*
RectAtOffset(const nsCSSStruct& aCSSStruct, size_t aOffset)
{
  return * NS_REINTERPRET_CAST(const nsCSSRect*const*,
                     NS_REINTERPRET_CAST(const char*, &aCSSStruct) + aOffset);
}

inline const nsCSSValueList*
ValueListAtOffset(const nsCSSStruct& aCSSStruct, size_t aOffset)
{
  return * NS_REINTERPRET_CAST(const nsCSSValueList*const*,
                     NS_REINTERPRET_CAST(const char*, &aCSSStruct) + aOffset);
}

inline const nsCSSCounterData*
CounterDataAtOffset(const nsCSSStruct& aCSSStruct, size_t aOffset)
{
  return * NS_REINTERPRET_CAST(const nsCSSCounterData*const*,
                     NS_REINTERPRET_CAST(const char*, &aCSSStruct) + aOffset);
}

inline const nsCSSQuotes*
QuotesAtOffset(const nsCSSStruct& aCSSStruct, size_t aOffset)
{
  return * NS_REINTERPRET_CAST(const nsCSSQuotes*const*,
                     NS_REINTERPRET_CAST(const char*, &aCSSStruct) + aOffset);
}

inline nsRuleNode::RuleDetail
nsRuleNode::CheckSpecifiedProperties(const nsStyleStructID aSID,
                                     const nsCSSStruct& aCSSStruct)
{
  const StructCheckData *structData = gCheckProperties + aSID;
  if (structData->callback) {
    nsRuleNode::RuleDetail res = (*structData->callback)(aCSSStruct);
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
          const nsCSSValue& value = ValueAtOffset(aCSSStruct, prop->offset);
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
          ExamineRectCoordProperties(RectAtOffset(aCSSStruct, prop->offset),
                                     specified, inherited,
                                     canHaveExplicitInherit);
        else
          ExamineRectProperties(RectAtOffset(aCSSStruct, prop->offset),
                                specified, inherited);
        break;

      case CHECKDATA_VALUELIST:
        {
          ++total;
          const nsCSSValueList* valueList =
              ValueListAtOffset(aCSSStruct, prop->offset);
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
              CounterDataAtOffset(aCSSStruct, prop->offset);
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
              QuotesAtOffset(aCSSStruct, prop->offset);
          if (quotes) {
            ++specified;
            if (eCSSUnit_Inherit == quotes->mOpen.GetUnit()) {
              ++inherited;
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
  if (specified == total)
    return eRuleFullMixed;
  if (specified == 0)
    return eRuleNone;
  if (specified == inherited)
    return eRulePartialInherited;
  return eRulePartialMixed;
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
nsRuleNode::WalkRuleTree(const nsStyleStructID aSID,
                         nsIStyleContext* aContext, 
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

  if (detail == eRuleNone && startStruct && !aRuleData->mPostResolveCallback) {
    // We specified absolutely no rule information, but a parent rule in the tree
    // specified all the rule information.  We set a bit along the branch from our
    // node in the tree to the node that specified the data that tells nodes on that
    // branch that they never need to examine their rules for this particular struct type
    // ever again.
    PropagateInheritBit(bit, ruleNode);
    if (eStyleStruct_Background == aSID && aRuleData->mPostResolveCallback)
      (*aRuleData->mPostResolveCallback) ((nsStyleStruct *)startStruct, aRuleData);
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
  ComputeStyleDataFn fn = gComputeStyleDataFn[aSID];
  const nsStyleStruct* res = (this->*fn)(startStruct, *aSpecificData, aContext, highestNode, detail,
                                         !aRuleData->mCanStoreInRuleTree);

  // If we have a post-resolve callback, handle that now.
  if (aRuleData->mPostResolveCallback)
    (*aRuleData->mPostResolveCallback)((nsStyleStruct*)res, aRuleData);

  // Now return the result.
  return res;
}

const nsStyleStruct*
nsRuleNode::SetDefaultOnRoot(const nsStyleStructID aSID, nsIStyleContext* aContext)
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

    case eStyleStruct_BorderPaddingShortcut:
      NS_ERROR("unexpected SID");
  }
  return nsnull;
}

nsRuleNode::ComputeStyleDataFn
nsRuleNode::gComputeStyleDataFn[] = {
  // Note that these must line up _exactly_ with the numeric values of
  // the nsStyleStructID enum.
  nsnull,
  &nsRuleNode::ComputeFontData,
  &nsRuleNode::ComputeColorData,
  &nsRuleNode::ComputeBackgroundData,
  &nsRuleNode::ComputeListData,
  &nsRuleNode::ComputePositionData,
  &nsRuleNode::ComputeTextData,
  &nsRuleNode::ComputeTextResetData,
  &nsRuleNode::ComputeDisplayData,
  &nsRuleNode::ComputeVisibilityData,
  &nsRuleNode::ComputeContentData,
  &nsRuleNode::ComputeQuotesData,
  &nsRuleNode::ComputeUIData,
  &nsRuleNode::ComputeUIResetData,
  &nsRuleNode::ComputeTableData,
  &nsRuleNode::ComputeTableBorderData,
  &nsRuleNode::ComputeMarginData,
  &nsRuleNode::ComputePaddingData,
  &nsRuleNode::ComputeBorderData,
  &nsRuleNode::ComputeOutlineData,
#ifdef INCLUDE_XUL
  &nsRuleNode::ComputeXULData,
#endif
  nsnull
};

const nsStyleStruct* 
nsRuleNode::ComputeFontData(nsStyleStruct* aStartStruct, const nsCSSStruct& aData, 
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

  const nsCSSFont& fontData = NS_STATIC_CAST(const nsCSSFont&, aData);
  nsStyleFont* font = nsnull;
  nsStyleFont* parentFont = font;
  PRBool inherited = aInherited;

  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    font = new (mPresContext) nsStyleFont(*NS_STATIC_CAST(nsStyleFont*, aStartStruct));
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
  if (eCSSUnit_String == fontData.mFamily.GetUnit()) {
    nsCOMPtr<nsIDeviceContext> dc;
    mPresContext->GetDeviceContext(getter_AddRefs(dc));
    if (dc) {
      nsAutoString  familyList;
      fontData.mFamily.GetStringValue(familyList);
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
  else if (eCSSUnit_Enumerated == fontData.mFamily.GetUnit()) {
    nsSystemAttrID sysID;
    switch (fontData.mFamily.GetIntValue()) {
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

    nsCompatibility mode = eCompatibility_Standard;

    if (sysID == eSystemAttr_Font_Field ||
        sysID == eSystemAttr_Font_List ||
        sysID == eSystemAttr_Font_Button) {
      nsCOMPtr<nsIPref> prefService(do_GetService(NS_PREF_CONTRACTID));
      if (prefService) {
        PRBool useEitherMode;
        if (NS_SUCCEEDED(prefService->GetBoolPref("layout.forms.use_standard_or_quirks", &useEitherMode))) {
          if (useEitherMode) {
            mPresContext->GetCompatibilityMode(&mode);
          }
        }
      }
    }

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

        default:
          NS_ERROR("unexpected SID");
      }
    }
#endif
  }
  else if (eCSSUnit_Inherit == fontData.mFamily.GetUnit()) {
    if (parentContext && !inherited) {
      inherited = PR_TRUE;
      parentFont = (nsStyleFont*)parentContext->GetStyleData(eStyleStruct_Font);
    }

    font->mFont.name = parentFont->mFont.name;
    font->mFixedFont.name = parentFont->mFixedFont.name;
    font->mFlags &= ~(NS_STYLE_FONT_FACE_EXPLICIT | NS_STYLE_FONT_USE_FIXED);
    font->mFlags |= (parentFont->mFlags & (NS_STYLE_FONT_FACE_EXPLICIT | NS_STYLE_FONT_USE_FIXED));
  }
  else if (eCSSUnit_Initial == fontData.mFamily.GetUnit()) {
    font->mFont.name = defaultFont.name;
    font->mFixedFont.name = defaultFixedFont.name;
  }

  // font-style: enum, normal, inherit
  if (eCSSUnit_Enumerated == fontData.mStyle.GetUnit()) {
    font->mFont.style = fontData.mStyle.GetIntValue();
    font->mFixedFont.style = fontData.mStyle.GetIntValue();
  }
  else if (eCSSUnit_Normal == fontData.mStyle.GetUnit()) {
    font->mFont.style = NS_STYLE_FONT_STYLE_NORMAL;
    font->mFixedFont.style = NS_STYLE_FONT_STYLE_NORMAL;
  }
  else if (eCSSUnit_Inherit == fontData.mStyle.GetUnit()) {
    if (parentContext && !inherited) {
      inherited = PR_TRUE;
      parentFont = (nsStyleFont*)parentContext->GetStyleData(eStyleStruct_Font);
    }

    font->mFont.style = parentFont->mFont.style;
    font->mFixedFont.style = parentFont->mFixedFont.style;
  }
  else if (eCSSUnit_Initial == fontData.mStyle.GetUnit()) {
    font->mFont.style = defaultFont.style;
    font->mFixedFont.style = defaultFixedFont.style;
  }

  // font-variant: enum, normal, inherit
  if (eCSSUnit_Enumerated == fontData.mVariant.GetUnit()) {
    font->mFont.variant = fontData.mVariant.GetIntValue();
    font->mFixedFont.variant = fontData.mVariant.GetIntValue();
  }
  else if (eCSSUnit_Normal == fontData.mVariant.GetUnit()) {
    font->mFont.variant = NS_STYLE_FONT_VARIANT_NORMAL;
    font->mFixedFont.variant = NS_STYLE_FONT_VARIANT_NORMAL;
  }
  else if (eCSSUnit_Inherit == fontData.mVariant.GetUnit()) {
    if (parentContext && !inherited) {
      inherited = PR_TRUE;
      parentFont = (nsStyleFont*)parentContext->GetStyleData(eStyleStruct_Font);
    }

    font->mFont.variant = parentFont->mFont.variant;
    font->mFixedFont.variant = parentFont->mFixedFont.variant;
  }
  else if (eCSSUnit_Initial == fontData.mVariant.GetUnit()) {
    font->mFont.variant = defaultFont.variant;
    font->mFixedFont.variant = defaultFixedFont.variant;
  }

  // font-weight: int, enum, normal, inherit
  if (eCSSUnit_Integer == fontData.mWeight.GetUnit()) {
    font->mFont.weight = fontData.mWeight.GetIntValue();
    font->mFixedFont.weight = fontData.mWeight.GetIntValue();
  }
  else if (eCSSUnit_Enumerated == fontData.mWeight.GetUnit()) {
    PRInt32 value = fontData.mWeight.GetIntValue();
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
  else if (eCSSUnit_Normal == fontData.mWeight.GetUnit()) {
    font->mFont.weight = NS_STYLE_FONT_WEIGHT_NORMAL;
    font->mFixedFont.weight = NS_STYLE_FONT_WEIGHT_NORMAL;
  }
  else if (eCSSUnit_Inherit == fontData.mWeight.GetUnit()) {
    if (parentContext && !inherited) {
      inherited = PR_TRUE;
      parentFont = (nsStyleFont*)parentContext->GetStyleData(eStyleStruct_Font);
    }

    font->mFont.weight = parentFont->mFont.weight;
    font->mFixedFont.weight = parentFont->mFixedFont.weight;
  }
  else if (eCSSUnit_Initial == fontData.mWeight.GetUnit()) {
    font->mFont.weight = defaultFont.weight;
    font->mFixedFont.weight = defaultFixedFont.weight;
  }
  

  // font-size: enum, length, percent, inherit
  if (eCSSUnit_Enumerated == fontData.mSize.GetUnit()) {
    PRInt32 value = fontData.mSize.GetIntValue();
    PRInt32 scaler;
    mPresContext->GetFontScaler(&scaler);
    float scaleFactor = nsStyleUtil::GetScalingFactor(scaler);

    if (parentContext && !inherited) {
      inherited = PR_TRUE;
      parentFont = (nsStyleFont*)parentContext->GetStyleData(eStyleStruct_Font);
    }

    if ((NS_STYLE_FONT_SIZE_XXSMALL <= value) && 
        (value <= NS_STYLE_FONT_SIZE_XXLARGE)) {
      font->mFont.size = nsStyleUtil::CalcFontPointSize(value, (PRInt32)defaultFont.size, scaleFactor, mPresContext, eFontSize_CSS);
      font->mFixedFont.size = nsStyleUtil::CalcFontPointSize(value, (PRInt32)defaultFixedFont.size, scaleFactor, mPresContext, eFontSize_CSS);
    }
    else if (NS_STYLE_FONT_SIZE_XXXLARGE == value) {
      // <font size="7"> is not specified in CSS, so we don't use eFontSize_CSS.
      font->mFont.size = nsStyleUtil::CalcFontPointSize(value, (PRInt32)defaultFont.size, scaleFactor, mPresContext);
      font->mFixedFont.size = nsStyleUtil::CalcFontPointSize(value, (PRInt32)defaultFixedFont.size, scaleFactor, mPresContext);
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
  else if (fontData.mSize.IsLengthUnit()) {
    if (parentContext && !inherited)
      parentFont = (nsStyleFont*)parentContext->GetStyleData(eStyleStruct_Font);

    font->mFont.size = CalcLength(fontData.mSize, &parentFont->mFont, nsnull, mPresContext, inherited);
    font->mFixedFont.size = CalcLength(fontData.mSize, &parentFont->mFixedFont, nsnull, mPresContext, inherited);
    font->mFlags |= NS_STYLE_FONT_SIZE_EXPLICIT;
  }
  else if (eCSSUnit_Percent == fontData.mSize.GetUnit()) {
    if (parentContext && !inherited) {
      inherited = PR_TRUE;
      parentFont = (nsStyleFont*)parentContext->GetStyleData(eStyleStruct_Font);
    }

    font->mFont.size = (nscoord)((float)(parentFont->mFont.size) * fontData.mSize.GetPercentValue());
    font->mFixedFont.size = (nscoord)((float)(parentFont->mFixedFont.size) * fontData.mSize.GetPercentValue());
    font->mFlags |= NS_STYLE_FONT_SIZE_EXPLICIT;
  }
  else if (eCSSUnit_Inherit == fontData.mSize.GetUnit()) {
    if (parentContext && !inherited) {
      inherited = PR_TRUE;
      parentFont = (nsStyleFont*)parentContext->GetStyleData(eStyleStruct_Font);
    }

    font->mFont.size = parentFont->mFont.size;
    font->mFixedFont.size = parentFont->mFixedFont.size;
    font->mFlags &= ~NS_STYLE_FONT_SIZE_EXPLICIT;
    font->mFlags |= (parentFont->mFlags & NS_STYLE_FONT_SIZE_EXPLICIT);
  }
  else if (eCSSUnit_Initial == fontData.mSize.GetUnit()) {
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
nsRuleNode::ComputeTextData(nsStyleStruct* aStartStruct, const nsCSSStruct& aData, 
                            nsIStyleContext* aContext, 
                            nsRuleNode* aHighestNode,
                            const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW TEXT CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());

  const nsCSSText& textData = NS_STATIC_CAST(const nsCSSText&, aData);
  nsStyleText* text = nsnull;
  nsStyleText* parentText = text;
  PRBool inherited = aInherited;

  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    text = new (mPresContext) nsStyleText(*NS_STATIC_CAST(nsStyleText*, aStartStruct));
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
  SetCoord(textData.mLetterSpacing, text->mLetterSpacing, parentText->mLetterSpacing,
           SETCOORD_LH | SETCOORD_NORMAL, aContext, mPresContext, inherited);

  // line-height: normal, number, length, percent, inherit
  SetCoord(textData.mLineHeight, text->mLineHeight, parentText->mLineHeight,
           SETCOORD_LPFHN, aContext, mPresContext, inherited);

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
nsRuleNode::ComputeTextResetData(nsStyleStruct* aStartData, const nsCSSStruct& aData, 
                                 nsIStyleContext* aContext, 
                                 nsRuleNode* aHighestNode,
                                 const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW TEXTRESET CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());

  const nsCSSText& textData = NS_STATIC_CAST(const nsCSSText&, aData);
  nsStyleTextReset* text;
  if (aStartData)
    // We only need to compute the delta between this computed data and our
    // computed data.
    text = new (mPresContext) nsStyleTextReset(*NS_STATIC_CAST(nsStyleTextReset*, aStartData));
  else
    text = new (mPresContext) nsStyleTextReset();
  nsStyleTextReset* parentText = text;

  if (parentContext)
    parentText = (nsStyleTextReset*)parentContext->GetStyleData(eStyleStruct_TextReset);
  PRBool inherited = aInherited;
  
  // vertical-align: enum, length, percent, inherit
  SetCoord(textData.mVerticalAlign, text->mVerticalAlign, parentText->mVerticalAlign,
           SETCOORD_LPH | SETCOORD_ENUMERATED, aContext, mPresContext, inherited);

  // text-decoration: none, enum (bit field), inherit
  if (eCSSUnit_Enumerated == textData.mDecoration.GetUnit()) {
    PRInt32 td = textData.mDecoration.GetIntValue();
    text->mTextDecoration = td;
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
nsRuleNode::ComputeUIData(nsStyleStruct* aStartData, const nsCSSStruct& aData, 
                          nsIStyleContext* aContext, 
                          nsRuleNode* aHighestNode,
                          const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW UI CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  const nsCSSUserInterface& uiData = NS_STATIC_CAST(const nsCSSUserInterface&, aData);
  nsStyleUserInterface* ui = nsnull;
  nsStyleUserInterface* parentUI = ui;
  PRBool inherited = aInherited;

  if (aStartData)
    // We only need to compute the delta between this computed data and our
    // computed data.
    ui = new (mPresContext) nsStyleUserInterface(*NS_STATIC_CAST(nsStyleUserInterface*, aStartData));
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
nsRuleNode::ComputeUIResetData(nsStyleStruct* aStartData, const nsCSSStruct& aData, 
                               nsIStyleContext* aContext, 
                               nsRuleNode* aHighestNode,
                               const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW UIRESET CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  const nsCSSUserInterface& uiData = NS_STATIC_CAST(const nsCSSUserInterface&, aData);
  nsStyleUIReset* ui;
  if (aStartData)
    // We only need to compute the delta between this computed data and our
    // computed data.
    ui = new (mPresContext) nsStyleUIReset(*NS_STATIC_CAST(nsStyleUIReset*, aStartData));
  else
    ui = new (mPresContext) nsStyleUIReset();
  nsStyleUIReset* parentUI = ui;

  if (parentContext)
    parentUI = (nsStyleUIReset*)parentContext->GetStyleData(eStyleStruct_UIReset);
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
nsRuleNode::ComputeDisplayData(nsStyleStruct* aStartStruct, const nsCSSStruct& aData, 
                               nsIStyleContext* aContext, 
                               nsRuleNode* aHighestNode,
                               const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW DISPLAY CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  const nsCSSDisplay& displayData = NS_STATIC_CAST(const nsCSSDisplay&, aData);
  nsStyleDisplay* display;
  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    display = new (mPresContext) nsStyleDisplay(*NS_STATIC_CAST(nsStyleDisplay*, aStartStruct));
  else
    display = new (mPresContext) nsStyleDisplay();
  nsStyleDisplay* parentDisplay = display;

  if (parentContext)
    parentDisplay = (nsStyleDisplay*)parentContext->GetStyleData(eStyleStruct_Display);
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
    if (display->mPosition != NS_STYLE_POSITION_NORMAL) {
      // :before and :after elts cannot be positioned.  We need to check for this
      // case.
      nsCOMPtr<nsIAtom> tag;
      aContext->GetPseudoType(*getter_AddRefs(tag));
      if (tag && tag.get() == nsCSSAtoms::beforePseudo || tag.get() == nsCSSAtoms::afterPseudo)
        display->mPosition = NS_STYLE_POSITION_NORMAL;
    }
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
    display->mOverflow = parentDisplay->mOverflow;
  }

  // clip property: length, auto, inherit
  if (nsnull != displayData.mClip) {
    if (eCSSUnit_Inherit == displayData.mClip->mTop.GetUnit()) { // if one is inherit, they all are
      inherited = PR_TRUE;
      display->mClipFlags = NS_STYLE_CLIP_INHERIT;
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
nsRuleNode::ComputeVisibilityData(nsStyleStruct* aStartStruct, const nsCSSStruct& aData, 
                                  nsIStyleContext* aContext, 
                                  nsRuleNode* aHighestNode,
                                  const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW VISIBILITY CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  const nsCSSDisplay& displayData = NS_STATIC_CAST(const nsCSSDisplay&, aData);
  nsStyleVisibility* visibility = nsnull;
  nsStyleVisibility* parentVisibility = visibility;
  PRBool inherited = aInherited;

  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    visibility = new (mPresContext) nsStyleVisibility(*NS_STATIC_CAST(nsStyleVisibility*, aStartStruct));
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
nsRuleNode::ComputeColorData(nsStyleStruct* aStartStruct, const nsCSSStruct& aData, 
                             nsIStyleContext* aContext, 
                             nsRuleNode* aHighestNode,
                             const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW COLOR CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  const nsCSSColor& colorData = NS_STATIC_CAST(const nsCSSColor&, aData);
  nsStyleColor* color = nsnull;
  nsStyleColor* parentColor = color;
  PRBool inherited = aInherited;

  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    color = new (mPresContext) nsStyleColor(*NS_STATIC_CAST(nsStyleColor*, aStartStruct));
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
  SetColor(colorData.mColor, parentColor->mColor, mPresContext, color->mColor, inherited);

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
nsRuleNode::ComputeBackgroundData(nsStyleStruct* aStartStruct, const nsCSSStruct& aData, 
                                  nsIStyleContext* aContext, 
                                  nsRuleNode* aHighestNode,
                                  const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW BACKGROUND CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  const nsCSSColor& colorData = NS_STATIC_CAST(const nsCSSColor&, aData);
  nsStyleBackground* bg;
  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    bg = new (mPresContext) nsStyleBackground(*NS_STATIC_CAST(nsStyleBackground*, aStartStruct));
  else
    bg = new (mPresContext) nsStyleBackground(mPresContext);
  nsStyleBackground* parentBG = bg;

  if (parentContext)
    parentBG = (nsStyleBackground*)parentContext->GetStyleData(eStyleStruct_Background);
  PRBool inherited = aInherited;

  // background-color: color, string, enum (flags), inherit
  if (eCSSUnit_Inherit == colorData.mBackColor.GetUnit()) { // do inherit first, so SetColor doesn't do it
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
    bg->mBackgroundFlags |= (parentBG->mBackgroundFlags & NS_STYLE_BG_IMAGE_NONE);
  }

  // background-repeat: enum, inherit
  if (eCSSUnit_Enumerated == colorData.mBackRepeat.GetUnit()) {
    bg->mBackgroundRepeat = colorData.mBackRepeat.GetIntValue();
  }
  else if (eCSSUnit_Inherit == colorData.mBackRepeat.GetUnit()) {
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
    bg->mBackgroundFlags |= (parentBG->mBackgroundFlags & (NS_STYLE_BG_X_POSITION_LENGTH | NS_STYLE_BG_X_POSITION_PERCENT));
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
nsRuleNode::ComputeMarginData(nsStyleStruct* aStartStruct, const nsCSSStruct& aData, 
                              nsIStyleContext* aContext, 
                              nsRuleNode* aHighestNode,
                              const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW MARGIN CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  const nsCSSMargin& marginData = NS_STATIC_CAST(const nsCSSMargin&, aData);
  nsStyleMargin* margin;
  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    margin = new (mPresContext) nsStyleMargin(*NS_STATIC_CAST(nsStyleMargin*, aStartStruct));
  else
    margin = new (mPresContext) nsStyleMargin();
  nsStyleMargin* parentMargin = margin;

  if (parentContext)
    parentMargin = (nsStyleMargin*)parentContext->GetStyleData(eStyleStruct_Margin);
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
nsRuleNode::ComputeBorderData(nsStyleStruct* aStartStruct, const nsCSSStruct& aData, 
                              nsIStyleContext* aContext, 
                              nsRuleNode* aHighestNode,
                              const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW BORDER CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  const nsCSSMargin& marginData = NS_STATIC_CAST(const nsCSSMargin&, aData);
  nsStyleBorder* border;
  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    border = new (mPresContext) nsStyleBorder(*NS_STATIC_CAST(nsStyleBorder*, aStartStruct));
  else
    border = new (mPresContext) nsStyleBorder(mPresContext);
  
  nsStyleBorder* parentBorder = border;
  if (parentContext)
    parentBorder = (nsStyleBorder*)parentContext->GetStyleData(eStyleStruct_Border);
  PRBool inherited = aInherited;

  // border-size: length, enum, inherit
  if (marginData.mBorderWidth) {
    nsStyleCoord  coord;
    nsStyleCoord  parentCoord;
    if (SetCoord(marginData.mBorderWidth->mLeft, coord, parentCoord, SETCOORD_LE, aContext, mPresContext, inherited))
      border->mBorder.SetLeft(coord);
    else if (eCSSUnit_Inherit == marginData.mBorderWidth->mLeft.GetUnit())
      border->mBorder.SetLeft(parentBorder->mBorder.GetLeft(coord));

    if (SetCoord(marginData.mBorderWidth->mTop, coord, parentCoord, SETCOORD_LE, aContext, mPresContext, inherited))
      border->mBorder.SetTop(coord);
    else if (eCSSUnit_Inherit == marginData.mBorderWidth->mTop.GetUnit())
      border->mBorder.SetTop(parentBorder->mBorder.GetTop(coord));

    if (SetCoord(marginData.mBorderWidth->mRight, coord, parentCoord, SETCOORD_LE, aContext, mPresContext, inherited))
      border->mBorder.SetRight(coord);
    else if (eCSSUnit_Inherit == marginData.mBorderWidth->mRight.GetUnit())
      border->mBorder.SetRight(parentBorder->mBorder.GetRight(coord));

    if (SetCoord(marginData.mBorderWidth->mBottom, coord, parentCoord, SETCOORD_LE, aContext, mPresContext, inherited))
      border->mBorder.SetBottom(coord);
    else if (eCSSUnit_Inherit == marginData.mBorderWidth->mBottom.GetUnit())
      border->mBorder.SetBottom(parentBorder->mBorder.GetBottom(coord));
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
nsRuleNode::ComputePaddingData(nsStyleStruct* aStartStruct, const nsCSSStruct& aData, 
                               nsIStyleContext* aContext, 
                               nsRuleNode* aHighestNode,
                               const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW PADDING CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  const nsCSSMargin& marginData = NS_STATIC_CAST(const nsCSSMargin&, aData);
  nsStylePadding* padding;
  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    padding = new (mPresContext) nsStylePadding(*NS_STATIC_CAST(nsStylePadding*, aStartStruct));
  else
    padding = new (mPresContext) nsStylePadding();
  
  nsStylePadding* parentPadding = padding;
  if (parentContext)
    parentPadding = (nsStylePadding*)parentContext->GetStyleData(eStyleStruct_Padding);
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
nsRuleNode::ComputeOutlineData(nsStyleStruct* aStartStruct, const nsCSSStruct& aData, 
                               nsIStyleContext* aContext, 
                               nsRuleNode* aHighestNode,
                               const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW OUTLINE CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  const nsCSSMargin& marginData = NS_STATIC_CAST(const nsCSSMargin&, aData);
  nsStyleOutline* outline;
  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    outline = new (mPresContext) nsStyleOutline(*NS_STATIC_CAST(nsStyleOutline*, aStartStruct));
  else
    outline = new (mPresContext) nsStyleOutline(mPresContext);
  
  nsStyleOutline* parentOutline = outline;
  if (parentContext)
    parentOutline = (nsStyleOutline*)parentContext->GetStyleData(eStyleStruct_Outline);
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
nsRuleNode::ComputeListData(nsStyleStruct* aStartStruct, const nsCSSStruct& aData, 
                            nsIStyleContext* aContext, 
                            nsRuleNode* aHighestNode,
                            const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW LIST CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  const nsCSSList& listData = NS_STATIC_CAST(const nsCSSList&, aData);
  nsStyleList* list = nsnull;
  nsStyleList* parentList = list;
  PRBool inherited = aInherited;

  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    list = new (mPresContext) nsStyleList(*NS_STATIC_CAST(nsStyleList*, aStartStruct));
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
nsRuleNode::ComputePositionData(nsStyleStruct* aStartStruct, const nsCSSStruct& aData, 
                                nsIStyleContext* aContext, 
                                nsRuleNode* aHighestNode,
                                const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW POSITION CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  const nsCSSPosition& posData = NS_STATIC_CAST(const nsCSSPosition&, aData);
  nsStylePosition* pos;
  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    pos = new (mPresContext) nsStylePosition(*NS_STATIC_CAST(nsStylePosition*, aStartStruct));
  else
    pos = new (mPresContext) nsStylePosition();
  
  nsStylePosition* parentPos = pos;
  if (parentContext)
    parentPos = (nsStylePosition*)parentContext->GetStyleData(eStyleStruct_Position);
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
nsRuleNode::ComputeTableData(nsStyleStruct* aStartStruct, const nsCSSStruct& aData, 
                             nsIStyleContext* aContext, 
                             nsRuleNode* aHighestNode,
                             const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW TABLE CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  const nsCSSTable& tableData = NS_STATIC_CAST(const nsCSSTable&, aData);
  nsStyleTable* table;
  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    table = new (mPresContext) nsStyleTable(*NS_STATIC_CAST(nsStyleTable*, aStartStruct));
  else
    table = new (mPresContext) nsStyleTable();
  
  nsStyleTable* parentTable = table;
  if (parentContext)
    parentTable = (nsStyleTable*)parentContext->GetStyleData(eStyleStruct_Table);
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
nsRuleNode::ComputeTableBorderData(nsStyleStruct* aStartStruct, const nsCSSStruct& aData, 
                                   nsIStyleContext* aContext, 
                                   nsRuleNode* aHighestNode,
                                   const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW TABLE BORDER CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  const nsCSSTable& tableData = NS_STATIC_CAST(const nsCSSTable&, aData);
  nsStyleTableBorder* table = nsnull;
  nsStyleTableBorder* parentTable = table;
  PRBool inherited = aInherited;

  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    table = new (mPresContext) nsStyleTableBorder(*NS_STATIC_CAST(nsStyleTableBorder*, aStartStruct));
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
nsRuleNode::ComputeContentData(nsStyleStruct* aStartStruct, const nsCSSStruct& aData, 
                               nsIStyleContext* aContext, 
                               nsRuleNode* aHighestNode,
                               const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW CONTENT CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  const nsCSSContent& contentData = NS_STATIC_CAST(const nsCSSContent&, aData);
  nsStyleContent* content;
  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    content = new (mPresContext) nsStyleContent(*NS_STATIC_CAST(nsStyleContent*, aStartStruct));
  else
    content = new (mPresContext) nsStyleContent();
  
  nsStyleContent* parentContent = content;
  if (parentContext)
    parentContent = (nsStyleContent*)parentContext->GetStyleData(eStyleStruct_Content);
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
nsRuleNode::ComputeQuotesData(nsStyleStruct* aStartStruct, const nsCSSStruct& aData, 
                                   nsIStyleContext* aContext, 
                                   nsRuleNode* aHighestNode,
                                   const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW QUOTES CREATED!\n");
#endif

  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  const nsCSSContent& contentData = NS_STATIC_CAST(const nsCSSContent&, aData);
  nsStyleQuotes* quotes = nsnull;
  nsStyleQuotes* parentQuotes = quotes;
  PRBool inherited = aInherited;

  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    quotes = new (mPresContext) nsStyleQuotes(*NS_STATIC_CAST(nsStyleQuotes*, aStartStruct));
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
nsRuleNode::ComputeXULData(nsStyleStruct* aStartStruct, const nsCSSStruct& aData, 
                           nsIStyleContext* aContext, 
                           nsRuleNode* aHighestNode,
                           const RuleDetail& aRuleDetail, PRBool aInherited)
{
#ifdef DEBUG_hyatt
  printf("NEW XUL CREATED!\n");
#endif
  nsCOMPtr<nsIStyleContext> parentContext = getter_AddRefs(aContext->GetParent());
  
  const nsCSSXUL& xulData = NS_STATIC_CAST(const nsCSSXUL&, aData);
  nsStyleXUL* xul = nsnull;
  
  if (aStartStruct)
    // We only need to compute the delta between this computed data and our
    // computed data.
    xul = new (mPresContext) nsStyleXUL(*NS_STATIC_CAST(nsStyleXUL*, aStartStruct));
  else
    xul = new (mPresContext) nsStyleXUL();

  nsStyleXUL* parentXUL = xul;

  if (parentContext)
    parentXUL = (nsStyleXUL*)parentContext->GetStyleData(eStyleStruct_XUL);

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
  // Note that these must line up _exactly_ with the numeric values of
  // the nsStyleStructID enum.
  nsnull,
  &nsRuleNode::GetFontData,
  &nsRuleNode::GetColorData,
  &nsRuleNode::GetBackgroundData,
  &nsRuleNode::GetListData,
  &nsRuleNode::GetPositionData,
  &nsRuleNode::GetTextData,
  &nsRuleNode::GetTextResetData,
  &nsRuleNode::GetDisplayData,
  &nsRuleNode::GetVisibilityData,
  &nsRuleNode::GetContentData,
  &nsRuleNode::GetQuotesData,
  &nsRuleNode::GetUIData,
  &nsRuleNode::GetUIResetData,
  &nsRuleNode::GetTableData,
  &nsRuleNode::GetTableBorderData,
  &nsRuleNode::GetMarginData,
  &nsRuleNode::GetPaddingData,
  &nsRuleNode::GetBorderData,
  &nsRuleNode::GetOutlineData,
#ifdef INCLUDE_XUL
  &nsRuleNode::GetXULData,
#endif
  nsnull
};

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

  // Nothing is cached.  We'll have to delve further and examine our rules.
  GetStyleDataFn fn = gGetStyleDataFn[aSID];
  return fn ? (this->*fn)(aContext) : nsnull;
}

