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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Hyatt <hyatt@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
 
#include "nsStyleConsts.h"
#include "nsString.h"
#include "nsUnitConversion.h"
#include "nsIPresContext.h"
#include "nsIStyleRule.h"
#include "nsISupportsArray.h"
#include "nsCRT.h"

#include "nsCOMPtr.h"
#include "nsIStyleSet.h"
#include "nsISizeOfHandler.h"
#include "nsIPresShell.h"
#include "nsLayoutAtoms.h"
#include "prenv.h"

#include "nsRuleNode.h"

#ifdef IBMBIDI
#include "nsIUBidiUtils.h"
#endif

#ifdef DEBUG
// #define NOISY_DEBUG
#endif

//----------------------------------------------------------------------

class nsStyleContext : public nsIStyleContext 
{
public:
  nsStyleContext(nsIStyleContext* aParent, nsIAtom* aPseudoTag, 
                   nsRuleNode* aRuleNode, 
                   nsIPresContext* aPresContext);
  virtual ~nsStyleContext();

  void* operator new(size_t sz, nsIPresContext* aPresContext);
  void Destroy();

  NS_DECL_ISUPPORTS

  virtual nsIStyleContext*  GetParent(void) const;
  NS_IMETHOD GetFirstChild(nsIStyleContext** aContext);

  NS_IMETHOD GetPseudoType(nsIAtom*& aPseudoTag) const;

  NS_IMETHOD FindChildWithRules(const nsIAtom* aPseudoTag, nsRuleNode* aRules,
                                nsIStyleContext*& aResult);

  virtual PRBool    Equals(const nsIStyleContext* aOther) const;
  virtual PRBool    HasTextDecorations() { return mBits & NS_STYLE_HAS_TEXT_DECORATIONS; };

  NS_IMETHOD GetBorderPaddingFor(nsStyleBorderPadding& aBorderPadding);

  NS_IMETHOD GetStyle(nsStyleStructID aSID, const nsStyleStruct** aStruct);
  NS_IMETHOD SetStyle(nsStyleStructID aSID, const nsStyleStruct& aStruct);

  NS_IMETHOD GetRuleNode(nsRuleNode** aResult) { *aResult = mRuleNode; return NS_OK; };
  NS_IMETHOD AddStyleBit(const PRUint32& aBit) { mBits |= aBit; return NS_OK; };
  NS_IMETHOD GetStyleBits(PRUint32* aBits) { *aBits = mBits; return NS_OK; };

  virtual const nsStyleStruct* GetStyleData(nsStyleStructID aSID);
  virtual nsStyleStruct* GetUniqueStyleData(nsIPresContext* aPresContext, const nsStyleStructID& aSID);

  virtual nsresult ClearCachedDataForRule(nsIStyleRule* aRule);

  virtual nsresult ClearStyleData(nsIPresContext* aPresContext, nsIStyleRule* aRule);

  virtual void ForceUnique(void);
  NS_IMETHOD  CalcStyleDifference(nsIStyleContext* aOther, PRInt32& aHint,PRBool aStopAtFirstDifference = PR_FALSE);

#ifdef DEBUG
  virtual void DumpRegressionData(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent);

  virtual void List(FILE* out, PRInt32 aIndent);

  virtual void SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize);
#endif

protected:
  void AppendChild(nsStyleContext* aChild);
  void RemoveChild(nsStyleContext* aChild);

  void ApplyStyleFixups(nsIPresContext* aPresContext);

  nsStyleContext* mParent;
  nsStyleContext* mChild;
  nsStyleContext* mEmptyChild;
  nsStyleContext* mPrevSibling;
  nsStyleContext* mNextSibling;

  nsIAtom*          mPseudoTag;

  PRUint32                mBits; // Which structs are inherited from the parent context.
  nsRuleNode*             mRuleNode;
  nsCachedStyleData       mCachedStyleData; // Our cached style data.
};

static PRInt32 gLastDataCode;

nsStyleContext::nsStyleContext(nsIStyleContext* aParent,
                                   nsIAtom* aPseudoTag,
                                   nsRuleNode* aRuleNode,
                                   nsIPresContext* aPresContext)
  : mParent((nsStyleContext*)aParent),
    mChild(nsnull),
    mEmptyChild(nsnull),
    mPseudoTag(aPseudoTag),
    mBits(0),
    mRuleNode(aRuleNode)
{
  NS_INIT_REFCNT();
  NS_IF_ADDREF(mPseudoTag);
  
  mNextSibling = this;
  mPrevSibling = this;
  if (nsnull != mParent) {
    NS_ADDREF(mParent);
    mParent->AppendChild(this);
  }

  ApplyStyleFixups(aPresContext);
}

nsStyleContext::~nsStyleContext()
{
  NS_ASSERTION((nsnull == mChild) && (nsnull == mEmptyChild), "destructing context with children");

  if (mParent) {
    mParent->RemoveChild(this);
    NS_RELEASE(mParent);
  }

  NS_IF_RELEASE(mPseudoTag);
  
  // Free up our data structs.
  if (mCachedStyleData.mResetData || mCachedStyleData.mInheritedData) {
    nsCOMPtr<nsIPresContext> presContext;
    mRuleNode->GetPresContext(getter_AddRefs(presContext));
    mCachedStyleData.Destroy(mBits, presContext);
  }
}

NS_IMPL_ADDREF(nsStyleContext)
NS_IMPL_RELEASE_WITH_DESTROY(nsStyleContext, Destroy())

NS_IMETHODIMP
nsStyleContext::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(nsnull != aInstancePtr, "null pointer");
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  if (aIID.Equals(NS_GET_IID(nsIStyleContext))) {
    *aInstancePtr = (void*)(nsIStyleContext*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) (nsISupports*)(nsIStyleContext*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsIStyleContext* nsStyleContext::GetParent(void) const
{
  NS_IF_ADDREF(mParent);
  return mParent;
}

NS_IMETHODIMP
nsStyleContext::GetFirstChild(nsIStyleContext** aContext)
{
  *aContext = mChild;
  NS_IF_ADDREF(*aContext);
  return NS_OK;
}

void nsStyleContext::AppendChild(nsStyleContext* aChild)
{
  nsRuleNode* ruleNode;
  aChild->GetRuleNode(&ruleNode);
  
  if (ruleNode->IsRoot()) {
    // We matched no rules.
    if (nsnull == mEmptyChild) {
      mEmptyChild = aChild;
    }
    else {
      aChild->mNextSibling = mEmptyChild;
      aChild->mPrevSibling = mEmptyChild->mPrevSibling;
      mEmptyChild->mPrevSibling->mNextSibling = aChild;
      mEmptyChild->mPrevSibling = aChild;
    }
  }
  else {
    if (nsnull == mChild) {
      mChild = aChild;
    }
    else {
      aChild->mNextSibling = mChild;
      aChild->mPrevSibling = mChild->mPrevSibling;
      mChild->mPrevSibling->mNextSibling = aChild;
      mChild->mPrevSibling = aChild;
    }
  }
}

void nsStyleContext::RemoveChild(nsStyleContext* aChild)
{
  NS_ASSERTION((nsnull != aChild) && (this == aChild->mParent), "bad argument");

  if ((nsnull == aChild) || (this != aChild->mParent)) {
    return;
  }

  nsRuleNode* ruleNode;
  aChild->GetRuleNode(&ruleNode);
  
  if (ruleNode->IsRoot()) { // is empty 
    if (aChild->mPrevSibling != aChild) { // has siblings
      if (mEmptyChild == aChild) {
        mEmptyChild = mEmptyChild->mNextSibling;
      }
    } 
    else {
      NS_ASSERTION(mEmptyChild == aChild, "bad sibling pointers");
      mEmptyChild = nsnull;
    }
  }
  else {  // isn't empty
    if (aChild->mPrevSibling != aChild) { // has siblings
      if (mChild == aChild) {
        mChild = mChild->mNextSibling;
      }
    }
    else {
      NS_ASSERTION(mChild == aChild, "bad sibling pointers");
      if (mChild == aChild) {
        mChild = nsnull;
      }
    }
  }
  aChild->mPrevSibling->mNextSibling = aChild->mNextSibling;
  aChild->mNextSibling->mPrevSibling = aChild->mPrevSibling;
  aChild->mNextSibling = aChild;
  aChild->mPrevSibling = aChild;
}

NS_IMETHODIMP
nsStyleContext::GetPseudoType(nsIAtom*& aPseudoTag) const
{
  aPseudoTag = mPseudoTag;
  NS_IF_ADDREF(aPseudoTag);
  return NS_OK;
}

NS_IMETHODIMP
nsStyleContext::FindChildWithRules(const nsIAtom* aPseudoTag, 
                                     nsRuleNode* aRuleNode,
                                     nsIStyleContext*& aResult)
{
  PRUint32 threshold = 10; // The # of siblings we're willing to examine
                           // before just giving this whole thing up.

  aResult = nsnull;

  if ((nsnull != mChild) || (nsnull != mEmptyChild)) {
    nsStyleContext* child;
    if (aRuleNode->IsRoot()) {
      if (nsnull != mEmptyChild) {
        child = mEmptyChild;
        do {
          if ((!(child->mBits & NS_STYLE_UNIQUE_CONTEXT)) &&  // only look at children with un-twiddled data
              (aPseudoTag == child->mPseudoTag)) {
            aResult = child;
            break;
          }
          child = child->mNextSibling;
          threshold--;
          if (threshold == 0)
            break;
        } while (child != mEmptyChild);
      }
    }
    else if (nsnull != mChild) {
      child = mChild;
      
      do {
        if ((!(child->mBits & NS_STYLE_UNIQUE_CONTEXT)) &&  // only look at children with un-twiddled data
            (child->mRuleNode == aRuleNode) &&
            (child->mPseudoTag == aPseudoTag)) {
          aResult = child;
          break;
        }
        child = child->mNextSibling;
        threshold--;
        if (threshold == 0)
          break;
      } while (child != mChild);
    }
  }
  NS_IF_ADDREF(aResult);
  return NS_OK;
}


PRBool nsStyleContext::Equals(const nsIStyleContext* aOther) const
{
  PRBool  result = PR_TRUE;
  const nsStyleContext* other = (nsStyleContext*)aOther;

  if (other != this) {
    if (mParent != other->mParent) {
      result = PR_FALSE;
    }
    else if (mBits != other->mBits) {
      result = PR_FALSE;
    }
    else if (mPseudoTag != other->mPseudoTag) {
      result = PR_FALSE;
    }
    else if (mRuleNode != other->mRuleNode) {
      result = PR_FALSE;
    }
  }
  return result;
}

//=========================================================================================================

const nsStyleStruct* nsStyleContext::GetStyleData(nsStyleStructID aSID)
{
  const nsStyleStruct* cachedData = mCachedStyleData.GetStyleData(aSID); 
  if (cachedData)
    return cachedData; // We have computed data stored on this node in the context tree.
  return mRuleNode->GetStyleData(aSID, this); // Our rule node will take care of it for us.
}

NS_IMETHODIMP
nsStyleContext::GetBorderPaddingFor(nsStyleBorderPadding& aBorderPadding)
{
  nsMargin border, padding;
  const nsStyleBorder* borderData = (const nsStyleBorder*)GetStyleData(eStyleStruct_Border);
  const nsStylePadding* paddingData = (const nsStylePadding*)GetStyleData(eStyleStruct_Padding);
  if (borderData->GetBorder(border)) {
	  if (paddingData->GetPadding(padding)) {
	    border += padding;
	    aBorderPadding.SetBorderPadding(border);
	  }
  }

  return NS_OK;
}

// This is an evil evil function, since it forces you to alloc your own separate copy of
// style data!  Do not use this function unless you absolutely have to!  You should avoid
// this at all costs! -dwh
nsStyleStruct* 
nsStyleContext::GetUniqueStyleData(nsIPresContext* aPresContext, const nsStyleStructID& aSID)
{
  nsStyleStruct* result = nsnull;
  switch (aSID) {
  case eStyleStruct_Display: {
    const nsStyleDisplay* dis = (const nsStyleDisplay*)GetStyleData(aSID);
    nsStyleDisplay* newDis = new (aPresContext) nsStyleDisplay(*dis);
    SetStyle(aSID, *newDis);
    result = newDis;
    mBits &= ~NS_STYLE_INHERIT_DISPLAY;
    break;
  }
  case eStyleStruct_Background: {
    const nsStyleBackground* bg = (const nsStyleBackground*)GetStyleData(aSID);
    nsStyleBackground* newBG = new (aPresContext) nsStyleBackground(*bg);
    SetStyle(aSID, *newBG);
    result = newBG;
    mBits &= ~NS_STYLE_INHERIT_BACKGROUND;
    break;
  }
  case eStyleStruct_Text: {
    const nsStyleText* text = (const nsStyleText*)GetStyleData(aSID);
    nsStyleText* newText = new (aPresContext) nsStyleText(*text);
    SetStyle(aSID, *newText);
    result = newText;
    mBits &= ~NS_STYLE_INHERIT_TEXT;
    break;
  }
  default:
    NS_ERROR("Struct type not supported.  Please find another way to do this if you can!\n");
  }

  return result;
}

NS_IMETHODIMP
nsStyleContext::GetStyle(nsStyleStructID aSID, const nsStyleStruct** aStruct)
{
  *aStruct = GetStyleData(aSID);
  return NS_OK;
}

NS_IMETHODIMP
nsStyleContext::SetStyle(nsStyleStructID aSID, const nsStyleStruct& aStruct)
{
  // This method should only be called from nsRuleNode!  It is not a public
  // method!
  nsresult result = NS_OK;
  
  PRBool isReset = mCachedStyleData.IsReset(aSID);
  if (isReset && !mCachedStyleData.mResetData) {
    nsCOMPtr<nsIPresContext> presContext;
    mRuleNode->GetPresContext(getter_AddRefs(presContext));
    mCachedStyleData.mResetData = new (presContext.get()) nsResetStyleData;
  }
  else if (!isReset && !mCachedStyleData.mInheritedData) {
    nsCOMPtr<nsIPresContext> presContext;
    mRuleNode->GetPresContext(getter_AddRefs(presContext));
    mCachedStyleData.mInheritedData = new (presContext.get()) nsInheritedStyleData;
  }

  switch (aSID) {
    case eStyleStruct_Font:
      mCachedStyleData.mInheritedData->mFontData = (nsStyleFont*)(const nsStyleFont*)(&aStruct);
      break;
    case eStyleStruct_Color:
      mCachedStyleData.mInheritedData->mColorData = (nsStyleColor*)(const nsStyleColor*)(&aStruct);
      break;
    case eStyleStruct_Background:
      mCachedStyleData.mResetData->mBackgroundData = (nsStyleBackground*)(const nsStyleBackground*)(&aStruct);
      break;
    case eStyleStruct_List:
      mCachedStyleData.mInheritedData->mListData = (nsStyleList*)(const nsStyleList*)(&aStruct);
      break;
    case eStyleStruct_Position:
      mCachedStyleData.mResetData->mPositionData = (nsStylePosition*)(const nsStylePosition*)(&aStruct);
      break;
    case eStyleStruct_Text:
      mCachedStyleData.mInheritedData->mTextData = (nsStyleText*)(const nsStyleText*)(&aStruct);
      break;
    case eStyleStruct_TextReset:
      mCachedStyleData.mResetData->mTextData = (nsStyleTextReset*)(const nsStyleTextReset*)(&aStruct);
      break;
    case eStyleStruct_Display:
      mCachedStyleData.mResetData->mDisplayData = (nsStyleDisplay*)(const nsStyleDisplay*)(&aStruct);
      break;
    case eStyleStruct_Visibility:
      mCachedStyleData.mInheritedData->mVisibilityData = (nsStyleVisibility*)(const nsStyleVisibility*)(&aStruct);
      break;
    case eStyleStruct_Table:
      mCachedStyleData.mResetData->mTableData = (nsStyleTable*)(const nsStyleTable*)(&aStruct);
      break;
    case eStyleStruct_TableBorder:
      mCachedStyleData.mInheritedData->mTableData = (nsStyleTableBorder*)(const nsStyleTableBorder*)(&aStruct);
      break;
    case eStyleStruct_Content:
      mCachedStyleData.mResetData->mContentData = (nsStyleContent*)(const nsStyleContent*)(&aStruct);
      break;
    case eStyleStruct_Quotes:
      mCachedStyleData.mInheritedData->mQuotesData = (nsStyleQuotes*)(const nsStyleQuotes*)(&aStruct);
      break;
    case eStyleStruct_UserInterface:
      mCachedStyleData.mInheritedData->mUIData = (nsStyleUserInterface*)(const nsStyleUserInterface*)(&aStruct);
      break;
    case eStyleStruct_UIReset:
      mCachedStyleData.mResetData->mUIData = (nsStyleUIReset*)(const nsStyleUIReset*)(&aStruct);
      break;
    case eStyleStruct_Margin:
      mCachedStyleData.mResetData->mMarginData = (nsStyleMargin*)(const nsStyleMargin*)(&aStruct);
      break;
    case eStyleStruct_Padding:
      mCachedStyleData.mResetData->mPaddingData = (nsStylePadding*)(const nsStylePadding*)(&aStruct);
      break;
    case eStyleStruct_Border:
      mCachedStyleData.mResetData->mBorderData = (nsStyleBorder*)(const nsStyleBorder*)(&aStruct);
      break;
    case eStyleStruct_Outline:
      mCachedStyleData.mResetData->mOutlineData = (nsStyleOutline*)(const nsStyleOutline*)(&aStruct);
      break;
#ifdef INCLUDE_XUL
    case eStyleStruct_XUL:
      mCachedStyleData.mResetData->mXULData = (nsStyleXUL*)(const nsStyleXUL*)(&aStruct);
      break;
#endif
    default:
      NS_ERROR("Invalid style struct id");
      result = NS_ERROR_INVALID_ARG;
      break;
  }
  return result;
}

void
nsStyleContext::ApplyStyleFixups(nsIPresContext* aPresContext)
{
  // See if we have any text decorations.
  // First see if our parent has text decorations.  If our parent does, then we inherit the bit.
  if (mParent && mParent->HasTextDecorations())
    mBits |= NS_STYLE_HAS_TEXT_DECORATIONS;
  else {
    // We might have defined a decoration.
    const nsStyleTextReset* text = (const nsStyleTextReset*)GetStyleData(eStyleStruct_TextReset);
    if (text->mTextDecoration != NS_STYLE_TEXT_DECORATION_NONE &&
        text->mTextDecoration != NS_STYLE_TEXT_DECORATION_OVERRIDE_ALL)
      mBits |= NS_STYLE_HAS_TEXT_DECORATIONS;
  }

  // Correct tables.
  const nsStyleDisplay* disp = (const nsStyleDisplay*)GetStyleData(eStyleStruct_Display);
  if (disp->mDisplay == NS_STYLE_DISPLAY_TABLE) {
    // -moz-center and -moz-right are used for HTML's alignment
    // This is covering the <div align="right"><table>...</table></div> case.
    // In this case, we don't want to inherit the text alignment into the table.
    const nsStyleText* text = (const nsStyleText*)GetStyleData(eStyleStruct_Text);
    
    if (text->mTextAlign == NS_STYLE_TEXT_ALIGN_MOZ_CENTER ||
        text->mTextAlign == NS_STYLE_TEXT_ALIGN_MOZ_RIGHT)
    {
      nsStyleText* uniqueText = (nsStyleText*)GetUniqueStyleData(aPresContext, eStyleStruct_Text);
      uniqueText->mTextAlign = NS_STYLE_TEXT_ALIGN_DEFAULT;
    }
  }
}

nsresult
nsStyleContext::ClearCachedDataForRule(nsIStyleRule* aInlineStyleRule)
{
  mRuleNode->ClearCachedData(aInlineStyleRule); // XXXdwh.  If we're willing to *really* special case
                                           // inline style, we could only invalidate the struct data
                                           // that actually changed.  For example, if someone changes
                                           // style.left, we really only need to blow away cached
                                           // data in the position struct.
  return NS_OK;
}

nsresult
nsStyleContext::ClearStyleData(nsIPresContext* aPresContext, nsIStyleRule* aRule)
{
  PRBool matched = PR_TRUE;
  if (aRule)
    mRuleNode->PathContainsRule(aRule, &matched);
  
  if (matched) {
    // First we need to clear out all of our style data.
    if (mCachedStyleData.mResetData || mCachedStyleData.mInheritedData)
      mCachedStyleData.Destroy(mBits, aPresContext);

    mBits = 0; // Clear all bits.
    aRule = nsnull;
  }

  ApplyStyleFixups(aPresContext);

  if (mChild) {
    nsStyleContext* child = mChild;
    do {
      child->ClearStyleData(aPresContext, aRule);
      child = child->mNextSibling;
    } while (mChild != child);
  }
  
  if (mEmptyChild) {
    nsStyleContext* child = mEmptyChild;
    do {
      child->ClearStyleData(aPresContext, aRule);
      child = child->mNextSibling;
    } while (mEmptyChild != child);
  }

  return NS_OK;
}

void nsStyleContext::ForceUnique(void)
{
  mBits |= NS_STYLE_UNIQUE_CONTEXT;
}

NS_IMETHODIMP
nsStyleContext::CalcStyleDifference(nsIStyleContext* aOther, PRInt32& aHint,PRBool aStopAtFirstDifference /*= PR_FALSE*/)
{
  if (aOther) {
    PRInt32 hint;
    const nsStyleContext* other = (const nsStyleContext*)aOther;

    const nsStyleFont* font = (const nsStyleFont*)GetStyleData(eStyleStruct_Font);
    const nsStyleFont* otherFont = (const nsStyleFont*)aOther->GetStyleData(eStyleStruct_Font);
    if (font != otherFont)
      aHint = font->CalcDifference(*otherFont);
    
    if (aStopAtFirstDifference && aHint > NS_STYLE_HINT_NONE) return NS_OK;
    if (aHint < NS_STYLE_HINT_MAX) {
      const nsStyleColor* color = (const nsStyleColor*)GetStyleData(eStyleStruct_Color);
      const nsStyleColor* otherColor = (const nsStyleColor*)aOther->GetStyleData(eStyleStruct_Color);
      if (color != otherColor) {
        hint = color->CalcDifference(*otherColor);
        if (aHint < hint)
          aHint = hint;
      }
    }

    if (aStopAtFirstDifference && aHint > NS_STYLE_HINT_NONE) return NS_OK;
    if (aHint < NS_STYLE_HINT_MAX) {
      const nsStyleBackground* background = (const nsStyleBackground*)GetStyleData(eStyleStruct_Background);
      const nsStyleBackground* otherBackground = (const nsStyleBackground*)aOther->GetStyleData(eStyleStruct_Background);
      if (background != otherBackground) {
        hint = background->CalcDifference(*otherBackground);
        if (aHint < hint)
          aHint = hint;
      }
    }

    if (aStopAtFirstDifference && aHint > NS_STYLE_HINT_NONE) return NS_OK;
    if (aHint < NS_STYLE_HINT_MAX) {
      const nsStyleMargin* margin = (const nsStyleMargin*)GetStyleData(eStyleStruct_Margin);
      const nsStyleMargin* otherMargin = (const nsStyleMargin*)aOther->GetStyleData(eStyleStruct_Margin);
      if (margin != otherMargin) {
        hint = margin->CalcDifference(*otherMargin);
        if (aHint < hint)
          aHint = hint;
      }
    }

    if (aStopAtFirstDifference && aHint > NS_STYLE_HINT_NONE) return NS_OK;
    if (aHint < NS_STYLE_HINT_MAX) {
      const nsStylePadding* padding = (const nsStylePadding*)GetStyleData(eStyleStruct_Padding);
      const nsStylePadding* otherPadding = (const nsStylePadding*)aOther->GetStyleData(eStyleStruct_Padding);
      if (padding != otherPadding) {
        hint = padding->CalcDifference(*otherPadding);
        if (aHint < hint)
          aHint = hint;
      }
    }
    if (aStopAtFirstDifference && aHint > NS_STYLE_HINT_NONE) return NS_OK;
    if (aHint < NS_STYLE_HINT_MAX) {
      const nsStyleBorder* border = (const nsStyleBorder*)GetStyleData(eStyleStruct_Border);
      const nsStyleBorder* otherBorder = (const nsStyleBorder*)aOther->GetStyleData(eStyleStruct_Border);
      if (border != otherBorder) {
        hint = border->CalcDifference(*otherBorder);
        if (aHint < hint)
          aHint = hint;
      }
    }
    if (aStopAtFirstDifference && aHint > NS_STYLE_HINT_NONE) return NS_OK;
    if (aHint < NS_STYLE_HINT_MAX) {
      const nsStyleOutline* outline = (const nsStyleOutline*)GetStyleData(eStyleStruct_Outline);
      const nsStyleOutline* otherOutline = (const nsStyleOutline*)aOther->GetStyleData(eStyleStruct_Outline);
      if (outline != otherOutline) {
        hint = outline->CalcDifference(*otherOutline);
        if (aHint < hint)
          aHint = hint;
      }
    }
    if (aStopAtFirstDifference && aHint > NS_STYLE_HINT_NONE) return NS_OK;
    if (aHint < NS_STYLE_HINT_MAX) {
      const nsStyleList* list = (const nsStyleList*)GetStyleData(eStyleStruct_List);
      const nsStyleList* otherList = (const nsStyleList*)aOther->GetStyleData(eStyleStruct_List);
      if (list != otherList) {
        hint = list->CalcDifference(*otherList);
        if (aHint < hint)
          aHint = hint;
      }
    }
    if (aStopAtFirstDifference && aHint > NS_STYLE_HINT_NONE) return NS_OK;
    if (aHint < NS_STYLE_HINT_MAX) {
      const nsStylePosition* pos = (const nsStylePosition*)GetStyleData(eStyleStruct_Position);
      const nsStylePosition* otherPosition = (const nsStylePosition*)aOther->GetStyleData(eStyleStruct_Position);
      if (pos != otherPosition) {
        hint = pos->CalcDifference(*otherPosition);
        if (aHint < hint)
          aHint = hint;
      }
    }
    if (aStopAtFirstDifference && aHint > NS_STYLE_HINT_NONE) return NS_OK;
    if (aHint < NS_STYLE_HINT_MAX) {
      const nsStyleText* text = (const nsStyleText*)GetStyleData(eStyleStruct_Text);
      const nsStyleText* otherText = (const nsStyleText*)aOther->GetStyleData(eStyleStruct_Text);
      if (text != otherText) {
        hint = text->CalcDifference(*otherText);
        if (aHint < hint)
          aHint = hint;
      }
    }
    
    if (aStopAtFirstDifference && aHint > NS_STYLE_HINT_NONE) return NS_OK;
    if (aHint < NS_STYLE_HINT_MAX) {
      const nsStyleTextReset* text = (const nsStyleTextReset*)GetStyleData(eStyleStruct_TextReset);
      const nsStyleTextReset* otherText = (const nsStyleTextReset*)aOther->GetStyleData(eStyleStruct_TextReset);
      if (text != otherText) {
        hint = text->CalcDifference(*otherText);
        if (aHint < hint)
          aHint = hint;
      }
    }

    if (aStopAtFirstDifference && aHint > NS_STYLE_HINT_NONE) return NS_OK;
    if (aHint < NS_STYLE_HINT_MAX) {
      const nsStyleVisibility* vis = (const nsStyleVisibility*)GetStyleData(eStyleStruct_Visibility);
      const nsStyleVisibility* otherVis = (const nsStyleVisibility*)aOther->GetStyleData(eStyleStruct_Visibility);
      if (vis != otherVis) {
        hint = vis->CalcDifference(*otherVis);
        if (aHint < hint)
          aHint = hint;
      }
    }

    if (aStopAtFirstDifference && aHint > NS_STYLE_HINT_NONE) return NS_OK;
    if (aHint < NS_STYLE_HINT_MAX) {
      const nsStyleDisplay* display = (const nsStyleDisplay*)GetStyleData(eStyleStruct_Display);
      const nsStyleDisplay* otherDisplay = (const nsStyleDisplay*)aOther->GetStyleData(eStyleStruct_Display);
      if (display != otherDisplay) {
        hint = display->CalcDifference(*otherDisplay);
        if (aHint < hint)
          aHint = hint;
      }
    }

#ifdef INCLUDE_XUL
    if (aStopAtFirstDifference && aHint > NS_STYLE_HINT_NONE) return NS_OK;
    if (aHint < NS_STYLE_HINT_MAX) {
      const nsStyleXUL* xul = (const nsStyleXUL*)GetStyleData(eStyleStruct_XUL);
      const nsStyleXUL* otherXUL = (const nsStyleXUL*)aOther->GetStyleData(eStyleStruct_XUL);
      if (xul != otherXUL) {
        hint = xul->CalcDifference(*otherXUL);
        if (aHint < hint)
          aHint = hint;
      }
    }
#endif

    if (aStopAtFirstDifference && aHint > NS_STYLE_HINT_NONE) return NS_OK;
    if (aHint < NS_STYLE_HINT_MAX) {
      const nsStyleTable* table = (const nsStyleTable*)GetStyleData(eStyleStruct_Table);
      const nsStyleTable* otherTable = (const nsStyleTable*)aOther->GetStyleData(eStyleStruct_Table);
      if (table != otherTable) {
        hint = table->CalcDifference(*otherTable);
        if (aHint < hint)
          aHint = hint;
      }
    }

    if (aStopAtFirstDifference && aHint > NS_STYLE_HINT_NONE) return NS_OK;
    if (aHint < NS_STYLE_HINT_MAX) {
      const nsStyleTableBorder* table = (const nsStyleTableBorder*)GetStyleData(eStyleStruct_TableBorder);
      const nsStyleTableBorder* otherTable = (const nsStyleTableBorder*)aOther->GetStyleData(eStyleStruct_TableBorder);
      if (table != otherTable) {
        hint = table->CalcDifference(*otherTable);
        if (aHint < hint)
          aHint = hint;
      }
    }

    if (aStopAtFirstDifference && aHint > NS_STYLE_HINT_NONE) return NS_OK;
    if (aHint < NS_STYLE_HINT_MAX) {
      const nsStyleContent* content = (const nsStyleContent*)GetStyleData(eStyleStruct_Content);
      const nsStyleContent* otherContent = (const nsStyleContent*)aOther->GetStyleData(eStyleStruct_Content);
      if (content != otherContent) {
        hint = content->CalcDifference(*otherContent);
        if (aHint < hint)
          aHint = hint;
      }
    }

    if (aStopAtFirstDifference && aHint > NS_STYLE_HINT_NONE) return NS_OK;
    if (aHint < NS_STYLE_HINT_MAX) {
      const nsStyleQuotes* content = (const nsStyleQuotes*)GetStyleData(eStyleStruct_Quotes);
      const nsStyleQuotes* otherContent = (const nsStyleQuotes*)aOther->GetStyleData(eStyleStruct_Quotes);
      if (content != otherContent) {
        hint = content->CalcDifference(*otherContent);
        if (aHint < hint)
          aHint = hint;
      }
    }

    if (aStopAtFirstDifference && aHint > NS_STYLE_HINT_NONE) return NS_OK;
    if (aHint < NS_STYLE_HINT_MAX) {
      const nsStyleUserInterface* ui = (const nsStyleUserInterface*)GetStyleData(eStyleStruct_UserInterface);
      const nsStyleUserInterface* otherUI = (const nsStyleUserInterface*)aOther->GetStyleData(eStyleStruct_UserInterface);
      if (ui != otherUI) {
        hint = ui->CalcDifference(*otherUI);
        if (aHint < hint) 
          aHint = hint;
      }
    }

    if (aStopAtFirstDifference && aHint > NS_STYLE_HINT_NONE) return NS_OK;
    if (aHint < NS_STYLE_HINT_MAX) {
      const nsStyleUIReset* ui = (const nsStyleUIReset*)GetStyleData(eStyleStruct_UIReset);
      const nsStyleUIReset* otherUI = (const nsStyleUIReset*)aOther->GetStyleData(eStyleStruct_UIReset);
      if (ui != otherUI) {
        hint = ui->CalcDifference(*otherUI);
        if (aHint < hint) 
          aHint = hint;
      }
    }
    if (aStopAtFirstDifference && aHint > NS_STYLE_HINT_NONE) return NS_OK;
  }
  return NS_OK;
}

#ifdef DEBUG
void nsStyleContext::List(FILE* out, PRInt32 aIndent)
{
  // Indent
  PRInt32 ix;
  for (ix = aIndent; --ix >= 0; ) fputs("  ", out);
  fprintf(out, "%p(%d) ", (void*)this, mRefCnt);
  if (nsnull != mPseudoTag) {
    nsAutoString  buffer;
    mPseudoTag->ToString(buffer);
    fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
    fputs(" ", out);
  }

  if (mRuleNode) {
    fputs("{\n", out);
    nsRuleNode* ruleNode = mRuleNode;
    while (ruleNode) {
      nsCOMPtr<nsIStyleRule> styleRule;
      ruleNode->GetRule(getter_AddRefs(styleRule));
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


/******************************************************************************
* SizeOf method:
*  
*  Self (reported as nsStyleContext's size): 
*    1) sizeof(*this) which gets all of the data members
*    2) adds in the size of the PseudoTag, if there is one
*  
*  Children / siblings / parents:
*    1) We recurse over the mChild and mEmptyChild instances if they exist.
*       These instances will then be accumulated seperately (not part of 
*       the containing instance's size)
*    2) We recurse over the siblings of the Child and Empty Child instances
*       and count then seperately as well.
*    3) We recurse over our direct siblings (if any).
*   
******************************************************************************/
void nsStyleContext::SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize)
{
  NS_ASSERTION(aSizeOfHandler != nsnull, "SizeOf handler cannot be null");

  // first get the unique items collection
  UNIQUE_STYLE_ITEMS(uniqueItems);

  if(! uniqueItems->AddItem((void*)this) ){
    // object has already been accounted for
    return;
  }

  PRUint32 localSize=0;

  // get or create a tag for this instance
  nsCOMPtr<nsIAtom> tag;
  tag = getter_AddRefs(NS_NewAtom("nsStyleContext"));
  // get the size of an empty instance and add to the sizeof handler
  aSize = sizeof(*this);
  // add in the size of the member mPseudoTag
  if(mPseudoTag){
    mPseudoTag->SizeOf(aSizeOfHandler, &localSize);
    aSize += localSize;
  }
  aSizeOfHandler->AddSize(tag,aSize);

  // now follow up with the child (and empty child) recursion
  if (nsnull != mChild) {
    nsStyleContext* child = mChild;
    do {
      child->SizeOf(aSizeOfHandler, localSize);
      child = child->mNextSibling;
    } while (mChild != child);
  }
  if (nsnull != mEmptyChild) {
    nsStyleContext* child = mEmptyChild;
    do {
      child->SizeOf(aSizeOfHandler, localSize);
      child = child->mNextSibling;
    } while (mEmptyChild != child);
  }
  // and finally our direct siblings (if any)
  if (nsnull != mNextSibling) {
    mNextSibling->SizeOf(aSizeOfHandler, localSize);
  }
}

static void IndentBy(FILE* out, PRInt32 aIndent) {
  while (--aIndent >= 0) fputs("  ", out);
}
// virtual 
void nsStyleContext::DumpRegressionData(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent)
{
  nsAutoString str;

  // FONT
  IndentBy(out,aIndent);
  const nsStyleFont* font = (const nsStyleFont*)GetStyleData(eStyleStruct_Font);
  fprintf(out, "<font %s %d %d %d />\n", 
          NS_ConvertUCS2toUTF8(font->mFont.name).get(),
          font->mFont.size,
          font->mSize,
          font->mFlags);

  // COLOR
  IndentBy(out,aIndent);
  const nsStyleColor* color = (const nsStyleColor*)GetStyleData(eStyleStruct_Color);
  fprintf(out, "<color data=\"%ld\"/>\n", 
    (long)color->mColor);

  // BACKGROUND
  IndentBy(out,aIndent);
  const nsStyleBackground* bg = (const nsStyleBackground*)GetStyleData(eStyleStruct_Background);
  fprintf(out, "<background data=\"%d %d %d %ld %ld %ld %s\"/>\n",
    (int)bg->mBackgroundAttachment,
    (int)bg->mBackgroundFlags,
    (int)bg->mBackgroundRepeat,
    (long)bg->mBackgroundColor,
    (long)bg->mBackgroundXPosition,
    (long)bg->mBackgroundYPosition,
    NS_ConvertUCS2toUTF8(bg->mBackgroundImage).get());
 
  // SPACING (ie. margin, padding, border, outline)
  IndentBy(out,aIndent);
  fprintf(out, "<spacing data=\"");

  const nsStyleMargin* margin = (const nsStyleMargin*)GetStyleData(eStyleStruct_Margin);
  margin->mMargin.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  
  const nsStylePadding* padding = (const nsStylePadding*)GetStyleData(eStyleStruct_Padding);
  padding->mPadding.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  
  const nsStyleBorder* border = (const nsStyleBorder*)GetStyleData(eStyleStruct_Border);
  border->mBorder.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  border->mBorderRadius.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  
  const nsStyleOutline* outline = (const nsStyleOutline*)GetStyleData(eStyleStruct_Outline);
  outline->mOutlineRadius.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  outline->mOutlineWidth.ToString(str);
  fprintf(out, "%s", NS_ConvertUCS2toUTF8(str).get());
  fprintf(out, "%d", (int)border->mFloatEdge);
  fprintf(out, "\" />\n");

  // LIST
  IndentBy(out,aIndent);
  const nsStyleList* list = (const nsStyleList*)GetStyleData(eStyleStruct_List);
  fprintf(out, "<list data=\"%d %d %s\" />\n",
    (int)list->mListStyleType,
    (int)list->mListStyleType,
    NS_ConvertUCS2toUTF8(list->mListStyleImage).get());

  // POSITION
  IndentBy(out,aIndent);
  const nsStylePosition* pos = (const nsStylePosition*)GetStyleData(eStyleStruct_Position);
  fprintf(out, "<position data=\"");
  pos->mOffset.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  pos->mWidth.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  pos->mMinWidth.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  pos->mMaxWidth.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  pos->mHeight.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  pos->mMinHeight.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  pos->mMaxHeight.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  fprintf(out, "%d ", (int)pos->mBoxSizing);
  pos->mZIndex.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  fprintf(out, "\" />\n");

  // TEXT
  IndentBy(out,aIndent);
  const nsStyleText* text = (const nsStyleText*)GetStyleData(eStyleStruct_Text);
  fprintf(out, "<text data=\"%d %d %d ",
    (int)text->mTextAlign,
    (int)text->mTextTransform,
    (int)text->mWhiteSpace);
  text->mLetterSpacing.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  text->mLineHeight.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  text->mTextIndent.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  text->mWordSpacing.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  fprintf(out, "\" />\n");
  
  // TEXT RESET
  IndentBy(out,aIndent);
  const nsStyleTextReset* textReset = (const nsStyleTextReset*)GetStyleData(eStyleStruct_TextReset);
  fprintf(out, "<textreset data=\"%d ",
    (int)textReset->mTextDecoration);
  textReset->mVerticalAlign.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  fprintf(out, "\" />\n");

  // DISPLAY
  IndentBy(out,aIndent);
  const nsStyleDisplay* disp = (const nsStyleDisplay*)GetStyleData(eStyleStruct_Display);
  fprintf(out, "<display data=\"%d %d %d %d %d %d %d %d %ld %ld %ld %ld %s\" />\n",
    (int)disp->mPosition,
    (int)disp->mDisplay,
    (int)disp->mFloats,
    (int)disp->mBreakType,
    (int)disp->mBreakBefore,
    (int)disp->mBreakAfter,
    (int)disp->mOverflow,
    (int)disp->mClipFlags,
    (long)disp->mClip.x,
    (long)disp->mClip.y,
    (long)disp->mClip.width,
    (long)disp->mClip.height,
    NS_ConvertUCS2toUTF8(disp->mBinding).get()
    );
  
  // VISIBILITY
  IndentBy(out,aIndent);
  const nsStyleVisibility* vis = (const nsStyleVisibility*)GetStyleData(eStyleStruct_Visibility);
  fprintf(out, "<visibility data=\"%d %d %f\" />\n",
    (int)vis->mDirection,
    (int)vis->mVisible,
    (float)vis->mOpacity
    );

  // TABLE
  IndentBy(out,aIndent);
  const nsStyleTable* table = (const nsStyleTable*)GetStyleData(eStyleStruct_Table);
  fprintf(out, "<table data=\"%d %d %d ",
    (int)table->mLayoutStrategy,
    (int)table->mFrame,
    (int)table->mRules);
  fprintf(out, "%ld %ld ",
    (long)table->mCols,
    (long)table->mSpan);
  fprintf(out, "\" />\n");

  // TABLEBORDER
  IndentBy(out,aIndent);
  const nsStyleTableBorder* tableBorder = (const nsStyleTableBorder*)GetStyleData(eStyleStruct_TableBorder);
  fprintf(out, "<tableborder data=\"%d ",
    (int)tableBorder->mBorderCollapse);
  tableBorder->mBorderSpacingX.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  tableBorder->mBorderSpacingY.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  fprintf(out, "%d %d ",
    (int)tableBorder->mCaptionSide,
    (int)tableBorder->mEmptyCells);
  fprintf(out, "\" />\n");

  // CONTENT
  IndentBy(out,aIndent);
  const nsStyleContent* content = (const nsStyleContent*)GetStyleData(eStyleStruct_Content);
  fprintf(out, "<content data=\"%ld %ld %ld ",
    (long)content->ContentCount(),
    (long)content->CounterIncrementCount(),
    (long)content->CounterResetCount());
  // XXX: iterate over the content and counters...
  content->mMarkerOffset.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  fprintf(out, "\" />\n");

  // QUOTES
  IndentBy(out,aIndent);
  const nsStyleQuotes* quotes = (const nsStyleQuotes*)GetStyleData(eStyleStruct_Quotes);
  fprintf(out, "<quotes data=\"%ld ",
    (long)quotes->QuotesCount());
  // XXX: iterate over the quotes...
  fprintf(out, "\" />\n");

  // UI
  IndentBy(out,aIndent);
  const nsStyleUserInterface* ui = (const nsStyleUserInterface*)GetStyleData(eStyleStruct_UserInterface);
  fprintf(out, "<ui data=\"%d %d %d %d %s\" />\n",
    (int)ui->mUserInput,
    (int)ui->mUserModify,
    (int)ui->mUserFocus, 
    (int)ui->mCursor,
    NS_ConvertUCS2toUTF8(ui->mCursorImage).get());

  // UIReset
  IndentBy(out,aIndent);
  const nsStyleUIReset* uiReset = (const nsStyleUIReset*)GetStyleData(eStyleStruct_UIReset);
  fprintf(out, "<uireset data=\"%d %d %d\" />\n",
    (int)uiReset->mUserSelect,
    (int)uiReset->mKeyEquivalent,
    (int)uiReset->mResizer);

  // XUL
#ifdef INCLUDE_XUL
  IndentBy(out,aIndent);
  const nsStyleXUL* xul = (const nsStyleXUL*)GetStyleData(eStyleStruct_XUL);
  fprintf(out, "<xul data=\"%d %d %d %d %d %d",
    (int)xul->mBoxAlign,
    (int)xul->mBoxDirection,
    (int)xul->mBoxFlex,
    (int)xul->mBoxOrient,
    (int)xul->mBoxPack,
    (int)xul->mBoxOrdinal);
  fprintf(out, "\" />\n");
#endif
  //#insert new style structs here#
}
#endif

// Overloaded new operator. Initializes the memory to 0 and relies on an arena
// (which comes from the presShell) to perform the allocation.
void* 
nsStyleContext::operator new(size_t sz, nsIPresContext* aPresContext)
{
  // Check the recycle list first.
  void* result = nsnull;
  aPresContext->AllocateFromShell(sz, &result);
  return result;
}

// Overridden to prevent the global delete from being called, since the memory
// came out of an nsIArena instead of the global delete operator's heap.
void 
nsStyleContext::Destroy()
{
  // Get the pres context from our rule node.
  nsCOMPtr<nsIPresContext> presContext;
  mRuleNode->GetPresContext(getter_AddRefs(presContext));

  // Call our destructor.
  this->~nsStyleContext();

  // Don't let the memory be freed, since it will be recycled
  // instead. Don't call the global operator delete.
  presContext->FreeToShell(sizeof(nsStyleContext), this);
}

NS_EXPORT nsresult
NS_NewStyleContext(nsIStyleContext** aInstancePtrResult,
                   nsIStyleContext* aParentContext,
                   nsIAtom* aPseudoTag,
                   nsRuleNode* aRuleNode,
                   nsIPresContext* aPresContext)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsStyleContext* context = new (aPresContext) nsStyleContext(aParentContext, aPseudoTag, 
                                                              aRuleNode, aPresContext);
  if (nsnull == context) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return context->QueryInterface(NS_GET_IID(nsIStyleContext), (void **) aInstancePtrResult);
}

