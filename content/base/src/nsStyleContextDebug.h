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
  PRBool mSetFromParent;
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
  PRBool mSetFromParent;
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
  PRBool mSetFromParent;
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
  PRBool mSetFromParent;
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
  PRBool mSetFromParent;
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
  PRBool mSetFromParent;
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
  PRBool mSetFromParent;
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
  PRBool mSetFromParent;
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
  PRBool mSetFromParent;
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
  PRBool mSetFromParent;
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
  PRBool mSetFromParent;
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
  PRBool mSetFromParent;
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
  PRBool mSetFromParent;
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
  PRBool mSetFromParent;
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
  PRBool mSetFromParent;
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
                                  nsIStyleContext* aStyleContext, PRBool aEnteringFunction);

#endif // LOG_GET_STYLE_DATA_CALLS

//=========================================================================================================

static void LogWriteMutableStyleDataCall(nsStyleStructID aSID,
                                  nsStyleStruct* aStyleStruct,
                                  StyleContextImpl* aStyleContext);

