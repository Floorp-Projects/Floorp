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
 
#include "nsStyleConsts.h"
#include "nsString.h"
#include "nsIPresContext.h"
#include "nsIStyleRule.h"
#include "nsISupportsArray.h"
#include "nsCRT.h"

#include "nsCOMPtr.h"
#include "nsStyleSet.h"
#include "nsIPresShell.h"
#include "nsLayoutAtoms.h"
#include "prenv.h"

#include "nsRuleNode.h"
#include "nsUnitConversion.h"
#include "nsStyleContext.h"
#include "imgIRequest.h"

#ifdef DEBUG
// #define NOISY_DEBUG
#endif

//----------------------------------------------------------------------


nsStyleContext::nsStyleContext(nsStyleContext* aParent,
                               nsIAtom* aPseudoTag,
                               nsRuleNode* aRuleNode,
                               nsIPresContext* aPresContext)
  : mParent((nsStyleContext*)aParent),
    mChild(nsnull),
    mEmptyChild(nsnull),
    mPseudoTag(aPseudoTag),
    mRuleNode(aRuleNode),
    mBits(0),
    mRefCnt(0)
{
  mNextSibling = this;
  mPrevSibling = this;
  if (mParent) {
    mParent->AddRef();
    mParent->AppendChild(this);
  }

  ApplyStyleFixups(aPresContext);

  NS_ASSERTION(NS_STYLE_INHERIT_MASK &
               (1 << PRInt32(nsStyleStructID_Length - 1)) != 0,
               "NS_STYLE_INHERIT_MASK must be bigger, and other bits shifted");
}

nsStyleContext::~nsStyleContext()
{
  NS_ASSERTION((nsnull == mChild) && (nsnull == mEmptyChild), "destructing context with children");

  nsIPresContext *presContext = mRuleNode->GetPresContext();

  presContext->PresShell()->StyleSet()->
    NotifyStyleContextDestroyed(presContext, this);

  if (mParent) {
    mParent->RemoveChild(this);
    mParent->Release();
  }

  // Free up our data structs.
  if (mCachedStyleData.mResetData || mCachedStyleData.mInheritedData) {
    mCachedStyleData.Destroy(mBits, presContext);
  }
}

void nsStyleContext::AppendChild(nsStyleContext* aChild)
{
  if (aChild->mRuleNode->IsRoot()) {
    // The child matched no rules.
    if (!mEmptyChild) {
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
    if (!mChild) {
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
  NS_PRECONDITION(nsnull != aChild && this == aChild->mParent, "bad argument");

  if (aChild->mRuleNode->IsRoot()) { // is empty 
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

already_AddRefed<nsStyleContext>
nsStyleContext::FindChildWithRules(const nsIAtom* aPseudoTag, 
                                   nsRuleNode* aRuleNode)
{
  PRUint32 threshold = 10; // The # of siblings we're willing to examine
                           // before just giving this whole thing up.

  nsStyleContext* aResult = nsnull;

  if ((nsnull != mChild) || (nsnull != mEmptyChild)) {
    nsStyleContext* child;
    if (aRuleNode->IsRoot()) {
      if (nsnull != mEmptyChild) {
        child = mEmptyChild;
        do {
          if (aPseudoTag == child->mPseudoTag) {
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
        if (child->mRuleNode == aRuleNode && child->mPseudoTag == aPseudoTag) {
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

  if (aResult)
    aResult->AddRef();

  return aResult;
}


PRBool nsStyleContext::Equals(const nsStyleContext* aOther) const
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
  return mRuleNode->GetStyleData(aSID, this, PR_TRUE); // Our rule node will take care of it for us.
}

inline const nsStyleStruct* nsStyleContext::PeekStyleData(nsStyleStructID aSID)
{
  const nsStyleStruct* cachedData = mCachedStyleData.GetStyleData(aSID); 
  if (cachedData)
    return cachedData; // We have computed data stored on this node in the context tree.
  return mRuleNode->GetStyleData(aSID, this, PR_FALSE); // Our rule node will take care of it for us.
}

void
nsStyleContext::GetBorderPaddingFor(nsStyleBorderPadding& aBorderPadding)
{
  nsMargin border, padding;
  if (GetStyleBorder()->GetBorder(border)) {
    if (GetStylePadding()->GetPadding(padding)) {
      border += padding;
      aBorderPadding.SetBorderPadding(border);
    }
  }
}

// This is an evil evil function, since it forces you to alloc your own separate copy of
// style data!  Do not use this function unless you absolutely have to!  You should avoid
// this at all costs! -dwh
nsStyleStruct* 
nsStyleContext::GetUniqueStyleData(const nsStyleStructID& aSID)
{
  // If we already own the struct and no kids could depend on it, then
  // just return it.  (We leak in this case if there are kids -- and this
  // function really shouldn't be called for style contexts that could
  // have kids depending on the data.  ClearStyleData would be OK, but
  // this test for no mChild or mEmptyChild doesn't catch that case.)
  const nsStyleStruct *current = GetStyleData(aSID);
  if (!mChild && !mEmptyChild &&
      !(mBits & nsCachedStyleData::GetBitForSID(aSID)) &&
      mCachedStyleData.GetStyleData(aSID))
    return NS_CONST_CAST(nsStyleStruct*, current);

  nsStyleStruct* result;
  nsIPresContext *presContext = PresContext();
  switch (aSID) {

#define UNIQUE_CASE(c_)                                                       \
  case eStyleStruct_##c_:                                                     \
    result = new (presContext) nsStyle##c_(                                   \
      * NS_STATIC_CAST(const nsStyle##c_ *, current));                        \
    break;

  UNIQUE_CASE(Display)
  UNIQUE_CASE(Background)
  UNIQUE_CASE(Text)
  UNIQUE_CASE(TextReset)

#undef UNIQUE_CASE

  default:
    NS_ERROR("Struct type not supported.  Please find another way to do this if you can!\n");
    return nsnull;
  }

  SetStyle(aSID, result);
  mBits &= ~nsCachedStyleData::GetBitForSID(aSID);

  return result;
}

void
nsStyleContext::SetStyle(nsStyleStructID aSID, nsStyleStruct* aStruct)
{
  // This method should only be called from nsRuleNode!  It is not a public
  // method!
  
  NS_ASSERTION(aSID >= 0 && aSID < nsStyleStructID_Length, "out of bounds");

  // NOTE:  nsCachedStyleData::GetStyleData works roughly the same way.
  // See the comments there (in nsRuleNode.h) for more details about
  // what this is doing and why.

  const nsCachedStyleData::StyleStructInfo& info =
      nsCachedStyleData::gInfo[aSID];
  char* resetOrInheritSlot = NS_REINTERPRET_CAST(char*, &mCachedStyleData) +
                             info.mCachedStyleDataOffset;
  char* resetOrInherit = NS_REINTERPRET_CAST(char*,
      *NS_REINTERPRET_CAST(void**, resetOrInheritSlot));
  if (!resetOrInherit) {
    nsIPresContext *presContext = mRuleNode->GetPresContext();
    if (mCachedStyleData.IsReset(aSID)) {
      mCachedStyleData.mResetData = new (presContext) nsResetStyleData;
      resetOrInherit = NS_REINTERPRET_CAST(char*, mCachedStyleData.mResetData);
    } else {
      mCachedStyleData.mInheritedData =
          new (presContext) nsInheritedStyleData;
      resetOrInherit =
          NS_REINTERPRET_CAST(char*, mCachedStyleData.mInheritedData);
    }
  }
  char* dataSlot = resetOrInherit + info.mInheritResetOffset;
  *NS_REINTERPRET_CAST(nsStyleStruct**, dataSlot) = aStruct;
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
    const nsStyleTextReset* text = GetStyleTextReset();
    if (text->mTextDecoration != NS_STYLE_TEXT_DECORATION_NONE &&
        text->mTextDecoration != NS_STYLE_TEXT_DECORATION_OVERRIDE_ALL)
      mBits |= NS_STYLE_HAS_TEXT_DECORATIONS;
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
}

void
nsStyleContext::ClearStyleData(nsIPresContext* aPresContext)
{
  // First we need to clear out all of our style data.
  if (mCachedStyleData.mResetData || mCachedStyleData.mInheritedData)
    mCachedStyleData.Destroy(mBits, aPresContext);

  mBits = 0; // Clear all bits.

  ApplyStyleFixups(aPresContext);

  if (mChild) {
    nsStyleContext* child = mChild;
    do {
      child->ClearStyleData(aPresContext);
      child = child->mNextSibling;
    } while (mChild != child);
  }
  
  if (mEmptyChild) {
    nsStyleContext* child = mEmptyChild;
    do {
      child->ClearStyleData(aPresContext);
      child = child->mNextSibling;
    } while (mEmptyChild != child);
  }
}

nsChangeHint
nsStyleContext::CalcStyleDifference(nsStyleContext* aOther)
{
  nsChangeHint hint = NS_STYLE_HINT_NONE;
  NS_ENSURE_TRUE(aOther, hint);
  // We must always ensure that we populate the structs on the new style
  // context that are filled in on the old context, so that if we get
  // two style changes in succession, the second of which causes a real
  // style change, the PeekStyleData doesn't fail.

  // If our rule nodes are the same, then we are looking at the same
  // style data.  We know this because CalcStyleDifference is always
  // called on two style contexts that point to the same element, so we
  // know that our position in the style context tree is the same and
  // our position in the rule node tree is also the same.
  PRBool compare = mRuleNode != aOther->mRuleNode;

  nsChangeHint maxHint = NS_STYLE_HINT_FRAMECHANGE;
  
#define DO_STRUCT_DIFFERENCE(struct_)                                         \
  PR_BEGIN_MACRO                                                              \
    const nsStyle##struct_* this##struct_ =                                   \
        NS_STATIC_CAST(const nsStyle##struct_*,                               \
                      PeekStyleData(NS_GET_STYLESTRUCTID(nsStyle##struct_))); \
    if (this##struct_) {                                                      \
      const nsStyle##struct_* other##struct_ =                                \
          NS_STATIC_CAST(const nsStyle##struct_*,                             \
               aOther->GetStyleData(NS_GET_STYLESTRUCTID(nsStyle##struct_))); \
      if (compare &&                                                          \
          !NS_IsHintSubset(maxHint, hint) &&                                  \
          this##struct_ != other##struct_) {                                  \
        NS_UpdateHint(hint, this##struct_->CalcDifference(*other##struct_));  \
      }                                                                       \
    }                                                                         \
  PR_END_MACRO

  // We begin by examining those style structs that are capable of
  // causing the maximal difference, a FRAMECHANGE.
  // FRAMECHANGE Structs: Display, XUL, Content, UserInterface,
  // Visibility, Quotes
  DO_STRUCT_DIFFERENCE(Display);
  DO_STRUCT_DIFFERENCE(XUL);
  DO_STRUCT_DIFFERENCE(Column);
  DO_STRUCT_DIFFERENCE(Content);
  DO_STRUCT_DIFFERENCE(UserInterface);
  DO_STRUCT_DIFFERENCE(Visibility);
  DO_STRUCT_DIFFERENCE(Outline);
#ifdef MOZ_SVG
  DO_STRUCT_DIFFERENCE(SVG);
#endif
  // If the quotes implementation is ever going to change we might not need
  // a framechange here and a reflow should be sufficient.  See bug 35768.
  DO_STRUCT_DIFFERENCE(Quotes);

  // At this point, we know that the worst kind of damage we could do is
  // a reflow.
  maxHint = NS_STYLE_HINT_REFLOW;
      
  // The following structs cause (as their maximal difference) a reflow
  // to occur.  REFLOW Structs: Font, Margin, Padding, Border, List,
  // Position, Text, TextReset, Table, TableBorder
  DO_STRUCT_DIFFERENCE(Font);
  DO_STRUCT_DIFFERENCE(Margin);
  DO_STRUCT_DIFFERENCE(Padding);
  DO_STRUCT_DIFFERENCE(Border);
  DO_STRUCT_DIFFERENCE(List);
  DO_STRUCT_DIFFERENCE(Position);
  DO_STRUCT_DIFFERENCE(Text);
  DO_STRUCT_DIFFERENCE(TextReset);
  DO_STRUCT_DIFFERENCE(Table);
  DO_STRUCT_DIFFERENCE(TableBorder);

  // At this point, we know that the worst kind of damage we could do is
  // a re-render (i.e., a VISUAL change).
  maxHint = NS_STYLE_HINT_VISUAL;

  // The following structs cause (as their maximal difference) a
  // re-render to occur.  VISUAL Structs: Color, Background, Outline,
  // UIReset
  DO_STRUCT_DIFFERENCE(Color);
  DO_STRUCT_DIFFERENCE(Background);
  DO_STRUCT_DIFFERENCE(UIReset);

#undef DO_STRUCT_DIFFERENCE

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

class URICString : public nsCAutoString {
public:
  URICString(nsIURI* aURI) {
    if (aURI) {
      aURI->GetSpec(*this);
    } else {
      Assign("[none]");
    }
  }

  URICString(imgIRequest* aImageRequest) {
    nsCOMPtr<nsIURI> uri;
    if (aImageRequest) {
      aImageRequest->GetURI(getter_AddRefs(uri));
    }
    if (uri) {
      uri->GetSpec(*this);
    } else {
      Assign("[none]");
    }
  }
  
  URICString& operator=(const URICString& aOther) {
    Assign(aOther);
    return *this;
  }
};

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
    fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
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

static void IndentBy(FILE* out, PRInt32 aIndent) {
  while (--aIndent >= 0) fputs("  ", out);
}
// virtual 
void nsStyleContext::DumpRegressionData(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent)
{
  nsAutoString str;

  // FONT
  IndentBy(out,aIndent);
  const nsStyleFont* font = GetStyleFont();
  fprintf(out, "<font %s %d %d %d />\n", 
          NS_ConvertUCS2toUTF8(font->mFont.name).get(),
          font->mFont.size,
          font->mSize,
          font->mFlags);

  // COLOR
  IndentBy(out,aIndent);
  const nsStyleColor* color = GetStyleColor();
  fprintf(out, "<color data=\"%ld\"/>\n", 
    (long)color->mColor);

  // BACKGROUND
  IndentBy(out,aIndent);
  const nsStyleBackground* bg = GetStyleBackground();
  fprintf(out, "<background data=\"%d %d %d %ld %ld %ld %s\"/>\n",
    (int)bg->mBackgroundAttachment,
    (int)bg->mBackgroundFlags,
    (int)bg->mBackgroundRepeat,
    (long)bg->mBackgroundColor,
    // XXX These aren't initialized unless flags are set:
    (long)bg->mBackgroundXPosition.mCoord, // potentially lossy on some platforms
    (long)bg->mBackgroundYPosition.mCoord, // potentially lossy on some platforms
    URICString(bg->mBackgroundImage).get());
 
  // SPACING (ie. margin, padding, border, outline)
  IndentBy(out,aIndent);
  fprintf(out, "<spacing data=\"");

  const nsStyleMargin* margin = GetStyleMargin();
  margin->mMargin.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  
  const nsStylePadding* padding = GetStylePadding();
  padding->mPadding.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  
  const nsStyleBorder* border = GetStyleBorder();
  border->mBorder.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  border->mBorderRadius.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  
  const nsStyleOutline* outline = GetStyleOutline();
  outline->mOutlineRadius.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  outline->mOutlineWidth.ToString(str);
  fprintf(out, "%s", NS_ConvertUCS2toUTF8(str).get());
  fprintf(out, "%d", (int)border->mFloatEdge);
  fprintf(out, "\" />\n");

  // LIST
  IndentBy(out,aIndent);
  const nsStyleList* list = GetStyleList();
  fprintf(out, "<list data=\"%d %d %s\" />\n",
    (int)list->mListStyleType,
    (int)list->mListStyleType,
    URICString(list->mListStyleImage).get());

  // POSITION
  IndentBy(out,aIndent);
  const nsStylePosition* pos = GetStylePosition();
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
  const nsStyleText* text = GetStyleText();
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
  const nsStyleTextReset* textReset = GetStyleTextReset();
  fprintf(out, "<textreset data=\"%d ",
    (int)textReset->mTextDecoration);
  textReset->mVerticalAlign.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  fprintf(out, "\" />\n");

  // DISPLAY
  IndentBy(out,aIndent);
  const nsStyleDisplay* disp = GetStyleDisplay();
  fprintf(out, "<display data=\"%d %d %f %d %d %d %d %d %d %ld %ld %ld %ld %s\" />\n",
    (int)disp->mPosition,
    (int)disp->mDisplay,
    (float)disp->mOpacity,      
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
    URICString(disp->mBinding).get()
    );
  
  // VISIBILITY
  IndentBy(out,aIndent);
  const nsStyleVisibility* vis = GetStyleVisibility();
  fprintf(out, "<visibility data=\"%d %d\" />\n",
    (int)vis->mDirection,
    (int)vis->mVisible
    );

  // TABLE
  IndentBy(out,aIndent);
  const nsStyleTable* table = GetStyleTable();
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
  const nsStyleTableBorder* tableBorder = GetStyleTableBorder();
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
  const nsStyleContent* content = GetStyleContent();
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
  const nsStyleQuotes* quotes = GetStyleQuotes();
  fprintf(out, "<quotes data=\"%ld ",
    (long)quotes->QuotesCount());
  // XXX: iterate over the quotes...
  fprintf(out, "\" />\n");

  // UI
  IndentBy(out,aIndent);
  const nsStyleUserInterface* ui = GetStyleUserInterface();
  fprintf(out, "<ui data=\"%d %d %d %d\" />\n",
    (int)ui->mUserInput,
    (int)ui->mUserModify,
    (int)ui->mUserFocus, 
    (int)ui->mCursor);

  // UIReset
  IndentBy(out,aIndent);
  const nsStyleUIReset* uiReset = GetStyleUIReset();
  fprintf(out, "<uireset data=\"%d %d\" />\n",
    (int)uiReset->mUserSelect,
    (int)uiReset->mKeyEquivalent);

  // Column
  IndentBy(out,aIndent);
  const nsStyleColumn* column = GetStyleColumn();
  fprintf(out, "<column data=\"%d ",
    (int)column->mColumnCount);
  column->mColumnWidth.ToString(str);
  fprintf(out, "%s", NS_ConvertUCS2toUTF8(str).get());
  fprintf(out, "\" />\n");

  // XUL
  IndentBy(out,aIndent);
  const nsStyleXUL* xul = GetStyleXUL();
  fprintf(out, "<xul data=\"%d %d %d %d %d %d",
    (int)xul->mBoxAlign,
    (int)xul->mBoxDirection,
    (int)xul->mBoxFlex,
    (int)xul->mBoxOrient,
    (int)xul->mBoxPack,
    (int)xul->mBoxOrdinal);
  fprintf(out, "\" />\n");

#ifdef MOZ_SVG
  // SVG
  IndentBy(out,aIndent);
  const nsStyleSVG* svg = GetStyleSVG();
  fprintf(out, "<svg data=\"%d %ld %f %d %d %d %d %ld %s %f %d %d %f %f %f %d %d\" />\n",
          (int)svg->mFill.mType,
          (long)svg->mFill.mColor,
          svg->mFillOpacity,
          (int)svg->mFillRule,
          (int)svg->mPointerEvents,
          (int)svg->mShapeRendering,
          (int)svg->mStroke.mType,
          (long)svg->mStroke.mColor,
          NS_ConvertUCS2toUTF8(svg->mStrokeDasharray).get(),
          svg->mStrokeDashoffset,
          (int)svg->mStrokeLinecap,
          (int)svg->mStrokeLinejoin,
          svg->mStrokeMiterlimit,
          svg->mStrokeOpacity,
          svg->mStrokeWidth,
          (int)svg->mTextAnchor,
          (int)svg->mTextRendering);

  // SVGReset
  IndentBy(out,aIndent);
  const nsStyleSVGReset* svgReset = GetStyleSVGReset();
  fprintf(out, "<svgreset data=\"%d\" />\n",
          (int)svgReset->mDominantBaseline);
#endif
  //#insert new style structs here#
}
#endif

// Overloaded new operator. Initializes the memory to 0 and relies on an arena
// (which comes from the presShell) to perform the allocation.
void* 
nsStyleContext::operator new(size_t sz, nsIPresContext* aPresContext) CPP_THROW_NEW
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
  nsCOMPtr<nsIPresContext> presContext = mRuleNode->GetPresContext();

  // Call our destructor.
  this->~nsStyleContext();

  // Don't let the memory be freed, since it will be recycled
  // instead. Don't call the global operator delete.
  presContext->FreeToShell(sizeof(nsStyleContext), this);
}

already_AddRefed<nsStyleContext>
NS_NewStyleContext(nsStyleContext* aParentContext,
                   nsIAtom* aPseudoTag,
                   nsRuleNode* aRuleNode,
                   nsIPresContext* aPresContext)
{
  nsStyleContext* context = new (aPresContext) nsStyleContext(aParentContext, aPseudoTag, 
                                                              aRuleNode, aPresContext);
  if (context)
    context->AddRef();
  return context;
}

