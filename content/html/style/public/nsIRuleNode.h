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
#ifndef nsIRuleNode_h___
#define nsIRuleNode_h___

#include "nsCOMPtr.h"
#include "nsHashtable.h"
#include "nsIStyleRule.h"
#include "nsIStyleContext.h"
#include "nsICSSDeclaration.h"

class nsIHTMLMappedAttributes;

typedef void (*nsPostResolveFunc)(nsStyleStruct* aStyleStruct, nsRuleData* aData);

struct nsInheritedStyleData
{
  nsStyleVisibility* mVisibilityData;
  nsStyleFont* mFontData;
  nsStyleList* mListData;
  nsStyleTableBorder* mTableData;
  nsStyleColor* mColorData;
  nsStyleQuotes* mQuotesData;
  nsStyleText* mTextData;
  nsStyleUserInterface* mUIData;

  void* operator new(size_t sz, nsIPresContext* aContext) {
    void* result = nsnull;
    aContext->AllocateFromShell(sz, &result);
    return result;
  };

  void ClearInheritedData(PRUint32 aBits) {
    if (mVisibilityData && (aBits & NS_STYLE_INHERIT_VISIBILITY))
      mVisibilityData = nsnull;
    if (mFontData && (aBits & NS_STYLE_INHERIT_FONT))
      mFontData = nsnull;
    if (mListData && (aBits & NS_STYLE_INHERIT_LIST))
      mListData = nsnull;
    if (mTableData && (aBits & NS_STYLE_INHERIT_TABLE_BORDER))
      mTableData = nsnull;
    if (mColorData && (aBits & NS_STYLE_INHERIT_COLOR))
      mColorData = nsnull;
    if (mQuotesData && (aBits & NS_STYLE_INHERIT_QUOTES))
      mQuotesData = nsnull;
    if (mTextData && (aBits & NS_STYLE_INHERIT_TEXT))
      mTextData = nsnull;
    if (mUIData && (aBits & NS_STYLE_INHERIT_UI))
      mUIData = nsnull;
  };

  void Destroy(PRUint32 aBits, nsIPresContext* aContext) {
    if (mVisibilityData && !(aBits & NS_STYLE_INHERIT_VISIBILITY))
      mVisibilityData->Destroy(aContext);
    if (mFontData && !(aBits & NS_STYLE_INHERIT_FONT))
      mFontData->Destroy(aContext);
    if (mListData && !(aBits & NS_STYLE_INHERIT_LIST))
      mListData->Destroy(aContext);
    if (mTableData && !(aBits & NS_STYLE_INHERIT_TABLE_BORDER))
      mTableData->Destroy(aContext);
    if (mColorData && !(aBits & NS_STYLE_INHERIT_COLOR))
      mColorData->Destroy(aContext);
    if (mQuotesData && !(aBits & NS_STYLE_INHERIT_QUOTES))
      mQuotesData->Destroy(aContext);
    if (mTextData && !(aBits & NS_STYLE_INHERIT_TEXT))
      mTextData->Destroy(aContext);
    if (mUIData && !(aBits & NS_STYLE_INHERIT_UI))
      mUIData->Destroy(aContext);
    aContext->FreeToShell(sizeof(nsInheritedStyleData), this);
  };

  nsInheritedStyleData() 
    :mVisibilityData(nsnull), mFontData(nsnull), mListData(nsnull), 
     mTableData(nsnull), mColorData(nsnull), mQuotesData(nsnull), mTextData(nsnull), mUIData(nsnull)
  {};
};

struct nsResetStyleData
{
  nsResetStyleData()
    :mDisplayData(nsnull), mMarginData(nsnull), mBorderData(nsnull), mPaddingData(nsnull), 
     mOutlineData(nsnull), mPositionData(nsnull), mTableData(nsnull), mBackgroundData(nsnull),
     mContentData(nsnull), mTextData(nsnull), mUIData(nsnull)
  {
#ifdef INCLUDE_XUL
    mXULData = nsnull;
#endif
  };

  void* operator new(size_t sz, nsIPresContext* aContext) {
    void* result = nsnull;
    aContext->AllocateFromShell(sz, &result);
    return result;
  }

  void ClearInheritedData(PRUint32 aBits) {
    if (mDisplayData && (aBits & NS_STYLE_INHERIT_DISPLAY))
      mDisplayData = nsnull;
    if (mMarginData && (aBits & NS_STYLE_INHERIT_MARGIN))
      mMarginData = nsnull;
    if (mBorderData && (aBits & NS_STYLE_INHERIT_BORDER))
      mBorderData = nsnull;
    if (mPaddingData && (aBits & NS_STYLE_INHERIT_PADDING))
      mPaddingData = nsnull;
    if (mOutlineData && (aBits & NS_STYLE_INHERIT_OUTLINE))
      mOutlineData = nsnull;
    if (mPositionData && (aBits & NS_STYLE_INHERIT_POSITION))
      mPositionData = nsnull;
    if (mTableData && (aBits & NS_STYLE_INHERIT_TABLE))
      mTableData = nsnull;
    if (mBackgroundData && (aBits & NS_STYLE_INHERIT_BACKGROUND))
      mBackgroundData = nsnull;
    if (mContentData && (aBits & NS_STYLE_INHERIT_CONTENT))
      mContentData = nsnull;
    if (mTextData && (aBits & NS_STYLE_INHERIT_TEXT_RESET))
      mTextData = nsnull;
    if (mUIData && (aBits & NS_STYLE_INHERIT_UI_RESET))
      mUIData = nsnull;
#ifdef INCLUDE_XUL
    if (mXULData && (aBits & NS_STYLE_INHERIT_XUL))
      mXULData = nsnull;
#endif
  };

  void Destroy(PRUint32 aBits, nsIPresContext* aContext) {
    if (mDisplayData && !(aBits & NS_STYLE_INHERIT_DISPLAY))
      mDisplayData->Destroy(aContext);
    if (mMarginData && !(aBits & NS_STYLE_INHERIT_MARGIN))
      mMarginData->Destroy(aContext);
    if (mBorderData && !(aBits & NS_STYLE_INHERIT_BORDER))
      mBorderData->Destroy(aContext);
    if (mPaddingData && !(aBits & NS_STYLE_INHERIT_PADDING))
      mPaddingData->Destroy(aContext);
    if (mOutlineData && !(aBits & NS_STYLE_INHERIT_OUTLINE))
      mOutlineData->Destroy(aContext);
    if (mPositionData && !(aBits & NS_STYLE_INHERIT_POSITION))
      mPositionData->Destroy(aContext);
    if (mTableData && !(aBits & NS_STYLE_INHERIT_TABLE))
      mTableData->Destroy(aContext);
    if (mBackgroundData && !(aBits & NS_STYLE_INHERIT_BACKGROUND))
      mBackgroundData->Destroy(aContext);
    if (mContentData && !(aBits & NS_STYLE_INHERIT_CONTENT))
      mContentData->Destroy(aContext);
    if (mTextData && !(aBits & NS_STYLE_INHERIT_TEXT_RESET))
      mTextData->Destroy(aContext);
    if (mUIData && !(aBits & NS_STYLE_INHERIT_UI_RESET))
      mUIData->Destroy(aContext);
#ifdef INCLUDE_XUL
    if (mXULData && !(aBits & NS_STYLE_INHERIT_XUL))
      mXULData->Destroy(aContext);
#endif
    aContext->FreeToShell(sizeof(nsResetStyleData), this);
  };

  nsStyleDisplay* mDisplayData;
  nsStyleMargin* mMarginData;
  nsStyleBorder* mBorderData;
  nsStylePadding* mPaddingData;
  nsStyleOutline* mOutlineData;
  nsStylePosition* mPositionData;
  nsStyleTable* mTableData;
  nsStyleBackground* mBackgroundData;
  nsStyleContent* mContentData;
  nsStyleTextReset* mTextData;
  nsStyleUIReset* mUIData;
#ifdef INCLUDE_XUL
  nsStyleXUL* mXULData;
#endif
};

struct nsCachedStyleData
{
  struct StyleStructInfo {
    ptrdiff_t mCachedStyleDataOffset;
    ptrdiff_t mInheritResetOffset;
    PRBool    mIsReset;
  };

  static StyleStructInfo gInfo[];

  nsInheritedStyleData* mInheritedData;
  nsResetStyleData* mResetData;

  static PRBool IsReset(const nsStyleStructID& aSID) {
    return gInfo[aSID].mIsReset;
  };

  static PRUint32 GetBitForSID(const nsStyleStructID& aSID) {
    return 1 << (aSID - 1);
  };

  nsStyleStruct* GetStyleData(const nsStyleStructID& aSID) {
    const StyleStructInfo& info = gInfo[aSID];
    char* resetOrInheritSlot = NS_REINTERPRET_CAST(char*, this) + info.mCachedStyleDataOffset;
    char* resetOrInherit = NS_REINTERPRET_CAST(char*, *NS_REINTERPRET_CAST(void**, resetOrInheritSlot));
    nsStyleStruct* data = nsnull;
    if (resetOrInherit) {
      char* dataSlot = resetOrInherit + info.mInheritResetOffset;
      data = *NS_REINTERPRET_CAST(nsStyleStruct**, dataSlot);
    }
    return data;
  };

  void ClearInheritedData(PRUint32 aBits) {
    if (mResetData)
      mResetData->ClearInheritedData(aBits);
    if (mInheritedData)
      mInheritedData->ClearInheritedData(aBits);
  }

  void Destroy(PRUint32 aBits, nsIPresContext* aContext) {
    if (mResetData)
      mResetData->Destroy(aBits, aContext);
    if (mInheritedData)
      mInheritedData->Destroy(aBits, aContext);
    mResetData = nsnull;
    mInheritedData = nsnull;
  }

  nsCachedStyleData() :mInheritedData(nsnull), mResetData(nsnull) {};
  ~nsCachedStyleData() {};
};

struct nsRuleData
{
  nsStyleStructID mSID;
  nsIPresContext* mPresContext;
  nsIStyleContext* mStyleContext;
  nsPostResolveFunc mPostResolveCallback;
  PRBool mCanStoreInRuleTree;

  nsIHTMLMappedAttributes* mAttributes; // Can be cached in the rule data by a content node for a post-resolve callback.

  nsCSSFont* mFontData; // Should always be stack-allocated! We don't own these structures!
  nsCSSDisplay* mDisplayData;
  nsCSSMargin* mMarginData;
  nsCSSList* mListData;
  nsCSSPosition* mPositionData;
  nsCSSTable* mTableData;
  nsCSSColor* mColorData;
  nsCSSContent* mContentData;
  nsCSSText* mTextData;
  nsCSSUserInterface* mUIData;

#ifdef INCLUDE_XUL
  nsCSSXUL* mXULData;
#endif

  nsRuleData(const nsStyleStructID& aSID, nsIPresContext* aContext, nsIStyleContext* aStyleContext) 
    :mSID(aSID), mPresContext(aContext), mStyleContext(aStyleContext), mPostResolveCallback(nsnull),
     mAttributes(nsnull), mFontData(nsnull), mDisplayData(nsnull), mMarginData(nsnull), mListData(nsnull), 
     mPositionData(nsnull), mTableData(nsnull), mColorData(nsnull), mContentData(nsnull), mTextData(nsnull),
     mUIData(nsnull)
  {
    mCanStoreInRuleTree = PR_TRUE;

#ifdef INCLUDE_XUL
    mXULData = nsnull;
#endif
  };
  ~nsRuleData() {};
};

// {ED86146A-00A0-4a9d-B075-91C164D8FF20}
#define NS_IRULENODE_IID \
{ 0xed86146a, 0xa0, 0x4a9d, { 0xb0, 0x75, 0x91, 0xc1, 0x64, 0xd8, 0xff, 0x20 } }

class nsIRuleNode : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IRULENODE_IID)

  enum { // Types of RuleBits
    eNoneBits,
    eInheritBits
  };

  NS_IMETHOD GetBits(PRInt32 aType, PRUint32* aResult)=0;

  NS_IMETHOD Transition(nsIStyleRule* aRule, nsIRuleNode** aResult)=0;
  
  NS_IMETHOD GetParent(nsIRuleNode** aResult)=0;
  NS_IMETHOD IsRoot(PRBool* aResult)=0;

  NS_IMETHOD GetRule(nsIStyleRule** aResult)=0;

  NS_IMETHOD GetPresContext(nsIPresContext** aResult)=0;

  NS_IMETHOD PathContainsRule(nsIStyleRule* aRule, PRBool* aMatched) = 0;

  NS_IMETHOD ClearCachedData(nsIStyleRule* aRule)=0;
  NS_IMETHOD ClearCachedDataInSubtree(nsIStyleRule* aRule)=0;
  
  virtual const nsStyleStruct* GetStyleData(nsStyleStructID aSID, 
                                            nsIStyleContext* aContext)=0;
};

#endif
