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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 */

//
// IMPORTANT:
//   This is not a real header file.  It is only included in nsStyleContext.cpp
//


#ifdef LOG_STYLE_STRUCTS
//  StyleFontImpl           mFont;
struct StyleFontImplLog: public StyleFontImpl {
  StyleFontImplLog(const nsFont& aVariableFont, const nsFont& aFixedFont)
    : StyleFontImpl(aVariableFont, aFixedFont),
    mInternalFont(aVariableFont, aFixedFont)
  {}
  void ResetFrom(const nsStyleFont* aParent, nsIPresContext* aPresContext);
  StyleFontImpl mInternalFont;
  bool mSetFromParent;
};

void StyleFontImplLog::ResetFrom(const nsStyleFont* aParent, nsIPresContext* aPresContext)
{
	StyleFontImpl::ResetFrom(aParent, aPresContext);
	CopyTo(mInternalFont);
	mSetFromParent = (aParent != nsnull);
}

//  StyleColorImpl          mColor;
struct StyleColorImplLog: public StyleColorImpl {
  void ResetFrom(const nsStyleColor* aParent, nsIPresContext* aPresContext);
  StyleColorImpl mInternalColor;
  bool mSetFromParent;
};

void StyleColorImplLog::ResetFrom(const nsStyleColor* aParent, nsIPresContext* aPresContext)
{
	StyleColorImpl::ResetFrom(aParent, aPresContext);
	CopyTo(mInternalColor);
	mSetFromParent = (aParent != nsnull);
}

//  StyleListImpl           mList;
struct StyleListImplLog: public StyleListImpl {
  void ResetFrom(const nsStyleList* aParent, nsIPresContext* aPresContext);
  StyleListImpl mInternalList;
  bool mSetFromParent;
};

void StyleListImplLog::ResetFrom(const nsStyleList* aParent, nsIPresContext* aPresContext)
{
	StyleListImpl::ResetFrom(aParent, aPresContext);
	CopyTo(mInternalList);
	mSetFromParent = (aParent != nsnull);
}

//  StylePositionImpl       mPosition;
struct StylePositionImplLog: public StylePositionImpl {
  void ResetFrom(const nsStylePosition* aParent, nsIPresContext* aPresContext);
  StylePositionImpl mInternalPosition;
  bool mSetFromParent;
};

void StylePositionImplLog::ResetFrom(const nsStylePosition* aParent, nsIPresContext* aPresContext)
{
	StylePositionImpl::ResetFrom(aParent, aPresContext);
	CopyTo(mInternalPosition);
	mSetFromParent = (aParent != nsnull);
}

//  StyleTextImpl           mText;
struct StyleTextImplLog: public StyleTextImpl {
  void ResetFrom(const nsStyleText* aParent, nsIPresContext* aPresContext);
  StyleTextImpl mInternalText;
  bool mSetFromParent;
};

void StyleTextImplLog::ResetFrom(const nsStyleText* aParent, nsIPresContext* aPresContext)
{
	StyleTextImpl::ResetFrom(aParent, aPresContext);
	CopyTo(mInternalText);
	mSetFromParent = (aParent != nsnull);
}

//  StyleDisplayImpl        mDisplay;
struct StyleDisplayImplLog: public StyleDisplayImpl {
  void ResetFrom(const nsStyleDisplay* aParent, nsIPresContext* aPresContext);
  StyleDisplayImpl mInternalDisplay;
  bool mSetFromParent;
};

void StyleDisplayImplLog::ResetFrom(const nsStyleDisplay* aParent, nsIPresContext* aPresContext)
{
	StyleDisplayImpl::ResetFrom(aParent, aPresContext);
	CopyTo(mInternalDisplay);
	mSetFromParent = (aParent != nsnull);
}

//  StyleTableImpl          mTable;
struct StyleTableImplLog: public StyleTableImpl {
  void ResetFrom(const nsStyleTable* aParent, nsIPresContext* aPresContext);
  StyleTableImpl mInternalTable;
  bool mSetFromParent;
};

void StyleTableImplLog::ResetFrom(const nsStyleTable* aParent, nsIPresContext* aPresContext)
{
	StyleTableImpl::ResetFrom(aParent, aPresContext);
	CopyTo(mInternalTable);
	mSetFromParent = (aParent != nsnull);
}

//  StyleContentImpl        mContent;
struct StyleContentImplLog: public StyleContentImpl {
  void ResetFrom(const StyleContentImpl* aParent, nsIPresContext* aPresContext);
  StyleContentImpl mInternalContent;
  bool mSetFromParent;
};

void StyleContentImplLog::ResetFrom(const StyleContentImpl* aParent, nsIPresContext* aPresContext)
{
	StyleContentImpl::ResetFrom(aParent, aPresContext);
	CopyTo(mInternalContent);
	mSetFromParent = (aParent != nsnull);
}

//  StyleUserInterfaceImpl  mUserInterface;
struct StyleUserInterfaceImplLog: public StyleUserInterfaceImpl {
  void ResetFrom(const nsStyleUserInterface* aParent, nsIPresContext* aPresContext);
  StyleUserInterfaceImpl mInternalUserInterface;
  bool mSetFromParent;
};

void StyleUserInterfaceImplLog::ResetFrom(const nsStyleUserInterface* aParent, nsIPresContext* aPresContext)
{
	StyleUserInterfaceImpl::ResetFrom(aParent, aPresContext);
	CopyTo(mInternalUserInterface);
	mSetFromParent = (aParent != nsnull);
}

//	StylePrintImpl					mPrint;
struct StylePrintImplLog: public StylePrintImpl {
  void ResetFrom(const nsStylePrint* aParent, nsIPresContext* aPresContext);
  StylePrintImpl mInternalPrint;
  bool mSetFromParent;
};

void StylePrintImplLog::ResetFrom(const nsStylePrint* aParent, nsIPresContext* aPresContext)
{
	StylePrintImpl::ResetFrom(aParent, aPresContext);
	CopyTo(mInternalPrint);
	mSetFromParent = (aParent != nsnull);
}

//	StyleMarginImpl					mMargin;
struct StyleMarginImplLog: public StyleMarginImpl {
  void ResetFrom(const nsStyleMargin* aParent, nsIPresContext* aPresContext);
  StyleMarginImpl mInternalMargin;
  bool mSetFromParent;
};

void StyleMarginImplLog::ResetFrom(const nsStyleMargin* aParent, nsIPresContext* aPresContext)
{
	StyleMarginImpl::ResetFrom(aParent, aPresContext);
	CopyTo(mInternalMargin);
	mSetFromParent = (aParent != nsnull);
}

//	StylePaddingImpl				mPadding;
struct StylePaddingImplLog: public StylePaddingImpl {
  void ResetFrom(const nsStylePadding* aParent, nsIPresContext* aPresContext);
  StylePaddingImpl mInternalPadding;
  bool mSetFromParent;
};

void StylePaddingImplLog::ResetFrom(const nsStylePadding* aParent, nsIPresContext* aPresContext)
{
	StylePaddingImpl::ResetFrom(aParent, aPresContext);
	CopyTo(mInternalPadding);
	mSetFromParent = (aParent != nsnull);
}

//	StyleBorderImpl					mBorder;
struct StyleBorderImplLog: public StyleBorderImpl {
  void ResetFrom(const nsStyleBorder* aParent, nsIPresContext* aPresContext);
  StyleBorderImpl mInternalBorder;
  bool mSetFromParent;
};

void StyleBorderImplLog::ResetFrom(const nsStyleBorder* aParent, nsIPresContext* aPresContext)
{
	StyleBorderImpl::ResetFrom(aParent, aPresContext);
	CopyTo(mInternalBorder);
	mSetFromParent = (aParent != nsnull);
}

//	StyleOutlineImpl				mOutline;
struct StyleOutlineImplLog: public StyleOutlineImpl {
  void ResetFrom(const nsStyleOutline* aParent, nsIPresContext* aPresContext);
  StyleOutlineImpl mInternalOutline;
  bool mSetFromParent;
};

void StyleOutlineImplLog::ResetFrom(const nsStyleOutline* aParent, nsIPresContext* aPresContext)
{
	StyleOutlineImpl::ResetFrom(aParent, aPresContext);
	CopyTo(mInternalOutline);
	mSetFromParent = (aParent != nsnull);
}

//  StyleXULImpl       mXUL;
#ifdef INCLUDE_XUL
struct StyleXULImplLog: public StyleXULImpl {
  void ResetFrom(const nsStyleXUL* aParent, nsIPresContext* aPresContext);
  StyleXULImpl mInternalXUL;
  bool mSetFromParent;
};

void StyleXULImplLog::ResetFrom(const nsStyleXUL* aParent, nsIPresContext* aPresContext)
{
	StyleXULImpl::ResetFrom(aParent, aPresContext);
	CopyTo(mInternalXUL);
	mSetFromParent = (aParent != nsnull);
}
#endif // INCLUDE_XUL

//=============================

static void LogStyleStructs(nsStyleContextData* aStyleContextData);

#endif // LOG_STYLE_STRUCTS


#ifdef XP_MAC
#pragma mark -
#endif
//=========================================================================================================

#ifdef LOG_GET_STYLE_DATA_CALLS

enum LogCallType {
  logCallType_GetStyleData = 0,
  logCallType_ReadMutableStyleData,
  logCallType_WriteMutableStyleData,
  logCallType_GetStyle,

  logCallType_Max
};

static void LogGetStyleDataCall(nsStyleStructID aSID, LogCallType aLogCallType,
                                  nsIStyleContext* aStyleContext, bool aEnteringFunction);

#endif // LOG_GET_STYLE_DATA_CALLS

//=========================================================================================================

static void LogWriteMutableStyleDataCall(nsStyleStructID aSID,
                                  nsStyleStruct* aStyleStruct,
                                  StyleContextImpl* aStyleContext);

