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

  NS_IMETHOD    GetAzimuth(nsAWritableString& aAzimuth)=0;
  NS_IMETHOD    SetAzimuth(const nsAReadableString& aAzimuth)=0;

  NS_IMETHOD    GetBackground(nsAWritableString& aBackground)=0;
  NS_IMETHOD    SetBackground(const nsAReadableString& aBackground)=0;

  NS_IMETHOD    GetBackgroundAttachment(nsAWritableString& aBackgroundAttachment)=0;
  NS_IMETHOD    SetBackgroundAttachment(const nsAReadableString& aBackgroundAttachment)=0;

  NS_IMETHOD    GetBackgroundColor(nsAWritableString& aBackgroundColor)=0;
  NS_IMETHOD    SetBackgroundColor(const nsAReadableString& aBackgroundColor)=0;

  NS_IMETHOD    GetBackgroundImage(nsAWritableString& aBackgroundImage)=0;
  NS_IMETHOD    SetBackgroundImage(const nsAReadableString& aBackgroundImage)=0;

  NS_IMETHOD    GetBackgroundPosition(nsAWritableString& aBackgroundPosition)=0;
  NS_IMETHOD    SetBackgroundPosition(const nsAReadableString& aBackgroundPosition)=0;

  NS_IMETHOD    GetBackgroundRepeat(nsAWritableString& aBackgroundRepeat)=0;
  NS_IMETHOD    SetBackgroundRepeat(const nsAReadableString& aBackgroundRepeat)=0;

  NS_IMETHOD    GetBorder(nsAWritableString& aBorder)=0;
  NS_IMETHOD    SetBorder(const nsAReadableString& aBorder)=0;

  NS_IMETHOD    GetBorderCollapse(nsAWritableString& aBorderCollapse)=0;
  NS_IMETHOD    SetBorderCollapse(const nsAReadableString& aBorderCollapse)=0;

  NS_IMETHOD    GetBorderColor(nsAWritableString& aBorderColor)=0;
  NS_IMETHOD    SetBorderColor(const nsAReadableString& aBorderColor)=0;

  NS_IMETHOD    GetBorderSpacing(nsAWritableString& aBorderSpacing)=0;
  NS_IMETHOD    SetBorderSpacing(const nsAReadableString& aBorderSpacing)=0;

  NS_IMETHOD    GetBorderStyle(nsAWritableString& aBorderStyle)=0;
  NS_IMETHOD    SetBorderStyle(const nsAReadableString& aBorderStyle)=0;

  NS_IMETHOD    GetBorderTop(nsAWritableString& aBorderTop)=0;
  NS_IMETHOD    SetBorderTop(const nsAReadableString& aBorderTop)=0;

  NS_IMETHOD    GetBorderRight(nsAWritableString& aBorderRight)=0;
  NS_IMETHOD    SetBorderRight(const nsAReadableString& aBorderRight)=0;

  NS_IMETHOD    GetBorderBottom(nsAWritableString& aBorderBottom)=0;
  NS_IMETHOD    SetBorderBottom(const nsAReadableString& aBorderBottom)=0;

  NS_IMETHOD    GetBorderLeft(nsAWritableString& aBorderLeft)=0;
  NS_IMETHOD    SetBorderLeft(const nsAReadableString& aBorderLeft)=0;

  NS_IMETHOD    GetBorderTopColor(nsAWritableString& aBorderTopColor)=0;
  NS_IMETHOD    SetBorderTopColor(const nsAReadableString& aBorderTopColor)=0;

  NS_IMETHOD    GetBorderRightColor(nsAWritableString& aBorderRightColor)=0;
  NS_IMETHOD    SetBorderRightColor(const nsAReadableString& aBorderRightColor)=0;

  NS_IMETHOD    GetBorderBottomColor(nsAWritableString& aBorderBottomColor)=0;
  NS_IMETHOD    SetBorderBottomColor(const nsAReadableString& aBorderBottomColor)=0;

  NS_IMETHOD    GetBorderLeftColor(nsAWritableString& aBorderLeftColor)=0;
  NS_IMETHOD    SetBorderLeftColor(const nsAReadableString& aBorderLeftColor)=0;

  NS_IMETHOD    GetBorderTopStyle(nsAWritableString& aBorderTopStyle)=0;
  NS_IMETHOD    SetBorderTopStyle(const nsAReadableString& aBorderTopStyle)=0;

  NS_IMETHOD    GetBorderRightStyle(nsAWritableString& aBorderRightStyle)=0;
  NS_IMETHOD    SetBorderRightStyle(const nsAReadableString& aBorderRightStyle)=0;

  NS_IMETHOD    GetBorderBottomStyle(nsAWritableString& aBorderBottomStyle)=0;
  NS_IMETHOD    SetBorderBottomStyle(const nsAReadableString& aBorderBottomStyle)=0;

  NS_IMETHOD    GetBorderLeftStyle(nsAWritableString& aBorderLeftStyle)=0;
  NS_IMETHOD    SetBorderLeftStyle(const nsAReadableString& aBorderLeftStyle)=0;

  NS_IMETHOD    GetBorderTopWidth(nsAWritableString& aBorderTopWidth)=0;
  NS_IMETHOD    SetBorderTopWidth(const nsAReadableString& aBorderTopWidth)=0;

  NS_IMETHOD    GetBorderRightWidth(nsAWritableString& aBorderRightWidth)=0;
  NS_IMETHOD    SetBorderRightWidth(const nsAReadableString& aBorderRightWidth)=0;

  NS_IMETHOD    GetBorderBottomWidth(nsAWritableString& aBorderBottomWidth)=0;
  NS_IMETHOD    SetBorderBottomWidth(const nsAReadableString& aBorderBottomWidth)=0;

  NS_IMETHOD    GetBorderLeftWidth(nsAWritableString& aBorderLeftWidth)=0;
  NS_IMETHOD    SetBorderLeftWidth(const nsAReadableString& aBorderLeftWidth)=0;

  NS_IMETHOD    GetBorderWidth(nsAWritableString& aBorderWidth)=0;
  NS_IMETHOD    SetBorderWidth(const nsAReadableString& aBorderWidth)=0;

  NS_IMETHOD    GetBottom(nsAWritableString& aBottom)=0;
  NS_IMETHOD    SetBottom(const nsAReadableString& aBottom)=0;

  NS_IMETHOD    GetCaptionSide(nsAWritableString& aCaptionSide)=0;
  NS_IMETHOD    SetCaptionSide(const nsAReadableString& aCaptionSide)=0;

  NS_IMETHOD    GetClear(nsAWritableString& aClear)=0;
  NS_IMETHOD    SetClear(const nsAReadableString& aClear)=0;

  NS_IMETHOD    GetClip(nsAWritableString& aClip)=0;
  NS_IMETHOD    SetClip(const nsAReadableString& aClip)=0;

  NS_IMETHOD    GetColor(nsAWritableString& aColor)=0;
  NS_IMETHOD    SetColor(const nsAReadableString& aColor)=0;

  NS_IMETHOD    GetContent(nsAWritableString& aContent)=0;
  NS_IMETHOD    SetContent(const nsAReadableString& aContent)=0;

  NS_IMETHOD    GetCounterIncrement(nsAWritableString& aCounterIncrement)=0;
  NS_IMETHOD    SetCounterIncrement(const nsAReadableString& aCounterIncrement)=0;

  NS_IMETHOD    GetCounterReset(nsAWritableString& aCounterReset)=0;
  NS_IMETHOD    SetCounterReset(const nsAReadableString& aCounterReset)=0;

  NS_IMETHOD    GetCue(nsAWritableString& aCue)=0;
  NS_IMETHOD    SetCue(const nsAReadableString& aCue)=0;

  NS_IMETHOD    GetCueAfter(nsAWritableString& aCueAfter)=0;
  NS_IMETHOD    SetCueAfter(const nsAReadableString& aCueAfter)=0;

  NS_IMETHOD    GetCueBefore(nsAWritableString& aCueBefore)=0;
  NS_IMETHOD    SetCueBefore(const nsAReadableString& aCueBefore)=0;

  NS_IMETHOD    GetCursor(nsAWritableString& aCursor)=0;
  NS_IMETHOD    SetCursor(const nsAReadableString& aCursor)=0;

  NS_IMETHOD    GetDirection(nsAWritableString& aDirection)=0;
  NS_IMETHOD    SetDirection(const nsAReadableString& aDirection)=0;

  NS_IMETHOD    GetDisplay(nsAWritableString& aDisplay)=0;
  NS_IMETHOD    SetDisplay(const nsAReadableString& aDisplay)=0;

  NS_IMETHOD    GetElevation(nsAWritableString& aElevation)=0;
  NS_IMETHOD    SetElevation(const nsAReadableString& aElevation)=0;

  NS_IMETHOD    GetEmptyCells(nsAWritableString& aEmptyCells)=0;
  NS_IMETHOD    SetEmptyCells(const nsAReadableString& aEmptyCells)=0;

  NS_IMETHOD    GetCssFloat(nsAWritableString& aCssFloat)=0;
  NS_IMETHOD    SetCssFloat(const nsAReadableString& aCssFloat)=0;

  NS_IMETHOD    GetFont(nsAWritableString& aFont)=0;
  NS_IMETHOD    SetFont(const nsAReadableString& aFont)=0;

  NS_IMETHOD    GetFontFamily(nsAWritableString& aFontFamily)=0;
  NS_IMETHOD    SetFontFamily(const nsAReadableString& aFontFamily)=0;

  NS_IMETHOD    GetFontSize(nsAWritableString& aFontSize)=0;
  NS_IMETHOD    SetFontSize(const nsAReadableString& aFontSize)=0;

  NS_IMETHOD    GetFontSizeAdjust(nsAWritableString& aFontSizeAdjust)=0;
  NS_IMETHOD    SetFontSizeAdjust(const nsAReadableString& aFontSizeAdjust)=0;

  NS_IMETHOD    GetFontStretch(nsAWritableString& aFontStretch)=0;
  NS_IMETHOD    SetFontStretch(const nsAReadableString& aFontStretch)=0;

  NS_IMETHOD    GetFontStyle(nsAWritableString& aFontStyle)=0;
  NS_IMETHOD    SetFontStyle(const nsAReadableString& aFontStyle)=0;

  NS_IMETHOD    GetFontVariant(nsAWritableString& aFontVariant)=0;
  NS_IMETHOD    SetFontVariant(const nsAReadableString& aFontVariant)=0;

  NS_IMETHOD    GetFontWeight(nsAWritableString& aFontWeight)=0;
  NS_IMETHOD    SetFontWeight(const nsAReadableString& aFontWeight)=0;

  NS_IMETHOD    GetHeight(nsAWritableString& aHeight)=0;
  NS_IMETHOD    SetHeight(const nsAReadableString& aHeight)=0;

  NS_IMETHOD    GetLeft(nsAWritableString& aLeft)=0;
  NS_IMETHOD    SetLeft(const nsAReadableString& aLeft)=0;

  NS_IMETHOD    GetLetterSpacing(nsAWritableString& aLetterSpacing)=0;
  NS_IMETHOD    SetLetterSpacing(const nsAReadableString& aLetterSpacing)=0;

  NS_IMETHOD    GetLineHeight(nsAWritableString& aLineHeight)=0;
  NS_IMETHOD    SetLineHeight(const nsAReadableString& aLineHeight)=0;

  NS_IMETHOD    GetListStyle(nsAWritableString& aListStyle)=0;
  NS_IMETHOD    SetListStyle(const nsAReadableString& aListStyle)=0;

  NS_IMETHOD    GetListStyleImage(nsAWritableString& aListStyleImage)=0;
  NS_IMETHOD    SetListStyleImage(const nsAReadableString& aListStyleImage)=0;

  NS_IMETHOD    GetListStylePosition(nsAWritableString& aListStylePosition)=0;
  NS_IMETHOD    SetListStylePosition(const nsAReadableString& aListStylePosition)=0;

  NS_IMETHOD    GetListStyleType(nsAWritableString& aListStyleType)=0;
  NS_IMETHOD    SetListStyleType(const nsAReadableString& aListStyleType)=0;

  NS_IMETHOD    GetMargin(nsAWritableString& aMargin)=0;
  NS_IMETHOD    SetMargin(const nsAReadableString& aMargin)=0;

  NS_IMETHOD    GetMarginTop(nsAWritableString& aMarginTop)=0;
  NS_IMETHOD    SetMarginTop(const nsAReadableString& aMarginTop)=0;

  NS_IMETHOD    GetMarginRight(nsAWritableString& aMarginRight)=0;
  NS_IMETHOD    SetMarginRight(const nsAReadableString& aMarginRight)=0;

  NS_IMETHOD    GetMarginBottom(nsAWritableString& aMarginBottom)=0;
  NS_IMETHOD    SetMarginBottom(const nsAReadableString& aMarginBottom)=0;

  NS_IMETHOD    GetMarginLeft(nsAWritableString& aMarginLeft)=0;
  NS_IMETHOD    SetMarginLeft(const nsAReadableString& aMarginLeft)=0;

  NS_IMETHOD    GetMarkerOffset(nsAWritableString& aMarkerOffset)=0;
  NS_IMETHOD    SetMarkerOffset(const nsAReadableString& aMarkerOffset)=0;

  NS_IMETHOD    GetMarks(nsAWritableString& aMarks)=0;
  NS_IMETHOD    SetMarks(const nsAReadableString& aMarks)=0;

  NS_IMETHOD    GetMaxHeight(nsAWritableString& aMaxHeight)=0;
  NS_IMETHOD    SetMaxHeight(const nsAReadableString& aMaxHeight)=0;

  NS_IMETHOD    GetMaxWidth(nsAWritableString& aMaxWidth)=0;
  NS_IMETHOD    SetMaxWidth(const nsAReadableString& aMaxWidth)=0;

  NS_IMETHOD    GetMinHeight(nsAWritableString& aMinHeight)=0;
  NS_IMETHOD    SetMinHeight(const nsAReadableString& aMinHeight)=0;

  NS_IMETHOD    GetMinWidth(nsAWritableString& aMinWidth)=0;
  NS_IMETHOD    SetMinWidth(const nsAReadableString& aMinWidth)=0;

  NS_IMETHOD    GetOrphans(nsAWritableString& aOrphans)=0;
  NS_IMETHOD    SetOrphans(const nsAReadableString& aOrphans)=0;

  NS_IMETHOD    GetOutline(nsAWritableString& aOutline)=0;
  NS_IMETHOD    SetOutline(const nsAReadableString& aOutline)=0;

  NS_IMETHOD    GetOutlineColor(nsAWritableString& aOutlineColor)=0;
  NS_IMETHOD    SetOutlineColor(const nsAReadableString& aOutlineColor)=0;

  NS_IMETHOD    GetOutlineStyle(nsAWritableString& aOutlineStyle)=0;
  NS_IMETHOD    SetOutlineStyle(const nsAReadableString& aOutlineStyle)=0;

  NS_IMETHOD    GetOutlineWidth(nsAWritableString& aOutlineWidth)=0;
  NS_IMETHOD    SetOutlineWidth(const nsAReadableString& aOutlineWidth)=0;

  NS_IMETHOD    GetOverflow(nsAWritableString& aOverflow)=0;
  NS_IMETHOD    SetOverflow(const nsAReadableString& aOverflow)=0;

  NS_IMETHOD    GetPadding(nsAWritableString& aPadding)=0;
  NS_IMETHOD    SetPadding(const nsAReadableString& aPadding)=0;

  NS_IMETHOD    GetPaddingTop(nsAWritableString& aPaddingTop)=0;
  NS_IMETHOD    SetPaddingTop(const nsAReadableString& aPaddingTop)=0;

  NS_IMETHOD    GetPaddingRight(nsAWritableString& aPaddingRight)=0;
  NS_IMETHOD    SetPaddingRight(const nsAReadableString& aPaddingRight)=0;

  NS_IMETHOD    GetPaddingBottom(nsAWritableString& aPaddingBottom)=0;
  NS_IMETHOD    SetPaddingBottom(const nsAReadableString& aPaddingBottom)=0;

  NS_IMETHOD    GetPaddingLeft(nsAWritableString& aPaddingLeft)=0;
  NS_IMETHOD    SetPaddingLeft(const nsAReadableString& aPaddingLeft)=0;

  NS_IMETHOD    GetPage(nsAWritableString& aPage)=0;
  NS_IMETHOD    SetPage(const nsAReadableString& aPage)=0;

  NS_IMETHOD    GetPageBreakAfter(nsAWritableString& aPageBreakAfter)=0;
  NS_IMETHOD    SetPageBreakAfter(const nsAReadableString& aPageBreakAfter)=0;

  NS_IMETHOD    GetPageBreakBefore(nsAWritableString& aPageBreakBefore)=0;
  NS_IMETHOD    SetPageBreakBefore(const nsAReadableString& aPageBreakBefore)=0;

  NS_IMETHOD    GetPageBreakInside(nsAWritableString& aPageBreakInside)=0;
  NS_IMETHOD    SetPageBreakInside(const nsAReadableString& aPageBreakInside)=0;

  NS_IMETHOD    GetPause(nsAWritableString& aPause)=0;
  NS_IMETHOD    SetPause(const nsAReadableString& aPause)=0;

  NS_IMETHOD    GetPauseAfter(nsAWritableString& aPauseAfter)=0;
  NS_IMETHOD    SetPauseAfter(const nsAReadableString& aPauseAfter)=0;

  NS_IMETHOD    GetPauseBefore(nsAWritableString& aPauseBefore)=0;
  NS_IMETHOD    SetPauseBefore(const nsAReadableString& aPauseBefore)=0;

  NS_IMETHOD    GetPitch(nsAWritableString& aPitch)=0;
  NS_IMETHOD    SetPitch(const nsAReadableString& aPitch)=0;

  NS_IMETHOD    GetPitchRange(nsAWritableString& aPitchRange)=0;
  NS_IMETHOD    SetPitchRange(const nsAReadableString& aPitchRange)=0;

  NS_IMETHOD    GetPlayDuring(nsAWritableString& aPlayDuring)=0;
  NS_IMETHOD    SetPlayDuring(const nsAReadableString& aPlayDuring)=0;

  NS_IMETHOD    GetPosition(nsAWritableString& aPosition)=0;
  NS_IMETHOD    SetPosition(const nsAReadableString& aPosition)=0;

  NS_IMETHOD    GetQuotes(nsAWritableString& aQuotes)=0;
  NS_IMETHOD    SetQuotes(const nsAReadableString& aQuotes)=0;

  NS_IMETHOD    GetRichness(nsAWritableString& aRichness)=0;
  NS_IMETHOD    SetRichness(const nsAReadableString& aRichness)=0;

  NS_IMETHOD    GetRight(nsAWritableString& aRight)=0;
  NS_IMETHOD    SetRight(const nsAReadableString& aRight)=0;

  NS_IMETHOD    GetSize(nsAWritableString& aSize)=0;
  NS_IMETHOD    SetSize(const nsAReadableString& aSize)=0;

  NS_IMETHOD    GetSpeak(nsAWritableString& aSpeak)=0;
  NS_IMETHOD    SetSpeak(const nsAReadableString& aSpeak)=0;

  NS_IMETHOD    GetSpeakHeader(nsAWritableString& aSpeakHeader)=0;
  NS_IMETHOD    SetSpeakHeader(const nsAReadableString& aSpeakHeader)=0;

  NS_IMETHOD    GetSpeakNumeral(nsAWritableString& aSpeakNumeral)=0;
  NS_IMETHOD    SetSpeakNumeral(const nsAReadableString& aSpeakNumeral)=0;

  NS_IMETHOD    GetSpeakPunctuation(nsAWritableString& aSpeakPunctuation)=0;
  NS_IMETHOD    SetSpeakPunctuation(const nsAReadableString& aSpeakPunctuation)=0;

  NS_IMETHOD    GetSpeechRate(nsAWritableString& aSpeechRate)=0;
  NS_IMETHOD    SetSpeechRate(const nsAReadableString& aSpeechRate)=0;

  NS_IMETHOD    GetStress(nsAWritableString& aStress)=0;
  NS_IMETHOD    SetStress(const nsAReadableString& aStress)=0;

  NS_IMETHOD    GetTableLayout(nsAWritableString& aTableLayout)=0;
  NS_IMETHOD    SetTableLayout(const nsAReadableString& aTableLayout)=0;

  NS_IMETHOD    GetTextAlign(nsAWritableString& aTextAlign)=0;
  NS_IMETHOD    SetTextAlign(const nsAReadableString& aTextAlign)=0;

  NS_IMETHOD    GetTextDecoration(nsAWritableString& aTextDecoration)=0;
  NS_IMETHOD    SetTextDecoration(const nsAReadableString& aTextDecoration)=0;

  NS_IMETHOD    GetTextIndent(nsAWritableString& aTextIndent)=0;
  NS_IMETHOD    SetTextIndent(const nsAReadableString& aTextIndent)=0;

  NS_IMETHOD    GetTextShadow(nsAWritableString& aTextShadow)=0;
  NS_IMETHOD    SetTextShadow(const nsAReadableString& aTextShadow)=0;

  NS_IMETHOD    GetTextTransform(nsAWritableString& aTextTransform)=0;
  NS_IMETHOD    SetTextTransform(const nsAReadableString& aTextTransform)=0;

  NS_IMETHOD    GetTop(nsAWritableString& aTop)=0;
  NS_IMETHOD    SetTop(const nsAReadableString& aTop)=0;

  NS_IMETHOD    GetUnicodeBidi(nsAWritableString& aUnicodeBidi)=0;
  NS_IMETHOD    SetUnicodeBidi(const nsAReadableString& aUnicodeBidi)=0;

  NS_IMETHOD    GetVerticalAlign(nsAWritableString& aVerticalAlign)=0;
  NS_IMETHOD    SetVerticalAlign(const nsAReadableString& aVerticalAlign)=0;

  NS_IMETHOD    GetVisibility(nsAWritableString& aVisibility)=0;
  NS_IMETHOD    SetVisibility(const nsAReadableString& aVisibility)=0;

  NS_IMETHOD    GetVoiceFamily(nsAWritableString& aVoiceFamily)=0;
  NS_IMETHOD    SetVoiceFamily(const nsAReadableString& aVoiceFamily)=0;

  NS_IMETHOD    GetVolume(nsAWritableString& aVolume)=0;
  NS_IMETHOD    SetVolume(const nsAReadableString& aVolume)=0;

  NS_IMETHOD    GetWhiteSpace(nsAWritableString& aWhiteSpace)=0;
  NS_IMETHOD    SetWhiteSpace(const nsAReadableString& aWhiteSpace)=0;

  NS_IMETHOD    GetWidows(nsAWritableString& aWidows)=0;
  NS_IMETHOD    SetWidows(const nsAReadableString& aWidows)=0;

  NS_IMETHOD    GetWidth(nsAWritableString& aWidth)=0;
  NS_IMETHOD    SetWidth(const nsAReadableString& aWidth)=0;

  NS_IMETHOD    GetWordSpacing(nsAWritableString& aWordSpacing)=0;
  NS_IMETHOD    SetWordSpacing(const nsAReadableString& aWordSpacing)=0;

  NS_IMETHOD    GetZIndex(nsAWritableString& aZIndex)=0;
  NS_IMETHOD    SetZIndex(const nsAReadableString& aZIndex)=0;

  NS_IMETHOD    GetMozBinding(nsAWritableString& aMozBinding)=0;
  NS_IMETHOD    SetMozBinding(const nsAReadableString& aMozBinding)=0;

  NS_IMETHOD    GetMozOpacity(nsAWritableString& aMozOpacity)=0;
  NS_IMETHOD    SetMozOpacity(const nsAReadableString& aMozOpacity)=0;
};


#define NS_DECL_IDOMCSS2PROPERTIES   \
  NS_IMETHOD    GetAzimuth(nsAWritableString& aAzimuth);  \
  NS_IMETHOD    SetAzimuth(const nsAReadableString& aAzimuth);  \
  NS_IMETHOD    GetBackground(nsAWritableString& aBackground);  \
  NS_IMETHOD    SetBackground(const nsAReadableString& aBackground);  \
  NS_IMETHOD    GetBackgroundAttachment(nsAWritableString& aBackgroundAttachment);  \
  NS_IMETHOD    SetBackgroundAttachment(const nsAReadableString& aBackgroundAttachment);  \
  NS_IMETHOD    GetBackgroundColor(nsAWritableString& aBackgroundColor);  \
  NS_IMETHOD    SetBackgroundColor(const nsAReadableString& aBackgroundColor);  \
  NS_IMETHOD    GetBackgroundImage(nsAWritableString& aBackgroundImage);  \
  NS_IMETHOD    SetBackgroundImage(const nsAReadableString& aBackgroundImage);  \
  NS_IMETHOD    GetBackgroundPosition(nsAWritableString& aBackgroundPosition);  \
  NS_IMETHOD    SetBackgroundPosition(const nsAReadableString& aBackgroundPosition);  \
  NS_IMETHOD    GetBackgroundRepeat(nsAWritableString& aBackgroundRepeat);  \
  NS_IMETHOD    SetBackgroundRepeat(const nsAReadableString& aBackgroundRepeat);  \
  NS_IMETHOD    GetBorder(nsAWritableString& aBorder);  \
  NS_IMETHOD    SetBorder(const nsAReadableString& aBorder);  \
  NS_IMETHOD    GetBorderCollapse(nsAWritableString& aBorderCollapse);  \
  NS_IMETHOD    SetBorderCollapse(const nsAReadableString& aBorderCollapse);  \
  NS_IMETHOD    GetBorderColor(nsAWritableString& aBorderColor);  \
  NS_IMETHOD    SetBorderColor(const nsAReadableString& aBorderColor);  \
  NS_IMETHOD    GetBorderSpacing(nsAWritableString& aBorderSpacing);  \
  NS_IMETHOD    SetBorderSpacing(const nsAReadableString& aBorderSpacing);  \
  NS_IMETHOD    GetBorderStyle(nsAWritableString& aBorderStyle);  \
  NS_IMETHOD    SetBorderStyle(const nsAReadableString& aBorderStyle);  \
  NS_IMETHOD    GetBorderTop(nsAWritableString& aBorderTop);  \
  NS_IMETHOD    SetBorderTop(const nsAReadableString& aBorderTop);  \
  NS_IMETHOD    GetBorderRight(nsAWritableString& aBorderRight);  \
  NS_IMETHOD    SetBorderRight(const nsAReadableString& aBorderRight);  \
  NS_IMETHOD    GetBorderBottom(nsAWritableString& aBorderBottom);  \
  NS_IMETHOD    SetBorderBottom(const nsAReadableString& aBorderBottom);  \
  NS_IMETHOD    GetBorderLeft(nsAWritableString& aBorderLeft);  \
  NS_IMETHOD    SetBorderLeft(const nsAReadableString& aBorderLeft);  \
  NS_IMETHOD    GetBorderTopColor(nsAWritableString& aBorderTopColor);  \
  NS_IMETHOD    SetBorderTopColor(const nsAReadableString& aBorderTopColor);  \
  NS_IMETHOD    GetBorderRightColor(nsAWritableString& aBorderRightColor);  \
  NS_IMETHOD    SetBorderRightColor(const nsAReadableString& aBorderRightColor);  \
  NS_IMETHOD    GetBorderBottomColor(nsAWritableString& aBorderBottomColor);  \
  NS_IMETHOD    SetBorderBottomColor(const nsAReadableString& aBorderBottomColor);  \
  NS_IMETHOD    GetBorderLeftColor(nsAWritableString& aBorderLeftColor);  \
  NS_IMETHOD    SetBorderLeftColor(const nsAReadableString& aBorderLeftColor);  \
  NS_IMETHOD    GetBorderTopStyle(nsAWritableString& aBorderTopStyle);  \
  NS_IMETHOD    SetBorderTopStyle(const nsAReadableString& aBorderTopStyle);  \
  NS_IMETHOD    GetBorderRightStyle(nsAWritableString& aBorderRightStyle);  \
  NS_IMETHOD    SetBorderRightStyle(const nsAReadableString& aBorderRightStyle);  \
  NS_IMETHOD    GetBorderBottomStyle(nsAWritableString& aBorderBottomStyle);  \
  NS_IMETHOD    SetBorderBottomStyle(const nsAReadableString& aBorderBottomStyle);  \
  NS_IMETHOD    GetBorderLeftStyle(nsAWritableString& aBorderLeftStyle);  \
  NS_IMETHOD    SetBorderLeftStyle(const nsAReadableString& aBorderLeftStyle);  \
  NS_IMETHOD    GetBorderTopWidth(nsAWritableString& aBorderTopWidth);  \
  NS_IMETHOD    SetBorderTopWidth(const nsAReadableString& aBorderTopWidth);  \
  NS_IMETHOD    GetBorderRightWidth(nsAWritableString& aBorderRightWidth);  \
  NS_IMETHOD    SetBorderRightWidth(const nsAReadableString& aBorderRightWidth);  \
  NS_IMETHOD    GetBorderBottomWidth(nsAWritableString& aBorderBottomWidth);  \
  NS_IMETHOD    SetBorderBottomWidth(const nsAReadableString& aBorderBottomWidth);  \
  NS_IMETHOD    GetBorderLeftWidth(nsAWritableString& aBorderLeftWidth);  \
  NS_IMETHOD    SetBorderLeftWidth(const nsAReadableString& aBorderLeftWidth);  \
  NS_IMETHOD    GetBorderWidth(nsAWritableString& aBorderWidth);  \
  NS_IMETHOD    SetBorderWidth(const nsAReadableString& aBorderWidth);  \
  NS_IMETHOD    GetBottom(nsAWritableString& aBottom);  \
  NS_IMETHOD    SetBottom(const nsAReadableString& aBottom);  \
  NS_IMETHOD    GetCaptionSide(nsAWritableString& aCaptionSide);  \
  NS_IMETHOD    SetCaptionSide(const nsAReadableString& aCaptionSide);  \
  NS_IMETHOD    GetClear(nsAWritableString& aClear);  \
  NS_IMETHOD    SetClear(const nsAReadableString& aClear);  \
  NS_IMETHOD    GetClip(nsAWritableString& aClip);  \
  NS_IMETHOD    SetClip(const nsAReadableString& aClip);  \
  NS_IMETHOD    GetColor(nsAWritableString& aColor);  \
  NS_IMETHOD    SetColor(const nsAReadableString& aColor);  \
  NS_IMETHOD    GetContent(nsAWritableString& aContent);  \
  NS_IMETHOD    SetContent(const nsAReadableString& aContent);  \
  NS_IMETHOD    GetCounterIncrement(nsAWritableString& aCounterIncrement);  \
  NS_IMETHOD    SetCounterIncrement(const nsAReadableString& aCounterIncrement);  \
  NS_IMETHOD    GetCounterReset(nsAWritableString& aCounterReset);  \
  NS_IMETHOD    SetCounterReset(const nsAReadableString& aCounterReset);  \
  NS_IMETHOD    GetCue(nsAWritableString& aCue);  \
  NS_IMETHOD    SetCue(const nsAReadableString& aCue);  \
  NS_IMETHOD    GetCueAfter(nsAWritableString& aCueAfter);  \
  NS_IMETHOD    SetCueAfter(const nsAReadableString& aCueAfter);  \
  NS_IMETHOD    GetCueBefore(nsAWritableString& aCueBefore);  \
  NS_IMETHOD    SetCueBefore(const nsAReadableString& aCueBefore);  \
  NS_IMETHOD    GetCursor(nsAWritableString& aCursor);  \
  NS_IMETHOD    SetCursor(const nsAReadableString& aCursor);  \
  NS_IMETHOD    GetDirection(nsAWritableString& aDirection);  \
  NS_IMETHOD    SetDirection(const nsAReadableString& aDirection);  \
  NS_IMETHOD    GetDisplay(nsAWritableString& aDisplay);  \
  NS_IMETHOD    SetDisplay(const nsAReadableString& aDisplay);  \
  NS_IMETHOD    GetElevation(nsAWritableString& aElevation);  \
  NS_IMETHOD    SetElevation(const nsAReadableString& aElevation);  \
  NS_IMETHOD    GetEmptyCells(nsAWritableString& aEmptyCells);  \
  NS_IMETHOD    SetEmptyCells(const nsAReadableString& aEmptyCells);  \
  NS_IMETHOD    GetCssFloat(nsAWritableString& aCssFloat);  \
  NS_IMETHOD    SetCssFloat(const nsAReadableString& aCssFloat);  \
  NS_IMETHOD    GetFont(nsAWritableString& aFont);  \
  NS_IMETHOD    SetFont(const nsAReadableString& aFont);  \
  NS_IMETHOD    GetFontFamily(nsAWritableString& aFontFamily);  \
  NS_IMETHOD    SetFontFamily(const nsAReadableString& aFontFamily);  \
  NS_IMETHOD    GetFontSize(nsAWritableString& aFontSize);  \
  NS_IMETHOD    SetFontSize(const nsAReadableString& aFontSize);  \
  NS_IMETHOD    GetFontSizeAdjust(nsAWritableString& aFontSizeAdjust);  \
  NS_IMETHOD    SetFontSizeAdjust(const nsAReadableString& aFontSizeAdjust);  \
  NS_IMETHOD    GetFontStretch(nsAWritableString& aFontStretch);  \
  NS_IMETHOD    SetFontStretch(const nsAReadableString& aFontStretch);  \
  NS_IMETHOD    GetFontStyle(nsAWritableString& aFontStyle);  \
  NS_IMETHOD    SetFontStyle(const nsAReadableString& aFontStyle);  \
  NS_IMETHOD    GetFontVariant(nsAWritableString& aFontVariant);  \
  NS_IMETHOD    SetFontVariant(const nsAReadableString& aFontVariant);  \
  NS_IMETHOD    GetFontWeight(nsAWritableString& aFontWeight);  \
  NS_IMETHOD    SetFontWeight(const nsAReadableString& aFontWeight);  \
  NS_IMETHOD    GetHeight(nsAWritableString& aHeight);  \
  NS_IMETHOD    SetHeight(const nsAReadableString& aHeight);  \
  NS_IMETHOD    GetLeft(nsAWritableString& aLeft);  \
  NS_IMETHOD    SetLeft(const nsAReadableString& aLeft);  \
  NS_IMETHOD    GetLetterSpacing(nsAWritableString& aLetterSpacing);  \
  NS_IMETHOD    SetLetterSpacing(const nsAReadableString& aLetterSpacing);  \
  NS_IMETHOD    GetLineHeight(nsAWritableString& aLineHeight);  \
  NS_IMETHOD    SetLineHeight(const nsAReadableString& aLineHeight);  \
  NS_IMETHOD    GetListStyle(nsAWritableString& aListStyle);  \
  NS_IMETHOD    SetListStyle(const nsAReadableString& aListStyle);  \
  NS_IMETHOD    GetListStyleImage(nsAWritableString& aListStyleImage);  \
  NS_IMETHOD    SetListStyleImage(const nsAReadableString& aListStyleImage);  \
  NS_IMETHOD    GetListStylePosition(nsAWritableString& aListStylePosition);  \
  NS_IMETHOD    SetListStylePosition(const nsAReadableString& aListStylePosition);  \
  NS_IMETHOD    GetListStyleType(nsAWritableString& aListStyleType);  \
  NS_IMETHOD    SetListStyleType(const nsAReadableString& aListStyleType);  \
  NS_IMETHOD    GetMargin(nsAWritableString& aMargin);  \
  NS_IMETHOD    SetMargin(const nsAReadableString& aMargin);  \
  NS_IMETHOD    GetMarginTop(nsAWritableString& aMarginTop);  \
  NS_IMETHOD    SetMarginTop(const nsAReadableString& aMarginTop);  \
  NS_IMETHOD    GetMarginRight(nsAWritableString& aMarginRight);  \
  NS_IMETHOD    SetMarginRight(const nsAReadableString& aMarginRight);  \
  NS_IMETHOD    GetMarginBottom(nsAWritableString& aMarginBottom);  \
  NS_IMETHOD    SetMarginBottom(const nsAReadableString& aMarginBottom);  \
  NS_IMETHOD    GetMarginLeft(nsAWritableString& aMarginLeft);  \
  NS_IMETHOD    SetMarginLeft(const nsAReadableString& aMarginLeft);  \
  NS_IMETHOD    GetMarkerOffset(nsAWritableString& aMarkerOffset);  \
  NS_IMETHOD    SetMarkerOffset(const nsAReadableString& aMarkerOffset);  \
  NS_IMETHOD    GetMarks(nsAWritableString& aMarks);  \
  NS_IMETHOD    SetMarks(const nsAReadableString& aMarks);  \
  NS_IMETHOD    GetMaxHeight(nsAWritableString& aMaxHeight);  \
  NS_IMETHOD    SetMaxHeight(const nsAReadableString& aMaxHeight);  \
  NS_IMETHOD    GetMaxWidth(nsAWritableString& aMaxWidth);  \
  NS_IMETHOD    SetMaxWidth(const nsAReadableString& aMaxWidth);  \
  NS_IMETHOD    GetMinHeight(nsAWritableString& aMinHeight);  \
  NS_IMETHOD    SetMinHeight(const nsAReadableString& aMinHeight);  \
  NS_IMETHOD    GetMinWidth(nsAWritableString& aMinWidth);  \
  NS_IMETHOD    SetMinWidth(const nsAReadableString& aMinWidth);  \
  NS_IMETHOD    GetOrphans(nsAWritableString& aOrphans);  \
  NS_IMETHOD    SetOrphans(const nsAReadableString& aOrphans);  \
  NS_IMETHOD    GetOutline(nsAWritableString& aOutline);  \
  NS_IMETHOD    SetOutline(const nsAReadableString& aOutline);  \
  NS_IMETHOD    GetOutlineColor(nsAWritableString& aOutlineColor);  \
  NS_IMETHOD    SetOutlineColor(const nsAReadableString& aOutlineColor);  \
  NS_IMETHOD    GetOutlineStyle(nsAWritableString& aOutlineStyle);  \
  NS_IMETHOD    SetOutlineStyle(const nsAReadableString& aOutlineStyle);  \
  NS_IMETHOD    GetOutlineWidth(nsAWritableString& aOutlineWidth);  \
  NS_IMETHOD    SetOutlineWidth(const nsAReadableString& aOutlineWidth);  \
  NS_IMETHOD    GetOverflow(nsAWritableString& aOverflow);  \
  NS_IMETHOD    SetOverflow(const nsAReadableString& aOverflow);  \
  NS_IMETHOD    GetPadding(nsAWritableString& aPadding);  \
  NS_IMETHOD    SetPadding(const nsAReadableString& aPadding);  \
  NS_IMETHOD    GetPaddingTop(nsAWritableString& aPaddingTop);  \
  NS_IMETHOD    SetPaddingTop(const nsAReadableString& aPaddingTop);  \
  NS_IMETHOD    GetPaddingRight(nsAWritableString& aPaddingRight);  \
  NS_IMETHOD    SetPaddingRight(const nsAReadableString& aPaddingRight);  \
  NS_IMETHOD    GetPaddingBottom(nsAWritableString& aPaddingBottom);  \
  NS_IMETHOD    SetPaddingBottom(const nsAReadableString& aPaddingBottom);  \
  NS_IMETHOD    GetPaddingLeft(nsAWritableString& aPaddingLeft);  \
  NS_IMETHOD    SetPaddingLeft(const nsAReadableString& aPaddingLeft);  \
  NS_IMETHOD    GetPage(nsAWritableString& aPage);  \
  NS_IMETHOD    SetPage(const nsAReadableString& aPage);  \
  NS_IMETHOD    GetPageBreakAfter(nsAWritableString& aPageBreakAfter);  \
  NS_IMETHOD    SetPageBreakAfter(const nsAReadableString& aPageBreakAfter);  \
  NS_IMETHOD    GetPageBreakBefore(nsAWritableString& aPageBreakBefore);  \
  NS_IMETHOD    SetPageBreakBefore(const nsAReadableString& aPageBreakBefore);  \
  NS_IMETHOD    GetPageBreakInside(nsAWritableString& aPageBreakInside);  \
  NS_IMETHOD    SetPageBreakInside(const nsAReadableString& aPageBreakInside);  \
  NS_IMETHOD    GetPause(nsAWritableString& aPause);  \
  NS_IMETHOD    SetPause(const nsAReadableString& aPause);  \
  NS_IMETHOD    GetPauseAfter(nsAWritableString& aPauseAfter);  \
  NS_IMETHOD    SetPauseAfter(const nsAReadableString& aPauseAfter);  \
  NS_IMETHOD    GetPauseBefore(nsAWritableString& aPauseBefore);  \
  NS_IMETHOD    SetPauseBefore(const nsAReadableString& aPauseBefore);  \
  NS_IMETHOD    GetPitch(nsAWritableString& aPitch);  \
  NS_IMETHOD    SetPitch(const nsAReadableString& aPitch);  \
  NS_IMETHOD    GetPitchRange(nsAWritableString& aPitchRange);  \
  NS_IMETHOD    SetPitchRange(const nsAReadableString& aPitchRange);  \
  NS_IMETHOD    GetPlayDuring(nsAWritableString& aPlayDuring);  \
  NS_IMETHOD    SetPlayDuring(const nsAReadableString& aPlayDuring);  \
  NS_IMETHOD    GetPosition(nsAWritableString& aPosition);  \
  NS_IMETHOD    SetPosition(const nsAReadableString& aPosition);  \
  NS_IMETHOD    GetQuotes(nsAWritableString& aQuotes);  \
  NS_IMETHOD    SetQuotes(const nsAReadableString& aQuotes);  \
  NS_IMETHOD    GetRichness(nsAWritableString& aRichness);  \
  NS_IMETHOD    SetRichness(const nsAReadableString& aRichness);  \
  NS_IMETHOD    GetRight(nsAWritableString& aRight);  \
  NS_IMETHOD    SetRight(const nsAReadableString& aRight);  \
  NS_IMETHOD    GetSize(nsAWritableString& aSize);  \
  NS_IMETHOD    SetSize(const nsAReadableString& aSize);  \
  NS_IMETHOD    GetSpeak(nsAWritableString& aSpeak);  \
  NS_IMETHOD    SetSpeak(const nsAReadableString& aSpeak);  \
  NS_IMETHOD    GetSpeakHeader(nsAWritableString& aSpeakHeader);  \
  NS_IMETHOD    SetSpeakHeader(const nsAReadableString& aSpeakHeader);  \
  NS_IMETHOD    GetSpeakNumeral(nsAWritableString& aSpeakNumeral);  \
  NS_IMETHOD    SetSpeakNumeral(const nsAReadableString& aSpeakNumeral);  \
  NS_IMETHOD    GetSpeakPunctuation(nsAWritableString& aSpeakPunctuation);  \
  NS_IMETHOD    SetSpeakPunctuation(const nsAReadableString& aSpeakPunctuation);  \
  NS_IMETHOD    GetSpeechRate(nsAWritableString& aSpeechRate);  \
  NS_IMETHOD    SetSpeechRate(const nsAReadableString& aSpeechRate);  \
  NS_IMETHOD    GetStress(nsAWritableString& aStress);  \
  NS_IMETHOD    SetStress(const nsAReadableString& aStress);  \
  NS_IMETHOD    GetTableLayout(nsAWritableString& aTableLayout);  \
  NS_IMETHOD    SetTableLayout(const nsAReadableString& aTableLayout);  \
  NS_IMETHOD    GetTextAlign(nsAWritableString& aTextAlign);  \
  NS_IMETHOD    SetTextAlign(const nsAReadableString& aTextAlign);  \
  NS_IMETHOD    GetTextDecoration(nsAWritableString& aTextDecoration);  \
  NS_IMETHOD    SetTextDecoration(const nsAReadableString& aTextDecoration);  \
  NS_IMETHOD    GetTextIndent(nsAWritableString& aTextIndent);  \
  NS_IMETHOD    SetTextIndent(const nsAReadableString& aTextIndent);  \
  NS_IMETHOD    GetTextShadow(nsAWritableString& aTextShadow);  \
  NS_IMETHOD    SetTextShadow(const nsAReadableString& aTextShadow);  \
  NS_IMETHOD    GetTextTransform(nsAWritableString& aTextTransform);  \
  NS_IMETHOD    SetTextTransform(const nsAReadableString& aTextTransform);  \
  NS_IMETHOD    GetTop(nsAWritableString& aTop);  \
  NS_IMETHOD    SetTop(const nsAReadableString& aTop);  \
  NS_IMETHOD    GetUnicodeBidi(nsAWritableString& aUnicodeBidi);  \
  NS_IMETHOD    SetUnicodeBidi(const nsAReadableString& aUnicodeBidi);  \
  NS_IMETHOD    GetVerticalAlign(nsAWritableString& aVerticalAlign);  \
  NS_IMETHOD    SetVerticalAlign(const nsAReadableString& aVerticalAlign);  \
  NS_IMETHOD    GetVisibility(nsAWritableString& aVisibility);  \
  NS_IMETHOD    SetVisibility(const nsAReadableString& aVisibility);  \
  NS_IMETHOD    GetVoiceFamily(nsAWritableString& aVoiceFamily);  \
  NS_IMETHOD    SetVoiceFamily(const nsAReadableString& aVoiceFamily);  \
  NS_IMETHOD    GetVolume(nsAWritableString& aVolume);  \
  NS_IMETHOD    SetVolume(const nsAReadableString& aVolume);  \
  NS_IMETHOD    GetWhiteSpace(nsAWritableString& aWhiteSpace);  \
  NS_IMETHOD    SetWhiteSpace(const nsAReadableString& aWhiteSpace);  \
  NS_IMETHOD    GetWidows(nsAWritableString& aWidows);  \
  NS_IMETHOD    SetWidows(const nsAReadableString& aWidows);  \
  NS_IMETHOD    GetWidth(nsAWritableString& aWidth);  \
  NS_IMETHOD    SetWidth(const nsAReadableString& aWidth);  \
  NS_IMETHOD    GetWordSpacing(nsAWritableString& aWordSpacing);  \
  NS_IMETHOD    SetWordSpacing(const nsAReadableString& aWordSpacing);  \
  NS_IMETHOD    GetZIndex(nsAWritableString& aZIndex);  \
  NS_IMETHOD    SetZIndex(const nsAReadableString& aZIndex);  \
  NS_IMETHOD    GetMozBinding(nsAWritableString& aMozBinding);  \
  NS_IMETHOD    SetMozBinding(const nsAReadableString& aMozBinding);  \
  NS_IMETHOD    GetMozOpacity(nsAWritableString& aMozOpacity);  \
  NS_IMETHOD    SetMozOpacity(const nsAReadableString& aMozOpacity);  \



#define NS_FORWARD_IDOMCSS2PROPERTIES(_to)  \
  NS_IMETHOD    GetAzimuth(nsAWritableString& aAzimuth) { return _to GetAzimuth(aAzimuth); } \
  NS_IMETHOD    SetAzimuth(const nsAReadableString& aAzimuth) { return _to SetAzimuth(aAzimuth); } \
  NS_IMETHOD    GetBackground(nsAWritableString& aBackground) { return _to GetBackground(aBackground); } \
  NS_IMETHOD    SetBackground(const nsAReadableString& aBackground) { return _to SetBackground(aBackground); } \
  NS_IMETHOD    GetBackgroundAttachment(nsAWritableString& aBackgroundAttachment) { return _to GetBackgroundAttachment(aBackgroundAttachment); } \
  NS_IMETHOD    SetBackgroundAttachment(const nsAReadableString& aBackgroundAttachment) { return _to SetBackgroundAttachment(aBackgroundAttachment); } \
  NS_IMETHOD    GetBackgroundColor(nsAWritableString& aBackgroundColor) { return _to GetBackgroundColor(aBackgroundColor); } \
  NS_IMETHOD    SetBackgroundColor(const nsAReadableString& aBackgroundColor) { return _to SetBackgroundColor(aBackgroundColor); } \
  NS_IMETHOD    GetBackgroundImage(nsAWritableString& aBackgroundImage) { return _to GetBackgroundImage(aBackgroundImage); } \
  NS_IMETHOD    SetBackgroundImage(const nsAReadableString& aBackgroundImage) { return _to SetBackgroundImage(aBackgroundImage); } \
  NS_IMETHOD    GetBackgroundPosition(nsAWritableString& aBackgroundPosition) { return _to GetBackgroundPosition(aBackgroundPosition); } \
  NS_IMETHOD    SetBackgroundPosition(const nsAReadableString& aBackgroundPosition) { return _to SetBackgroundPosition(aBackgroundPosition); } \
  NS_IMETHOD    GetBackgroundRepeat(nsAWritableString& aBackgroundRepeat) { return _to GetBackgroundRepeat(aBackgroundRepeat); } \
  NS_IMETHOD    SetBackgroundRepeat(const nsAReadableString& aBackgroundRepeat) { return _to SetBackgroundRepeat(aBackgroundRepeat); } \
  NS_IMETHOD    GetBorder(nsAWritableString& aBorder) { return _to GetBorder(aBorder); } \
  NS_IMETHOD    SetBorder(const nsAReadableString& aBorder) { return _to SetBorder(aBorder); } \
  NS_IMETHOD    GetBorderCollapse(nsAWritableString& aBorderCollapse) { return _to GetBorderCollapse(aBorderCollapse); } \
  NS_IMETHOD    SetBorderCollapse(const nsAReadableString& aBorderCollapse) { return _to SetBorderCollapse(aBorderCollapse); } \
  NS_IMETHOD    GetBorderColor(nsAWritableString& aBorderColor) { return _to GetBorderColor(aBorderColor); } \
  NS_IMETHOD    SetBorderColor(const nsAReadableString& aBorderColor) { return _to SetBorderColor(aBorderColor); } \
  NS_IMETHOD    GetBorderSpacing(nsAWritableString& aBorderSpacing) { return _to GetBorderSpacing(aBorderSpacing); } \
  NS_IMETHOD    SetBorderSpacing(const nsAReadableString& aBorderSpacing) { return _to SetBorderSpacing(aBorderSpacing); } \
  NS_IMETHOD    GetBorderStyle(nsAWritableString& aBorderStyle) { return _to GetBorderStyle(aBorderStyle); } \
  NS_IMETHOD    SetBorderStyle(const nsAReadableString& aBorderStyle) { return _to SetBorderStyle(aBorderStyle); } \
  NS_IMETHOD    GetBorderTop(nsAWritableString& aBorderTop) { return _to GetBorderTop(aBorderTop); } \
  NS_IMETHOD    SetBorderTop(const nsAReadableString& aBorderTop) { return _to SetBorderTop(aBorderTop); } \
  NS_IMETHOD    GetBorderRight(nsAWritableString& aBorderRight) { return _to GetBorderRight(aBorderRight); } \
  NS_IMETHOD    SetBorderRight(const nsAReadableString& aBorderRight) { return _to SetBorderRight(aBorderRight); } \
  NS_IMETHOD    GetBorderBottom(nsAWritableString& aBorderBottom) { return _to GetBorderBottom(aBorderBottom); } \
  NS_IMETHOD    SetBorderBottom(const nsAReadableString& aBorderBottom) { return _to SetBorderBottom(aBorderBottom); } \
  NS_IMETHOD    GetBorderLeft(nsAWritableString& aBorderLeft) { return _to GetBorderLeft(aBorderLeft); } \
  NS_IMETHOD    SetBorderLeft(const nsAReadableString& aBorderLeft) { return _to SetBorderLeft(aBorderLeft); } \
  NS_IMETHOD    GetBorderTopColor(nsAWritableString& aBorderTopColor) { return _to GetBorderTopColor(aBorderTopColor); } \
  NS_IMETHOD    SetBorderTopColor(const nsAReadableString& aBorderTopColor) { return _to SetBorderTopColor(aBorderTopColor); } \
  NS_IMETHOD    GetBorderRightColor(nsAWritableString& aBorderRightColor) { return _to GetBorderRightColor(aBorderRightColor); } \
  NS_IMETHOD    SetBorderRightColor(const nsAReadableString& aBorderRightColor) { return _to SetBorderRightColor(aBorderRightColor); } \
  NS_IMETHOD    GetBorderBottomColor(nsAWritableString& aBorderBottomColor) { return _to GetBorderBottomColor(aBorderBottomColor); } \
  NS_IMETHOD    SetBorderBottomColor(const nsAReadableString& aBorderBottomColor) { return _to SetBorderBottomColor(aBorderBottomColor); } \
  NS_IMETHOD    GetBorderLeftColor(nsAWritableString& aBorderLeftColor) { return _to GetBorderLeftColor(aBorderLeftColor); } \
  NS_IMETHOD    SetBorderLeftColor(const nsAReadableString& aBorderLeftColor) { return _to SetBorderLeftColor(aBorderLeftColor); } \
  NS_IMETHOD    GetBorderTopStyle(nsAWritableString& aBorderTopStyle) { return _to GetBorderTopStyle(aBorderTopStyle); } \
  NS_IMETHOD    SetBorderTopStyle(const nsAReadableString& aBorderTopStyle) { return _to SetBorderTopStyle(aBorderTopStyle); } \
  NS_IMETHOD    GetBorderRightStyle(nsAWritableString& aBorderRightStyle) { return _to GetBorderRightStyle(aBorderRightStyle); } \
  NS_IMETHOD    SetBorderRightStyle(const nsAReadableString& aBorderRightStyle) { return _to SetBorderRightStyle(aBorderRightStyle); } \
  NS_IMETHOD    GetBorderBottomStyle(nsAWritableString& aBorderBottomStyle) { return _to GetBorderBottomStyle(aBorderBottomStyle); } \
  NS_IMETHOD    SetBorderBottomStyle(const nsAReadableString& aBorderBottomStyle) { return _to SetBorderBottomStyle(aBorderBottomStyle); } \
  NS_IMETHOD    GetBorderLeftStyle(nsAWritableString& aBorderLeftStyle) { return _to GetBorderLeftStyle(aBorderLeftStyle); } \
  NS_IMETHOD    SetBorderLeftStyle(const nsAReadableString& aBorderLeftStyle) { return _to SetBorderLeftStyle(aBorderLeftStyle); } \
  NS_IMETHOD    GetBorderTopWidth(nsAWritableString& aBorderTopWidth) { return _to GetBorderTopWidth(aBorderTopWidth); } \
  NS_IMETHOD    SetBorderTopWidth(const nsAReadableString& aBorderTopWidth) { return _to SetBorderTopWidth(aBorderTopWidth); } \
  NS_IMETHOD    GetBorderRightWidth(nsAWritableString& aBorderRightWidth) { return _to GetBorderRightWidth(aBorderRightWidth); } \
  NS_IMETHOD    SetBorderRightWidth(const nsAReadableString& aBorderRightWidth) { return _to SetBorderRightWidth(aBorderRightWidth); } \
  NS_IMETHOD    GetBorderBottomWidth(nsAWritableString& aBorderBottomWidth) { return _to GetBorderBottomWidth(aBorderBottomWidth); } \
  NS_IMETHOD    SetBorderBottomWidth(const nsAReadableString& aBorderBottomWidth) { return _to SetBorderBottomWidth(aBorderBottomWidth); } \
  NS_IMETHOD    GetBorderLeftWidth(nsAWritableString& aBorderLeftWidth) { return _to GetBorderLeftWidth(aBorderLeftWidth); } \
  NS_IMETHOD    SetBorderLeftWidth(const nsAReadableString& aBorderLeftWidth) { return _to SetBorderLeftWidth(aBorderLeftWidth); } \
  NS_IMETHOD    GetBorderWidth(nsAWritableString& aBorderWidth) { return _to GetBorderWidth(aBorderWidth); } \
  NS_IMETHOD    SetBorderWidth(const nsAReadableString& aBorderWidth) { return _to SetBorderWidth(aBorderWidth); } \
  NS_IMETHOD    GetBottom(nsAWritableString& aBottom) { return _to GetBottom(aBottom); } \
  NS_IMETHOD    SetBottom(const nsAReadableString& aBottom) { return _to SetBottom(aBottom); } \
  NS_IMETHOD    GetCaptionSide(nsAWritableString& aCaptionSide) { return _to GetCaptionSide(aCaptionSide); } \
  NS_IMETHOD    SetCaptionSide(const nsAReadableString& aCaptionSide) { return _to SetCaptionSide(aCaptionSide); } \
  NS_IMETHOD    GetClear(nsAWritableString& aClear) { return _to GetClear(aClear); } \
  NS_IMETHOD    SetClear(const nsAReadableString& aClear) { return _to SetClear(aClear); } \
  NS_IMETHOD    GetClip(nsAWritableString& aClip) { return _to GetClip(aClip); } \
  NS_IMETHOD    SetClip(const nsAReadableString& aClip) { return _to SetClip(aClip); } \
  NS_IMETHOD    GetColor(nsAWritableString& aColor) { return _to GetColor(aColor); } \
  NS_IMETHOD    SetColor(const nsAReadableString& aColor) { return _to SetColor(aColor); } \
  NS_IMETHOD    GetContent(nsAWritableString& aContent) { return _to GetContent(aContent); } \
  NS_IMETHOD    SetContent(const nsAReadableString& aContent) { return _to SetContent(aContent); } \
  NS_IMETHOD    GetCounterIncrement(nsAWritableString& aCounterIncrement) { return _to GetCounterIncrement(aCounterIncrement); } \
  NS_IMETHOD    SetCounterIncrement(const nsAReadableString& aCounterIncrement) { return _to SetCounterIncrement(aCounterIncrement); } \
  NS_IMETHOD    GetCounterReset(nsAWritableString& aCounterReset) { return _to GetCounterReset(aCounterReset); } \
  NS_IMETHOD    SetCounterReset(const nsAReadableString& aCounterReset) { return _to SetCounterReset(aCounterReset); } \
  NS_IMETHOD    GetCue(nsAWritableString& aCue) { return _to GetCue(aCue); } \
  NS_IMETHOD    SetCue(const nsAReadableString& aCue) { return _to SetCue(aCue); } \
  NS_IMETHOD    GetCueAfter(nsAWritableString& aCueAfter) { return _to GetCueAfter(aCueAfter); } \
  NS_IMETHOD    SetCueAfter(const nsAReadableString& aCueAfter) { return _to SetCueAfter(aCueAfter); } \
  NS_IMETHOD    GetCueBefore(nsAWritableString& aCueBefore) { return _to GetCueBefore(aCueBefore); } \
  NS_IMETHOD    SetCueBefore(const nsAReadableString& aCueBefore) { return _to SetCueBefore(aCueBefore); } \
  NS_IMETHOD    GetCursor(nsAWritableString& aCursor) { return _to GetCursor(aCursor); } \
  NS_IMETHOD    SetCursor(const nsAReadableString& aCursor) { return _to SetCursor(aCursor); } \
  NS_IMETHOD    GetDirection(nsAWritableString& aDirection) { return _to GetDirection(aDirection); } \
  NS_IMETHOD    SetDirection(const nsAReadableString& aDirection) { return _to SetDirection(aDirection); } \
  NS_IMETHOD    GetDisplay(nsAWritableString& aDisplay) { return _to GetDisplay(aDisplay); } \
  NS_IMETHOD    SetDisplay(const nsAReadableString& aDisplay) { return _to SetDisplay(aDisplay); } \
  NS_IMETHOD    GetElevation(nsAWritableString& aElevation) { return _to GetElevation(aElevation); } \
  NS_IMETHOD    SetElevation(const nsAReadableString& aElevation) { return _to SetElevation(aElevation); } \
  NS_IMETHOD    GetEmptyCells(nsAWritableString& aEmptyCells) { return _to GetEmptyCells(aEmptyCells); } \
  NS_IMETHOD    SetEmptyCells(const nsAReadableString& aEmptyCells) { return _to SetEmptyCells(aEmptyCells); } \
  NS_IMETHOD    GetCssFloat(nsAWritableString& aCssFloat) { return _to GetCssFloat(aCssFloat); } \
  NS_IMETHOD    SetCssFloat(const nsAReadableString& aCssFloat) { return _to SetCssFloat(aCssFloat); } \
  NS_IMETHOD    GetFont(nsAWritableString& aFont) { return _to GetFont(aFont); } \
  NS_IMETHOD    SetFont(const nsAReadableString& aFont) { return _to SetFont(aFont); } \
  NS_IMETHOD    GetFontFamily(nsAWritableString& aFontFamily) { return _to GetFontFamily(aFontFamily); } \
  NS_IMETHOD    SetFontFamily(const nsAReadableString& aFontFamily) { return _to SetFontFamily(aFontFamily); } \
  NS_IMETHOD    GetFontSize(nsAWritableString& aFontSize) { return _to GetFontSize(aFontSize); } \
  NS_IMETHOD    SetFontSize(const nsAReadableString& aFontSize) { return _to SetFontSize(aFontSize); } \
  NS_IMETHOD    GetFontSizeAdjust(nsAWritableString& aFontSizeAdjust) { return _to GetFontSizeAdjust(aFontSizeAdjust); } \
  NS_IMETHOD    SetFontSizeAdjust(const nsAReadableString& aFontSizeAdjust) { return _to SetFontSizeAdjust(aFontSizeAdjust); } \
  NS_IMETHOD    GetFontStretch(nsAWritableString& aFontStretch) { return _to GetFontStretch(aFontStretch); } \
  NS_IMETHOD    SetFontStretch(const nsAReadableString& aFontStretch) { return _to SetFontStretch(aFontStretch); } \
  NS_IMETHOD    GetFontStyle(nsAWritableString& aFontStyle) { return _to GetFontStyle(aFontStyle); } \
  NS_IMETHOD    SetFontStyle(const nsAReadableString& aFontStyle) { return _to SetFontStyle(aFontStyle); } \
  NS_IMETHOD    GetFontVariant(nsAWritableString& aFontVariant) { return _to GetFontVariant(aFontVariant); } \
  NS_IMETHOD    SetFontVariant(const nsAReadableString& aFontVariant) { return _to SetFontVariant(aFontVariant); } \
  NS_IMETHOD    GetFontWeight(nsAWritableString& aFontWeight) { return _to GetFontWeight(aFontWeight); } \
  NS_IMETHOD    SetFontWeight(const nsAReadableString& aFontWeight) { return _to SetFontWeight(aFontWeight); } \
  NS_IMETHOD    GetHeight(nsAWritableString& aHeight) { return _to GetHeight(aHeight); } \
  NS_IMETHOD    SetHeight(const nsAReadableString& aHeight) { return _to SetHeight(aHeight); } \
  NS_IMETHOD    GetLeft(nsAWritableString& aLeft) { return _to GetLeft(aLeft); } \
  NS_IMETHOD    SetLeft(const nsAReadableString& aLeft) { return _to SetLeft(aLeft); } \
  NS_IMETHOD    GetLetterSpacing(nsAWritableString& aLetterSpacing) { return _to GetLetterSpacing(aLetterSpacing); } \
  NS_IMETHOD    SetLetterSpacing(const nsAReadableString& aLetterSpacing) { return _to SetLetterSpacing(aLetterSpacing); } \
  NS_IMETHOD    GetLineHeight(nsAWritableString& aLineHeight) { return _to GetLineHeight(aLineHeight); } \
  NS_IMETHOD    SetLineHeight(const nsAReadableString& aLineHeight) { return _to SetLineHeight(aLineHeight); } \
  NS_IMETHOD    GetListStyle(nsAWritableString& aListStyle) { return _to GetListStyle(aListStyle); } \
  NS_IMETHOD    SetListStyle(const nsAReadableString& aListStyle) { return _to SetListStyle(aListStyle); } \
  NS_IMETHOD    GetListStyleImage(nsAWritableString& aListStyleImage) { return _to GetListStyleImage(aListStyleImage); } \
  NS_IMETHOD    SetListStyleImage(const nsAReadableString& aListStyleImage) { return _to SetListStyleImage(aListStyleImage); } \
  NS_IMETHOD    GetListStylePosition(nsAWritableString& aListStylePosition) { return _to GetListStylePosition(aListStylePosition); } \
  NS_IMETHOD    SetListStylePosition(const nsAReadableString& aListStylePosition) { return _to SetListStylePosition(aListStylePosition); } \
  NS_IMETHOD    GetListStyleType(nsAWritableString& aListStyleType) { return _to GetListStyleType(aListStyleType); } \
  NS_IMETHOD    SetListStyleType(const nsAReadableString& aListStyleType) { return _to SetListStyleType(aListStyleType); } \
  NS_IMETHOD    GetMargin(nsAWritableString& aMargin) { return _to GetMargin(aMargin); } \
  NS_IMETHOD    SetMargin(const nsAReadableString& aMargin) { return _to SetMargin(aMargin); } \
  NS_IMETHOD    GetMarginTop(nsAWritableString& aMarginTop) { return _to GetMarginTop(aMarginTop); } \
  NS_IMETHOD    SetMarginTop(const nsAReadableString& aMarginTop) { return _to SetMarginTop(aMarginTop); } \
  NS_IMETHOD    GetMarginRight(nsAWritableString& aMarginRight) { return _to GetMarginRight(aMarginRight); } \
  NS_IMETHOD    SetMarginRight(const nsAReadableString& aMarginRight) { return _to SetMarginRight(aMarginRight); } \
  NS_IMETHOD    GetMarginBottom(nsAWritableString& aMarginBottom) { return _to GetMarginBottom(aMarginBottom); } \
  NS_IMETHOD    SetMarginBottom(const nsAReadableString& aMarginBottom) { return _to SetMarginBottom(aMarginBottom); } \
  NS_IMETHOD    GetMarginLeft(nsAWritableString& aMarginLeft) { return _to GetMarginLeft(aMarginLeft); } \
  NS_IMETHOD    SetMarginLeft(const nsAReadableString& aMarginLeft) { return _to SetMarginLeft(aMarginLeft); } \
  NS_IMETHOD    GetMarkerOffset(nsAWritableString& aMarkerOffset) { return _to GetMarkerOffset(aMarkerOffset); } \
  NS_IMETHOD    SetMarkerOffset(const nsAReadableString& aMarkerOffset) { return _to SetMarkerOffset(aMarkerOffset); } \
  NS_IMETHOD    GetMarks(nsAWritableString& aMarks) { return _to GetMarks(aMarks); } \
  NS_IMETHOD    SetMarks(const nsAReadableString& aMarks) { return _to SetMarks(aMarks); } \
  NS_IMETHOD    GetMaxHeight(nsAWritableString& aMaxHeight) { return _to GetMaxHeight(aMaxHeight); } \
  NS_IMETHOD    SetMaxHeight(const nsAReadableString& aMaxHeight) { return _to SetMaxHeight(aMaxHeight); } \
  NS_IMETHOD    GetMaxWidth(nsAWritableString& aMaxWidth) { return _to GetMaxWidth(aMaxWidth); } \
  NS_IMETHOD    SetMaxWidth(const nsAReadableString& aMaxWidth) { return _to SetMaxWidth(aMaxWidth); } \
  NS_IMETHOD    GetMinHeight(nsAWritableString& aMinHeight) { return _to GetMinHeight(aMinHeight); } \
  NS_IMETHOD    SetMinHeight(const nsAReadableString& aMinHeight) { return _to SetMinHeight(aMinHeight); } \
  NS_IMETHOD    GetMinWidth(nsAWritableString& aMinWidth) { return _to GetMinWidth(aMinWidth); } \
  NS_IMETHOD    SetMinWidth(const nsAReadableString& aMinWidth) { return _to SetMinWidth(aMinWidth); } \
  NS_IMETHOD    GetOrphans(nsAWritableString& aOrphans) { return _to GetOrphans(aOrphans); } \
  NS_IMETHOD    SetOrphans(const nsAReadableString& aOrphans) { return _to SetOrphans(aOrphans); } \
  NS_IMETHOD    GetOutline(nsAWritableString& aOutline) { return _to GetOutline(aOutline); } \
  NS_IMETHOD    SetOutline(const nsAReadableString& aOutline) { return _to SetOutline(aOutline); } \
  NS_IMETHOD    GetOutlineColor(nsAWritableString& aOutlineColor) { return _to GetOutlineColor(aOutlineColor); } \
  NS_IMETHOD    SetOutlineColor(const nsAReadableString& aOutlineColor) { return _to SetOutlineColor(aOutlineColor); } \
  NS_IMETHOD    GetOutlineStyle(nsAWritableString& aOutlineStyle) { return _to GetOutlineStyle(aOutlineStyle); } \
  NS_IMETHOD    SetOutlineStyle(const nsAReadableString& aOutlineStyle) { return _to SetOutlineStyle(aOutlineStyle); } \
  NS_IMETHOD    GetOutlineWidth(nsAWritableString& aOutlineWidth) { return _to GetOutlineWidth(aOutlineWidth); } \
  NS_IMETHOD    SetOutlineWidth(const nsAReadableString& aOutlineWidth) { return _to SetOutlineWidth(aOutlineWidth); } \
  NS_IMETHOD    GetOverflow(nsAWritableString& aOverflow) { return _to GetOverflow(aOverflow); } \
  NS_IMETHOD    SetOverflow(const nsAReadableString& aOverflow) { return _to SetOverflow(aOverflow); } \
  NS_IMETHOD    GetPadding(nsAWritableString& aPadding) { return _to GetPadding(aPadding); } \
  NS_IMETHOD    SetPadding(const nsAReadableString& aPadding) { return _to SetPadding(aPadding); } \
  NS_IMETHOD    GetPaddingTop(nsAWritableString& aPaddingTop) { return _to GetPaddingTop(aPaddingTop); } \
  NS_IMETHOD    SetPaddingTop(const nsAReadableString& aPaddingTop) { return _to SetPaddingTop(aPaddingTop); } \
  NS_IMETHOD    GetPaddingRight(nsAWritableString& aPaddingRight) { return _to GetPaddingRight(aPaddingRight); } \
  NS_IMETHOD    SetPaddingRight(const nsAReadableString& aPaddingRight) { return _to SetPaddingRight(aPaddingRight); } \
  NS_IMETHOD    GetPaddingBottom(nsAWritableString& aPaddingBottom) { return _to GetPaddingBottom(aPaddingBottom); } \
  NS_IMETHOD    SetPaddingBottom(const nsAReadableString& aPaddingBottom) { return _to SetPaddingBottom(aPaddingBottom); } \
  NS_IMETHOD    GetPaddingLeft(nsAWritableString& aPaddingLeft) { return _to GetPaddingLeft(aPaddingLeft); } \
  NS_IMETHOD    SetPaddingLeft(const nsAReadableString& aPaddingLeft) { return _to SetPaddingLeft(aPaddingLeft); } \
  NS_IMETHOD    GetPage(nsAWritableString& aPage) { return _to GetPage(aPage); } \
  NS_IMETHOD    SetPage(const nsAReadableString& aPage) { return _to SetPage(aPage); } \
  NS_IMETHOD    GetPageBreakAfter(nsAWritableString& aPageBreakAfter) { return _to GetPageBreakAfter(aPageBreakAfter); } \
  NS_IMETHOD    SetPageBreakAfter(const nsAReadableString& aPageBreakAfter) { return _to SetPageBreakAfter(aPageBreakAfter); } \
  NS_IMETHOD    GetPageBreakBefore(nsAWritableString& aPageBreakBefore) { return _to GetPageBreakBefore(aPageBreakBefore); } \
  NS_IMETHOD    SetPageBreakBefore(const nsAReadableString& aPageBreakBefore) { return _to SetPageBreakBefore(aPageBreakBefore); } \
  NS_IMETHOD    GetPageBreakInside(nsAWritableString& aPageBreakInside) { return _to GetPageBreakInside(aPageBreakInside); } \
  NS_IMETHOD    SetPageBreakInside(const nsAReadableString& aPageBreakInside) { return _to SetPageBreakInside(aPageBreakInside); } \
  NS_IMETHOD    GetPause(nsAWritableString& aPause) { return _to GetPause(aPause); } \
  NS_IMETHOD    SetPause(const nsAReadableString& aPause) { return _to SetPause(aPause); } \
  NS_IMETHOD    GetPauseAfter(nsAWritableString& aPauseAfter) { return _to GetPauseAfter(aPauseAfter); } \
  NS_IMETHOD    SetPauseAfter(const nsAReadableString& aPauseAfter) { return _to SetPauseAfter(aPauseAfter); } \
  NS_IMETHOD    GetPauseBefore(nsAWritableString& aPauseBefore) { return _to GetPauseBefore(aPauseBefore); } \
  NS_IMETHOD    SetPauseBefore(const nsAReadableString& aPauseBefore) { return _to SetPauseBefore(aPauseBefore); } \
  NS_IMETHOD    GetPitch(nsAWritableString& aPitch) { return _to GetPitch(aPitch); } \
  NS_IMETHOD    SetPitch(const nsAReadableString& aPitch) { return _to SetPitch(aPitch); } \
  NS_IMETHOD    GetPitchRange(nsAWritableString& aPitchRange) { return _to GetPitchRange(aPitchRange); } \
  NS_IMETHOD    SetPitchRange(const nsAReadableString& aPitchRange) { return _to SetPitchRange(aPitchRange); } \
  NS_IMETHOD    GetPlayDuring(nsAWritableString& aPlayDuring) { return _to GetPlayDuring(aPlayDuring); } \
  NS_IMETHOD    SetPlayDuring(const nsAReadableString& aPlayDuring) { return _to SetPlayDuring(aPlayDuring); } \
  NS_IMETHOD    GetPosition(nsAWritableString& aPosition) { return _to GetPosition(aPosition); } \
  NS_IMETHOD    SetPosition(const nsAReadableString& aPosition) { return _to SetPosition(aPosition); } \
  NS_IMETHOD    GetQuotes(nsAWritableString& aQuotes) { return _to GetQuotes(aQuotes); } \
  NS_IMETHOD    SetQuotes(const nsAReadableString& aQuotes) { return _to SetQuotes(aQuotes); } \
  NS_IMETHOD    GetRichness(nsAWritableString& aRichness) { return _to GetRichness(aRichness); } \
  NS_IMETHOD    SetRichness(const nsAReadableString& aRichness) { return _to SetRichness(aRichness); } \
  NS_IMETHOD    GetRight(nsAWritableString& aRight) { return _to GetRight(aRight); } \
  NS_IMETHOD    SetRight(const nsAReadableString& aRight) { return _to SetRight(aRight); } \
  NS_IMETHOD    GetSize(nsAWritableString& aSize) { return _to GetSize(aSize); } \
  NS_IMETHOD    SetSize(const nsAReadableString& aSize) { return _to SetSize(aSize); } \
  NS_IMETHOD    GetSpeak(nsAWritableString& aSpeak) { return _to GetSpeak(aSpeak); } \
  NS_IMETHOD    SetSpeak(const nsAReadableString& aSpeak) { return _to SetSpeak(aSpeak); } \
  NS_IMETHOD    GetSpeakHeader(nsAWritableString& aSpeakHeader) { return _to GetSpeakHeader(aSpeakHeader); } \
  NS_IMETHOD    SetSpeakHeader(const nsAReadableString& aSpeakHeader) { return _to SetSpeakHeader(aSpeakHeader); } \
  NS_IMETHOD    GetSpeakNumeral(nsAWritableString& aSpeakNumeral) { return _to GetSpeakNumeral(aSpeakNumeral); } \
  NS_IMETHOD    SetSpeakNumeral(const nsAReadableString& aSpeakNumeral) { return _to SetSpeakNumeral(aSpeakNumeral); } \
  NS_IMETHOD    GetSpeakPunctuation(nsAWritableString& aSpeakPunctuation) { return _to GetSpeakPunctuation(aSpeakPunctuation); } \
  NS_IMETHOD    SetSpeakPunctuation(const nsAReadableString& aSpeakPunctuation) { return _to SetSpeakPunctuation(aSpeakPunctuation); } \
  NS_IMETHOD    GetSpeechRate(nsAWritableString& aSpeechRate) { return _to GetSpeechRate(aSpeechRate); } \
  NS_IMETHOD    SetSpeechRate(const nsAReadableString& aSpeechRate) { return _to SetSpeechRate(aSpeechRate); } \
  NS_IMETHOD    GetStress(nsAWritableString& aStress) { return _to GetStress(aStress); } \
  NS_IMETHOD    SetStress(const nsAReadableString& aStress) { return _to SetStress(aStress); } \
  NS_IMETHOD    GetTableLayout(nsAWritableString& aTableLayout) { return _to GetTableLayout(aTableLayout); } \
  NS_IMETHOD    SetTableLayout(const nsAReadableString& aTableLayout) { return _to SetTableLayout(aTableLayout); } \
  NS_IMETHOD    GetTextAlign(nsAWritableString& aTextAlign) { return _to GetTextAlign(aTextAlign); } \
  NS_IMETHOD    SetTextAlign(const nsAReadableString& aTextAlign) { return _to SetTextAlign(aTextAlign); } \
  NS_IMETHOD    GetTextDecoration(nsAWritableString& aTextDecoration) { return _to GetTextDecoration(aTextDecoration); } \
  NS_IMETHOD    SetTextDecoration(const nsAReadableString& aTextDecoration) { return _to SetTextDecoration(aTextDecoration); } \
  NS_IMETHOD    GetTextIndent(nsAWritableString& aTextIndent) { return _to GetTextIndent(aTextIndent); } \
  NS_IMETHOD    SetTextIndent(const nsAReadableString& aTextIndent) { return _to SetTextIndent(aTextIndent); } \
  NS_IMETHOD    GetTextShadow(nsAWritableString& aTextShadow) { return _to GetTextShadow(aTextShadow); } \
  NS_IMETHOD    SetTextShadow(const nsAReadableString& aTextShadow) { return _to SetTextShadow(aTextShadow); } \
  NS_IMETHOD    GetTextTransform(nsAWritableString& aTextTransform) { return _to GetTextTransform(aTextTransform); } \
  NS_IMETHOD    SetTextTransform(const nsAReadableString& aTextTransform) { return _to SetTextTransform(aTextTransform); } \
  NS_IMETHOD    GetTop(nsAWritableString& aTop) { return _to GetTop(aTop); } \
  NS_IMETHOD    SetTop(const nsAReadableString& aTop) { return _to SetTop(aTop); } \
  NS_IMETHOD    GetUnicodeBidi(nsAWritableString& aUnicodeBidi) { return _to GetUnicodeBidi(aUnicodeBidi); } \
  NS_IMETHOD    SetUnicodeBidi(const nsAReadableString& aUnicodeBidi) { return _to SetUnicodeBidi(aUnicodeBidi); } \
  NS_IMETHOD    GetVerticalAlign(nsAWritableString& aVerticalAlign) { return _to GetVerticalAlign(aVerticalAlign); } \
  NS_IMETHOD    SetVerticalAlign(const nsAReadableString& aVerticalAlign) { return _to SetVerticalAlign(aVerticalAlign); } \
  NS_IMETHOD    GetVisibility(nsAWritableString& aVisibility) { return _to GetVisibility(aVisibility); } \
  NS_IMETHOD    SetVisibility(const nsAReadableString& aVisibility) { return _to SetVisibility(aVisibility); } \
  NS_IMETHOD    GetVoiceFamily(nsAWritableString& aVoiceFamily) { return _to GetVoiceFamily(aVoiceFamily); } \
  NS_IMETHOD    SetVoiceFamily(const nsAReadableString& aVoiceFamily) { return _to SetVoiceFamily(aVoiceFamily); } \
  NS_IMETHOD    GetVolume(nsAWritableString& aVolume) { return _to GetVolume(aVolume); } \
  NS_IMETHOD    SetVolume(const nsAReadableString& aVolume) { return _to SetVolume(aVolume); } \
  NS_IMETHOD    GetWhiteSpace(nsAWritableString& aWhiteSpace) { return _to GetWhiteSpace(aWhiteSpace); } \
  NS_IMETHOD    SetWhiteSpace(const nsAReadableString& aWhiteSpace) { return _to SetWhiteSpace(aWhiteSpace); } \
  NS_IMETHOD    GetWidows(nsAWritableString& aWidows) { return _to GetWidows(aWidows); } \
  NS_IMETHOD    SetWidows(const nsAReadableString& aWidows) { return _to SetWidows(aWidows); } \
  NS_IMETHOD    GetWidth(nsAWritableString& aWidth) { return _to GetWidth(aWidth); } \
  NS_IMETHOD    SetWidth(const nsAReadableString& aWidth) { return _to SetWidth(aWidth); } \
  NS_IMETHOD    GetWordSpacing(nsAWritableString& aWordSpacing) { return _to GetWordSpacing(aWordSpacing); } \
  NS_IMETHOD    SetWordSpacing(const nsAReadableString& aWordSpacing) { return _to SetWordSpacing(aWordSpacing); } \
  NS_IMETHOD    GetZIndex(nsAWritableString& aZIndex) { return _to GetZIndex(aZIndex); } \
  NS_IMETHOD    SetZIndex(const nsAReadableString& aZIndex) { return _to SetZIndex(aZIndex); } \
  NS_IMETHOD    GetMozBinding(nsAWritableString& aMozBinding) { return _to GetMozBinding(aMozBinding); } \
  NS_IMETHOD    SetMozBinding(const nsAReadableString& aMozBinding) { return _to SetMozBinding(aMozBinding); } \
  NS_IMETHOD    GetMozOpacity(nsAWritableString& aMozOpacity) { return _to GetMozOpacity(aMozOpacity); } \
  NS_IMETHOD    SetMozOpacity(const nsAReadableString& aMozOpacity) { return _to SetMozOpacity(aMozOpacity); } \


extern "C" NS_DOM nsresult NS_InitCSS2PropertiesClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptCSS2Properties(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMCSS2Properties_h__
