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

#include "nsDOMCSSDeclaration.h"
#include "nsIDOMCSSRule.h"
#include "nsICSSParser.h"
#include "nsIStyleRule.h"
#include "nsICSSDeclaration.h"
#include "nsCSSProps.h"
#include "nsCOMPtr.h"
#include "nsIURL.h"

#include "nsContentUtils.h"


nsDOMCSSDeclaration::nsDOMCSSDeclaration()
{
  NS_INIT_REFCNT();
}

nsDOMCSSDeclaration::~nsDOMCSSDeclaration()
{
}


// QueryInterface implementation for CSSStyleSheetImpl
NS_INTERFACE_MAP_BEGIN(nsDOMCSSDeclaration)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSStyleDeclaration)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSS2Properties)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSCSS2Properties)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMCSS2Properties)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CSSStyleDeclaration)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsDOMCSSDeclaration);
NS_IMPL_RELEASE(nsDOMCSSDeclaration);


NS_IMETHODIMP
nsDOMCSSDeclaration::GetCssText(nsAWritableString& aCssText)
{
  nsCOMPtr<nsICSSDeclaration> decl;
  aCssText.Truncate();
  GetCSSDeclaration(getter_AddRefs(decl), PR_FALSE);
  NS_ASSERTION(decl, "null CSSDeclaration");

  if (decl) {
    decl->ToString(aCssText);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCssText(const nsAReadableString& aCssText)
{
  // XXX TBI
  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetLength(PRUint32* aLength)
{
  nsICSSDeclaration *decl;
  nsresult result = GetCSSDeclaration(&decl, PR_FALSE);
 
  *aLength = 0;
  if ((NS_OK == result) && (nsnull != decl)) {
    result = decl->Count(aLength);
    NS_RELEASE(decl);
  }

  return result;
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  NS_ENSURE_ARG_POINTER(aParentRule);
  *aParentRule = nsnull;

  nsCOMPtr<nsISupports> parent;

  GetParent(getter_AddRefs(parent));

  if (parent) {
    parent->QueryInterface(NS_GET_IID(nsIDOMCSSRule), (void **)aParentRule);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPropertyCSSValue(const nsAReadableString& aPropertyName,
                                         nsIDOMCSSValue** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  // We don't support CSSValue yet so we'll just return null...
  *aReturn = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSDeclaration::Item(PRUint32 aIndex, nsAWritableString& aReturn)
{
  nsICSSDeclaration *decl;
  nsresult result = GetCSSDeclaration(&decl, PR_FALSE);

  aReturn.SetLength(0);
  if ((NS_OK == result) && (nsnull != decl)) {
    result = decl->GetNthProperty(aIndex, aReturn);
    NS_RELEASE(decl);
  }

  return result;
}

NS_IMETHODIMP    
nsDOMCSSDeclaration::GetPropertyValue(const nsAReadableString& aPropertyName, 
                                     nsAWritableString& aReturn)
{
  nsCSSValue val;
  nsICSSDeclaration *decl;
  nsresult result = GetCSSDeclaration(&decl, PR_FALSE);

  aReturn.SetLength(0);
  if ((NS_OK == result) && (nsnull != decl)) {
    result = decl->GetValue(aPropertyName, aReturn);
    NS_RELEASE(decl);
  }

  return result;
}

NS_IMETHODIMP    
nsDOMCSSDeclaration::GetPropertyPriority(const nsAReadableString& aPropertyName, 
                                        nsAWritableString& aReturn)
{
  nsICSSDeclaration *decl;
  nsresult result = GetCSSDeclaration(&decl, PR_FALSE);
  PRBool isImportant = PR_FALSE;

  if ((NS_OK == result) && (nsnull != decl)) {
    result = decl->GetValueIsImportant(aPropertyName, isImportant);
    NS_RELEASE(decl);
  }

  if ((NS_OK == result) && isImportant) {
    aReturn.Assign(NS_LITERAL_STRING("!important"));    
  }
  else {
    aReturn.SetLength(0);
  }

  return result;
}

NS_IMETHODIMP    
nsDOMCSSDeclaration::SetProperty(const nsAReadableString& aPropertyName, 
                                 const nsAReadableString& aValue, 
                                 const nsAReadableString& aPriority)
{
  if (!aValue.Length()) {
     // If the new value of the property is an empty string we remove the
     // property.
    nsAutoString tmp;
    return RemoveProperty(aPropertyName, tmp);
  }

  nsAutoString declString;

  declString.Assign(aPropertyName);
  declString.Append(PRUnichar(':'));
  declString.Append(aValue);
  declString.Append(aPriority);

  return ParseDeclaration(declString, PR_TRUE, PR_FALSE);
}

/**
 * Helper function to reduce code size.
 */
static nsresult
CallSetProperty(nsDOMCSSDeclaration* aDecl,
                const nsAReadableString& aPropName,
                const nsAReadableString& aValue)
{
  PRUnichar nullChar = PRUnichar('\0');
  return aDecl->SetProperty(aPropName, aValue,
                            nsDependentString(&nullChar, PRUint32(0)));
}

#define IMPL_CSSPROP(attname_, propname_)                                     \
  NS_IMETHODIMP                                                               \
  nsDOMCSSDeclaration::Get##attname_(nsAWritableString& aValue)               \
  {                                                                           \
    return GetPropertyValue(propname_, aValue);                               \
  }                                                                           \
                                                                              \
  NS_IMETHODIMP                                                               \
  nsDOMCSSDeclaration::Set##attname_(const nsAReadableString& aValue)         \
  {                                                                           \
    return CallSetProperty(this, propname_, aValue);                          \
  }

  // nsIDOMCSS2Properties
IMPL_CSSPROP(Azimuth, NS_LITERAL_STRING("azimuth"))
IMPL_CSSPROP(Background, NS_LITERAL_STRING("background"))
IMPL_CSSPROP(BackgroundAttachment, NS_LITERAL_STRING("background-attachment"))
IMPL_CSSPROP(BackgroundColor, NS_LITERAL_STRING("background-color"))
IMPL_CSSPROP(BackgroundImage, NS_LITERAL_STRING("background-image"))
IMPL_CSSPROP(BackgroundPosition, NS_LITERAL_STRING("background-position"))
IMPL_CSSPROP(BackgroundRepeat, NS_LITERAL_STRING("background-repeat"))
IMPL_CSSPROP(Border, NS_LITERAL_STRING("border"))
IMPL_CSSPROP(BorderCollapse, NS_LITERAL_STRING("border-collapse"))
IMPL_CSSPROP(BorderColor, NS_LITERAL_STRING("border-color"))
IMPL_CSSPROP(BorderSpacing, NS_LITERAL_STRING("border-spacing"))
IMPL_CSSPROP(BorderStyle, NS_LITERAL_STRING("border-style"))
IMPL_CSSPROP(BorderTop, NS_LITERAL_STRING("border-top"))
IMPL_CSSPROP(BorderRight, NS_LITERAL_STRING("border-right"))
IMPL_CSSPROP(BorderBottom, NS_LITERAL_STRING("border-bottom"))
IMPL_CSSPROP(BorderLeft, NS_LITERAL_STRING("border-left"))
IMPL_CSSPROP(BorderTopColor, NS_LITERAL_STRING("border-top-color"))
IMPL_CSSPROP(BorderRightColor, NS_LITERAL_STRING("border-right-color"))
IMPL_CSSPROP(BorderBottomColor, NS_LITERAL_STRING("border-bottom-color"))
IMPL_CSSPROP(BorderLeftColor, NS_LITERAL_STRING("border-left-color"))
IMPL_CSSPROP(BorderTopStyle, NS_LITERAL_STRING("border-top-style"))
IMPL_CSSPROP(BorderRightStyle, NS_LITERAL_STRING("border-right-style"))
IMPL_CSSPROP(BorderBottomStyle, NS_LITERAL_STRING("border-bottom-style"))
IMPL_CSSPROP(BorderLeftStyle, NS_LITERAL_STRING("border-left-style"))
IMPL_CSSPROP(BorderTopWidth, NS_LITERAL_STRING("border-top-width"))
IMPL_CSSPROP(BorderRightWidth, NS_LITERAL_STRING("border-right-width"))
IMPL_CSSPROP(BorderBottomWidth, NS_LITERAL_STRING("border-bottom-width"))
IMPL_CSSPROP(BorderLeftWidth, NS_LITERAL_STRING("border-left-width"))
IMPL_CSSPROP(BorderWidth, NS_LITERAL_STRING("border-width"))
IMPL_CSSPROP(Bottom, NS_LITERAL_STRING("bottom"))
IMPL_CSSPROP(CaptionSide, NS_LITERAL_STRING("caption-side"))
IMPL_CSSPROP(Clear, NS_LITERAL_STRING("clear"))
IMPL_CSSPROP(Clip, NS_LITERAL_STRING("clip"))
IMPL_CSSPROP(Color, NS_LITERAL_STRING("color"))
IMPL_CSSPROP(Content, NS_LITERAL_STRING("content"))
IMPL_CSSPROP(CounterIncrement, NS_LITERAL_STRING("counter-increment"))
IMPL_CSSPROP(CounterReset, NS_LITERAL_STRING("counter-reset"))
IMPL_CSSPROP(CssFloat, NS_LITERAL_STRING("float"))
IMPL_CSSPROP(Cue, NS_LITERAL_STRING("cue"))
IMPL_CSSPROP(CueAfter, NS_LITERAL_STRING("cue-after"))
IMPL_CSSPROP(CueBefore, NS_LITERAL_STRING("cue-before"))
IMPL_CSSPROP(Cursor, NS_LITERAL_STRING("cursor"))
IMPL_CSSPROP(Direction, NS_LITERAL_STRING("direction"))
IMPL_CSSPROP(Display, NS_LITERAL_STRING("display"))
IMPL_CSSPROP(Elevation, NS_LITERAL_STRING("elevation"))
IMPL_CSSPROP(EmptyCells, NS_LITERAL_STRING("empty-cells"))
IMPL_CSSPROP(Font, NS_LITERAL_STRING("font"))
IMPL_CSSPROP(FontFamily, NS_LITERAL_STRING("font-family"))
IMPL_CSSPROP(FontSize, NS_LITERAL_STRING("font-size"))
IMPL_CSSPROP(FontSizeAdjust, NS_LITERAL_STRING("font-size-adjust"))
IMPL_CSSPROP(FontStretch, NS_LITERAL_STRING("font-stretch"))
IMPL_CSSPROP(FontStyle, NS_LITERAL_STRING("font-style"))
IMPL_CSSPROP(FontVariant, NS_LITERAL_STRING("font-variant"))
IMPL_CSSPROP(FontWeight, NS_LITERAL_STRING("font-weight"))
IMPL_CSSPROP(Height, NS_LITERAL_STRING("height"))
IMPL_CSSPROP(Left, NS_LITERAL_STRING("left"))
IMPL_CSSPROP(LetterSpacing, NS_LITERAL_STRING("letter-spacing"))
IMPL_CSSPROP(LineHeight, NS_LITERAL_STRING("line-height"))
IMPL_CSSPROP(ListStyle, NS_LITERAL_STRING("list-style"))
IMPL_CSSPROP(ListStyleImage, NS_LITERAL_STRING("list-style-image"))
IMPL_CSSPROP(ListStylePosition, NS_LITERAL_STRING("list-style-position"))
IMPL_CSSPROP(ListStyleType, NS_LITERAL_STRING("list-style-type"))
IMPL_CSSPROP(Margin, NS_LITERAL_STRING("margin"))
IMPL_CSSPROP(MarginTop, NS_LITERAL_STRING("margin-top"))
IMPL_CSSPROP(MarginRight, NS_LITERAL_STRING("margin-right"))
IMPL_CSSPROP(MarginBottom, NS_LITERAL_STRING("margin-bottom"))
IMPL_CSSPROP(MarginLeft, NS_LITERAL_STRING("margin-left"))
IMPL_CSSPROP(MarkerOffset, NS_LITERAL_STRING("marker-offset"))
IMPL_CSSPROP(Marks, NS_LITERAL_STRING("marks"))
IMPL_CSSPROP(MaxHeight, NS_LITERAL_STRING("max-height"))
IMPL_CSSPROP(MaxWidth, NS_LITERAL_STRING("max-width"))
IMPL_CSSPROP(MinHeight, NS_LITERAL_STRING("min-height"))
IMPL_CSSPROP(MinWidth, NS_LITERAL_STRING("min-width"))
IMPL_CSSPROP(Orphans, NS_LITERAL_STRING("orphans"))
  // XXX Because of the renaming to -moz-outline, these 4 do nothing:
IMPL_CSSPROP(Outline, NS_LITERAL_STRING("outline"))
IMPL_CSSPROP(OutlineColor, NS_LITERAL_STRING("outline-color"))
IMPL_CSSPROP(OutlineStyle, NS_LITERAL_STRING("outline-style"))
IMPL_CSSPROP(OutlineWidth, NS_LITERAL_STRING("outline-width"))
IMPL_CSSPROP(Overflow, NS_LITERAL_STRING("overflow"))
IMPL_CSSPROP(Padding, NS_LITERAL_STRING("padding"))
IMPL_CSSPROP(PaddingTop, NS_LITERAL_STRING("padding-top"))
IMPL_CSSPROP(PaddingRight, NS_LITERAL_STRING("padding-right"))
IMPL_CSSPROP(PaddingBottom, NS_LITERAL_STRING("padding-bottom"))
IMPL_CSSPROP(PaddingLeft, NS_LITERAL_STRING("padding-left"))
IMPL_CSSPROP(Page, NS_LITERAL_STRING("page"))
IMPL_CSSPROP(PageBreakAfter, NS_LITERAL_STRING("page-break-after"))
IMPL_CSSPROP(PageBreakBefore, NS_LITERAL_STRING("page-break-before"))
IMPL_CSSPROP(PageBreakInside, NS_LITERAL_STRING("page-break-inside"))
IMPL_CSSPROP(Pause, NS_LITERAL_STRING("pause"))
IMPL_CSSPROP(PauseAfter, NS_LITERAL_STRING("pause-after"))
IMPL_CSSPROP(PauseBefore, NS_LITERAL_STRING("pause-before"))
IMPL_CSSPROP(Pitch, NS_LITERAL_STRING("pitch"))
IMPL_CSSPROP(PitchRange, NS_LITERAL_STRING("pitch-range"))
IMPL_CSSPROP(PlayDuring, NS_LITERAL_STRING("play-during"))
IMPL_CSSPROP(Position, NS_LITERAL_STRING("position"))
IMPL_CSSPROP(Quotes, NS_LITERAL_STRING("quotes"))
IMPL_CSSPROP(Richness, NS_LITERAL_STRING("richness"))
IMPL_CSSPROP(Right, NS_LITERAL_STRING("right"))
IMPL_CSSPROP(Size, NS_LITERAL_STRING("size"))
IMPL_CSSPROP(Speak, NS_LITERAL_STRING("speak"))
IMPL_CSSPROP(SpeakHeader, NS_LITERAL_STRING("speak-header"))
IMPL_CSSPROP(SpeakNumeral, NS_LITERAL_STRING("speak-numeral"))
IMPL_CSSPROP(SpeakPunctuation, NS_LITERAL_STRING("speak-punctuation"))
IMPL_CSSPROP(SpeechRate, NS_LITERAL_STRING("speech-rate"))
IMPL_CSSPROP(Stress, NS_LITERAL_STRING("stress"))
IMPL_CSSPROP(TableLayout, NS_LITERAL_STRING("table-layout"))
IMPL_CSSPROP(TextAlign, NS_LITERAL_STRING("text-align"))
IMPL_CSSPROP(TextDecoration, NS_LITERAL_STRING("text-decoration"))
IMPL_CSSPROP(TextIndent, NS_LITERAL_STRING("text-indent"))
IMPL_CSSPROP(TextShadow, NS_LITERAL_STRING("text-shadow"))
IMPL_CSSPROP(TextTransform, NS_LITERAL_STRING("text-transform"))
IMPL_CSSPROP(Top, NS_LITERAL_STRING("top"))
IMPL_CSSPROP(UnicodeBidi, NS_LITERAL_STRING("unicode-bidi"))
IMPL_CSSPROP(VerticalAlign, NS_LITERAL_STRING("vertical-align"))
IMPL_CSSPROP(Visibility, NS_LITERAL_STRING("visibility"))
IMPL_CSSPROP(VoiceFamily, NS_LITERAL_STRING("voice-family"))
IMPL_CSSPROP(Volume, NS_LITERAL_STRING("volume"))
IMPL_CSSPROP(WhiteSpace, NS_LITERAL_STRING("white-space"))
IMPL_CSSPROP(Widows, NS_LITERAL_STRING("widows"))
IMPL_CSSPROP(Width, NS_LITERAL_STRING("width"))
IMPL_CSSPROP(WordSpacing, NS_LITERAL_STRING("word-spacing"))
IMPL_CSSPROP(ZIndex, NS_LITERAL_STRING("z-index"))

  // nsIDOMNSCSS2Properties
IMPL_CSSPROP(MozBinding, NS_LITERAL_STRING("-moz-binding"))
IMPL_CSSPROP(MozBorderRadius, NS_LITERAL_STRING("-moz-border-radius"))
IMPL_CSSPROP(MozBorderRadiusTopleft, NS_LITERAL_STRING("-moz-border-radius-topleft"))
IMPL_CSSPROP(MozBorderRadiusTopright, NS_LITERAL_STRING("-moz-border-radius-topright"))
IMPL_CSSPROP(MozBorderRadiusBottomleft, NS_LITERAL_STRING("-moz-border-radius-bottomleft"))
IMPL_CSSPROP(MozBorderRadiusBottomright, NS_LITERAL_STRING("-moz-border-radius-bottomright"))
IMPL_CSSPROP(MozBoxAlign, NS_LITERAL_STRING("-moz-box-align"))
IMPL_CSSPROP(MozBoxDirection, NS_LITERAL_STRING("-moz-box-direction"))
IMPL_CSSPROP(MozBoxFlex, NS_LITERAL_STRING("-moz-box-flex"))
IMPL_CSSPROP(MozBoxFlexGroup, NS_LITERAL_STRING("-moz-box-flex-group"))
IMPL_CSSPROP(MozBoxOrient, NS_LITERAL_STRING("-moz-box-orient"))
IMPL_CSSPROP(MozBoxPack, NS_LITERAL_STRING("-moz-box-pack"))
IMPL_CSSPROP(MozBoxSizing, NS_LITERAL_STRING("-moz-box-sizing"))
IMPL_CSSPROP(MozFloatEdge, NS_LITERAL_STRING("-moz-float-edge"))
IMPL_CSSPROP(MozKeyEquivalent, NS_LITERAL_STRING("-moz-key-equivalent"))
IMPL_CSSPROP(MozOpacity, NS_LITERAL_STRING("-moz-opacity"))
IMPL_CSSPROP(MozOutline, NS_LITERAL_STRING("-moz-outline"))
IMPL_CSSPROP(MozOutlineColor, NS_LITERAL_STRING("-moz-outline-color"))
IMPL_CSSPROP(MozOutlineRadius, NS_LITERAL_STRING("-moz-outline-radius"))
IMPL_CSSPROP(MozOutlineRadiusTopleft, NS_LITERAL_STRING("-moz-outline-radius-topleft"))
IMPL_CSSPROP(MozOutlineRadiusTopright, NS_LITERAL_STRING("-moz-outline-radius-topright"))
IMPL_CSSPROP(MozOutlineRadiusBottomleft, NS_LITERAL_STRING("-moz-outline-radius-bottomleft"))
IMPL_CSSPROP(MozOutlineRadiusBottomright, NS_LITERAL_STRING("-moz-outline-radius-bottomright"))
IMPL_CSSPROP(MozOutlineStyle, NS_LITERAL_STRING("-moz-outline-style"))
IMPL_CSSPROP(MozOutlineWidth, NS_LITERAL_STRING("-moz-outline-width"))
IMPL_CSSPROP(MozResizer, NS_LITERAL_STRING("-moz-resizer"))
IMPL_CSSPROP(MozUserFocus, NS_LITERAL_STRING("-moz-user-focus"))
IMPL_CSSPROP(MozUserInput, NS_LITERAL_STRING("-moz-user-input"))
IMPL_CSSPROP(MozUserModify, NS_LITERAL_STRING("-moz-user-modify"))
IMPL_CSSPROP(MozUserSelect, NS_LITERAL_STRING("-moz-user-select"))
