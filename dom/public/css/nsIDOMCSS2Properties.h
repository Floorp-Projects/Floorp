/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* AUTO-GENERATED. DO NOT EDIT!!! */

#ifndef nsIDOMCSS2Properties_h__
#define nsIDOMCSS2Properties_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMCSSStyleDeclaration.h"


#define NS_IDOMCSS2PROPERTIES_IID \
 { 0xa6cf90d1, 0x15b3, 0x11d2, \
    {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMCSS2Properties : public nsIDOMCSSStyleDeclaration {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMCSS2PROPERTIES_IID; return iid; }

  NS_IMETHOD    GetAzimuth(nsString& aAzimuth)=0;
  NS_IMETHOD    SetAzimuth(const nsString& aAzimuth)=0;

  NS_IMETHOD    GetBackground(nsString& aBackground)=0;
  NS_IMETHOD    SetBackground(const nsString& aBackground)=0;

  NS_IMETHOD    GetBackgroundAttachment(nsString& aBackgroundAttachment)=0;
  NS_IMETHOD    SetBackgroundAttachment(const nsString& aBackgroundAttachment)=0;

  NS_IMETHOD    GetBackgroundColor(nsString& aBackgroundColor)=0;
  NS_IMETHOD    SetBackgroundColor(const nsString& aBackgroundColor)=0;

  NS_IMETHOD    GetBackgroundImage(nsString& aBackgroundImage)=0;
  NS_IMETHOD    SetBackgroundImage(const nsString& aBackgroundImage)=0;

  NS_IMETHOD    GetBackgroundPosition(nsString& aBackgroundPosition)=0;
  NS_IMETHOD    SetBackgroundPosition(const nsString& aBackgroundPosition)=0;

  NS_IMETHOD    GetBackgroundRepeat(nsString& aBackgroundRepeat)=0;
  NS_IMETHOD    SetBackgroundRepeat(const nsString& aBackgroundRepeat)=0;

  NS_IMETHOD    GetBorder(nsString& aBorder)=0;
  NS_IMETHOD    SetBorder(const nsString& aBorder)=0;

  NS_IMETHOD    GetBorderCollapse(nsString& aBorderCollapse)=0;
  NS_IMETHOD    SetBorderCollapse(const nsString& aBorderCollapse)=0;

  NS_IMETHOD    GetBorderColor(nsString& aBorderColor)=0;
  NS_IMETHOD    SetBorderColor(const nsString& aBorderColor)=0;

  NS_IMETHOD    GetBorderSpacing(nsString& aBorderSpacing)=0;
  NS_IMETHOD    SetBorderSpacing(const nsString& aBorderSpacing)=0;

  NS_IMETHOD    GetBorderStyle(nsString& aBorderStyle)=0;
  NS_IMETHOD    SetBorderStyle(const nsString& aBorderStyle)=0;

  NS_IMETHOD    GetBorderTop(nsString& aBorderTop)=0;
  NS_IMETHOD    SetBorderTop(const nsString& aBorderTop)=0;

  NS_IMETHOD    GetBorderRight(nsString& aBorderRight)=0;
  NS_IMETHOD    SetBorderRight(const nsString& aBorderRight)=0;

  NS_IMETHOD    GetBorderBottom(nsString& aBorderBottom)=0;
  NS_IMETHOD    SetBorderBottom(const nsString& aBorderBottom)=0;

  NS_IMETHOD    GetBorderLeft(nsString& aBorderLeft)=0;
  NS_IMETHOD    SetBorderLeft(const nsString& aBorderLeft)=0;

  NS_IMETHOD    GetBorderTopColor(nsString& aBorderTopColor)=0;
  NS_IMETHOD    SetBorderTopColor(const nsString& aBorderTopColor)=0;

  NS_IMETHOD    GetBorderRightColor(nsString& aBorderRightColor)=0;
  NS_IMETHOD    SetBorderRightColor(const nsString& aBorderRightColor)=0;

  NS_IMETHOD    GetBorderBottomColor(nsString& aBorderBottomColor)=0;
  NS_IMETHOD    SetBorderBottomColor(const nsString& aBorderBottomColor)=0;

  NS_IMETHOD    GetBorderLeftColor(nsString& aBorderLeftColor)=0;
  NS_IMETHOD    SetBorderLeftColor(const nsString& aBorderLeftColor)=0;

  NS_IMETHOD    GetBorderTopStyle(nsString& aBorderTopStyle)=0;
  NS_IMETHOD    SetBorderTopStyle(const nsString& aBorderTopStyle)=0;

  NS_IMETHOD    GetBorderRightStyle(nsString& aBorderRightStyle)=0;
  NS_IMETHOD    SetBorderRightStyle(const nsString& aBorderRightStyle)=0;

  NS_IMETHOD    GetBorderBottomStyle(nsString& aBorderBottomStyle)=0;
  NS_IMETHOD    SetBorderBottomStyle(const nsString& aBorderBottomStyle)=0;

  NS_IMETHOD    GetBorderLeftStyle(nsString& aBorderLeftStyle)=0;
  NS_IMETHOD    SetBorderLeftStyle(const nsString& aBorderLeftStyle)=0;

  NS_IMETHOD    GetBorderTopWidth(nsString& aBorderTopWidth)=0;
  NS_IMETHOD    SetBorderTopWidth(const nsString& aBorderTopWidth)=0;

  NS_IMETHOD    GetBorderRightWidth(nsString& aBorderRightWidth)=0;
  NS_IMETHOD    SetBorderRightWidth(const nsString& aBorderRightWidth)=0;

  NS_IMETHOD    GetBorderBottomWidth(nsString& aBorderBottomWidth)=0;
  NS_IMETHOD    SetBorderBottomWidth(const nsString& aBorderBottomWidth)=0;

  NS_IMETHOD    GetBorderLeftWidth(nsString& aBorderLeftWidth)=0;
  NS_IMETHOD    SetBorderLeftWidth(const nsString& aBorderLeftWidth)=0;

  NS_IMETHOD    GetBorderWidth(nsString& aBorderWidth)=0;
  NS_IMETHOD    SetBorderWidth(const nsString& aBorderWidth)=0;

  NS_IMETHOD    GetBottom(nsString& aBottom)=0;
  NS_IMETHOD    SetBottom(const nsString& aBottom)=0;

  NS_IMETHOD    GetCaptionSide(nsString& aCaptionSide)=0;
  NS_IMETHOD    SetCaptionSide(const nsString& aCaptionSide)=0;

  NS_IMETHOD    GetClear(nsString& aClear)=0;
  NS_IMETHOD    SetClear(const nsString& aClear)=0;

  NS_IMETHOD    GetClip(nsString& aClip)=0;
  NS_IMETHOD    SetClip(const nsString& aClip)=0;

  NS_IMETHOD    GetColor(nsString& aColor)=0;
  NS_IMETHOD    SetColor(const nsString& aColor)=0;

  NS_IMETHOD    GetContent(nsString& aContent)=0;
  NS_IMETHOD    SetContent(const nsString& aContent)=0;

  NS_IMETHOD    GetCounterIncrement(nsString& aCounterIncrement)=0;
  NS_IMETHOD    SetCounterIncrement(const nsString& aCounterIncrement)=0;

  NS_IMETHOD    GetCounterReset(nsString& aCounterReset)=0;
  NS_IMETHOD    SetCounterReset(const nsString& aCounterReset)=0;

  NS_IMETHOD    GetCue(nsString& aCue)=0;
  NS_IMETHOD    SetCue(const nsString& aCue)=0;

  NS_IMETHOD    GetCueAfter(nsString& aCueAfter)=0;
  NS_IMETHOD    SetCueAfter(const nsString& aCueAfter)=0;

  NS_IMETHOD    GetCueBefore(nsString& aCueBefore)=0;
  NS_IMETHOD    SetCueBefore(const nsString& aCueBefore)=0;

  NS_IMETHOD    GetCursor(nsString& aCursor)=0;
  NS_IMETHOD    SetCursor(const nsString& aCursor)=0;

  NS_IMETHOD    GetDirection(nsString& aDirection)=0;
  NS_IMETHOD    SetDirection(const nsString& aDirection)=0;

  NS_IMETHOD    GetDisplay(nsString& aDisplay)=0;
  NS_IMETHOD    SetDisplay(const nsString& aDisplay)=0;

  NS_IMETHOD    GetElevation(nsString& aElevation)=0;
  NS_IMETHOD    SetElevation(const nsString& aElevation)=0;

  NS_IMETHOD    GetEmptyCells(nsString& aEmptyCells)=0;
  NS_IMETHOD    SetEmptyCells(const nsString& aEmptyCells)=0;

  NS_IMETHOD    GetCssFloat(nsString& aCssFloat)=0;
  NS_IMETHOD    SetCssFloat(const nsString& aCssFloat)=0;

  NS_IMETHOD    GetFont(nsString& aFont)=0;
  NS_IMETHOD    SetFont(const nsString& aFont)=0;

  NS_IMETHOD    GetFontFamily(nsString& aFontFamily)=0;
  NS_IMETHOD    SetFontFamily(const nsString& aFontFamily)=0;

  NS_IMETHOD    GetFontSize(nsString& aFontSize)=0;
  NS_IMETHOD    SetFontSize(const nsString& aFontSize)=0;

  NS_IMETHOD    GetFontSizeAdjust(nsString& aFontSizeAdjust)=0;
  NS_IMETHOD    SetFontSizeAdjust(const nsString& aFontSizeAdjust)=0;

  NS_IMETHOD    GetFontStretch(nsString& aFontStretch)=0;
  NS_IMETHOD    SetFontStretch(const nsString& aFontStretch)=0;

  NS_IMETHOD    GetFontStyle(nsString& aFontStyle)=0;
  NS_IMETHOD    SetFontStyle(const nsString& aFontStyle)=0;

  NS_IMETHOD    GetFontVariant(nsString& aFontVariant)=0;
  NS_IMETHOD    SetFontVariant(const nsString& aFontVariant)=0;

  NS_IMETHOD    GetFontWeight(nsString& aFontWeight)=0;
  NS_IMETHOD    SetFontWeight(const nsString& aFontWeight)=0;

  NS_IMETHOD    GetHeight(nsString& aHeight)=0;
  NS_IMETHOD    SetHeight(const nsString& aHeight)=0;

  NS_IMETHOD    GetLeft(nsString& aLeft)=0;
  NS_IMETHOD    SetLeft(const nsString& aLeft)=0;

  NS_IMETHOD    GetLetterSpacing(nsString& aLetterSpacing)=0;
  NS_IMETHOD    SetLetterSpacing(const nsString& aLetterSpacing)=0;

  NS_IMETHOD    GetLineHeight(nsString& aLineHeight)=0;
  NS_IMETHOD    SetLineHeight(const nsString& aLineHeight)=0;

  NS_IMETHOD    GetListStyle(nsString& aListStyle)=0;
  NS_IMETHOD    SetListStyle(const nsString& aListStyle)=0;

  NS_IMETHOD    GetListStyleImage(nsString& aListStyleImage)=0;
  NS_IMETHOD    SetListStyleImage(const nsString& aListStyleImage)=0;

  NS_IMETHOD    GetListStylePosition(nsString& aListStylePosition)=0;
  NS_IMETHOD    SetListStylePosition(const nsString& aListStylePosition)=0;

  NS_IMETHOD    GetListStyleType(nsString& aListStyleType)=0;
  NS_IMETHOD    SetListStyleType(const nsString& aListStyleType)=0;

  NS_IMETHOD    GetMargin(nsString& aMargin)=0;
  NS_IMETHOD    SetMargin(const nsString& aMargin)=0;

  NS_IMETHOD    GetMarginTop(nsString& aMarginTop)=0;
  NS_IMETHOD    SetMarginTop(const nsString& aMarginTop)=0;

  NS_IMETHOD    GetMarginRight(nsString& aMarginRight)=0;
  NS_IMETHOD    SetMarginRight(const nsString& aMarginRight)=0;

  NS_IMETHOD    GetMarginBottom(nsString& aMarginBottom)=0;
  NS_IMETHOD    SetMarginBottom(const nsString& aMarginBottom)=0;

  NS_IMETHOD    GetMarginLeft(nsString& aMarginLeft)=0;
  NS_IMETHOD    SetMarginLeft(const nsString& aMarginLeft)=0;

  NS_IMETHOD    GetMarkerOffset(nsString& aMarkerOffset)=0;
  NS_IMETHOD    SetMarkerOffset(const nsString& aMarkerOffset)=0;

  NS_IMETHOD    GetMarks(nsString& aMarks)=0;
  NS_IMETHOD    SetMarks(const nsString& aMarks)=0;

  NS_IMETHOD    GetMaxHeight(nsString& aMaxHeight)=0;
  NS_IMETHOD    SetMaxHeight(const nsString& aMaxHeight)=0;

  NS_IMETHOD    GetMaxWidth(nsString& aMaxWidth)=0;
  NS_IMETHOD    SetMaxWidth(const nsString& aMaxWidth)=0;

  NS_IMETHOD    GetMinHeight(nsString& aMinHeight)=0;
  NS_IMETHOD    SetMinHeight(const nsString& aMinHeight)=0;

  NS_IMETHOD    GetMinWidth(nsString& aMinWidth)=0;
  NS_IMETHOD    SetMinWidth(const nsString& aMinWidth)=0;

  NS_IMETHOD    GetOrphans(nsString& aOrphans)=0;
  NS_IMETHOD    SetOrphans(const nsString& aOrphans)=0;

  NS_IMETHOD    GetOutline(nsString& aOutline)=0;
  NS_IMETHOD    SetOutline(const nsString& aOutline)=0;

  NS_IMETHOD    GetOutlineColor(nsString& aOutlineColor)=0;
  NS_IMETHOD    SetOutlineColor(const nsString& aOutlineColor)=0;

  NS_IMETHOD    GetOutlineStyle(nsString& aOutlineStyle)=0;
  NS_IMETHOD    SetOutlineStyle(const nsString& aOutlineStyle)=0;

  NS_IMETHOD    GetOutlineWidth(nsString& aOutlineWidth)=0;
  NS_IMETHOD    SetOutlineWidth(const nsString& aOutlineWidth)=0;

  NS_IMETHOD    GetOverflow(nsString& aOverflow)=0;
  NS_IMETHOD    SetOverflow(const nsString& aOverflow)=0;

  NS_IMETHOD    GetPadding(nsString& aPadding)=0;
  NS_IMETHOD    SetPadding(const nsString& aPadding)=0;

  NS_IMETHOD    GetPaddingTop(nsString& aPaddingTop)=0;
  NS_IMETHOD    SetPaddingTop(const nsString& aPaddingTop)=0;

  NS_IMETHOD    GetPaddingRight(nsString& aPaddingRight)=0;
  NS_IMETHOD    SetPaddingRight(const nsString& aPaddingRight)=0;

  NS_IMETHOD    GetPaddingBottom(nsString& aPaddingBottom)=0;
  NS_IMETHOD    SetPaddingBottom(const nsString& aPaddingBottom)=0;

  NS_IMETHOD    GetPaddingLeft(nsString& aPaddingLeft)=0;
  NS_IMETHOD    SetPaddingLeft(const nsString& aPaddingLeft)=0;

  NS_IMETHOD    GetPage(nsString& aPage)=0;
  NS_IMETHOD    SetPage(const nsString& aPage)=0;

  NS_IMETHOD    GetPageBreakAfter(nsString& aPageBreakAfter)=0;
  NS_IMETHOD    SetPageBreakAfter(const nsString& aPageBreakAfter)=0;

  NS_IMETHOD    GetPageBreakBefore(nsString& aPageBreakBefore)=0;
  NS_IMETHOD    SetPageBreakBefore(const nsString& aPageBreakBefore)=0;

  NS_IMETHOD    GetPageBreakInside(nsString& aPageBreakInside)=0;
  NS_IMETHOD    SetPageBreakInside(const nsString& aPageBreakInside)=0;

  NS_IMETHOD    GetPause(nsString& aPause)=0;
  NS_IMETHOD    SetPause(const nsString& aPause)=0;

  NS_IMETHOD    GetPauseAfter(nsString& aPauseAfter)=0;
  NS_IMETHOD    SetPauseAfter(const nsString& aPauseAfter)=0;

  NS_IMETHOD    GetPauseBefore(nsString& aPauseBefore)=0;
  NS_IMETHOD    SetPauseBefore(const nsString& aPauseBefore)=0;

  NS_IMETHOD    GetPitch(nsString& aPitch)=0;
  NS_IMETHOD    SetPitch(const nsString& aPitch)=0;

  NS_IMETHOD    GetPitchRange(nsString& aPitchRange)=0;
  NS_IMETHOD    SetPitchRange(const nsString& aPitchRange)=0;

  NS_IMETHOD    GetPlayDuring(nsString& aPlayDuring)=0;
  NS_IMETHOD    SetPlayDuring(const nsString& aPlayDuring)=0;

  NS_IMETHOD    GetPosition(nsString& aPosition)=0;
  NS_IMETHOD    SetPosition(const nsString& aPosition)=0;

  NS_IMETHOD    GetQuotes(nsString& aQuotes)=0;
  NS_IMETHOD    SetQuotes(const nsString& aQuotes)=0;

  NS_IMETHOD    GetRichness(nsString& aRichness)=0;
  NS_IMETHOD    SetRichness(const nsString& aRichness)=0;

  NS_IMETHOD    GetRight(nsString& aRight)=0;
  NS_IMETHOD    SetRight(const nsString& aRight)=0;

  NS_IMETHOD    GetSize(nsString& aSize)=0;
  NS_IMETHOD    SetSize(const nsString& aSize)=0;

  NS_IMETHOD    GetSpeak(nsString& aSpeak)=0;
  NS_IMETHOD    SetSpeak(const nsString& aSpeak)=0;

  NS_IMETHOD    GetSpeakHeader(nsString& aSpeakHeader)=0;
  NS_IMETHOD    SetSpeakHeader(const nsString& aSpeakHeader)=0;

  NS_IMETHOD    GetSpeakNumeral(nsString& aSpeakNumeral)=0;
  NS_IMETHOD    SetSpeakNumeral(const nsString& aSpeakNumeral)=0;

  NS_IMETHOD    GetSpeakPunctuation(nsString& aSpeakPunctuation)=0;
  NS_IMETHOD    SetSpeakPunctuation(const nsString& aSpeakPunctuation)=0;

  NS_IMETHOD    GetSpeechRate(nsString& aSpeechRate)=0;
  NS_IMETHOD    SetSpeechRate(const nsString& aSpeechRate)=0;

  NS_IMETHOD    GetStress(nsString& aStress)=0;
  NS_IMETHOD    SetStress(const nsString& aStress)=0;

  NS_IMETHOD    GetTableLayout(nsString& aTableLayout)=0;
  NS_IMETHOD    SetTableLayout(const nsString& aTableLayout)=0;

  NS_IMETHOD    GetTextAlign(nsString& aTextAlign)=0;
  NS_IMETHOD    SetTextAlign(const nsString& aTextAlign)=0;

  NS_IMETHOD    GetTextDecoration(nsString& aTextDecoration)=0;
  NS_IMETHOD    SetTextDecoration(const nsString& aTextDecoration)=0;

  NS_IMETHOD    GetTextIndent(nsString& aTextIndent)=0;
  NS_IMETHOD    SetTextIndent(const nsString& aTextIndent)=0;

  NS_IMETHOD    GetTextShadow(nsString& aTextShadow)=0;
  NS_IMETHOD    SetTextShadow(const nsString& aTextShadow)=0;

  NS_IMETHOD    GetTextTransform(nsString& aTextTransform)=0;
  NS_IMETHOD    SetTextTransform(const nsString& aTextTransform)=0;

  NS_IMETHOD    GetTop(nsString& aTop)=0;
  NS_IMETHOD    SetTop(const nsString& aTop)=0;

  NS_IMETHOD    GetUnicodeBidi(nsString& aUnicodeBidi)=0;
  NS_IMETHOD    SetUnicodeBidi(const nsString& aUnicodeBidi)=0;

  NS_IMETHOD    GetVerticalAlign(nsString& aVerticalAlign)=0;
  NS_IMETHOD    SetVerticalAlign(const nsString& aVerticalAlign)=0;

  NS_IMETHOD    GetVisibility(nsString& aVisibility)=0;
  NS_IMETHOD    SetVisibility(const nsString& aVisibility)=0;

  NS_IMETHOD    GetVoiceFamily(nsString& aVoiceFamily)=0;
  NS_IMETHOD    SetVoiceFamily(const nsString& aVoiceFamily)=0;

  NS_IMETHOD    GetVolume(nsString& aVolume)=0;
  NS_IMETHOD    SetVolume(const nsString& aVolume)=0;

  NS_IMETHOD    GetWhiteSpace(nsString& aWhiteSpace)=0;
  NS_IMETHOD    SetWhiteSpace(const nsString& aWhiteSpace)=0;

  NS_IMETHOD    GetWidows(nsString& aWidows)=0;
  NS_IMETHOD    SetWidows(const nsString& aWidows)=0;

  NS_IMETHOD    GetWidth(nsString& aWidth)=0;
  NS_IMETHOD    SetWidth(const nsString& aWidth)=0;

  NS_IMETHOD    GetWordSpacing(nsString& aWordSpacing)=0;
  NS_IMETHOD    SetWordSpacing(const nsString& aWordSpacing)=0;

  NS_IMETHOD    GetZIndex(nsString& aZIndex)=0;
  NS_IMETHOD    SetZIndex(const nsString& aZIndex)=0;

  NS_IMETHOD    GetOpacity(nsString& aOpacity)=0;
  NS_IMETHOD    SetOpacity(const nsString& aOpacity)=0;
};


#define NS_DECL_IDOMCSS2PROPERTIES   \
  NS_IMETHOD    GetAzimuth(nsString& aAzimuth);  \
  NS_IMETHOD    SetAzimuth(const nsString& aAzimuth);  \
  NS_IMETHOD    GetBackground(nsString& aBackground);  \
  NS_IMETHOD    SetBackground(const nsString& aBackground);  \
  NS_IMETHOD    GetBackgroundAttachment(nsString& aBackgroundAttachment);  \
  NS_IMETHOD    SetBackgroundAttachment(const nsString& aBackgroundAttachment);  \
  NS_IMETHOD    GetBackgroundColor(nsString& aBackgroundColor);  \
  NS_IMETHOD    SetBackgroundColor(const nsString& aBackgroundColor);  \
  NS_IMETHOD    GetBackgroundImage(nsString& aBackgroundImage);  \
  NS_IMETHOD    SetBackgroundImage(const nsString& aBackgroundImage);  \
  NS_IMETHOD    GetBackgroundPosition(nsString& aBackgroundPosition);  \
  NS_IMETHOD    SetBackgroundPosition(const nsString& aBackgroundPosition);  \
  NS_IMETHOD    GetBackgroundRepeat(nsString& aBackgroundRepeat);  \
  NS_IMETHOD    SetBackgroundRepeat(const nsString& aBackgroundRepeat);  \
  NS_IMETHOD    GetBorder(nsString& aBorder);  \
  NS_IMETHOD    SetBorder(const nsString& aBorder);  \
  NS_IMETHOD    GetBorderCollapse(nsString& aBorderCollapse);  \
  NS_IMETHOD    SetBorderCollapse(const nsString& aBorderCollapse);  \
  NS_IMETHOD    GetBorderColor(nsString& aBorderColor);  \
  NS_IMETHOD    SetBorderColor(const nsString& aBorderColor);  \
  NS_IMETHOD    GetBorderSpacing(nsString& aBorderSpacing);  \
  NS_IMETHOD    SetBorderSpacing(const nsString& aBorderSpacing);  \
  NS_IMETHOD    GetBorderStyle(nsString& aBorderStyle);  \
  NS_IMETHOD    SetBorderStyle(const nsString& aBorderStyle);  \
  NS_IMETHOD    GetBorderTop(nsString& aBorderTop);  \
  NS_IMETHOD    SetBorderTop(const nsString& aBorderTop);  \
  NS_IMETHOD    GetBorderRight(nsString& aBorderRight);  \
  NS_IMETHOD    SetBorderRight(const nsString& aBorderRight);  \
  NS_IMETHOD    GetBorderBottom(nsString& aBorderBottom);  \
  NS_IMETHOD    SetBorderBottom(const nsString& aBorderBottom);  \
  NS_IMETHOD    GetBorderLeft(nsString& aBorderLeft);  \
  NS_IMETHOD    SetBorderLeft(const nsString& aBorderLeft);  \
  NS_IMETHOD    GetBorderTopColor(nsString& aBorderTopColor);  \
  NS_IMETHOD    SetBorderTopColor(const nsString& aBorderTopColor);  \
  NS_IMETHOD    GetBorderRightColor(nsString& aBorderRightColor);  \
  NS_IMETHOD    SetBorderRightColor(const nsString& aBorderRightColor);  \
  NS_IMETHOD    GetBorderBottomColor(nsString& aBorderBottomColor);  \
  NS_IMETHOD    SetBorderBottomColor(const nsString& aBorderBottomColor);  \
  NS_IMETHOD    GetBorderLeftColor(nsString& aBorderLeftColor);  \
  NS_IMETHOD    SetBorderLeftColor(const nsString& aBorderLeftColor);  \
  NS_IMETHOD    GetBorderTopStyle(nsString& aBorderTopStyle);  \
  NS_IMETHOD    SetBorderTopStyle(const nsString& aBorderTopStyle);  \
  NS_IMETHOD    GetBorderRightStyle(nsString& aBorderRightStyle);  \
  NS_IMETHOD    SetBorderRightStyle(const nsString& aBorderRightStyle);  \
  NS_IMETHOD    GetBorderBottomStyle(nsString& aBorderBottomStyle);  \
  NS_IMETHOD    SetBorderBottomStyle(const nsString& aBorderBottomStyle);  \
  NS_IMETHOD    GetBorderLeftStyle(nsString& aBorderLeftStyle);  \
  NS_IMETHOD    SetBorderLeftStyle(const nsString& aBorderLeftStyle);  \
  NS_IMETHOD    GetBorderTopWidth(nsString& aBorderTopWidth);  \
  NS_IMETHOD    SetBorderTopWidth(const nsString& aBorderTopWidth);  \
  NS_IMETHOD    GetBorderRightWidth(nsString& aBorderRightWidth);  \
  NS_IMETHOD    SetBorderRightWidth(const nsString& aBorderRightWidth);  \
  NS_IMETHOD    GetBorderBottomWidth(nsString& aBorderBottomWidth);  \
  NS_IMETHOD    SetBorderBottomWidth(const nsString& aBorderBottomWidth);  \
  NS_IMETHOD    GetBorderLeftWidth(nsString& aBorderLeftWidth);  \
  NS_IMETHOD    SetBorderLeftWidth(const nsString& aBorderLeftWidth);  \
  NS_IMETHOD    GetBorderWidth(nsString& aBorderWidth);  \
  NS_IMETHOD    SetBorderWidth(const nsString& aBorderWidth);  \
  NS_IMETHOD    GetBottom(nsString& aBottom);  \
  NS_IMETHOD    SetBottom(const nsString& aBottom);  \
  NS_IMETHOD    GetCaptionSide(nsString& aCaptionSide);  \
  NS_IMETHOD    SetCaptionSide(const nsString& aCaptionSide);  \
  NS_IMETHOD    GetClear(nsString& aClear);  \
  NS_IMETHOD    SetClear(const nsString& aClear);  \
  NS_IMETHOD    GetClip(nsString& aClip);  \
  NS_IMETHOD    SetClip(const nsString& aClip);  \
  NS_IMETHOD    GetColor(nsString& aColor);  \
  NS_IMETHOD    SetColor(const nsString& aColor);  \
  NS_IMETHOD    GetContent(nsString& aContent);  \
  NS_IMETHOD    SetContent(const nsString& aContent);  \
  NS_IMETHOD    GetCounterIncrement(nsString& aCounterIncrement);  \
  NS_IMETHOD    SetCounterIncrement(const nsString& aCounterIncrement);  \
  NS_IMETHOD    GetCounterReset(nsString& aCounterReset);  \
  NS_IMETHOD    SetCounterReset(const nsString& aCounterReset);  \
  NS_IMETHOD    GetCue(nsString& aCue);  \
  NS_IMETHOD    SetCue(const nsString& aCue);  \
  NS_IMETHOD    GetCueAfter(nsString& aCueAfter);  \
  NS_IMETHOD    SetCueAfter(const nsString& aCueAfter);  \
  NS_IMETHOD    GetCueBefore(nsString& aCueBefore);  \
  NS_IMETHOD    SetCueBefore(const nsString& aCueBefore);  \
  NS_IMETHOD    GetCursor(nsString& aCursor);  \
  NS_IMETHOD    SetCursor(const nsString& aCursor);  \
  NS_IMETHOD    GetDirection(nsString& aDirection);  \
  NS_IMETHOD    SetDirection(const nsString& aDirection);  \
  NS_IMETHOD    GetDisplay(nsString& aDisplay);  \
  NS_IMETHOD    SetDisplay(const nsString& aDisplay);  \
  NS_IMETHOD    GetElevation(nsString& aElevation);  \
  NS_IMETHOD    SetElevation(const nsString& aElevation);  \
  NS_IMETHOD    GetEmptyCells(nsString& aEmptyCells);  \
  NS_IMETHOD    SetEmptyCells(const nsString& aEmptyCells);  \
  NS_IMETHOD    GetCssFloat(nsString& aCssFloat);  \
  NS_IMETHOD    SetCssFloat(const nsString& aCssFloat);  \
  NS_IMETHOD    GetFont(nsString& aFont);  \
  NS_IMETHOD    SetFont(const nsString& aFont);  \
  NS_IMETHOD    GetFontFamily(nsString& aFontFamily);  \
  NS_IMETHOD    SetFontFamily(const nsString& aFontFamily);  \
  NS_IMETHOD    GetFontSize(nsString& aFontSize);  \
  NS_IMETHOD    SetFontSize(const nsString& aFontSize);  \
  NS_IMETHOD    GetFontSizeAdjust(nsString& aFontSizeAdjust);  \
  NS_IMETHOD    SetFontSizeAdjust(const nsString& aFontSizeAdjust);  \
  NS_IMETHOD    GetFontStretch(nsString& aFontStretch);  \
  NS_IMETHOD    SetFontStretch(const nsString& aFontStretch);  \
  NS_IMETHOD    GetFontStyle(nsString& aFontStyle);  \
  NS_IMETHOD    SetFontStyle(const nsString& aFontStyle);  \
  NS_IMETHOD    GetFontVariant(nsString& aFontVariant);  \
  NS_IMETHOD    SetFontVariant(const nsString& aFontVariant);  \
  NS_IMETHOD    GetFontWeight(nsString& aFontWeight);  \
  NS_IMETHOD    SetFontWeight(const nsString& aFontWeight);  \
  NS_IMETHOD    GetHeight(nsString& aHeight);  \
  NS_IMETHOD    SetHeight(const nsString& aHeight);  \
  NS_IMETHOD    GetLeft(nsString& aLeft);  \
  NS_IMETHOD    SetLeft(const nsString& aLeft);  \
  NS_IMETHOD    GetLetterSpacing(nsString& aLetterSpacing);  \
  NS_IMETHOD    SetLetterSpacing(const nsString& aLetterSpacing);  \
  NS_IMETHOD    GetLineHeight(nsString& aLineHeight);  \
  NS_IMETHOD    SetLineHeight(const nsString& aLineHeight);  \
  NS_IMETHOD    GetListStyle(nsString& aListStyle);  \
  NS_IMETHOD    SetListStyle(const nsString& aListStyle);  \
  NS_IMETHOD    GetListStyleImage(nsString& aListStyleImage);  \
  NS_IMETHOD    SetListStyleImage(const nsString& aListStyleImage);  \
  NS_IMETHOD    GetListStylePosition(nsString& aListStylePosition);  \
  NS_IMETHOD    SetListStylePosition(const nsString& aListStylePosition);  \
  NS_IMETHOD    GetListStyleType(nsString& aListStyleType);  \
  NS_IMETHOD    SetListStyleType(const nsString& aListStyleType);  \
  NS_IMETHOD    GetMargin(nsString& aMargin);  \
  NS_IMETHOD    SetMargin(const nsString& aMargin);  \
  NS_IMETHOD    GetMarginTop(nsString& aMarginTop);  \
  NS_IMETHOD    SetMarginTop(const nsString& aMarginTop);  \
  NS_IMETHOD    GetMarginRight(nsString& aMarginRight);  \
  NS_IMETHOD    SetMarginRight(const nsString& aMarginRight);  \
  NS_IMETHOD    GetMarginBottom(nsString& aMarginBottom);  \
  NS_IMETHOD    SetMarginBottom(const nsString& aMarginBottom);  \
  NS_IMETHOD    GetMarginLeft(nsString& aMarginLeft);  \
  NS_IMETHOD    SetMarginLeft(const nsString& aMarginLeft);  \
  NS_IMETHOD    GetMarkerOffset(nsString& aMarkerOffset);  \
  NS_IMETHOD    SetMarkerOffset(const nsString& aMarkerOffset);  \
  NS_IMETHOD    GetMarks(nsString& aMarks);  \
  NS_IMETHOD    SetMarks(const nsString& aMarks);  \
  NS_IMETHOD    GetMaxHeight(nsString& aMaxHeight);  \
  NS_IMETHOD    SetMaxHeight(const nsString& aMaxHeight);  \
  NS_IMETHOD    GetMaxWidth(nsString& aMaxWidth);  \
  NS_IMETHOD    SetMaxWidth(const nsString& aMaxWidth);  \
  NS_IMETHOD    GetMinHeight(nsString& aMinHeight);  \
  NS_IMETHOD    SetMinHeight(const nsString& aMinHeight);  \
  NS_IMETHOD    GetMinWidth(nsString& aMinWidth);  \
  NS_IMETHOD    SetMinWidth(const nsString& aMinWidth);  \
  NS_IMETHOD    GetOrphans(nsString& aOrphans);  \
  NS_IMETHOD    SetOrphans(const nsString& aOrphans);  \
  NS_IMETHOD    GetOutline(nsString& aOutline);  \
  NS_IMETHOD    SetOutline(const nsString& aOutline);  \
  NS_IMETHOD    GetOutlineColor(nsString& aOutlineColor);  \
  NS_IMETHOD    SetOutlineColor(const nsString& aOutlineColor);  \
  NS_IMETHOD    GetOutlineStyle(nsString& aOutlineStyle);  \
  NS_IMETHOD    SetOutlineStyle(const nsString& aOutlineStyle);  \
  NS_IMETHOD    GetOutlineWidth(nsString& aOutlineWidth);  \
  NS_IMETHOD    SetOutlineWidth(const nsString& aOutlineWidth);  \
  NS_IMETHOD    GetOverflow(nsString& aOverflow);  \
  NS_IMETHOD    SetOverflow(const nsString& aOverflow);  \
  NS_IMETHOD    GetPadding(nsString& aPadding);  \
  NS_IMETHOD    SetPadding(const nsString& aPadding);  \
  NS_IMETHOD    GetPaddingTop(nsString& aPaddingTop);  \
  NS_IMETHOD    SetPaddingTop(const nsString& aPaddingTop);  \
  NS_IMETHOD    GetPaddingRight(nsString& aPaddingRight);  \
  NS_IMETHOD    SetPaddingRight(const nsString& aPaddingRight);  \
  NS_IMETHOD    GetPaddingBottom(nsString& aPaddingBottom);  \
  NS_IMETHOD    SetPaddingBottom(const nsString& aPaddingBottom);  \
  NS_IMETHOD    GetPaddingLeft(nsString& aPaddingLeft);  \
  NS_IMETHOD    SetPaddingLeft(const nsString& aPaddingLeft);  \
  NS_IMETHOD    GetPage(nsString& aPage);  \
  NS_IMETHOD    SetPage(const nsString& aPage);  \
  NS_IMETHOD    GetPageBreakAfter(nsString& aPageBreakAfter);  \
  NS_IMETHOD    SetPageBreakAfter(const nsString& aPageBreakAfter);  \
  NS_IMETHOD    GetPageBreakBefore(nsString& aPageBreakBefore);  \
  NS_IMETHOD    SetPageBreakBefore(const nsString& aPageBreakBefore);  \
  NS_IMETHOD    GetPageBreakInside(nsString& aPageBreakInside);  \
  NS_IMETHOD    SetPageBreakInside(const nsString& aPageBreakInside);  \
  NS_IMETHOD    GetPause(nsString& aPause);  \
  NS_IMETHOD    SetPause(const nsString& aPause);  \
  NS_IMETHOD    GetPauseAfter(nsString& aPauseAfter);  \
  NS_IMETHOD    SetPauseAfter(const nsString& aPauseAfter);  \
  NS_IMETHOD    GetPauseBefore(nsString& aPauseBefore);  \
  NS_IMETHOD    SetPauseBefore(const nsString& aPauseBefore);  \
  NS_IMETHOD    GetPitch(nsString& aPitch);  \
  NS_IMETHOD    SetPitch(const nsString& aPitch);  \
  NS_IMETHOD    GetPitchRange(nsString& aPitchRange);  \
  NS_IMETHOD    SetPitchRange(const nsString& aPitchRange);  \
  NS_IMETHOD    GetPlayDuring(nsString& aPlayDuring);  \
  NS_IMETHOD    SetPlayDuring(const nsString& aPlayDuring);  \
  NS_IMETHOD    GetPosition(nsString& aPosition);  \
  NS_IMETHOD    SetPosition(const nsString& aPosition);  \
  NS_IMETHOD    GetQuotes(nsString& aQuotes);  \
  NS_IMETHOD    SetQuotes(const nsString& aQuotes);  \
  NS_IMETHOD    GetRichness(nsString& aRichness);  \
  NS_IMETHOD    SetRichness(const nsString& aRichness);  \
  NS_IMETHOD    GetRight(nsString& aRight);  \
  NS_IMETHOD    SetRight(const nsString& aRight);  \
  NS_IMETHOD    GetSize(nsString& aSize);  \
  NS_IMETHOD    SetSize(const nsString& aSize);  \
  NS_IMETHOD    GetSpeak(nsString& aSpeak);  \
  NS_IMETHOD    SetSpeak(const nsString& aSpeak);  \
  NS_IMETHOD    GetSpeakHeader(nsString& aSpeakHeader);  \
  NS_IMETHOD    SetSpeakHeader(const nsString& aSpeakHeader);  \
  NS_IMETHOD    GetSpeakNumeral(nsString& aSpeakNumeral);  \
  NS_IMETHOD    SetSpeakNumeral(const nsString& aSpeakNumeral);  \
  NS_IMETHOD    GetSpeakPunctuation(nsString& aSpeakPunctuation);  \
  NS_IMETHOD    SetSpeakPunctuation(const nsString& aSpeakPunctuation);  \
  NS_IMETHOD    GetSpeechRate(nsString& aSpeechRate);  \
  NS_IMETHOD    SetSpeechRate(const nsString& aSpeechRate);  \
  NS_IMETHOD    GetStress(nsString& aStress);  \
  NS_IMETHOD    SetStress(const nsString& aStress);  \
  NS_IMETHOD    GetTableLayout(nsString& aTableLayout);  \
  NS_IMETHOD    SetTableLayout(const nsString& aTableLayout);  \
  NS_IMETHOD    GetTextAlign(nsString& aTextAlign);  \
  NS_IMETHOD    SetTextAlign(const nsString& aTextAlign);  \
  NS_IMETHOD    GetTextDecoration(nsString& aTextDecoration);  \
  NS_IMETHOD    SetTextDecoration(const nsString& aTextDecoration);  \
  NS_IMETHOD    GetTextIndent(nsString& aTextIndent);  \
  NS_IMETHOD    SetTextIndent(const nsString& aTextIndent);  \
  NS_IMETHOD    GetTextShadow(nsString& aTextShadow);  \
  NS_IMETHOD    SetTextShadow(const nsString& aTextShadow);  \
  NS_IMETHOD    GetTextTransform(nsString& aTextTransform);  \
  NS_IMETHOD    SetTextTransform(const nsString& aTextTransform);  \
  NS_IMETHOD    GetTop(nsString& aTop);  \
  NS_IMETHOD    SetTop(const nsString& aTop);  \
  NS_IMETHOD    GetUnicodeBidi(nsString& aUnicodeBidi);  \
  NS_IMETHOD    SetUnicodeBidi(const nsString& aUnicodeBidi);  \
  NS_IMETHOD    GetVerticalAlign(nsString& aVerticalAlign);  \
  NS_IMETHOD    SetVerticalAlign(const nsString& aVerticalAlign);  \
  NS_IMETHOD    GetVisibility(nsString& aVisibility);  \
  NS_IMETHOD    SetVisibility(const nsString& aVisibility);  \
  NS_IMETHOD    GetVoiceFamily(nsString& aVoiceFamily);  \
  NS_IMETHOD    SetVoiceFamily(const nsString& aVoiceFamily);  \
  NS_IMETHOD    GetVolume(nsString& aVolume);  \
  NS_IMETHOD    SetVolume(const nsString& aVolume);  \
  NS_IMETHOD    GetWhiteSpace(nsString& aWhiteSpace);  \
  NS_IMETHOD    SetWhiteSpace(const nsString& aWhiteSpace);  \
  NS_IMETHOD    GetWidows(nsString& aWidows);  \
  NS_IMETHOD    SetWidows(const nsString& aWidows);  \
  NS_IMETHOD    GetWidth(nsString& aWidth);  \
  NS_IMETHOD    SetWidth(const nsString& aWidth);  \
  NS_IMETHOD    GetWordSpacing(nsString& aWordSpacing);  \
  NS_IMETHOD    SetWordSpacing(const nsString& aWordSpacing);  \
  NS_IMETHOD    GetZIndex(nsString& aZIndex);  \
  NS_IMETHOD    SetZIndex(const nsString& aZIndex);  \
  NS_IMETHOD    GetOpacity(nsString& aOpacity);  \
  NS_IMETHOD    SetOpacity(const nsString& aOpacity);  \



#define NS_FORWARD_IDOMCSS2PROPERTIES(_to)  \
  NS_IMETHOD    GetAzimuth(nsString& aAzimuth) { return _to GetAzimuth(aAzimuth); } \
  NS_IMETHOD    SetAzimuth(const nsString& aAzimuth) { return _to SetAzimuth(aAzimuth); } \
  NS_IMETHOD    GetBackground(nsString& aBackground) { return _to GetBackground(aBackground); } \
  NS_IMETHOD    SetBackground(const nsString& aBackground) { return _to SetBackground(aBackground); } \
  NS_IMETHOD    GetBackgroundAttachment(nsString& aBackgroundAttachment) { return _to GetBackgroundAttachment(aBackgroundAttachment); } \
  NS_IMETHOD    SetBackgroundAttachment(const nsString& aBackgroundAttachment) { return _to SetBackgroundAttachment(aBackgroundAttachment); } \
  NS_IMETHOD    GetBackgroundColor(nsString& aBackgroundColor) { return _to GetBackgroundColor(aBackgroundColor); } \
  NS_IMETHOD    SetBackgroundColor(const nsString& aBackgroundColor) { return _to SetBackgroundColor(aBackgroundColor); } \
  NS_IMETHOD    GetBackgroundImage(nsString& aBackgroundImage) { return _to GetBackgroundImage(aBackgroundImage); } \
  NS_IMETHOD    SetBackgroundImage(const nsString& aBackgroundImage) { return _to SetBackgroundImage(aBackgroundImage); } \
  NS_IMETHOD    GetBackgroundPosition(nsString& aBackgroundPosition) { return _to GetBackgroundPosition(aBackgroundPosition); } \
  NS_IMETHOD    SetBackgroundPosition(const nsString& aBackgroundPosition) { return _to SetBackgroundPosition(aBackgroundPosition); } \
  NS_IMETHOD    GetBackgroundRepeat(nsString& aBackgroundRepeat) { return _to GetBackgroundRepeat(aBackgroundRepeat); } \
  NS_IMETHOD    SetBackgroundRepeat(const nsString& aBackgroundRepeat) { return _to SetBackgroundRepeat(aBackgroundRepeat); } \
  NS_IMETHOD    GetBorder(nsString& aBorder) { return _to GetBorder(aBorder); } \
  NS_IMETHOD    SetBorder(const nsString& aBorder) { return _to SetBorder(aBorder); } \
  NS_IMETHOD    GetBorderCollapse(nsString& aBorderCollapse) { return _to GetBorderCollapse(aBorderCollapse); } \
  NS_IMETHOD    SetBorderCollapse(const nsString& aBorderCollapse) { return _to SetBorderCollapse(aBorderCollapse); } \
  NS_IMETHOD    GetBorderColor(nsString& aBorderColor) { return _to GetBorderColor(aBorderColor); } \
  NS_IMETHOD    SetBorderColor(const nsString& aBorderColor) { return _to SetBorderColor(aBorderColor); } \
  NS_IMETHOD    GetBorderSpacing(nsString& aBorderSpacing) { return _to GetBorderSpacing(aBorderSpacing); } \
  NS_IMETHOD    SetBorderSpacing(const nsString& aBorderSpacing) { return _to SetBorderSpacing(aBorderSpacing); } \
  NS_IMETHOD    GetBorderStyle(nsString& aBorderStyle) { return _to GetBorderStyle(aBorderStyle); } \
  NS_IMETHOD    SetBorderStyle(const nsString& aBorderStyle) { return _to SetBorderStyle(aBorderStyle); } \
  NS_IMETHOD    GetBorderTop(nsString& aBorderTop) { return _to GetBorderTop(aBorderTop); } \
  NS_IMETHOD    SetBorderTop(const nsString& aBorderTop) { return _to SetBorderTop(aBorderTop); } \
  NS_IMETHOD    GetBorderRight(nsString& aBorderRight) { return _to GetBorderRight(aBorderRight); } \
  NS_IMETHOD    SetBorderRight(const nsString& aBorderRight) { return _to SetBorderRight(aBorderRight); } \
  NS_IMETHOD    GetBorderBottom(nsString& aBorderBottom) { return _to GetBorderBottom(aBorderBottom); } \
  NS_IMETHOD    SetBorderBottom(const nsString& aBorderBottom) { return _to SetBorderBottom(aBorderBottom); } \
  NS_IMETHOD    GetBorderLeft(nsString& aBorderLeft) { return _to GetBorderLeft(aBorderLeft); } \
  NS_IMETHOD    SetBorderLeft(const nsString& aBorderLeft) { return _to SetBorderLeft(aBorderLeft); } \
  NS_IMETHOD    GetBorderTopColor(nsString& aBorderTopColor) { return _to GetBorderTopColor(aBorderTopColor); } \
  NS_IMETHOD    SetBorderTopColor(const nsString& aBorderTopColor) { return _to SetBorderTopColor(aBorderTopColor); } \
  NS_IMETHOD    GetBorderRightColor(nsString& aBorderRightColor) { return _to GetBorderRightColor(aBorderRightColor); } \
  NS_IMETHOD    SetBorderRightColor(const nsString& aBorderRightColor) { return _to SetBorderRightColor(aBorderRightColor); } \
  NS_IMETHOD    GetBorderBottomColor(nsString& aBorderBottomColor) { return _to GetBorderBottomColor(aBorderBottomColor); } \
  NS_IMETHOD    SetBorderBottomColor(const nsString& aBorderBottomColor) { return _to SetBorderBottomColor(aBorderBottomColor); } \
  NS_IMETHOD    GetBorderLeftColor(nsString& aBorderLeftColor) { return _to GetBorderLeftColor(aBorderLeftColor); } \
  NS_IMETHOD    SetBorderLeftColor(const nsString& aBorderLeftColor) { return _to SetBorderLeftColor(aBorderLeftColor); } \
  NS_IMETHOD    GetBorderTopStyle(nsString& aBorderTopStyle) { return _to GetBorderTopStyle(aBorderTopStyle); } \
  NS_IMETHOD    SetBorderTopStyle(const nsString& aBorderTopStyle) { return _to SetBorderTopStyle(aBorderTopStyle); } \
  NS_IMETHOD    GetBorderRightStyle(nsString& aBorderRightStyle) { return _to GetBorderRightStyle(aBorderRightStyle); } \
  NS_IMETHOD    SetBorderRightStyle(const nsString& aBorderRightStyle) { return _to SetBorderRightStyle(aBorderRightStyle); } \
  NS_IMETHOD    GetBorderBottomStyle(nsString& aBorderBottomStyle) { return _to GetBorderBottomStyle(aBorderBottomStyle); } \
  NS_IMETHOD    SetBorderBottomStyle(const nsString& aBorderBottomStyle) { return _to SetBorderBottomStyle(aBorderBottomStyle); } \
  NS_IMETHOD    GetBorderLeftStyle(nsString& aBorderLeftStyle) { return _to GetBorderLeftStyle(aBorderLeftStyle); } \
  NS_IMETHOD    SetBorderLeftStyle(const nsString& aBorderLeftStyle) { return _to SetBorderLeftStyle(aBorderLeftStyle); } \
  NS_IMETHOD    GetBorderTopWidth(nsString& aBorderTopWidth) { return _to GetBorderTopWidth(aBorderTopWidth); } \
  NS_IMETHOD    SetBorderTopWidth(const nsString& aBorderTopWidth) { return _to SetBorderTopWidth(aBorderTopWidth); } \
  NS_IMETHOD    GetBorderRightWidth(nsString& aBorderRightWidth) { return _to GetBorderRightWidth(aBorderRightWidth); } \
  NS_IMETHOD    SetBorderRightWidth(const nsString& aBorderRightWidth) { return _to SetBorderRightWidth(aBorderRightWidth); } \
  NS_IMETHOD    GetBorderBottomWidth(nsString& aBorderBottomWidth) { return _to GetBorderBottomWidth(aBorderBottomWidth); } \
  NS_IMETHOD    SetBorderBottomWidth(const nsString& aBorderBottomWidth) { return _to SetBorderBottomWidth(aBorderBottomWidth); } \
  NS_IMETHOD    GetBorderLeftWidth(nsString& aBorderLeftWidth) { return _to GetBorderLeftWidth(aBorderLeftWidth); } \
  NS_IMETHOD    SetBorderLeftWidth(const nsString& aBorderLeftWidth) { return _to SetBorderLeftWidth(aBorderLeftWidth); } \
  NS_IMETHOD    GetBorderWidth(nsString& aBorderWidth) { return _to GetBorderWidth(aBorderWidth); } \
  NS_IMETHOD    SetBorderWidth(const nsString& aBorderWidth) { return _to SetBorderWidth(aBorderWidth); } \
  NS_IMETHOD    GetBottom(nsString& aBottom) { return _to GetBottom(aBottom); } \
  NS_IMETHOD    SetBottom(const nsString& aBottom) { return _to SetBottom(aBottom); } \
  NS_IMETHOD    GetCaptionSide(nsString& aCaptionSide) { return _to GetCaptionSide(aCaptionSide); } \
  NS_IMETHOD    SetCaptionSide(const nsString& aCaptionSide) { return _to SetCaptionSide(aCaptionSide); } \
  NS_IMETHOD    GetClear(nsString& aClear) { return _to GetClear(aClear); } \
  NS_IMETHOD    SetClear(const nsString& aClear) { return _to SetClear(aClear); } \
  NS_IMETHOD    GetClip(nsString& aClip) { return _to GetClip(aClip); } \
  NS_IMETHOD    SetClip(const nsString& aClip) { return _to SetClip(aClip); } \
  NS_IMETHOD    GetColor(nsString& aColor) { return _to GetColor(aColor); } \
  NS_IMETHOD    SetColor(const nsString& aColor) { return _to SetColor(aColor); } \
  NS_IMETHOD    GetContent(nsString& aContent) { return _to GetContent(aContent); } \
  NS_IMETHOD    SetContent(const nsString& aContent) { return _to SetContent(aContent); } \
  NS_IMETHOD    GetCounterIncrement(nsString& aCounterIncrement) { return _to GetCounterIncrement(aCounterIncrement); } \
  NS_IMETHOD    SetCounterIncrement(const nsString& aCounterIncrement) { return _to SetCounterIncrement(aCounterIncrement); } \
  NS_IMETHOD    GetCounterReset(nsString& aCounterReset) { return _to GetCounterReset(aCounterReset); } \
  NS_IMETHOD    SetCounterReset(const nsString& aCounterReset) { return _to SetCounterReset(aCounterReset); } \
  NS_IMETHOD    GetCue(nsString& aCue) { return _to GetCue(aCue); } \
  NS_IMETHOD    SetCue(const nsString& aCue) { return _to SetCue(aCue); } \
  NS_IMETHOD    GetCueAfter(nsString& aCueAfter) { return _to GetCueAfter(aCueAfter); } \
  NS_IMETHOD    SetCueAfter(const nsString& aCueAfter) { return _to SetCueAfter(aCueAfter); } \
  NS_IMETHOD    GetCueBefore(nsString& aCueBefore) { return _to GetCueBefore(aCueBefore); } \
  NS_IMETHOD    SetCueBefore(const nsString& aCueBefore) { return _to SetCueBefore(aCueBefore); } \
  NS_IMETHOD    GetCursor(nsString& aCursor) { return _to GetCursor(aCursor); } \
  NS_IMETHOD    SetCursor(const nsString& aCursor) { return _to SetCursor(aCursor); } \
  NS_IMETHOD    GetDirection(nsString& aDirection) { return _to GetDirection(aDirection); } \
  NS_IMETHOD    SetDirection(const nsString& aDirection) { return _to SetDirection(aDirection); } \
  NS_IMETHOD    GetDisplay(nsString& aDisplay) { return _to GetDisplay(aDisplay); } \
  NS_IMETHOD    SetDisplay(const nsString& aDisplay) { return _to SetDisplay(aDisplay); } \
  NS_IMETHOD    GetElevation(nsString& aElevation) { return _to GetElevation(aElevation); } \
  NS_IMETHOD    SetElevation(const nsString& aElevation) { return _to SetElevation(aElevation); } \
  NS_IMETHOD    GetEmptyCells(nsString& aEmptyCells) { return _to GetEmptyCells(aEmptyCells); } \
  NS_IMETHOD    SetEmptyCells(const nsString& aEmptyCells) { return _to SetEmptyCells(aEmptyCells); } \
  NS_IMETHOD    GetCssFloat(nsString& aCssFloat) { return _to GetCssFloat(aCssFloat); } \
  NS_IMETHOD    SetCssFloat(const nsString& aCssFloat) { return _to SetCssFloat(aCssFloat); } \
  NS_IMETHOD    GetFont(nsString& aFont) { return _to GetFont(aFont); } \
  NS_IMETHOD    SetFont(const nsString& aFont) { return _to SetFont(aFont); } \
  NS_IMETHOD    GetFontFamily(nsString& aFontFamily) { return _to GetFontFamily(aFontFamily); } \
  NS_IMETHOD    SetFontFamily(const nsString& aFontFamily) { return _to SetFontFamily(aFontFamily); } \
  NS_IMETHOD    GetFontSize(nsString& aFontSize) { return _to GetFontSize(aFontSize); } \
  NS_IMETHOD    SetFontSize(const nsString& aFontSize) { return _to SetFontSize(aFontSize); } \
  NS_IMETHOD    GetFontSizeAdjust(nsString& aFontSizeAdjust) { return _to GetFontSizeAdjust(aFontSizeAdjust); } \
  NS_IMETHOD    SetFontSizeAdjust(const nsString& aFontSizeAdjust) { return _to SetFontSizeAdjust(aFontSizeAdjust); } \
  NS_IMETHOD    GetFontStretch(nsString& aFontStretch) { return _to GetFontStretch(aFontStretch); } \
  NS_IMETHOD    SetFontStretch(const nsString& aFontStretch) { return _to SetFontStretch(aFontStretch); } \
  NS_IMETHOD    GetFontStyle(nsString& aFontStyle) { return _to GetFontStyle(aFontStyle); } \
  NS_IMETHOD    SetFontStyle(const nsString& aFontStyle) { return _to SetFontStyle(aFontStyle); } \
  NS_IMETHOD    GetFontVariant(nsString& aFontVariant) { return _to GetFontVariant(aFontVariant); } \
  NS_IMETHOD    SetFontVariant(const nsString& aFontVariant) { return _to SetFontVariant(aFontVariant); } \
  NS_IMETHOD    GetFontWeight(nsString& aFontWeight) { return _to GetFontWeight(aFontWeight); } \
  NS_IMETHOD    SetFontWeight(const nsString& aFontWeight) { return _to SetFontWeight(aFontWeight); } \
  NS_IMETHOD    GetHeight(nsString& aHeight) { return _to GetHeight(aHeight); } \
  NS_IMETHOD    SetHeight(const nsString& aHeight) { return _to SetHeight(aHeight); } \
  NS_IMETHOD    GetLeft(nsString& aLeft) { return _to GetLeft(aLeft); } \
  NS_IMETHOD    SetLeft(const nsString& aLeft) { return _to SetLeft(aLeft); } \
  NS_IMETHOD    GetLetterSpacing(nsString& aLetterSpacing) { return _to GetLetterSpacing(aLetterSpacing); } \
  NS_IMETHOD    SetLetterSpacing(const nsString& aLetterSpacing) { return _to SetLetterSpacing(aLetterSpacing); } \
  NS_IMETHOD    GetLineHeight(nsString& aLineHeight) { return _to GetLineHeight(aLineHeight); } \
  NS_IMETHOD    SetLineHeight(const nsString& aLineHeight) { return _to SetLineHeight(aLineHeight); } \
  NS_IMETHOD    GetListStyle(nsString& aListStyle) { return _to GetListStyle(aListStyle); } \
  NS_IMETHOD    SetListStyle(const nsString& aListStyle) { return _to SetListStyle(aListStyle); } \
  NS_IMETHOD    GetListStyleImage(nsString& aListStyleImage) { return _to GetListStyleImage(aListStyleImage); } \
  NS_IMETHOD    SetListStyleImage(const nsString& aListStyleImage) { return _to SetListStyleImage(aListStyleImage); } \
  NS_IMETHOD    GetListStylePosition(nsString& aListStylePosition) { return _to GetListStylePosition(aListStylePosition); } \
  NS_IMETHOD    SetListStylePosition(const nsString& aListStylePosition) { return _to SetListStylePosition(aListStylePosition); } \
  NS_IMETHOD    GetListStyleType(nsString& aListStyleType) { return _to GetListStyleType(aListStyleType); } \
  NS_IMETHOD    SetListStyleType(const nsString& aListStyleType) { return _to SetListStyleType(aListStyleType); } \
  NS_IMETHOD    GetMargin(nsString& aMargin) { return _to GetMargin(aMargin); } \
  NS_IMETHOD    SetMargin(const nsString& aMargin) { return _to SetMargin(aMargin); } \
  NS_IMETHOD    GetMarginTop(nsString& aMarginTop) { return _to GetMarginTop(aMarginTop); } \
  NS_IMETHOD    SetMarginTop(const nsString& aMarginTop) { return _to SetMarginTop(aMarginTop); } \
  NS_IMETHOD    GetMarginRight(nsString& aMarginRight) { return _to GetMarginRight(aMarginRight); } \
  NS_IMETHOD    SetMarginRight(const nsString& aMarginRight) { return _to SetMarginRight(aMarginRight); } \
  NS_IMETHOD    GetMarginBottom(nsString& aMarginBottom) { return _to GetMarginBottom(aMarginBottom); } \
  NS_IMETHOD    SetMarginBottom(const nsString& aMarginBottom) { return _to SetMarginBottom(aMarginBottom); } \
  NS_IMETHOD    GetMarginLeft(nsString& aMarginLeft) { return _to GetMarginLeft(aMarginLeft); } \
  NS_IMETHOD    SetMarginLeft(const nsString& aMarginLeft) { return _to SetMarginLeft(aMarginLeft); } \
  NS_IMETHOD    GetMarkerOffset(nsString& aMarkerOffset) { return _to GetMarkerOffset(aMarkerOffset); } \
  NS_IMETHOD    SetMarkerOffset(const nsString& aMarkerOffset) { return _to SetMarkerOffset(aMarkerOffset); } \
  NS_IMETHOD    GetMarks(nsString& aMarks) { return _to GetMarks(aMarks); } \
  NS_IMETHOD    SetMarks(const nsString& aMarks) { return _to SetMarks(aMarks); } \
  NS_IMETHOD    GetMaxHeight(nsString& aMaxHeight) { return _to GetMaxHeight(aMaxHeight); } \
  NS_IMETHOD    SetMaxHeight(const nsString& aMaxHeight) { return _to SetMaxHeight(aMaxHeight); } \
  NS_IMETHOD    GetMaxWidth(nsString& aMaxWidth) { return _to GetMaxWidth(aMaxWidth); } \
  NS_IMETHOD    SetMaxWidth(const nsString& aMaxWidth) { return _to SetMaxWidth(aMaxWidth); } \
  NS_IMETHOD    GetMinHeight(nsString& aMinHeight) { return _to GetMinHeight(aMinHeight); } \
  NS_IMETHOD    SetMinHeight(const nsString& aMinHeight) { return _to SetMinHeight(aMinHeight); } \
  NS_IMETHOD    GetMinWidth(nsString& aMinWidth) { return _to GetMinWidth(aMinWidth); } \
  NS_IMETHOD    SetMinWidth(const nsString& aMinWidth) { return _to SetMinWidth(aMinWidth); } \
  NS_IMETHOD    GetOrphans(nsString& aOrphans) { return _to GetOrphans(aOrphans); } \
  NS_IMETHOD    SetOrphans(const nsString& aOrphans) { return _to SetOrphans(aOrphans); } \
  NS_IMETHOD    GetOutline(nsString& aOutline) { return _to GetOutline(aOutline); } \
  NS_IMETHOD    SetOutline(const nsString& aOutline) { return _to SetOutline(aOutline); } \
  NS_IMETHOD    GetOutlineColor(nsString& aOutlineColor) { return _to GetOutlineColor(aOutlineColor); } \
  NS_IMETHOD    SetOutlineColor(const nsString& aOutlineColor) { return _to SetOutlineColor(aOutlineColor); } \
  NS_IMETHOD    GetOutlineStyle(nsString& aOutlineStyle) { return _to GetOutlineStyle(aOutlineStyle); } \
  NS_IMETHOD    SetOutlineStyle(const nsString& aOutlineStyle) { return _to SetOutlineStyle(aOutlineStyle); } \
  NS_IMETHOD    GetOutlineWidth(nsString& aOutlineWidth) { return _to GetOutlineWidth(aOutlineWidth); } \
  NS_IMETHOD    SetOutlineWidth(const nsString& aOutlineWidth) { return _to SetOutlineWidth(aOutlineWidth); } \
  NS_IMETHOD    GetOverflow(nsString& aOverflow) { return _to GetOverflow(aOverflow); } \
  NS_IMETHOD    SetOverflow(const nsString& aOverflow) { return _to SetOverflow(aOverflow); } \
  NS_IMETHOD    GetPadding(nsString& aPadding) { return _to GetPadding(aPadding); } \
  NS_IMETHOD    SetPadding(const nsString& aPadding) { return _to SetPadding(aPadding); } \
  NS_IMETHOD    GetPaddingTop(nsString& aPaddingTop) { return _to GetPaddingTop(aPaddingTop); } \
  NS_IMETHOD    SetPaddingTop(const nsString& aPaddingTop) { return _to SetPaddingTop(aPaddingTop); } \
  NS_IMETHOD    GetPaddingRight(nsString& aPaddingRight) { return _to GetPaddingRight(aPaddingRight); } \
  NS_IMETHOD    SetPaddingRight(const nsString& aPaddingRight) { return _to SetPaddingRight(aPaddingRight); } \
  NS_IMETHOD    GetPaddingBottom(nsString& aPaddingBottom) { return _to GetPaddingBottom(aPaddingBottom); } \
  NS_IMETHOD    SetPaddingBottom(const nsString& aPaddingBottom) { return _to SetPaddingBottom(aPaddingBottom); } \
  NS_IMETHOD    GetPaddingLeft(nsString& aPaddingLeft) { return _to GetPaddingLeft(aPaddingLeft); } \
  NS_IMETHOD    SetPaddingLeft(const nsString& aPaddingLeft) { return _to SetPaddingLeft(aPaddingLeft); } \
  NS_IMETHOD    GetPage(nsString& aPage) { return _to GetPage(aPage); } \
  NS_IMETHOD    SetPage(const nsString& aPage) { return _to SetPage(aPage); } \
  NS_IMETHOD    GetPageBreakAfter(nsString& aPageBreakAfter) { return _to GetPageBreakAfter(aPageBreakAfter); } \
  NS_IMETHOD    SetPageBreakAfter(const nsString& aPageBreakAfter) { return _to SetPageBreakAfter(aPageBreakAfter); } \
  NS_IMETHOD    GetPageBreakBefore(nsString& aPageBreakBefore) { return _to GetPageBreakBefore(aPageBreakBefore); } \
  NS_IMETHOD    SetPageBreakBefore(const nsString& aPageBreakBefore) { return _to SetPageBreakBefore(aPageBreakBefore); } \
  NS_IMETHOD    GetPageBreakInside(nsString& aPageBreakInside) { return _to GetPageBreakInside(aPageBreakInside); } \
  NS_IMETHOD    SetPageBreakInside(const nsString& aPageBreakInside) { return _to SetPageBreakInside(aPageBreakInside); } \
  NS_IMETHOD    GetPause(nsString& aPause) { return _to GetPause(aPause); } \
  NS_IMETHOD    SetPause(const nsString& aPause) { return _to SetPause(aPause); } \
  NS_IMETHOD    GetPauseAfter(nsString& aPauseAfter) { return _to GetPauseAfter(aPauseAfter); } \
  NS_IMETHOD    SetPauseAfter(const nsString& aPauseAfter) { return _to SetPauseAfter(aPauseAfter); } \
  NS_IMETHOD    GetPauseBefore(nsString& aPauseBefore) { return _to GetPauseBefore(aPauseBefore); } \
  NS_IMETHOD    SetPauseBefore(const nsString& aPauseBefore) { return _to SetPauseBefore(aPauseBefore); } \
  NS_IMETHOD    GetPitch(nsString& aPitch) { return _to GetPitch(aPitch); } \
  NS_IMETHOD    SetPitch(const nsString& aPitch) { return _to SetPitch(aPitch); } \
  NS_IMETHOD    GetPitchRange(nsString& aPitchRange) { return _to GetPitchRange(aPitchRange); } \
  NS_IMETHOD    SetPitchRange(const nsString& aPitchRange) { return _to SetPitchRange(aPitchRange); } \
  NS_IMETHOD    GetPlayDuring(nsString& aPlayDuring) { return _to GetPlayDuring(aPlayDuring); } \
  NS_IMETHOD    SetPlayDuring(const nsString& aPlayDuring) { return _to SetPlayDuring(aPlayDuring); } \
  NS_IMETHOD    GetPosition(nsString& aPosition) { return _to GetPosition(aPosition); } \
  NS_IMETHOD    SetPosition(const nsString& aPosition) { return _to SetPosition(aPosition); } \
  NS_IMETHOD    GetQuotes(nsString& aQuotes) { return _to GetQuotes(aQuotes); } \
  NS_IMETHOD    SetQuotes(const nsString& aQuotes) { return _to SetQuotes(aQuotes); } \
  NS_IMETHOD    GetRichness(nsString& aRichness) { return _to GetRichness(aRichness); } \
  NS_IMETHOD    SetRichness(const nsString& aRichness) { return _to SetRichness(aRichness); } \
  NS_IMETHOD    GetRight(nsString& aRight) { return _to GetRight(aRight); } \
  NS_IMETHOD    SetRight(const nsString& aRight) { return _to SetRight(aRight); } \
  NS_IMETHOD    GetSize(nsString& aSize) { return _to GetSize(aSize); } \
  NS_IMETHOD    SetSize(const nsString& aSize) { return _to SetSize(aSize); } \
  NS_IMETHOD    GetSpeak(nsString& aSpeak) { return _to GetSpeak(aSpeak); } \
  NS_IMETHOD    SetSpeak(const nsString& aSpeak) { return _to SetSpeak(aSpeak); } \
  NS_IMETHOD    GetSpeakHeader(nsString& aSpeakHeader) { return _to GetSpeakHeader(aSpeakHeader); } \
  NS_IMETHOD    SetSpeakHeader(const nsString& aSpeakHeader) { return _to SetSpeakHeader(aSpeakHeader); } \
  NS_IMETHOD    GetSpeakNumeral(nsString& aSpeakNumeral) { return _to GetSpeakNumeral(aSpeakNumeral); } \
  NS_IMETHOD    SetSpeakNumeral(const nsString& aSpeakNumeral) { return _to SetSpeakNumeral(aSpeakNumeral); } \
  NS_IMETHOD    GetSpeakPunctuation(nsString& aSpeakPunctuation) { return _to GetSpeakPunctuation(aSpeakPunctuation); } \
  NS_IMETHOD    SetSpeakPunctuation(const nsString& aSpeakPunctuation) { return _to SetSpeakPunctuation(aSpeakPunctuation); } \
  NS_IMETHOD    GetSpeechRate(nsString& aSpeechRate) { return _to GetSpeechRate(aSpeechRate); } \
  NS_IMETHOD    SetSpeechRate(const nsString& aSpeechRate) { return _to SetSpeechRate(aSpeechRate); } \
  NS_IMETHOD    GetStress(nsString& aStress) { return _to GetStress(aStress); } \
  NS_IMETHOD    SetStress(const nsString& aStress) { return _to SetStress(aStress); } \
  NS_IMETHOD    GetTableLayout(nsString& aTableLayout) { return _to GetTableLayout(aTableLayout); } \
  NS_IMETHOD    SetTableLayout(const nsString& aTableLayout) { return _to SetTableLayout(aTableLayout); } \
  NS_IMETHOD    GetTextAlign(nsString& aTextAlign) { return _to GetTextAlign(aTextAlign); } \
  NS_IMETHOD    SetTextAlign(const nsString& aTextAlign) { return _to SetTextAlign(aTextAlign); } \
  NS_IMETHOD    GetTextDecoration(nsString& aTextDecoration) { return _to GetTextDecoration(aTextDecoration); } \
  NS_IMETHOD    SetTextDecoration(const nsString& aTextDecoration) { return _to SetTextDecoration(aTextDecoration); } \
  NS_IMETHOD    GetTextIndent(nsString& aTextIndent) { return _to GetTextIndent(aTextIndent); } \
  NS_IMETHOD    SetTextIndent(const nsString& aTextIndent) { return _to SetTextIndent(aTextIndent); } \
  NS_IMETHOD    GetTextShadow(nsString& aTextShadow) { return _to GetTextShadow(aTextShadow); } \
  NS_IMETHOD    SetTextShadow(const nsString& aTextShadow) { return _to SetTextShadow(aTextShadow); } \
  NS_IMETHOD    GetTextTransform(nsString& aTextTransform) { return _to GetTextTransform(aTextTransform); } \
  NS_IMETHOD    SetTextTransform(const nsString& aTextTransform) { return _to SetTextTransform(aTextTransform); } \
  NS_IMETHOD    GetTop(nsString& aTop) { return _to GetTop(aTop); } \
  NS_IMETHOD    SetTop(const nsString& aTop) { return _to SetTop(aTop); } \
  NS_IMETHOD    GetUnicodeBidi(nsString& aUnicodeBidi) { return _to GetUnicodeBidi(aUnicodeBidi); } \
  NS_IMETHOD    SetUnicodeBidi(const nsString& aUnicodeBidi) { return _to SetUnicodeBidi(aUnicodeBidi); } \
  NS_IMETHOD    GetVerticalAlign(nsString& aVerticalAlign) { return _to GetVerticalAlign(aVerticalAlign); } \
  NS_IMETHOD    SetVerticalAlign(const nsString& aVerticalAlign) { return _to SetVerticalAlign(aVerticalAlign); } \
  NS_IMETHOD    GetVisibility(nsString& aVisibility) { return _to GetVisibility(aVisibility); } \
  NS_IMETHOD    SetVisibility(const nsString& aVisibility) { return _to SetVisibility(aVisibility); } \
  NS_IMETHOD    GetVoiceFamily(nsString& aVoiceFamily) { return _to GetVoiceFamily(aVoiceFamily); } \
  NS_IMETHOD    SetVoiceFamily(const nsString& aVoiceFamily) { return _to SetVoiceFamily(aVoiceFamily); } \
  NS_IMETHOD    GetVolume(nsString& aVolume) { return _to GetVolume(aVolume); } \
  NS_IMETHOD    SetVolume(const nsString& aVolume) { return _to SetVolume(aVolume); } \
  NS_IMETHOD    GetWhiteSpace(nsString& aWhiteSpace) { return _to GetWhiteSpace(aWhiteSpace); } \
  NS_IMETHOD    SetWhiteSpace(const nsString& aWhiteSpace) { return _to SetWhiteSpace(aWhiteSpace); } \
  NS_IMETHOD    GetWidows(nsString& aWidows) { return _to GetWidows(aWidows); } \
  NS_IMETHOD    SetWidows(const nsString& aWidows) { return _to SetWidows(aWidows); } \
  NS_IMETHOD    GetWidth(nsString& aWidth) { return _to GetWidth(aWidth); } \
  NS_IMETHOD    SetWidth(const nsString& aWidth) { return _to SetWidth(aWidth); } \
  NS_IMETHOD    GetWordSpacing(nsString& aWordSpacing) { return _to GetWordSpacing(aWordSpacing); } \
  NS_IMETHOD    SetWordSpacing(const nsString& aWordSpacing) { return _to SetWordSpacing(aWordSpacing); } \
  NS_IMETHOD    GetZIndex(nsString& aZIndex) { return _to GetZIndex(aZIndex); } \
  NS_IMETHOD    SetZIndex(const nsString& aZIndex) { return _to SetZIndex(aZIndex); } \
  NS_IMETHOD    GetOpacity(nsString& aOpacity) { return _to GetOpacity(aOpacity); } \
  NS_IMETHOD    SetOpacity(const nsString& aOpacity) { return _to SetOpacity(aOpacity); } \


extern "C" NS_DOM nsresult NS_InitCSS2PropertiesClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptCSS2Properties(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMCSS2Properties_h__
