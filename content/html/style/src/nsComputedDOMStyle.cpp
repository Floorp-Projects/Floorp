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
 *   Daniel Glazman <glazman@netscape.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *   Christopher A. Aillon <christopher@aillon.com>
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

#include "nsComputedDOMStyle.h"

#include "nsDOMError.h"
#include "nsIDOMCSS2Properties.h"
#include "nsIDOMElement.h"
#include "nsIStyleContext.h"
#include "nsIScrollableFrame.h"
#include "nsContentUtils.h"
#include "prprf.h"

#include "nsCSSProps.h"
#include "nsCSSKeywords.h"
#include "nsDOMCSSRect.h"
#include "nsLayoutAtoms.h"
#include "nsThemeConstants.h"

#include "nsIPresContext.h"
#include "nsIDocument.h"

#if defined(DEBUG_bzbarsky) || defined(DEBUG_caillon)
#define DEBUG_ComputedDOMStyle
#endif

/*
 * This is the implementation of the readonly CSSStyleDeclaration that is
 * returned by the getComputedStyle() function.
 *
 * This file is very much a work in progress.  There's a lot of code in this
 * file that is #ifdef'd out placeholders.
 */

static const nsCSSProperty queryableProperties[] = {
  /* ******************************************************************* *\
   * Properties below are listed in alphabetical order.                  *
   * Please keep them that way.                                          *
   *                                                                     *
   * Properties commented out with // are not yet implemented            *
   * Properties commented out with //// are shorthands and not queryable *
  \* ******************************************************************* */

  /* ****************************** *\
   * Implementations of CSS2 styles *
  \* ****************************** */

  // eCSSProperty_azimuth,

  //// eCSSProperty_background,
  eCSSProperty_background_attachment,
  eCSSProperty_background_color,
  eCSSProperty_background_image,
  //// eCSSProperty_background_position,
  eCSSProperty_background_repeat,
  //// eCSSProperty_border,
  //// eCSSProperty_border_bottom,
  eCSSProperty_border_bottom_color,
  eCSSProperty_border_bottom_style,
  eCSSProperty_border_bottom_width,
  eCSSProperty_border_collapse,
  //// eCSSProperty_border_color,
  //// eCSSProperty_border_left,
  eCSSProperty_border_left_color,
  eCSSProperty_border_left_style,
  eCSSProperty_border_left_width,
  //// eCSSProperty_border_right,
  eCSSProperty_border_right_color,
  eCSSProperty_border_right_style,
  eCSSProperty_border_right_width,
  //// eCSSProperty_border_spacing,
  //// eCSSProperty_border_style,
  //// eCSSProperty_border_top,
  eCSSProperty_border_top_color,
  eCSSProperty_border_top_style,
  eCSSProperty_border_top_width,
  //// eCSSProperty_border_width,
  eCSSProperty_bottom,

  eCSSProperty_caption_side,
  eCSSProperty_clear,
  eCSSProperty_clip,
  eCSSProperty_color,
  // eCSSProperty_content,
  // eCSSProperty_counter_increment,
  // eCSSProperty_counter_reset,
  //// eCSSProperty_cue,
  // eCSSProperty_cue_after,
  // eCSSProperty_cue_before,
  eCSSProperty_cursor,

  eCSSProperty_direction,
  eCSSProperty_display,

  // eCSSProperty_elevation,
  eCSSProperty_empty_cells,

  eCSSProperty_float,
  //// eCSSProperty_font,
  eCSSProperty_font_family,
  eCSSProperty_font_size,
  eCSSProperty_font_size_adjust,
  // eCSSProperty_font_stretch,
  eCSSProperty_font_style,
  eCSSProperty_font_variant,
  eCSSProperty_font_weight,

  eCSSProperty_height,

  eCSSProperty_left,
  eCSSProperty_letter_spacing,
  eCSSProperty_line_height,
  //// eCSSProperty_list_style,
  eCSSProperty_list_style_image,
  eCSSProperty_list_style_position,
  eCSSProperty_list_style_type,

  //// eCSSProperty_margin,
  eCSSProperty_margin_bottom,
  eCSSProperty_margin_left,
  eCSSProperty_margin_right,
  eCSSProperty_margin_top,
  eCSSProperty_marker_offset,
  // eCSSProperty_marks,
  eCSSProperty_max_height,
  eCSSProperty_max_width,
  eCSSProperty_min_height,
  eCSSProperty_min_width,

  // eCSSProperty_orphans,
  //// eCSSProperty_outline,
  // eCSSProperty_outline_color,
  // eCSSProperty_outline_style,
  // eCSSProperty_outline_width,
  eCSSProperty_overflow,

  //// eCSSProperty_padding,
  eCSSProperty_padding_bottom,
  eCSSProperty_padding_left,
  eCSSProperty_padding_right,
  eCSSProperty_padding_top,
  // eCSSProperty_page,
  // eCSSProperty_page_break_after,
  // eCSSProperty_page_break_before,
  // eCSSProperty_page_break_inside,
  //// eCSSProperty_pause,
  // eCSSProperty_pause_after,
  // eCSSProperty_pause_before,
  // eCSSProperty_pitch,
  // eCSSProperty_pitch_range,
  //// eCSSProperty_play_during,
  eCSSProperty_position,

  // eCSSProperty_quotes,

  // eCSSProperty_richness,
  eCSSProperty_right,

  //// eCSSProperty_size,
  // eCSSProperty_speak,
  // eCSSProperty_speak_header,
  // eCSSProperty_speak_numeral,
  // eCSSProperty_speak_punctuation,
  // eCSSProperty_speech_rate,
  // eCSSProperty_stress,

  eCSSProperty_table_layout,
  eCSSProperty_text_align,
  eCSSProperty_text_decoration,
  eCSSProperty_text_indent,
  // eCSSProperty_text_shadow,
  eCSSProperty_text_transform,
  eCSSProperty_top,

  eCSSProperty_unicode_bidi,

  eCSSProperty_vertical_align,
  eCSSProperty_visibility,
  // eCSSProperty_voice_family,
  // eCSSProperty_volume,

  eCSSProperty_white_space,
  // eCSSProperty_widows,
  eCSSProperty_width,
  eCSSProperty_word_spacing,

  eCSSProperty_z_index,

  /* ******************************* *\
   * Implementations of -moz- styles *
  \* ******************************* */

  eCSSProperty_appearance,
  eCSSProperty_binding,

  eCSSProperty_border_bottom_colors,
  eCSSProperty_border_left_colors,
  eCSSProperty_border_right_colors,
  eCSSProperty_border_top_colors,

  eCSSProperty__moz_border_radius_bottomLeft,
  eCSSProperty__moz_border_radius_bottomRight,
  eCSSProperty__moz_border_radius_topLeft,
  eCSSProperty__moz_border_radius_topRight,

#ifdef INCLUDE_XUL
  eCSSProperty_box_align,
  eCSSProperty_box_direction,
  eCSSProperty_box_flex,
  eCSSProperty_box_ordinal_group,
  eCSSProperty_box_orient,
  eCSSProperty_box_pack,
#endif
  eCSSProperty_box_sizing,

  eCSSProperty_float_edge,

  eCSSProperty_opacity,
  //// eCSSProperty__moz_outline,
  eCSSProperty__moz_outline_color,
  eCSSProperty__moz_outline_style,
  eCSSProperty__moz_outline_width,

  eCSSProperty_user_focus,
  eCSSProperty_user_input,
  eCSSProperty_user_modify,
  eCSSProperty_user_select

};

static nsComputedDOMStyle *sCachedComputedDOMStyle;

nsresult
NS_NewComputedDOMStyle(nsIComputedDOMStyle** aComputedStyle)
{
  NS_ENSURE_ARG_POINTER(aComputedStyle);

  if (sCachedComputedDOMStyle) {
    // There's an unused nsComputedDOMStyle cached, use it.
    // But before we use it, re-initialize the object.

    // Oh yeah baby, placement new!
    *aComputedStyle = new (sCachedComputedDOMStyle) nsComputedDOMStyle();

    sCachedComputedDOMStyle = nsnull;
  } else {
    // No nsComputedDOMStyle cached, create a new one.

    *aComputedStyle = new nsComputedDOMStyle();
    NS_ENSURE_TRUE(*aComputedStyle, NS_ERROR_OUT_OF_MEMORY);
  }

  NS_ADDREF(*aComputedStyle);

  return NS_OK;
}

nsComputedDOMStyle::nsComputedDOMStyle()
  : mInner(nsnull), mPresShellWeak(nsnull), mT2P(0.0f)
{
  NS_INIT_ISUPPORTS();
}


nsComputedDOMStyle::~nsComputedDOMStyle()
{
}

void
nsComputedDOMStyle::Shutdown()
{
  // We want to de-allocate without calling the dtor since we
  // already did that manually in doDestroyComputedDOMStyle(),
  // so cast our cached object to something that doesn't know
  // about our dtor.
  delete NS_REINTERPRET_CAST(char*, sCachedComputedDOMStyle);
  sCachedComputedDOMStyle = nsnull;
}


NS_IMETHODIMP
nsComputedDOMStyle::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (!aInstancePtr) {
    NS_ERROR("QueryInterface requires a non-NULL destination!");
    return NS_ERROR_NULL_POINTER;
  }

  nsISupports* inst;

  if (aIID.Equals(NS_GET_IID(nsIComputedDOMStyle))) {
    inst = NS_STATIC_CAST(nsIComputedDOMStyle*, this);
  } else if (aIID.Equals(NS_GET_IID(nsIDOMCSSStyleDeclaration))) {
    inst = NS_STATIC_CAST(nsIDOMCSSStyleDeclaration*, this);
  } else if (aIID.Equals(NS_GET_IID(nsIDOMCSS2Properties))) {
    if (!mInner) {
      mInner = new CSS2PropertiesTearoff(this);
      NS_ENSURE_TRUE(mInner, NS_ERROR_OUT_OF_MEMORY);
    }
    inst = NS_STATIC_CAST(nsIDOMCSS2Properties*,
                          NS_STATIC_CAST(nsISupports*, mInner));
  } else if (aIID.Equals(NS_GET_IID(nsIDOMNSCSS2Properties))) {
    if (!mInner) {
      mInner = new CSS2PropertiesTearoff(this);
      NS_ENSURE_TRUE(mInner, NS_ERROR_OUT_OF_MEMORY);
    }
    inst = NS_STATIC_CAST(nsIDOMNSCSS2Properties*,
                          NS_STATIC_CAST(nsISupports*, mInner));
  } else if (aIID.Equals(NS_GET_IID(nsISupports))) {
    inst = NS_STATIC_CAST(nsISupports*,
                          NS_STATIC_CAST(nsIComputedDOMStyle*, this));
  } else if (aIID.Equals(NS_GET_IID(nsIClassInfo))) {
    inst = nsContentUtils::GetClassInfoInstance(eDOMClassInfo_ComputedCSSStyleDeclaration_id);
    NS_ENSURE_TRUE(inst, NS_ERROR_OUT_OF_MEMORY);
  } else {
    inst = nsnull;
  }

  nsresult rv;
  if (!inst) {
    rv = NS_NOINTERFACE;
  } else {
    NS_ADDREF(inst);
    rv = NS_OK;
  }

  *aInstancePtr = inst;
  return rv;
}


static void doDestroyComputedDOMStyle(nsComputedDOMStyle *aComputedStyle)
{
  if (!sCachedComputedDOMStyle) {
    // The cache is empty, store aComputedStyle in the cache.

    sCachedComputedDOMStyle = aComputedStyle;
    sCachedComputedDOMStyle->~nsComputedDOMStyle();
  } else {
    // The cache is full, delete aComputedStyle

    delete aComputedStyle;
  }
}

NS_IMPL_ADDREF(nsComputedDOMStyle);
NS_IMPL_RELEASE_WITH_DESTROY(nsComputedDOMStyle,
                             doDestroyComputedDOMStyle(this));


NS_IMETHODIMP
nsComputedDOMStyle::Init(nsIDOMElement *aElement,
                         const nsAString& aPseudoElt,
                         nsIPresShell *aPresShell)
{
  NS_ENSURE_ARG_POINTER(aElement);
  NS_ENSURE_ARG_POINTER(aPresShell);

  mPresShellWeak = getter_AddRefs(NS_GetWeakReference(aPresShell));

  mContent = do_QueryInterface(aElement);
  if (!mContent) {
    // This should not happen, all our elements support nsIContent!

    return NS_ERROR_FAILURE;
  }

  if (!DOMStringIsNull(aPseudoElt) && !aPseudoElt.IsEmpty()) {
    mPseudo = dont_AddRef(NS_NewAtom(aPseudoElt));
    NS_ENSURE_TRUE(mPseudo, NS_ERROR_OUT_OF_MEMORY);
  }

  nsCOMPtr<nsIPresContext> presCtx;

  aPresShell->GetPresContext(getter_AddRefs(presCtx));

  NS_ENSURE_TRUE(presCtx, NS_ERROR_FAILURE);

  presCtx->GetTwipsToPixels(&mT2P);

  return NS_OK;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetCssText(nsAString& aCssText)
{
  aCssText.Truncate();

  return NS_OK;
}


NS_IMETHODIMP
nsComputedDOMStyle::SetCssText(const nsAString& aCssText)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}


NS_IMETHODIMP
nsComputedDOMStyle::GetLength(PRUint32* aLength)
{
  NS_ENSURE_ARG_POINTER(aLength);
  *aLength = sizeof(queryableProperties) / sizeof(nsCSSProperty);

  return NS_OK;
}


NS_IMETHODIMP
nsComputedDOMStyle::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  NS_ENSURE_ARG_POINTER(aParentRule);
  *aParentRule = nsnull;

  return NS_OK;
}


NS_IMETHODIMP
nsComputedDOMStyle::GetPropertyValue(const nsAString& aPropertyName,
                                     nsAString& aReturn)
{
  nsCOMPtr<nsIDOMCSSValue> val;

  aReturn.Truncate();

  nsresult rv = GetPropertyCSSValue(aPropertyName, getter_AddRefs(val));
  NS_ENSURE_SUCCESS(rv, rv);

  if (val) {
    rv = val->GetCssText(aReturn);
  }

  return rv;
}


NS_IMETHODIMP
nsComputedDOMStyle::GetPropertyCSSValue(const nsAString& aPropertyName,
                                        nsIDOMCSSValue** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShellWeak);

  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  nsIFrame *frame = nsnull;
  presShell->GetPrimaryFrameFor(mContent, &frame);

  nsresult rv = NS_OK;

  // XXX FIX THIS!!!
  nsCSSProperty prop = nsCSSProps::LookupProperty(aPropertyName);

  switch (prop) {
    case eCSSProperty_appearance:
      rv = GetAppearance(frame, aReturn); break;
    case eCSSProperty_binding :
      rv = GetBinding(frame, aReturn); break;

#if INCLUDE_XUL
    case eCSSProperty_box_align:
      rv = GetBoxAlign(frame, aReturn); break;
    case eCSSProperty_box_direction:
      rv = GetBoxDirection(frame, aReturn); break;
    case eCSSProperty_box_flex:
      rv = GetBoxFlex(frame, aReturn); break;
    case eCSSProperty_box_ordinal_group:
      rv = GetBoxOrdinalGroup(frame, aReturn); break;
    case eCSSProperty_box_orient:
      rv = GetBoxOrient(frame, aReturn); break;
    case eCSSProperty_box_pack:
      rv = GetBoxPack(frame, aReturn); break;
#endif // INCLUDE_XUL
    case eCSSProperty_box_sizing:
      rv = GetBoxSizing(frame, aReturn); break;

    case eCSSProperty_display :
      rv = GetDisplay(frame, aReturn); break;
    case eCSSProperty_position :
      rv = GetPosition(frame, aReturn); break;
    case eCSSProperty_clear:
      rv = GetClear(frame, aReturn); break;
    case eCSSProperty_float :
      rv = GetCssFloat(frame, aReturn); break;
    case eCSSProperty_float_edge:
      rv = GetFloatEdge(frame, aReturn); break;
    case eCSSProperty_width :
      rv = GetWidth(frame, aReturn); break;
    case eCSSProperty_height :
      rv = GetHeight(frame, aReturn); break;
    case eCSSProperty_max_height :
      rv = GetMaxHeight(frame, aReturn); break;
    case eCSSProperty_max_width :
      rv = GetMaxWidth(frame, aReturn); break;
    case eCSSProperty_min_height :
      rv = GetMinHeight(frame, aReturn); break;
    case eCSSProperty_min_width :
      rv = GetMinWidth(frame, aReturn); break;
    case eCSSProperty_left :
      rv = GetLeft(frame, aReturn); break;
    case eCSSProperty_top :
      rv = GetTop(frame, aReturn); break;
    case eCSSProperty_right :
      rv = GetRight(frame, aReturn); break;
    case eCSSProperty_bottom :
      rv = GetBottom(frame, aReturn); break;

      // Font properties
    case eCSSProperty_color :
      rv = GetColor(frame, aReturn); break;
    case eCSSProperty_font_family :
      rv = GetFontFamily(frame, aReturn); break;
    case eCSSProperty_font_size :
      rv = GetFontSize(frame, aReturn); break;
    case eCSSProperty_font_size_adjust:
      rv = GetFontSizeAdjust(frame, aReturn); break;
    case eCSSProperty_font_style :
      rv = GetFontStyle(frame, aReturn); break;
    case eCSSProperty_font_weight :
      rv = GetFontWeight(frame, aReturn); break;
    case eCSSProperty_font_variant :
      rv = GetFontVariant(frame, aReturn); break;

      // Background properties
    case eCSSProperty_background_attachment:
      rv = GetBackgroundAttachment(frame, aReturn); break;
    case eCSSProperty_background_color :
      rv = GetBackgroundColor(frame, aReturn); break;
    case eCSSProperty_background_image :
      rv = GetBackgroundImage(frame, aReturn); break;
    case eCSSProperty_background_repeat:
      rv = GetBackgroundRepeat(frame, aReturn); break;

      // Padding properties
    case eCSSProperty_padding :
      rv = GetPadding(frame, aReturn); break;
    case eCSSProperty_padding_top :
      rv = GetPaddingTop(frame, aReturn); break;
    case eCSSProperty_padding_bottom :
      rv = GetPaddingBottom(frame, aReturn); break;
    case eCSSProperty_padding_left :
      rv = GetPaddingLeft(frame, aReturn); break;
    case eCSSProperty_padding_right :
      rv = GetPaddingRight(frame, aReturn); break;

      // Table properties
    case eCSSProperty_border_collapse:
      rv = GetBorderCollapse(frame, aReturn); break;
    case eCSSProperty_caption_side:
      rv = GetCaptionSide(frame, aReturn); break;
    case eCSSProperty_empty_cells:
      rv = GetEmptyCells(frame, aReturn); break;
    case eCSSProperty_table_layout:
      rv = GetTableLayout(frame, aReturn); break;
    case eCSSProperty_vertical_align:
      rv = GetVerticalAlign(frame, aReturn); break;

      // Border properties
    case eCSSProperty_border_top_style :
      rv = GetBorderTopStyle(frame, aReturn); break;
    case eCSSProperty_border_bottom_style :
      rv = GetBorderBottomStyle(frame, aReturn); break;
    case eCSSProperty_border_left_style :
      rv = GetBorderLeftStyle(frame, aReturn); break;
    case eCSSProperty_border_right_style :
      rv = GetBorderRightStyle(frame, aReturn); break;
    case eCSSProperty_border_top_width :
      rv = GetBorderTopWidth(frame, aReturn); break;
    case eCSSProperty_border_bottom_width :
      rv = GetBorderBottomWidth(frame, aReturn); break;
    case eCSSProperty_border_left_width :
      rv = GetBorderLeftWidth(frame, aReturn); break;
    case eCSSProperty_border_right_width :
      rv = GetBorderRightWidth(frame, aReturn); break;
    case eCSSProperty_border_top_color :
      rv = GetBorderTopColor(frame, aReturn); break;
    case eCSSProperty_border_bottom_color :
      rv = GetBorderBottomColor(frame, aReturn); break;
    case eCSSProperty_border_left_color :
      rv = GetBorderLeftColor(frame, aReturn); break;
    case eCSSProperty_border_right_color :
      rv = GetBorderRightColor(frame, aReturn); break;
    case eCSSProperty_border_bottom_colors:
      rv = GetBorderBottomColors(frame, aReturn); break;
    case eCSSProperty_border_left_colors:
      rv = GetBorderLeftColors(frame, aReturn); break;
    case eCSSProperty_border_right_colors:
      rv = GetBorderRightColors(frame, aReturn); break;
    case eCSSProperty_border_top_colors:
      rv = GetBorderTopColors(frame, aReturn); break;
    case eCSSProperty__moz_border_radius_bottomLeft:
      rv = GetBorderRadiusBottomLeft(frame, aReturn); break;
    case eCSSProperty__moz_border_radius_bottomRight:
      rv = GetBorderRadiusBottomRight(frame, aReturn); break;
    case eCSSProperty__moz_border_radius_topLeft:
      rv = GetBorderRadiusTopLeft(frame, aReturn); break;
    case eCSSProperty__moz_border_radius_topRight:
      rv = GetBorderRadiusTopRight(frame, aReturn); break;

      // Margin properties
    case eCSSProperty_margin_top :
      rv = GetMarginTopWidth(frame, aReturn); break;
    case eCSSProperty_margin_bottom :
      rv = GetMarginBottomWidth(frame, aReturn); break;
    case eCSSProperty_margin_left :
      rv = GetMarginLeftWidth(frame, aReturn); break;
    case eCSSProperty_margin_right :
      rv = GetMarginRightWidth(frame, aReturn); break;

      // Marker property
    case eCSSProperty_marker_offset :
      rv = GetMarkerOffset(frame, aReturn); break;

      // Outline properties
    case eCSSProperty__moz_outline_width :
      rv = GetOutlineWidth(frame, aReturn); break;
    case eCSSProperty__moz_outline_style :
      rv = GetOutlineStyle(frame, aReturn); break;
    case eCSSProperty__moz_outline_color:
      rv = GetOutlineColor(frame, aReturn); break;

      // Text properties
    case eCSSProperty_line_height:
      rv = GetLineHeight(frame, aReturn); break;
    case eCSSProperty_text_align:
      rv = GetTextAlign(frame, aReturn); break;
    case eCSSProperty_text_decoration:
      rv = GetTextDecoration(frame, aReturn); break;
    case eCSSProperty_text_indent:
      rv = GetTextIndent(frame, aReturn); break;
    case eCSSProperty_text_transform:
      rv = GetTextTransform(frame, aReturn); break;
    case eCSSProperty_letter_spacing:
      rv = GetLetterSpacing(frame, aReturn); break;
    case eCSSProperty_word_spacing:
      rv = GetWordSpacing(frame, aReturn); break;
    case eCSSProperty_white_space:
      rv = GetWhiteSpace(frame, aReturn); break;
      
      // List properties
    case eCSSProperty_list_style_image:
      rv = GetListStyleImage(frame, aReturn); break; 
    case eCSSProperty_list_style_position:
      rv = GetListStylePosition(frame, aReturn); break; 
    case eCSSProperty_list_style_type:
      rv = GetListStyleType(frame, aReturn); break; 

      // Display properties
    case eCSSProperty_visibility:
      rv = GetVisibility(frame, aReturn); break;
    case eCSSProperty_opacity:
      rv = GetOpacity(frame, aReturn); break;

      // Z-Index property
    case eCSSProperty_z_index:
      rv = GetZIndex(frame, aReturn); break;

      // Clip
    case eCSSProperty_clip:
      rv = GetClip(frame, aReturn); break;
    case eCSSProperty_overflow:
      rv = GetOverflow(frame, aReturn); break;

      // Direction
    case eCSSProperty_direction:
      rv = GetDirection(frame, aReturn); break;
    case eCSSProperty_unicode_bidi:
      rv = GetUnicodeBidi(frame, aReturn); break;

      // User interface
    case eCSSProperty_cursor:
      rv = GetCursor(frame, aReturn); break;
    case eCSSProperty_user_focus:
      rv = GetUserFocus(frame, aReturn); break;
    case eCSSProperty_user_input:
      rv = GetUserInput(frame, aReturn); break;
    case eCSSProperty_user_modify:
      rv = GetUserModify(frame, aReturn); break;
    case eCSSProperty_user_select:
      rv = GetUserSelect(frame, aReturn); break;

    default :
#ifdef DEBUG_ComputedDOMStyle
      NS_ERROR("Someone is trying to get computed style of a property we "
               "don't handle!");
#endif
      break;
  }

  if (NS_FAILED(rv)) {
    *aReturn = nsnull;
  }

  // Release the current style context for it should be re-resolved
  // whenever a frame is not available.
  mStyleContextHolder = nsnull;

  return rv;
}


NS_IMETHODIMP
nsComputedDOMStyle::RemoveProperty(const nsAString& aPropertyName,
                                   nsAString& aReturn)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}


NS_IMETHODIMP
nsComputedDOMStyle::GetPropertyPriority(const nsAString& aPropertyName,
                                        nsAString& aReturn)
{
  aReturn.Truncate();

  return NS_OK;
}


NS_IMETHODIMP
nsComputedDOMStyle::SetProperty(const nsAString& aPropertyName,
                                const nsAString& aValue,
                                const nsAString& aPriority)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}


NS_IMETHODIMP
nsComputedDOMStyle::Item(PRUint32 aIndex, nsAString& aReturn)
{
  PRUint32 length;
  GetLength(&length);
  aReturn.Truncate();
  if (aIndex < length) {
    CopyASCIItoUCS2(nsCSSProps::GetStringValue(queryableProperties[aIndex]),
                    aReturn);
  }

  return NS_OK;
}


// Property getters...

nsresult
nsComputedDOMStyle::GetBinding(nsIFrame *aFrame,
                               nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay* display = nsnull;

  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display, aFrame);

  if (display && !display->mBinding.IsEmpty()) {
    val->SetURI(display->mBinding);
  } else {
    val->SetIdent(NS_LITERAL_STRING("none"));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetClear(nsIFrame *aFrame,
                             nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay *display = nsnull;
  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display, aFrame);

  if (display && display->mBreakType != NS_STYLE_CLEAR_NONE) {
    const nsAFlatCString& clear =
      nsCSSProps::SearchKeywordTable(display->mBreakType,
                                     nsCSSProps::kClearKTable);
    val->SetIdent(clear);
  } else {
    val->SetIdent(NS_LITERAL_STRING("none"));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetCssFloat(nsIFrame *aFrame,
                                nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay* display = nsnull;
  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display, aFrame);

  if (display && display->mFloats != NS_STYLE_FLOAT_NONE) {
    const nsAFlatCString& cssFloat =
      nsCSSProps::SearchKeywordTable(display->mFloats,
                                     nsCSSProps::kFloatKTable);
    val->SetIdent(cssFloat);
  } else {
    val->SetIdent(NS_LITERAL_STRING("none"));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBottom(nsIFrame *aFrame,
                              nsIDOMCSSValue** aValue)
{
  return GetOffsetWidthFor(NS_SIDE_BOTTOM, aFrame, aValue);
}

nsDOMCSSRGBColor*
nsComputedDOMStyle::GetDOMCSSRGBColor(nscolor aColor)
{
  nsROCSSPrimitiveValue *red   = GetROCSSPrimitiveValue();
  nsROCSSPrimitiveValue *green = GetROCSSPrimitiveValue();
  nsROCSSPrimitiveValue *blue  = GetROCSSPrimitiveValue();

  if (red && green && blue) {
    nsDOMCSSRGBColor *rgbColor = new nsDOMCSSRGBColor(red, green, blue);

    if (rgbColor) {
      red->SetNumber(NS_GET_R(aColor));
      green->SetNumber(NS_GET_G(aColor));
      blue->SetNumber(NS_GET_B(aColor));

      return rgbColor;
    }
  }

  delete red;
  delete green;
  delete blue;

  return nsnull;
}

nsresult
nsComputedDOMStyle::GetColor(nsIFrame *aFrame,
                             nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleColor* color = nsnull;
  GetStyleData(eStyleStruct_Color, (const nsStyleStruct*&)color, aFrame);

  nsDOMCSSRGBColor *rgb = nsnull;

  if (color) {
    rgb = GetDOMCSSRGBColor(color->mColor);
    if (!rgb) {
      delete val;

      return NS_ERROR_OUT_OF_MEMORY;
    }

    val->SetColor(rgb);
  } else {
    val->SetString(NS_LITERAL_STRING(""));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetOpacity(nsIFrame *aFrame,
                               nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleVisibility *visibility = nsnull;
  GetStyleData(eStyleStruct_Visibility, (const nsStyleStruct*&)visibility,
               aFrame);

  if (visibility) {
    val->SetNumber(visibility->mOpacity);
  } else {
    val->SetNumber(1.0f);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetFontFamily(nsIFrame *aFrame,
                                  nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleFont* font = nsnull;
  GetStyleData(eStyleStruct_Font, (const nsStyleStruct*&)font, aFrame);

  if (font) {
    nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShellWeak);
    NS_ASSERTION(presShell, "pres shell is required");
    nsCOMPtr<nsIPresContext> presContext;
    presShell->GetPresContext(getter_AddRefs(presContext));
    NS_ASSERTION(presContext, "pres context is required");

    const nsString& fontName = font->mFont.name;
    PRUint8 generic = font->mFlags & NS_STYLE_FONT_FACE_MASK;
    if (generic == kGenericFont_NONE) { 
      const nsFont* defaultFont; 
      presContext->GetDefaultFont(kPresContext_DefaultVariableFont_ID,
                                  &defaultFont);
      PRInt32 lendiff = fontName.Length() - defaultFont->name.Length();
      if (lendiff > 0) {
        val->SetString(Substring(fontName, 0, lendiff-1)); // -1 removes comma
      } else {
        val->SetString(fontName);
      }
    } else {
      val->SetString(fontName);
    }
  } else {
    val->SetString(NS_LITERAL_STRING(""));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetFontSize(nsIFrame *aFrame,
                                nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleFont* font = nsnull;
  GetStyleData(eStyleStruct_Font, (const nsStyleStruct*&)font, aFrame);

  // Note: font->mSize is the 'computed size'; font->mFont.size is the
  // 'actual size'
  val->SetTwips(font? font->mSize:0);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetFontSizeAdjust(nsIFrame *aFrame,
                                      nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleFont *font = nsnull;
  GetStyleData(eStyleStruct_Font, (const nsStyleStruct*&)font, aFrame);

  if (font && font->mFont.sizeAdjust) {
    val->SetNumber(font->mFont.sizeAdjust);
  } else {
    val->SetIdent(NS_LITERAL_STRING("none"));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetFontStyle(nsIFrame *aFrame,
                                 nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleFont* font = nsnull;
  GetStyleData(eStyleStruct_Font, (const nsStyleStruct*&)font, aFrame);

  if (font && font->mFont.style != NS_STYLE_FONT_STYLE_NORMAL) {
    const nsAFlatCString& style=
      nsCSSProps::SearchKeywordTable(font->mFont.style,
                                     nsCSSProps::kFontStyleKTable);
    val->SetIdent(style);
  } else {
    val->SetIdent(NS_LITERAL_STRING("normal"));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetFontWeight(nsIFrame *aFrame,
                                  nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleFont* font = nsnull;
  GetStyleData(eStyleStruct_Font, (const nsStyleStruct*&)font, aFrame);

  if (font) {
    const nsAFlatCString& str_weight=
      nsCSSProps::SearchKeywordTable(font->mFont.weight,
                                     nsCSSProps::kFontWeightKTable);
    if (!str_weight.IsEmpty()) {
      val->SetIdent(str_weight);
    } else {
      val->SetNumber(font->mFont.weight);
    }
  } else {
    val->SetIdent(NS_LITERAL_STRING("normal"));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetFontVariant(nsIFrame *aFrame,
                                   nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleFont* font = nsnull;
  GetStyleData(eStyleStruct_Font, (const nsStyleStruct*&)font, aFrame);

  if (font && font->mFont.variant != NS_STYLE_FONT_VARIANT_NORMAL) {
    const nsAFlatCString& variant=
      nsCSSProps::SearchKeywordTable(font->mFont.variant,
                                     nsCSSProps::kFontVariantKTable);
    val->SetIdent(variant);
  } else {
    val->SetIdent(NS_LITERAL_STRING("normal"));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBackgroundAttachment(nsIFrame *aFrame,
                                            nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleBackground *background = nsnull;
  GetStyleData(eStyleStruct_Background, (const nsStyleStruct*&)background,
               aFrame);

  if (background) {
    const nsAFlatCString& backgroundAttachment =
      nsCSSProps::SearchKeywordTable(background->mBackgroundAttachment,
                                     nsCSSProps::kBackgroundAttachmentKTable);
    val->SetIdent(backgroundAttachment);
  } else {
    val->SetIdent(NS_LITERAL_STRING("scroll"));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBackgroundColor(nsIFrame *aFrame,
                                       nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleBackground* color = nsnull;
  GetStyleData(eStyleStruct_Background, (const nsStyleStruct*&)color, aFrame);

  if (color) {
    if (color->mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT) {
      const nsAFlatCString& backgroundColor =
        nsCSSProps::SearchKeywordTable(NS_STYLE_BG_COLOR_TRANSPARENT,
                                       nsCSSProps::kBackgroundColorKTable);
      val->SetIdent(backgroundColor);
    } else {
      nsDOMCSSRGBColor *rgb = nsnull;
      rgb = GetDOMCSSRGBColor(color->mBackgroundColor);
      if (!rgb) {
        delete val;

        return NS_ERROR_OUT_OF_MEMORY;
      }

      val->SetColor(rgb);
    }
  } else {
    val->SetString(NS_LITERAL_STRING(""));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBackgroundImage(nsIFrame *aFrame,
                                       nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleBackground* color = nsnull;
  GetStyleData(eStyleStruct_Background, (const nsStyleStruct*&)color, aFrame);

  if (color) {
    if (color->mBackgroundFlags & NS_STYLE_BG_IMAGE_NONE) {
      val->SetIdent(NS_LITERAL_STRING("none"));
    } else {
      val->SetURI(color->mBackgroundImage);
    }
  } else {
    val->SetIdent(NS_LITERAL_STRING("none"));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBackgroundRepeat(nsIFrame *aFrame,
                                        nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleBackground *background = nsnull;
  GetStyleData(eStyleStruct_Background, (const nsStyleStruct*&)background,
               aFrame);

  if (background) {
    const nsAFlatCString& backgroundRepeat =
      nsCSSProps::SearchKeywordTable(background->mBackgroundRepeat,
                                     nsCSSProps::kBackgroundRepeatKTable);
    val->SetIdent(backgroundRepeat);
  } else {
    val->SetIdent(NS_LITERAL_STRING("repeat"));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetPadding(nsIFrame *aFrame,
                               nsIDOMCSSValue** aValue)
{
  // return null per spec.
  aValue = nsnull;

  return NS_OK;
}

nsresult
nsComputedDOMStyle::GetPaddingTop(nsIFrame *aFrame,
                                  nsIDOMCSSValue** aValue)
{
  return GetPaddingWidthFor(NS_SIDE_TOP, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetPaddingBottom(nsIFrame *aFrame,
                                     nsIDOMCSSValue** aValue)
{
  return GetPaddingWidthFor(NS_SIDE_BOTTOM, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetPaddingLeft(nsIFrame *aFrame,
                                   nsIDOMCSSValue** aValue)
{
  return GetPaddingWidthFor(NS_SIDE_LEFT, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetPaddingRight(nsIFrame *aFrame,
                                    nsIDOMCSSValue** aValue)
{
  return GetPaddingWidthFor(NS_SIDE_RIGHT, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderCollapse(nsIFrame *aFrame,
                                      nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleTableBorder* table = nsnull;
  GetStyleData(eStyleStruct_TableBorder, (const nsStyleStruct*&)table, aFrame);

  if (table) {
    const nsAFlatCString& ident=
      nsCSSProps::SearchKeywordTable(table->mBorderCollapse,
                                     nsCSSProps::kBorderCollapseKTable);
    val->SetIdent(ident);
  } else {
    val->SetIdent(NS_LITERAL_STRING("collapse"));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderSpacing(nsIFrame *aFrame,
                                     nsIDOMCSSValue** aValue)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

nsresult
nsComputedDOMStyle::GetCaptionSide(nsIFrame *aFrame,
                                   nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleTableBorder *table = nsnull;
  GetStyleData(eStyleStruct_TableBorder, (const nsStyleStruct*&)table, aFrame);

  if (table) {
    const nsAFlatCString& side =
      nsCSSProps::SearchKeywordTable(table->mCaptionSide,
                                     nsCSSProps::kCaptionSideKTable);
    val->SetIdent(side);
  } else {
    val->SetIdent(NS_LITERAL_STRING("top"));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetEmptyCells(nsIFrame *aFrame,
                                  nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleTableBorder *table = nsnull;
  GetStyleData(eStyleStruct_TableBorder, (const nsStyleStruct*&)table, aFrame);

  if (table) {
    const nsAFlatCString& emptyCells =
      nsCSSProps::SearchKeywordTable(table->mEmptyCells,
                                     nsCSSProps::kEmptyCellsKTable);
    val->SetIdent(emptyCells);
  } else {
    val->SetIdent(NS_LITERAL_STRING("show"));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetTableLayout(nsIFrame *aFrame,
                                   nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleTable *table = nsnull;
  GetStyleData(eStyleStruct_Table, (const nsStyleStruct*&)table, aFrame);

  if (table && table->mLayoutStrategy != NS_STYLE_TABLE_LAYOUT_AUTO) {
    const nsAFlatCString& tableLayout =
      nsCSSProps::SearchKeywordTable(table->mLayoutStrategy,
                                     nsCSSProps::kTableLayoutKTable);
    val->SetIdent(tableLayout);
  } else {
    val->SetIdent(NS_LITERAL_STRING("auto"));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderStyle(nsIFrame *aFrame,
                                   nsIDOMCSSValue** aValue)
{
  // return null per spec.
  aValue = nsnull;

  return NS_OK;
}

nsresult
nsComputedDOMStyle::GetBorderTopStyle(nsIFrame *aFrame,
                                      nsIDOMCSSValue** aValue)
{
  return GetBorderStyleFor(NS_SIDE_TOP, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderBottomStyle(nsIFrame *aFrame,
                                         nsIDOMCSSValue** aValue)
{
  return GetBorderStyleFor(NS_SIDE_BOTTOM, aFrame, aValue);
}
nsresult
nsComputedDOMStyle::GetBorderLeftStyle(nsIFrame *aFrame,
                                       nsIDOMCSSValue** aValue)
{
  return GetBorderStyleFor(NS_SIDE_LEFT, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderRightStyle(nsIFrame *aFrame,
                                        nsIDOMCSSValue** aValue)
{
  return GetBorderStyleFor(NS_SIDE_RIGHT, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderBottomColors(nsIFrame *aFrame,
                                          nsIDOMCSSValue** aValue)
{
  return GetBorderColorsFor(NS_SIDE_BOTTOM, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderLeftColors(nsIFrame *aFrame,
                                        nsIDOMCSSValue** aValue)
{
  return GetBorderColorsFor(NS_SIDE_LEFT, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderRightColors(nsIFrame *aFrame,
                                         nsIDOMCSSValue** aValue)
{
  return GetBorderColorsFor(NS_SIDE_RIGHT, aFrame, aValue);
}


nsresult
nsComputedDOMStyle::GetBorderTopColors(nsIFrame *aFrame,
                                       nsIDOMCSSValue** aValue)
{
  return GetBorderColorsFor(NS_SIDE_TOP, aFrame, aValue);
}


nsresult
nsComputedDOMStyle::GetBorderRadiusBottomLeft(nsIFrame *aFrame,
                                              nsIDOMCSSValue** aValue)
{
  return GetBorderRadiusFor(NS_SIDE_LEFT, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderRadiusBottomRight(nsIFrame *aFrame,
                                               nsIDOMCSSValue** aValue)
{
  return GetBorderRadiusFor(NS_SIDE_BOTTOM, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderRadiusTopLeft(nsIFrame *aFrame,
                                           nsIDOMCSSValue** aValue)
{
  return GetBorderRadiusFor(NS_SIDE_TOP, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderRadiusTopRight(nsIFrame *aFrame,
                                            nsIDOMCSSValue** aValue)
{
  return GetBorderRadiusFor(NS_SIDE_RIGHT, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderWidth(nsIFrame *aFrame,
                                   nsIDOMCSSValue** aValue)
{
  // return null per spec.
  aValue = nsnull;

  return NS_OK;
}

nsresult
nsComputedDOMStyle::GetBorderTopWidth(nsIFrame *aFrame,
                                      nsIDOMCSSValue** aValue)
{
  return GetBorderWidthFor(NS_SIDE_TOP, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderBottomWidth(nsIFrame *aFrame,
                                         nsIDOMCSSValue** aValue)
{
  return GetBorderWidthFor(NS_SIDE_BOTTOM, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderLeftWidth(nsIFrame *aFrame,
                                       nsIDOMCSSValue** aValue)
{
  return GetBorderWidthFor(NS_SIDE_LEFT, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderRightWidth(nsIFrame *aFrame,
                                        nsIDOMCSSValue** aValue)
{
  return GetBorderWidthFor(NS_SIDE_RIGHT, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderTopColor(nsIFrame *aFrame,
                                      nsIDOMCSSValue** aValue)
{
  return GetBorderColorFor(NS_SIDE_TOP, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderBottomColor(nsIFrame *aFrame,
                                         nsIDOMCSSValue** aValue)
{
  return GetBorderColorFor(NS_SIDE_BOTTOM, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderLeftColor(nsIFrame *aFrame,
                                       nsIDOMCSSValue** aValue)
{
  return GetBorderColorFor(NS_SIDE_LEFT, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderRightColor(nsIFrame *aFrame,
                                        nsIDOMCSSValue** aValue)
{
  return GetBorderColorFor(NS_SIDE_RIGHT, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetMarginWidth(nsIFrame *aFrame,
                                   nsIDOMCSSValue** aValue)
{
  // return null per spec.
  aValue = nsnull;

  return NS_OK;
}

nsresult
nsComputedDOMStyle::GetMarginTopWidth(nsIFrame *aFrame,
                                      nsIDOMCSSValue** aValue)
{
  return GetMarginWidthFor(NS_SIDE_TOP, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetMarginBottomWidth(nsIFrame *aFrame,
                                         nsIDOMCSSValue** aValue)
{
  return GetMarginWidthFor(NS_SIDE_BOTTOM, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetMarginLeftWidth(nsIFrame *aFrame,
                                       nsIDOMCSSValue** aValue)
{
  return GetMarginWidthFor(NS_SIDE_LEFT, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetMarginRightWidth(nsIFrame *aFrame,
                                        nsIDOMCSSValue** aValue)
{
  return GetMarginWidthFor(NS_SIDE_RIGHT, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetMarkerOffset(nsIFrame *aFrame,
                                    nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleContent* content = nsnull;
  GetStyleData(eStyleStruct_Content, (const nsStyleStruct*&)content, aFrame);

  if (content) {
    switch (content->mMarkerOffset.GetUnit()) {
      case eStyleUnit_Coord:
        val->SetTwips(content->mMarkerOffset.GetCoordValue());
        break;
      case eStyleUnit_Auto:
        val->SetIdent(NS_LITERAL_STRING("auto"));
        break;
      case eStyleUnit_Null:
        val->SetIdent(NS_LITERAL_STRING("none"));
        break;
      default:
        NS_WARNING("Double check the unit");
        val->SetTwips(0);
        break;
    }
  } else {
    val->SetTwips(0);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetOutline(nsIFrame *aFrame,
                               nsIDOMCSSValue** aValue)
{
  // return null per spec.
  aValue = nsnull;

  return NS_OK;
}

nsresult
nsComputedDOMStyle::GetOutlineWidth(nsIFrame *aFrame,
                                    nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleOutline* outline = nsnull;
  GetStyleData(eStyleStruct_Outline, (const nsStyleStruct*&)outline, aFrame);

  if (outline) {
    switch (outline->mOutlineWidth.GetUnit()) {
      case eStyleUnit_Coord:
        val->SetTwips(outline->mOutlineWidth.GetCoordValue());
        break;
      case eStyleUnit_Integer:
      case eStyleUnit_Proportional:
      case eStyleUnit_Enumerated:
      case eStyleUnit_Chars:
        {
          const nsAFlatCString& width =
            nsCSSProps::LookupPropertyValue(eCSSProperty__moz_outline_width,
                                            outline->mOutlineWidth.GetIntValue());
          val->SetIdent(width);
          break;
        }
      default:
        NS_WARNING("Double check the unit");
        val->SetTwips(0);
        break;
    }
  } else {
    val->SetTwips(0);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetOutlineStyle(nsIFrame *aFrame,
                                    nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleOutline* outline = nsnull;
  GetStyleData(eStyleStruct_Outline, (const nsStyleStruct*&)outline, aFrame);

  if (outline) {
    PRUint8 outlineStyle = outline->GetOutlineStyle();
    if (outlineStyle == NS_STYLE_BORDER_STYLE_NONE) {
      val->SetIdent(NS_LITERAL_STRING("none"));
    } else {
      const nsAFlatCString& style=
        nsCSSProps::SearchKeywordTable(outlineStyle,
                                       nsCSSProps::kBorderStyleKTable);
      val->SetIdent(style);
    }
  } else {
    val->SetIdent(NS_LITERAL_STRING("none"));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetOutlineColor(nsIFrame *aFrame,
                                    nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleOutline* outline = nsnull;
  GetStyleData(eStyleStruct_Outline, (const nsStyleStruct*&)outline, aFrame);

  if (outline) {
    nscolor color;
    outline->GetOutlineColor(color);

    nsDOMCSSRGBColor *rgb = nsnull;
    rgb = GetDOMCSSRGBColor(color);
    if (!rgb) {
      delete val;

      return NS_ERROR_OUT_OF_MEMORY;
    }

    val->SetColor(rgb);
  } else {
    val->SetString(NS_LITERAL_STRING(""));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetZIndex(nsIFrame *aFrame,
                              nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStylePosition* position = nsnull;

  GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)position, aFrame);
  if (position) {
    switch (position->mZIndex.GetUnit()) {
      case eStyleUnit_Integer:
        val->SetNumber(position->mZIndex.GetIntValue());
        break;
      case eStyleUnit_Auto:
        val->SetIdent(NS_LITERAL_STRING("auto"));
        break;
      case eStyleUnit_Inherit:
        val->SetIdent(NS_LITERAL_STRING("inherit"));
        break;
      default:
        NS_WARNING("Double Check the Unit!");
        val->SetIdent(NS_LITERAL_STRING("auto"));
        break;
    }
  } else {
    val->SetIdent(NS_LITERAL_STRING("auto"));
  }
    
  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetListStyleImage(nsIFrame *aFrame,
                                      nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleList* list = nsnull;
  GetStyleData(eStyleStruct_List, (const nsStyleStruct*&)list, aFrame);

  if (list) {
    if (list->mListStyleImage.IsEmpty()) {
      val->SetIdent(NS_LITERAL_STRING("none"));
    } else {
      val->SetURI(list->mListStyleImage);
    }
  } else {
    val->SetIdent(NS_LITERAL_STRING("none"));
  }
    
  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetListStylePosition(nsIFrame *aFrame,
                                         nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleList *list = nsnull;
  GetStyleData(eStyleStruct_List, (const nsStyleStruct*&)list, aFrame);

  if (list) {
    const nsAFlatCString& style =
      nsCSSProps::SearchKeywordTable(list->mListStylePosition,
                                     nsCSSProps::kListStylePositionKTable);
    val->SetIdent(style);
  } else {
    val->SetIdent(NS_LITERAL_STRING("outside"));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetListStyleType(nsIFrame *aFrame,
                                     nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleList *list = nsnull;
  GetStyleData(eStyleStruct_List, (const nsStyleStruct*&)list, aFrame);

  if (list && list->mListStyleType != NS_STYLE_LIST_STYLE_BASIC) {
    if (list->mListStyleType == NS_STYLE_LIST_STYLE_NONE) {
      val->SetIdent(NS_LITERAL_STRING("none"));
    } else {
      const nsAFlatCString& style =
        nsCSSProps::SearchKeywordTable(list->mListStyleType,
                                       nsCSSProps::kListStyleKTable);
      val->SetIdent(style);
    }
  } else {
    val->SetIdent(NS_LITERAL_STRING("disc"));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetLineHeight(nsIFrame *aFrame,
                                  nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleText *text = nsnull;
  GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&)text, aFrame);

  nscoord lineHeight;
  nsresult rv = GetLineHeightCoord(aFrame, text, lineHeight);

  if (NS_SUCCEEDED(rv)) {
    val->SetTwips(lineHeight);
  } else if (text) {
    switch (text->mLineHeight.GetUnit()) {
      case eStyleUnit_Percent:
        val->SetPercent(text->mLineHeight.GetPercentValue());
        break;
      case eStyleUnit_Factor:
        val->SetNumber(text->mLineHeight.GetFactorValue());
        break;
      default:
#ifdef DEBUG_ComputedDOMStyle
        NS_WARNING("double check the line-height");
#endif
        val->SetIdent(NS_LITERAL_STRING("normal"));
        break;
    }
  } else {
    val->SetIdent(NS_LITERAL_STRING("normal"));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetVerticalAlign(nsIFrame *aFrame,
                                     nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleTextReset *text = nsnull;
  GetStyleData(eStyleStruct_TextReset, (const nsStyleStruct*&)text, aFrame);

  if (text) {
    switch (text->mVerticalAlign.GetUnit()) {
      case eStyleUnit_Coord:
        val->SetTwips(text->mVerticalAlign.GetCoordValue());
        break;
      case eStyleUnit_Enumerated:
        {
          const nsAFlatCString& align =
            nsCSSProps::SearchKeywordTable(text->mVerticalAlign.GetIntValue(),
                                           nsCSSProps::kVerticalAlignKTable);
          val->SetIdent(align);
          break;
        }
      case eStyleUnit_Percent:
        {
          const nsStyleText *textData = nsnull;
          GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&)textData,
                       aFrame);

          nscoord lineHeight = 0;
          nsresult rv = GetLineHeightCoord(aFrame, textData, lineHeight);

          if (NS_SUCCEEDED(rv)) {
            val->SetTwips(lineHeight * text->mVerticalAlign.GetPercentValue());
          } else {
            val->SetPercent(text->mVerticalAlign.GetPercentValue());
          }

          break;
        }
      default:
        NS_WARNING("double check the vertical-align");
        val->SetIdent(NS_LITERAL_STRING("baseline"));
        break;
    }
  } else {
    val->SetIdent(NS_LITERAL_STRING("baseline"));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetTextAlign(nsIFrame *aFrame,
                                 nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleText* text = nsnull;
  GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&)text, aFrame);

  if (text) {
    const nsAFlatCString& align=
      nsCSSProps::SearchKeywordTable(text->mTextAlign,
                                     nsCSSProps::kTextAlignKTable);
    val->SetIdent(align);
  } else {
    val->SetIdent(NS_LITERAL_STRING("start"));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetTextDecoration(nsIFrame *aFrame,
                                      nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleTextReset* text = nsnull;
  GetStyleData(eStyleStruct_TextReset, (const nsStyleStruct*&)text, aFrame);

  if (text) {
    if (NS_STYLE_TEXT_DECORATION_NONE == text->mTextDecoration) {
      const nsAFlatCString& decoration=
        nsCSSKeywords::GetStringValue(eCSSKeyword_none);
      val->SetIdent(decoration);
    } else {
      nsAutoString decorationString;
      if (text->mTextDecoration & NS_STYLE_TEXT_DECORATION_UNDERLINE) {
        const nsAFlatCString& decoration=
          nsCSSProps::SearchKeywordTable(NS_STYLE_TEXT_DECORATION_UNDERLINE,
                                         nsCSSProps::kTextDecorationKTable);
        decorationString.AppendWithConversion(decoration.get());
      }
      if (text->mTextDecoration & NS_STYLE_TEXT_DECORATION_OVERLINE) {
        if (!decorationString.IsEmpty()) {
          decorationString.Append(PRUnichar(' '));
        }
        const nsAFlatCString& decoration=
          nsCSSProps::SearchKeywordTable(NS_STYLE_TEXT_DECORATION_OVERLINE,
                                         nsCSSProps::kTextDecorationKTable);
        decorationString.AppendWithConversion(decoration.get());
      }
      if (text->mTextDecoration & NS_STYLE_TEXT_DECORATION_LINE_THROUGH) {
        if (!decorationString.IsEmpty()) {
          decorationString.Append(PRUnichar(' '));
        }
        const nsAFlatCString& decoration=
          nsCSSProps::SearchKeywordTable(NS_STYLE_TEXT_DECORATION_LINE_THROUGH,
                                         nsCSSProps::kTextDecorationKTable);
        decorationString.AppendWithConversion(decoration.get());
      }
      if (text->mTextDecoration & NS_STYLE_TEXT_DECORATION_BLINK) {
        if (!decorationString.IsEmpty()) {
          decorationString.Append(PRUnichar(' '));
        }
        const nsAFlatCString& decoration=
          nsCSSProps::SearchKeywordTable(NS_STYLE_TEXT_DECORATION_BLINK,
                                         nsCSSProps::kTextDecorationKTable);
        decorationString.AppendWithConversion(decoration.get());
      }
      val->SetString(decorationString);
    }
  } else {
    val->SetIdent(NS_LITERAL_STRING("none"));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetTextIndent(nsIFrame *aFrame,
                                  nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleText *text = nsnull;
  GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&)text, aFrame);

  // Flush all pending notifications so that our frames are up to date
  nsCOMPtr<nsIDocument> document;
  mContent->GetDocument(*getter_AddRefs(document));
  if (document) {
    document->FlushPendingNotifications();
  }

  if (text) {
    switch (text->mTextIndent.GetUnit()) {
      case eStyleUnit_Coord:
        val->SetTwips(text->mTextIndent.GetCoordValue());
        break;
      case eStyleUnit_Percent:
        {
          nsIFrame *container = nsnull;
          container = GetContainingBlock(aFrame);
          if (container) {
            nsSize size;
            container->GetSize(size);
            val->SetTwips(size.width * text->mTextIndent.GetPercentValue());
          } else {
            // no containing block
            val->SetPercent(text->mTextIndent.GetPercentValue());
          }
          break;
        }
      case eStyleUnit_Inherit:
        val->SetIdent(NS_LITERAL_STRING("inherit"));
        break;
      default:
        val->SetTwips(0);
        break;
    }
  } else {
    val->SetTwips(0);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetTextTransform(nsIFrame *aFrame,
                                     nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleText *text = nsnull;
  GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&)text, aFrame);

  if (text && text->mTextTransform != NS_STYLE_TEXT_TRANSFORM_NONE) {
    const nsAFlatCString& textTransform =
      nsCSSProps::SearchKeywordTable(text->mTextTransform,
                                     nsCSSProps::kTextTransformKTable);
    val->SetIdent(textTransform);
  } else {
    val->SetIdent(NS_LITERAL_STRING("none"));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetLetterSpacing(nsIFrame *aFrame,
                                     nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleText *text = nsnull;
  GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&)text, aFrame);

  if (text && text->mLetterSpacing.GetUnit() == eStyleUnit_Coord) {
    val->SetTwips(text->mLetterSpacing.GetCoordValue());
  } else {
    val->SetIdent(NS_LITERAL_STRING("normal"));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetWordSpacing(nsIFrame *aFrame,
                                   nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleText *text = nsnull;
  GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&)text, aFrame);

  if (text && text->mWordSpacing.GetUnit() == eStyleUnit_Coord) {
    val->SetTwips(text->mWordSpacing.GetCoordValue());
  } else {
    val->SetIdent(NS_LITERAL_STRING("normal"));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetWhiteSpace(nsIFrame *aFrame,
                                  nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleText *text = nsnull;
  GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&)text, aFrame);

  if (text && text->mWhiteSpace != NS_STYLE_WHITESPACE_NORMAL) {
    const nsAFlatCString& whiteSpace =
      nsCSSProps::SearchKeywordTable(text->mWhiteSpace,
                                     nsCSSProps::kWhitespaceKTable);
    val->SetIdent(whiteSpace);
  } else {
    val->SetIdent(NS_LITERAL_STRING("normal"));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetVisibility(nsIFrame *aFrame,
                                  nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleVisibility* visibility = nsnull;
  GetStyleData(eStyleStruct_Visibility, (const nsStyleStruct*&)visibility,
               aFrame);

  if (visibility) {
    const nsAFlatCString& value=
      nsCSSProps::SearchKeywordTable(visibility->mVisible,
                                     nsCSSProps::kVisibilityKTable);
    val->SetIdent(value);
  } else {
    val->SetIdent(NS_LITERAL_STRING("visible"));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetDirection(nsIFrame *aFrame,
                                 nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleVisibility *visibility = nsnull;
  GetStyleData(eStyleStruct_Visibility, (const nsStyleStruct*&)visibility,
               aFrame);

  if (visibility) {
    const nsAFlatCString & direction =
      nsCSSProps::SearchKeywordTable(visibility->mDirection,
                                     nsCSSProps::kDirectionKTable);
    val->SetIdent(direction);
  } else {
    val->SetIdent(NS_LITERAL_STRING("ltr"));
  }
  
  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetUnicodeBidi(nsIFrame *aFrame,
                                   nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

#ifdef IBMBIDI
  const nsStyleTextReset *text = nsnull;
  GetStyleData(eStyleStruct_TextReset, (const nsStyleStruct*&)text, aFrame);

  if (text && text->mUnicodeBidi != NS_STYLE_UNICODE_BIDI_NORMAL) {
    const nsAFlatCString& bidi =
      nsCSSProps::SearchKeywordTable(text->mUnicodeBidi,
                                     nsCSSProps::kUnicodeBidiKTable);
    val->SetIdent(bidi);
  } else {
    val->SetIdent(NS_LITERAL_STRING("normal"));
  }
#else // IBMBIDI
  val->SetIdent(NS_LITERAL_STRING("normal"));
#endif // IBMBIDI

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetCursor(nsIFrame *aFrame,
                              nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleUserInterface *ui = nsnull;
  GetStyleData(eStyleStruct_UserInterface, (const nsStyleStruct*&)ui, aFrame);

  if (ui) {
    if (!ui->mCursorImage.IsEmpty()) {
      val->SetURI(ui->mCursorImage);
    } else {
      if (ui->mCursor == NS_STYLE_CURSOR_AUTO) {
        val->SetIdent(NS_LITERAL_STRING("auto"));
      } else {
        const nsAFlatCString& cursor =
          nsCSSProps::SearchKeywordTable(ui->mCursor,
                                         nsCSSProps::kCursorKTable);
        val->SetIdent(cursor);
      }
    }
  } else {
    val->SetIdent(NS_LITERAL_STRING("auto"));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetAppearance(nsIFrame *aFrame,
                                  nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay *displayData = nsnull;
  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)displayData,
               aFrame);

  PRUint8 appearance = NS_THEME_NONE;
  if (displayData) {
    appearance = displayData->mAppearance;
  }

  const nsAFlatCString& appearanceIdent =
    nsCSSProps::SearchKeywordTable(appearance,
                                   nsCSSProps::kAppearanceKTable);
  val->SetIdent(appearanceIdent);

  return CallQueryInterface(val, aValue);
}


#ifdef INCLUDE_XUL
nsresult
nsComputedDOMStyle::GetBoxAlign(nsIFrame *aFrame,
                                nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleXUL *xul = nsnull;
  GetStyleData(eStyleStruct_XUL, (const nsStyleStruct*&)xul, aFrame);

  PRUint8 boxAlign = NS_STYLE_BOX_ALIGN_STRETCH;
  if (xul) {
    boxAlign = xul->mBoxAlign;
  }

  const nsAFlatCString& boxAlignIdent =
    nsCSSProps::SearchKeywordTable(boxAlign,
                                   nsCSSProps::kBoxAlignKTable);
  val->SetIdent(boxAlignIdent);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBoxDirection(nsIFrame *aFrame,
                                    nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleXUL *xul = nsnull;
  GetStyleData(eStyleStruct_XUL, (const nsStyleStruct*&)xul, aFrame);

  PRUint8 boxDirection = NS_STYLE_BOX_DIRECTION_NORMAL;
  if (xul) {
    boxDirection = xul->mBoxDirection;
  }

  const nsAFlatCString& boxDirectionIdent =
    nsCSSProps::SearchKeywordTable(boxDirection,
                                   nsCSSProps::kBoxDirectionKTable);
  val->SetIdent(boxDirectionIdent);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBoxFlex(nsIFrame *aFrame,
                               nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleXUL *xul = nsnull;
  GetStyleData(eStyleStruct_XUL, (const nsStyleStruct*&)xul, aFrame);

  float boxFlex = 0.0f;
  if (xul) {
    boxFlex = xul->mBoxFlex;
  }

  val->SetNumber(boxFlex);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBoxOrdinalGroup(nsIFrame *aFrame,
                                       nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleXUL *xul = nsnull;
  GetStyleData(eStyleStruct_XUL, (const nsStyleStruct*&)xul, aFrame);

  PRUint32 boxOrdinalGroup = 1;
  if (xul) {
    boxOrdinalGroup = xul->mBoxOrdinal;
  }

  val->SetNumber(boxOrdinalGroup);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBoxOrient(nsIFrame *aFrame,
                                 nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleXUL *xul = nsnull;
  GetStyleData(eStyleStruct_XUL, (const nsStyleStruct*&)xul, aFrame);

  PRUint8 boxOrient = NS_STYLE_BOX_ORIENT_HORIZONTAL;
  if (xul) {
    boxOrient = xul->mBoxOrient;
  }

  const nsAFlatCString& boxOrientIdent =
    nsCSSProps::SearchKeywordTable(boxOrient,
                                   nsCSSProps::kBoxOrientKTable);
  val->SetIdent(boxOrientIdent);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBoxPack(nsIFrame *aFrame,
                               nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleXUL *xul = nsnull;
  GetStyleData(eStyleStruct_XUL, (const nsStyleStruct*&)xul, aFrame);

  PRUint8 boxPack = NS_STYLE_BOX_PACK_START;
  if (xul) {
    boxPack = xul->mBoxPack;
  }

  const nsAFlatCString& boxPackIdent =
    nsCSSProps::SearchKeywordTable(boxPack,
                                   nsCSSProps::kBoxPackKTable);
  val->SetIdent(boxPackIdent);

  return CallQueryInterface(val, aValue);
}

#endif // INCLUDE_XUL

nsresult
nsComputedDOMStyle::GetBoxSizing(nsIFrame *aFrame,
                                 nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStylePosition *positionData = nsnull;
  GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)positionData,
               aFrame);

  PRUint8 boxSizing = NS_STYLE_BOX_SIZING_CONTENT;
  if (positionData) {
    boxSizing = positionData->mBoxSizing;
  }

  const nsAFlatCString& boxSizingIdent =
    nsCSSProps::SearchKeywordTable(boxSizing,
                                   nsCSSProps::kBoxSizingKTable);
  val->SetIdent(boxSizingIdent);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetFloatEdge(nsIFrame *aFrame,
                                 nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleBorder *borderData = nsnull;
  GetStyleData(eStyleStruct_Border, (const nsStyleStruct*&)borderData, aFrame);

  PRUint8 floatEdge = NS_STYLE_FLOAT_EDGE_CONTENT;
  if (borderData) {
    floatEdge = borderData->mFloatEdge;
  }

  const nsAFlatCString& floatEdgeIdent =
    nsCSSProps::SearchKeywordTable(floatEdge,
                                   nsCSSProps::kFloatEdgeKTable);
  val->SetIdent(floatEdgeIdent);

  return CallQueryInterface(val, aValue);
}


nsresult
nsComputedDOMStyle::GetUserFocus(nsIFrame *aFrame,
                                 nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleUserInterface *uiData = nsnull;
  GetStyleData(eStyleStruct_UserInterface, (const nsStyleStruct*&)uiData,
               aFrame);

  if (uiData && uiData->mUserFocus != NS_STYLE_USER_FOCUS_NONE) {
    if (uiData->mUserFocus == NS_STYLE_USER_FOCUS_NORMAL) {
      const nsAFlatCString& userFocusIdent =
              nsCSSKeywords::GetStringValue(eCSSKeyword_normal);
      val->SetIdent(userFocusIdent);
    } else {
      const nsAFlatCString& userFocusIdent =
        nsCSSProps::SearchKeywordTable(uiData->mUserFocus,
                                       nsCSSProps::kUserFocusKTable);
      val->SetIdent(userFocusIdent);
    }
  } else {
    const nsAFlatCString& userFocusIdent =
            nsCSSKeywords::GetStringValue(eCSSKeyword_none);
    val->SetIdent(userFocusIdent);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetUserInput(nsIFrame *aFrame,
                                 nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleUserInterface *uiData = nsnull;
  GetStyleData(eStyleStruct_UserInterface, (const nsStyleStruct*&)uiData,
               aFrame);

  if (uiData && uiData->mUserInput != NS_STYLE_USER_INPUT_AUTO) {
    if (uiData->mUserInput == NS_STYLE_USER_INPUT_NONE) {
      const nsAFlatCString& userInputIdent =
        nsCSSKeywords::GetStringValue(eCSSKeyword_none);
      val->SetIdent(userInputIdent);
    } else {
      const nsAFlatCString& userInputIdent =
        nsCSSProps::SearchKeywordTable(uiData->mUserInput,
                                       nsCSSProps::kUserInputKTable);
      val->SetIdent(userInputIdent);
    }
  } else {
    const nsAFlatCString& userInputIdent =
            nsCSSKeywords::GetStringValue(eCSSKeyword_auto);
    val->SetIdent(userInputIdent);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetUserModify(nsIFrame *aFrame,
                                  nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleUserInterface *uiData = nsnull;
  GetStyleData(eStyleStruct_UserInterface, (const nsStyleStruct*&)uiData,
               aFrame);

  PRUint8 userModify = NS_STYLE_USER_MODIFY_READ_ONLY;
  if (uiData) {
    userModify = uiData->mUserModify;
  }

  const nsAFlatCString& userModifyIdent =
    nsCSSProps::SearchKeywordTable(userModify,
                                   nsCSSProps::kUserModifyKTable);
  val->SetIdent(userModifyIdent);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetUserSelect(nsIFrame *aFrame,
                                  nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleUIReset *uiData = nsnull;
  GetStyleData(eStyleStruct_UIReset, (const nsStyleStruct*&)uiData, aFrame);

  if (uiData && uiData->mUserSelect != NS_STYLE_USER_SELECT_AUTO) {
    if (uiData->mUserSelect == NS_STYLE_USER_SELECT_NONE) {
      const nsAFlatCString& userSelectIdent =
        nsCSSKeywords::GetStringValue(eCSSKeyword_none);
      val->SetIdent(userSelectIdent);
    } else {
      const nsAFlatCString& userSelectIdent =
        nsCSSProps::SearchKeywordTable(uiData->mUserSelect,
                                       nsCSSProps::kUserSelectKTable);
      val->SetIdent(userSelectIdent);
    }
  } else {
    const nsAFlatCString& userSelectIdent =
            nsCSSKeywords::GetStringValue(eCSSKeyword_auto);
    val->SetIdent(userSelectIdent);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetDisplay(nsIFrame *aFrame,
                               nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay *displayData = nsnull;
  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)displayData,
               aFrame);

  if (displayData) {
    if (displayData->mDisplay == NS_STYLE_DISPLAY_NONE) {
      val->SetIdent(NS_LITERAL_STRING("none"));
    } else {
      const nsAFlatCString& display =
        nsCSSProps::SearchKeywordTable(displayData->mDisplay,
                                       nsCSSProps::kDisplayKTable);
      val->SetIdent(display);
    }
  } else {
    val->SetIdent(NS_LITERAL_STRING("inline"));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetPosition(nsIFrame *aFrame,
                                nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay* display = nsnull;

  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display, aFrame);

  if (display) {
    switch (display->mPosition) {
      case NS_STYLE_POSITION_STATIC:
        val->SetIdent(NS_LITERAL_STRING("static"));
        break;
      case NS_STYLE_POSITION_RELATIVE:
        val->SetIdent(NS_LITERAL_STRING("relative"));
        break;
      case NS_STYLE_POSITION_ABSOLUTE:
        val->SetIdent(NS_LITERAL_STRING("absolute"));
        break;
      case NS_STYLE_POSITION_FIXED:
        val->SetIdent(NS_LITERAL_STRING("fixed"));
        break;
      default:
        NS_WARNING("Double check the position!");
        break;
    }
  }
  
  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetClip(nsIFrame *aFrame,
                            nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay* display = nsnull;

  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display, aFrame);

  nsresult rv = NS_OK;
  nsROCSSPrimitiveValue *topVal = nsnull;
  nsROCSSPrimitiveValue *rightVal = nsnull;
  nsROCSSPrimitiveValue *bottomVal = nsnull;
  nsROCSSPrimitiveValue *leftVal = nsnull;
  if (display) {
    if (display->mClipFlags == NS_STYLE_CLIP_AUTO ||
        display->mClipFlags == (NS_STYLE_CLIP_TOP_AUTO |
                                NS_STYLE_CLIP_RIGHT_AUTO |
                                NS_STYLE_CLIP_BOTTOM_AUTO |
                                NS_STYLE_CLIP_LEFT_AUTO)) {
      val->SetIdent(NS_LITERAL_STRING("auto"));
    } else {
      // create the cssvalues for the sides, stick them in the rect object
      topVal = GetROCSSPrimitiveValue();
      rightVal = GetROCSSPrimitiveValue();
      bottomVal = GetROCSSPrimitiveValue();
      leftVal = GetROCSSPrimitiveValue();
      if (topVal && rightVal && bottomVal && leftVal) {
        nsDOMCSSRect * domRect = new nsDOMCSSRect(topVal, rightVal,
                                                  bottomVal, leftVal);
        if (domRect) {
          if (display->mClipFlags & NS_STYLE_CLIP_TOP_AUTO) {
            topVal->SetIdent(NS_LITERAL_STRING("auto"));
          } else {
            topVal->SetTwips(display->mClip.y);
          }
        
          if (display->mClipFlags & NS_STYLE_CLIP_RIGHT_AUTO) {
            rightVal->SetIdent(NS_LITERAL_STRING("auto"));
          } else {
            rightVal->SetTwips(display->mClip.width + display->mClip.x);
          }
        
          if (display->mClipFlags & NS_STYLE_CLIP_BOTTOM_AUTO) {
            bottomVal->SetIdent(NS_LITERAL_STRING("auto"));
          } else {
            bottomVal->SetTwips(display->mClip.height + display->mClip.y);
          }
          
          if (display->mClipFlags & NS_STYLE_CLIP_LEFT_AUTO) {
            leftVal->SetIdent(NS_LITERAL_STRING("auto"));
          } else {
            leftVal->SetTwips(display->mClip.x);
          }

          val->SetRect(domRect);
        } else {
          rv = NS_ERROR_OUT_OF_MEMORY;
        }
      } else {
        rv = NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  if (NS_FAILED(rv)) {
    delete topVal;
    delete rightVal;
    delete bottomVal;
    delete leftVal;
    delete val;

    return rv;
  }
  
  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetOverflow(nsIFrame *aFrame,
                                nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay* display = nsnull;
  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display, aFrame);

  if (display && display->mOverflow != NS_STYLE_OVERFLOW_AUTO) {
    const nsAFlatCString& overflow =
      nsCSSProps::SearchKeywordTable(display->mOverflow,
                                     nsCSSProps::kOverflowKTable);
    val->SetIdent(overflow);
  } else {
    val->SetIdent(NS_LITERAL_STRING("auto"));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetHeight(nsIFrame *aFrame,
                              nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  PRBool calcHeight = PR_FALSE;
  
  if (aFrame) {
    calcHeight = PR_TRUE;
    // Flush all pending notifications so that our frames are up to date
    nsCOMPtr<nsIDocument> document;
    mContent->GetDocument(*getter_AddRefs(document));

    if (document) {
      document->FlushPendingNotifications();
    }
  
    const nsStyleDisplay* displayData = nsnull;
    GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)displayData,
                 aFrame);
    if (displayData && displayData->mDisplay == NS_STYLE_DISPLAY_INLINE) {
      nsFrameState frameState;
      aFrame->GetFrameState(&frameState);
      if (!(frameState & NS_FRAME_REPLACED_ELEMENT)) {
        calcHeight = PR_FALSE;
      }
    }
  }

  if (calcHeight) {
    nsSize size;
    nsMargin padding;
    nsMargin border;
    aFrame->GetSize(size);
    const nsStylePadding* paddingData = nsnull;
    GetStyleData(eStyleStruct_Padding, (const nsStyleStruct*&)paddingData,
                 aFrame);
    if (paddingData) {
      paddingData->CalcPaddingFor(aFrame, padding);
    }
    const nsStyleBorder* borderData = nsnull;
    GetStyleData(eStyleStruct_Border, (const nsStyleStruct*&)borderData,
                 aFrame);
    if (borderData) {
      borderData->CalcBorderFor(aFrame, border);
    }
  
    val->SetTwips(size.height - padding.top - padding.bottom -
                  border.top - border.bottom);
  } else {
    // Just return the value in the style context
    const nsStylePosition* positionData = nsnull;
    GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)positionData,
                 aFrame);
    if (positionData) {
      switch (positionData->mHeight.GetUnit()) {
        case eStyleUnit_Coord:
          val->SetTwips(positionData->mHeight.GetCoordValue());
          break;
        case eStyleUnit_Percent:
          val->SetPercent(positionData->mHeight.GetPercentValue());
          break;
        case eStyleUnit_Auto:
          val->SetIdent(NS_LITERAL_STRING("auto"));
          break;
        case eStyleUnit_Inherit:
          val->SetIdent(NS_LITERAL_STRING("inherit"));
          break;
        default:
          NS_WARNING("Double check the unit");
          val->SetTwips(0);
          break;
      }
    } else {
      val->SetTwips(0);
    }
  }
  
  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetWidth(nsIFrame *aFrame,
                             nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  PRBool calcWidth = PR_FALSE;

  if (aFrame) {
    calcWidth = PR_TRUE;
    // Flush all pending notifications so that our frames are up to date
    nsCOMPtr<nsIDocument> document;
    mContent->GetDocument(*getter_AddRefs(document));
  
    if (document) {
      document->FlushPendingNotifications();
    }

    const nsStyleDisplay *displayData = nsnull;
    GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)displayData,
                 aFrame);
    if (displayData && displayData->mDisplay == NS_STYLE_DISPLAY_INLINE) {
      nsFrameState frameState;
      aFrame->GetFrameState(&frameState);
      if (!(frameState & NS_FRAME_REPLACED_ELEMENT)) {
        calcWidth = PR_FALSE;
      }
    }
  }

  if (calcWidth) {
    nsSize size;
    nsMargin padding;
    nsMargin border;
    aFrame->GetSize(size);
    const nsStylePadding *paddingData = nsnull;
    GetStyleData(eStyleStruct_Padding, (const nsStyleStruct*&)paddingData,
                 aFrame);
    if (paddingData) {
      paddingData->CalcPaddingFor(aFrame, padding);
    }
    const nsStyleBorder *borderData = nsnull;
    GetStyleData(eStyleStruct_Border, (const nsStyleStruct*&)borderData,
                 aFrame);
    if (borderData) {
      borderData->CalcBorderFor(aFrame, border);
    }
    val->SetTwips(size.width - padding.left - padding.right -
                  border.left - border.right);
  } else {
    // Just return the value in the style context
    const nsStylePosition *positionData = nsnull;
    GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)positionData,
                 aFrame);
    if (positionData) {
      switch (positionData->mWidth.GetUnit()) {
        case eStyleUnit_Coord:
          val->SetTwips(positionData->mWidth.GetCoordValue());
          break;
        case eStyleUnit_Percent:
          val->SetPercent(positionData->mWidth.GetPercentValue());
          break;
        case eStyleUnit_Auto:
          val->SetIdent(NS_LITERAL_STRING("auto"));
          break;
        case eStyleUnit_Inherit:
          val->SetIdent(NS_LITERAL_STRING("inherit"));
          break;
        default:
          NS_WARNING("Double check the unit");
          val->SetTwips(0);
          break;
      }
    } else {
      val->SetTwips(0);
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetMaxHeight(nsIFrame *aFrame,
                                 nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStylePosition *positionData = nsnull;
  GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)positionData,
               aFrame);

  // Flush all pending notifications so that our frames are up to date
  nsCOMPtr<nsIDocument> document;
  mContent->GetDocument(*getter_AddRefs(document));
  if (document) {
    document->FlushPendingNotifications();
  }

  if (positionData) {
    nsIFrame *container = nsnull;
    nsSize size;
    nscoord minHeight = 0;

    if (positionData->mMinHeight.GetUnit() == eStyleUnit_Percent) {
      container = GetContainingBlock(aFrame);
      if (container) {
        container->GetSize(size);
        minHeight = nscoord(size.height *
                            positionData->mMinHeight.GetPercentValue());
      }
    } else if (positionData->mMinHeight.GetUnit() == eStyleUnit_Coord) {
      minHeight = positionData->mMinHeight.GetCoordValue();
    }

    switch (positionData->mMaxHeight.GetUnit()) {
      case eStyleUnit_Coord:
        val->SetTwips(PR_MAX(minHeight,
                             positionData->mMaxHeight.GetCoordValue()));
        break;
      case eStyleUnit_Percent:
        if (!container) {
          container = GetContainingBlock(aFrame);
          if (container) {
            container->GetSize(size);
          } else {
            // no containing block
            val->SetPercent(positionData->mMaxHeight.GetPercentValue());
          }
        }
        if (container) {
          val->SetTwips(PR_MAX(minHeight, size.height *
                               positionData->mMaxHeight.GetPercentValue()));
        }

        break;
      case eStyleUnit_Inherit:
        val->SetIdent(NS_LITERAL_STRING("inherit"));

        break;
      default:
        val->SetIdent(NS_LITERAL_STRING("none"));

        break;
    }
  } else {
    val->SetIdent(NS_LITERAL_STRING("none"));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetMaxWidth(nsIFrame *aFrame,
                                nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStylePosition *positionData = nsnull;
  GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)positionData,
               aFrame);

  // Flush all pending notifications so that our frames are up to date
  nsCOMPtr<nsIDocument> document;
  mContent->GetDocument(*getter_AddRefs(document));
  if (document) {
    document->FlushPendingNotifications();
  }

  if (positionData) {
    nsIFrame *container = nsnull;
    nsSize size;
    nscoord minWidth = 0;

    if (positionData->mMinWidth.GetUnit() == eStyleUnit_Percent) {
      container = GetContainingBlock(aFrame);
      if (container) {
        container->GetSize(size);
        minWidth = nscoord(size.width *
                           positionData->mMinWidth.GetPercentValue());
      }
    } else if (positionData->mMinWidth.GetUnit() == eStyleUnit_Coord) {
      minWidth = positionData->mMinWidth.GetCoordValue();
    }

    switch (positionData->mMaxWidth.GetUnit()) {
      case eStyleUnit_Coord:
        val->SetTwips(PR_MAX(minWidth,
                             positionData->mMaxWidth.GetCoordValue()));
        break;
      case eStyleUnit_Percent:
        if (!container) {
          container = GetContainingBlock(aFrame);
          if (container) {
            container->GetSize(size);
          } else {
            // no containing block
            val->SetPercent(positionData->mMaxWidth.GetPercentValue());
          }
        }
        if (container) {
          val->SetTwips(PR_MAX(minWidth, size.width *
                               positionData->mMaxWidth.GetPercentValue()));
        }

        break;
      case eStyleUnit_Inherit:
        val->SetIdent(NS_LITERAL_STRING("inherit"));

        break;
      default:
        val->SetIdent(NS_LITERAL_STRING("none"));

        break;
    }
  } else {
    val->SetIdent(NS_LITERAL_STRING("none"));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetMinHeight(nsIFrame *aFrame,
                                 nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStylePosition *positionData = nsnull;
  GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)positionData,
               aFrame);

  // Flush all pending notifications so that our frames are up to date
  nsCOMPtr<nsIDocument> document;
  mContent->GetDocument(*getter_AddRefs(document));
  if (document) {
    document->FlushPendingNotifications();
  }

  if (positionData) {
    nsIFrame *container = nsnull;
    nsSize size;
    switch (positionData->mMinHeight.GetUnit()) {
      case eStyleUnit_Coord:
        val->SetTwips(positionData->mMinHeight.GetCoordValue());
        break;
      case eStyleUnit_Percent:
        container = GetContainingBlock(aFrame);
        if (container) {
          container->GetSize(size);
          val->SetTwips(size.height *
                        positionData->mMinHeight.GetPercentValue());
        } else {
          // no containing block
          val->SetPercent(positionData->mMinHeight.GetPercentValue());
        }

        break;
      case eStyleUnit_Inherit:
        val->SetIdent(NS_LITERAL_STRING("inherit"));

        break;
      default:
        val->SetTwips(0);

        break;
    }
  } else {
    val->SetTwips(0);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetMinWidth(nsIFrame *aFrame,
                                nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStylePosition *positionData = nsnull;
  GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)positionData,
               aFrame);

  // Flush all pending notifications so that our frames are up to date
  nsCOMPtr<nsIDocument> document;
  mContent->GetDocument(*getter_AddRefs(document));
  if (document) {
    document->FlushPendingNotifications();
  }

  if (positionData) {
    nsIFrame *container = nsnull;
    nsSize size;
    switch (positionData->mMinWidth.GetUnit()) {
      case eStyleUnit_Coord:
        val->SetTwips(positionData->mMinWidth.GetCoordValue());
        break;
      case eStyleUnit_Percent:
        container = GetContainingBlock(aFrame);
        if (container) {
          container->GetSize(size);
          val->SetTwips(size.width *
                        positionData->mMinWidth.GetPercentValue());
        } else {
          // no containing block
          val->SetPercent(positionData->mMinWidth.GetPercentValue());
        }
        break;
      case eStyleUnit_Inherit:
        val->SetIdent(NS_LITERAL_STRING("inherit"));
        break;
      default:
        val->SetTwips(0);
        break;
    }
  } else {
    val->SetTwips(0);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetLeft(nsIFrame *aFrame, nsIDOMCSSValue** aValue)
{
  return GetOffsetWidthFor(NS_SIDE_LEFT, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetRight(nsIFrame *aFrame, nsIDOMCSSValue** aValue)
{
  return GetOffsetWidthFor(NS_SIDE_RIGHT, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetTop(nsIFrame *aFrame, nsIDOMCSSValue** aValue)
{
  return GetOffsetWidthFor(NS_SIDE_TOP, aFrame, aValue);
}

nsROCSSPrimitiveValue*
nsComputedDOMStyle::GetROCSSPrimitiveValue()
{
  nsROCSSPrimitiveValue *primitiveValue = new nsROCSSPrimitiveValue(mT2P);

  NS_ASSERTION(primitiveValue != 0, "ran out of memory");

  return primitiveValue;
}

nsDOMCSSValueList*
nsComputedDOMStyle::GetROCSSValueList(PRBool aCommaDelimited)
{
  nsDOMCSSValueList *valueList = new nsDOMCSSValueList(aCommaDelimited,
                                                       PR_TRUE);
  NS_ASSERTION(valueList != 0, "ran out of memory");

  return valueList;
}

nsresult
nsComputedDOMStyle::GetOffsetWidthFor(PRUint8 aSide, nsIFrame* aFrame,
                                      nsIDOMCSSValue** aValue)
{
  const nsStyleDisplay* display = nsnull;
  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display, aFrame);

  // Flush all pending notifications so that our frames are up to date
  nsCOMPtr<nsIDocument> document;
  mContent->GetDocument(*getter_AddRefs(document));
  if (document) {
    document->FlushPendingNotifications();
  }

  nsresult rv = NS_OK;
  if (display) {
    switch (display->mPosition) {
      case NS_STYLE_POSITION_STATIC:
        rv = GetStaticOffset(aSide, aFrame, aValue);
        break;
      case NS_STYLE_POSITION_RELATIVE:
        rv = GetRelativeOffset(aSide, aFrame, aValue);
        break;
      case NS_STYLE_POSITION_ABSOLUTE:
      case NS_STYLE_POSITION_FIXED:
        rv = GetAbsoluteOffset(aSide, aFrame, aValue);
        break;
      default:
        NS_WARNING("double check the position");
        break;
    }
  }

  return rv;
}

nsresult
nsComputedDOMStyle::GetAbsoluteOffset(PRUint8 aSide, nsIFrame* aFrame,
                                      nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  nsIFrame* container = GetContainingBlock(aFrame);
  if (container) {
    nsRect rect;
    nsRect containerRect;
    nscoord margin = GetMarginWidthCoordFor(aSide, aFrame);
    nscoord border = GetBorderWidthCoordFor(aSide, container);
    nscoord horScrollBarHeight = 0;
    nscoord verScrollBarWidth = 0;
    aFrame->GetRect(rect);
    container->GetRect(containerRect);
      
    nsCOMPtr<nsIAtom> typeAtom;
    container->GetFrameType(getter_AddRefs(typeAtom));
    if (typeAtom == nsLayoutAtoms::viewportFrame) {
      // For absolutely positioned frames scrollbars are taken into
      // account by virtue of getting a containing block that does
      // _not_ include the scrollbars.  For fixed positioned frames,
      // the containing block is the viewport, which _does_ include
      // scrollbars.  We have to do some extra work.
      nsIFrame* scrollingChild;
      nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShellWeak);
      NS_ASSERTION(presShell, "Must have a presshell!");
      nsCOMPtr<nsIPresContext> presContext;
      presShell->GetPresContext(getter_AddRefs(presContext));
      // the first child in the default frame list is what we want
      container->FirstChild(presContext, nsnull, &scrollingChild);
      nsCOMPtr<nsIScrollableFrame> scrollFrame =
        do_QueryInterface(scrollingChild);
      if (scrollFrame) {
        scrollFrame->GetScrollbarSizes(presContext, &verScrollBarWidth,
                                       &horScrollBarHeight);
        PRBool verScrollBarVisible;
        PRBool horScrollBarVisible;
        scrollFrame->GetScrollbarVisibility(presContext, &verScrollBarVisible,
                                            &horScrollBarVisible);
        if (!verScrollBarVisible) {
          verScrollBarWidth = 0;
        }
        if (!horScrollBarVisible) {
          horScrollBarHeight = 0;
        }
      }
    }

    nscoord offset = 0;
    switch (aSide) {
      case NS_SIDE_TOP:
        offset = rect.y - margin - border;

        break;
      case NS_SIDE_RIGHT:
        offset = containerRect.width - rect.width -
          rect.x - margin - border - verScrollBarWidth;

        break;
      case NS_SIDE_BOTTOM:
        offset = containerRect.height - rect.height -
          rect.y - margin - border - horScrollBarHeight;

        break;
      case NS_SIDE_LEFT:
        offset = rect.x - margin - border;

        break;
      default:
        NS_WARNING("double check the side");

        break;
    }
    val->SetTwips(offset);
  } else {
    // XXX no frame.  This property makes no sense
    val->SetTwips(0);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetRelativeOffset(PRUint8 aSide, nsIFrame* aFrame,
                                      nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStylePosition* positionData = nsnull;
  GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)positionData,
               aFrame);
  if (positionData) {
    nsStyleCoord coord;
    PRInt32 sign = 1;
    switch (aSide) {
      case NS_SIDE_TOP:
        positionData->mOffset.GetTop(coord);
        if (coord.GetUnit() != eStyleUnit_Coord &&
            coord.GetUnit() != eStyleUnit_Percent) {
          positionData->mOffset.GetBottom(coord);
          sign = -1;
        }
        break;
      case NS_SIDE_RIGHT:
        positionData->mOffset.GetRight(coord);
        if (coord.GetUnit() != eStyleUnit_Coord &&
            coord.GetUnit() != eStyleUnit_Percent) {
          positionData->mOffset.GetLeft(coord);
          sign = -1;
        }
        break;
      case NS_SIDE_BOTTOM:
        positionData->mOffset.GetBottom(coord);
        if (coord.GetUnit() != eStyleUnit_Coord &&
            coord.GetUnit() != eStyleUnit_Percent) {
          positionData->mOffset.GetTop(coord);
          sign = -1;
        }
        break;
      case NS_SIDE_LEFT:
        positionData->mOffset.GetLeft(coord);
        if (coord.GetUnit() != eStyleUnit_Coord &&
            coord.GetUnit() != eStyleUnit_Percent) {
          positionData->mOffset.GetRight(coord);
          sign = -1;
        }
        break;
      default:
        NS_WARNING("double check the side");
        break;
    }

    nsIFrame* container = nsnull;
    const nsStyleBorder* borderData = nsnull;
    const nsStylePadding* paddingData = nsnull;
    nsMargin border;
    nsMargin padding;
    nsSize size;
    switch(coord.GetUnit()) {
      case eStyleUnit_Coord:
        val->SetTwips(sign * coord.GetCoordValue());
        break;
      case eStyleUnit_Percent:
        container = GetContainingBlock(aFrame);
        if (container) {
          container->GetStyleData(eStyleStruct_Border,
                                  (const nsStyleStruct*&)borderData);
          if (borderData) {
            borderData->CalcBorderFor(container, border);
          }
          container->GetStyleData(eStyleStruct_Padding,
                                  (const nsStyleStruct*&)paddingData);
          if (paddingData) {
            paddingData->CalcPaddingFor(container, padding);
          }
          container->GetSize(size);
          if (aSide == NS_SIDE_LEFT || aSide == NS_SIDE_RIGHT) {
            val->SetTwips(sign * coord.GetPercentValue() *
                          (size.width - border.left - border.right -
                           padding.left - padding.right));
          } else {
            val->SetTwips(sign * coord.GetPercentValue() *
                          (size.height - border.top - border.bottom -
                           padding.top - padding.bottom));
          }
        } else {
          // XXX no containing block.
          val->SetTwips(0);
        }
        break;
      default:
        NS_WARNING("double check the unit");
        val->SetTwips(0);
        break;
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetStaticOffset(PRUint8 aSide, nsIFrame* aFrame,
                                    nsIDOMCSSValue** aValue)

{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStylePosition* positionData = nsnull;
  GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)positionData,
               aFrame);
  if (positionData) {
    nsStyleCoord coord;
    switch (aSide) {
      case NS_SIDE_TOP:
        positionData->mOffset.GetTop(coord);

        break;
      case NS_SIDE_RIGHT:
        positionData->mOffset.GetRight(coord);

        break;
      case NS_SIDE_BOTTOM:
        positionData->mOffset.GetBottom(coord);

        break;
      case NS_SIDE_LEFT:
        positionData->mOffset.GetLeft(coord);

        break;
      default:
        NS_WARNING("double check the side");

        break;
    }

    switch(coord.GetUnit()) {
      case eStyleUnit_Coord:
        val->SetTwips(coord.GetCoordValue());

        break;
      case eStyleUnit_Percent:
        val->SetPercent(coord.GetPercentValue());

        break;
      case eStyleUnit_Auto:
        val->SetIdent(NS_LITERAL_STRING("auto"));

        break;
      default:
        NS_WARNING("double check the unit");
        val->SetTwips(0);

        break;
    }
  }
  
  return CallQueryInterface(val, aValue);
}


nsIFrame*
nsComputedDOMStyle::GetContainingBlock(nsIFrame *aFrame)
{
  if (!aFrame) {
    // Tell caller that they have no containing block, seeing as they
    // are not being displayed

    return nsnull;
  }

  nsIFrame* container = aFrame;
  PRBool done = PR_FALSE;
  do {
    container->GetParent(&container);
    if (container) {
      (container)->IsPercentageBase(done);
    }
  } while (!done && container);

  NS_POSTCONDITION(container, "Frame has no containing block");
  
  return container;
}

nsresult
nsComputedDOMStyle::GetStyleData(nsStyleStructID aID,
                                 const nsStyleStruct*& aStyleStruct,
                                 nsIFrame* aFrame)
{
  if (aFrame && !mPseudo) {
    aFrame->GetStyleData(aID, aStyleStruct);
  } else if (mStyleContextHolder) {
    aStyleStruct = mStyleContextHolder->GetStyleData(aID);    
  } else {
    nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShellWeak);

    NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

    nsCOMPtr<nsIPresContext> pctx;
    presShell->GetPresContext(getter_AddRefs(pctx));
    if (pctx) {
      nsCOMPtr<nsIStyleContext> sctx;
      if (!mPseudo) {
        pctx->ResolveStyleContextFor(mContent, nsnull,
                                     getter_AddRefs(sctx));
      } else {
        pctx->ResolvePseudoStyleContextFor(mContent, mPseudo, nsnull,
                                           getter_AddRefs(sctx));
      }
      if (sctx) {
        aStyleStruct = sctx->GetStyleData(aID);
      }
      mStyleContextHolder = sctx;
    }
  }
  NS_ASSERTION(aStyleStruct, "Failed to get a style struct");

  return NS_OK;
}

nsresult
nsComputedDOMStyle::GetPaddingWidthFor(PRUint8 aSide, nsIFrame *aFrame,
                                       nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);
  
  nscoord width = GetPaddingWidthCoordFor(aSide, aFrame);
  val->SetTwips(width);

  return CallQueryInterface(val, aValue);
}

nscoord
nsComputedDOMStyle::GetPaddingWidthCoordFor(PRUint8 aSide, nsIFrame* aFrame)
{
  const nsStylePadding* paddingData = nsnull;
  GetStyleData(eStyleStruct_Padding, (const nsStyleStruct*&)paddingData,
               aFrame);

  if (paddingData) {
    nsMargin padding;
    paddingData->CalcPaddingFor(aFrame, padding);
    switch(aSide) {
      case NS_SIDE_TOP    :
        return padding.top;
      case NS_SIDE_BOTTOM :
        return padding.bottom;
      case NS_SIDE_LEFT   :
        return padding.left;
      case NS_SIDE_RIGHT  :
        return padding.right;
      default:
        NS_WARNING("double check the side");
        break;
    }
  }

  return 0;
}

nscoord
nsComputedDOMStyle::GetBorderWidthCoordFor(PRUint8 aSide, nsIFrame *aFrame)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleBorder* borderData = nsnull;
  GetStyleData(eStyleStruct_Border, (const nsStyleStruct*&)borderData, aFrame);

  if (borderData) {
    nsMargin border;
    borderData->CalcBorderFor(aFrame, border);
    switch(aSide) {
      case NS_SIDE_TOP    :
        return border.top;
      case NS_SIDE_BOTTOM :
        return border.bottom;
      case NS_SIDE_LEFT   :
        return border.left;
      case NS_SIDE_RIGHT  :
        return border.right;
      default:
        NS_WARNING("double check the side");
        break;
    }
  }

  return 0;
}

nsresult
nsComputedDOMStyle::GetLineHeightCoord(nsIFrame *aFrame,
                                       const nsStyleText *aText,
                                       nscoord& aCoord)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (aText) {
    const nsStyleFont *font = nsnull;
    GetStyleData(eStyleStruct_Font, (const nsStyleStruct*&)font, aFrame);
    switch (aText->mLineHeight.GetUnit()) {
      case eStyleUnit_Coord:
        aCoord = aText->mLineHeight.GetCoordValue();
        rv = NS_OK;
        break;
      case eStyleUnit_Percent:
        if (font) {
          aCoord = nscoord(aText->mLineHeight.GetPercentValue() * font->mSize);
          rv = NS_OK;
        }
        break;
      case eStyleUnit_Factor:
        if (font) {
          aCoord = nscoord(aText->mLineHeight.GetFactorValue() * font->mSize);
          rv = NS_OK;
        }
        break;
      default:
        break;
    }
  }

  if (NS_FAILED(rv))
    aCoord = 0;

  return rv;
}

nsresult
nsComputedDOMStyle::GetBorderColorsFor(PRUint8 aSide, nsIFrame *aFrame,
                                       nsIDOMCSSValue** aValue)
{
  const nsStyleBorder *border = nsnull;
  GetStyleData(eStyleStruct_Border, (const nsStyleStruct*&)border, aFrame);

  if (border && border->mBorderColors) {
    nsBorderColors* borderColors = nsnull;
    switch (aSide) {
      case NS_SIDE_TOP:
        borderColors = border->mBorderColors[0];
        break;
      case NS_SIDE_RIGHT:
        borderColors = border->mBorderColors[1];
        break;
      case NS_SIDE_BOTTOM:
        borderColors = border->mBorderColors[2];
        break;
      case NS_SIDE_LEFT:
        borderColors = border->mBorderColors[3];
        break;
      default:
        NS_WARNING("double check the side");
        break;
    }

    if (borderColors) {
      nsDOMCSSValueList *valueList = GetROCSSValueList(PR_FALSE);
      NS_ENSURE_TRUE(valueList, NS_ERROR_OUT_OF_MEMORY);

      do {
        nsROCSSPrimitiveValue *primitive = GetROCSSPrimitiveValue();
        if (!primitive) {
          delete valueList;

          return NS_ERROR_OUT_OF_MEMORY;
        }
        nsDOMCSSRGBColor *rgb = nsnull;
        rgb = GetDOMCSSRGBColor(borderColors->mColor);
        if (rgb) {
          primitive->SetColor(rgb);
        } else {
          delete valueList;
          delete primitive;

          return NS_ERROR_OUT_OF_MEMORY;
        }

        nsresult rv = valueList->AppendCSSValue(primitive);
        if (NS_FAILED(rv)) {
          delete valueList;
          delete primitive;

          return NS_ERROR_OUT_OF_MEMORY;
        }
        borderColors = borderColors->mNext;
      } while (borderColors);

      return CallQueryInterface(valueList, aValue);
    }
  }

  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(NS_LITERAL_STRING("none"));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderRadiusFor(PRUint8 aSide, nsIFrame *aFrame,
                                       nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleBorder *border = nsnull;
  GetStyleData(eStyleStruct_Border, (const nsStyleStruct*&)border, aFrame);

  if (border) {
    nsStyleCoord coord;
    switch (aSide) {
      case NS_SIDE_TOP:
        border->mBorderRadius.GetTop(coord);
        break;
      case NS_SIDE_BOTTOM:
        border->mBorderRadius.GetBottom(coord);
        break;
      case NS_SIDE_LEFT:
        border->mBorderRadius.GetLeft(coord);
        break;
      case NS_SIDE_RIGHT:
        border->mBorderRadius.GetRight(coord);
        break;
      default:
        NS_WARNING("double check the side");
        break;
    }

    switch (coord.GetUnit()) {
      case eStyleUnit_Coord:
        val->SetTwips(coord.GetCoordValue());
        break;
      case eStyleUnit_Percent:
        if (aFrame) {
          nsSize size;
          aFrame->GetSize(size);
          val->SetTwips(coord.GetPercentValue() * size.width);
        } else {
          val->SetPercent(coord.GetPercentValue());
        }
        break;
      case eStyleUnit_Inherit:
        // XXX This will only happen if we are inheriting from
        // a node with a percentage style unit for its relevant
        // border radius property. Layout currently drops this
        // one inherit case, so we do the same thing here.
        val->SetIdent(NS_LITERAL_STRING("inherit"));
        break;
      default:
#ifdef DEBUG_ComputedDOMStyle
        NS_WARNING("double check the border radius");
#endif
        break;
    }
  } else {
    val->SetTwips(0);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderWidthFor(PRUint8 aSide, nsIFrame *aFrame,
                                      nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleBorder* border = nsnull;
  GetStyleData(eStyleStruct_Border, (const nsStyleStruct*&)border, aFrame);

  if (border) {
    nsStyleCoord coord;
    PRUint8 borderStyle = border->GetBorderStyle(aSide);
    if (borderStyle == NS_STYLE_BORDER_STYLE_NONE) {
      coord.SetCoordValue(0);
    } else {
      switch(aSide) {
        case NS_SIDE_TOP:
          border->mBorder.GetTop(coord); break;
        case NS_SIDE_BOTTOM :
          border->mBorder.GetBottom(coord); break;
        case NS_SIDE_LEFT :
          border->mBorder.GetLeft(coord); break;
        case NS_SIDE_RIGHT :
          border->mBorder.GetRight(coord); break;
        default:
          NS_WARNING("double check the side");
          break;
      }
   }

    switch(coord.GetUnit()) {
      case eStyleUnit_Coord:
        val->SetTwips(coord.GetCoordValue());  break;
      case eStyleUnit_Integer:
      case eStyleUnit_Proportional:
      case eStyleUnit_Enumerated:
      case eStyleUnit_Chars:
        {
          const nsAFlatCString& width=
            nsCSSProps::SearchKeywordTable(coord.GetIntValue(),
                                           nsCSSProps::kBorderWidthKTable);
          val->SetIdent(width);
          break;
        }
      default:
        NS_WARNING("double check the unit");
        break;
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderColorFor(PRUint8 aSide, nsIFrame *aFrame,
                                      nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleBorder* border = nsnull;
  GetStyleData(eStyleStruct_Border, (const nsStyleStruct*&)border, aFrame);

  if (border) {
    nscolor color; 
    PRBool transparent;
    PRBool foreground;
    border->GetBorderColor(aSide, color, transparent, foreground);
    if (foreground) {
      const nsStyleColor* colorStruct = nsnull;
      GetStyleData(eStyleStruct_Color, (const nsStyleStruct*&)colorStruct,
                   aFrame);
      color = colorStruct->mColor;
    }

    nsDOMCSSRGBColor *rgb = nsnull;
    rgb = GetDOMCSSRGBColor(color);
    if (!rgb) {
      delete val;

      return NS_ERROR_OUT_OF_MEMORY;
    }

    val->SetColor(rgb);
  } else {
    val->SetString(NS_LITERAL_STRING(""));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetMarginWidthFor(PRUint8 aSide, nsIFrame *aFrame,
                                      nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  nscoord width = GetMarginWidthCoordFor(aSide, aFrame);
  val->SetTwips(width);

  return CallQueryInterface(val, aValue);
}

nscoord
nsComputedDOMStyle::GetMarginWidthCoordFor(PRUint8 aSide, nsIFrame *aFrame)
{
  const nsStyleMargin* marginData = nsnull;
  GetStyleData(eStyleStruct_Margin, (const nsStyleStruct*&)marginData, aFrame);
  if (marginData) {
    nsMargin margin;
    marginData->CalcMarginFor(aFrame, margin);
    switch(aSide) {
      case NS_SIDE_TOP    :
        return margin.top;
      case NS_SIDE_BOTTOM :
        return margin.bottom;
      case NS_SIDE_LEFT   :
        return margin.left;
      case NS_SIDE_RIGHT  :
        return margin.right;
      default:
        NS_WARNING("double check the side");
        break;
    }
  }

  return 0;
}

nsresult
nsComputedDOMStyle::GetBorderStyleFor(PRUint8 aSide, nsIFrame *aFrame,
                                      nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleBorder* border = nsnull;
  GetStyleData(eStyleStruct_Border, (const nsStyleStruct*&)border, aFrame);

  PRUint8 borderStyle = NS_STYLE_BORDER_STYLE_NONE;

  if (border) {
    borderStyle = border->GetBorderStyle(aSide);
  }

  if (borderStyle != NS_STYLE_BORDER_STYLE_NONE) {
    const nsAFlatCString& style=
      nsCSSProps::SearchKeywordTable(borderStyle,
                                     nsCSSProps::kBorderStyleKTable);
    val->SetIdent(style);
  } else {
    val->SetIdent(NS_LITERAL_STRING("none"));
  }

  return CallQueryInterface(val, aValue);
}
