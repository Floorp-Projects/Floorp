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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsCSSProps.h"
#include "nsCSSKeywords.h"
#include "nsStyleConsts.h"
#include "nsThemeConstants.h"  // For system widget appearance types

#include "nsILookAndFeel.h" // for system colors

#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsStaticNameTable.h"

// required to make the symbol external, so that TestCSSPropertyLookup.cpp can link with it
extern const char* const kCSSRawProperties[];

// define an array of all CSS properties
const char* const kCSSRawProperties[] = {
#define CSS_PROP(name_, id_, method_, datastruct_, member_, type_, iscoord_, kwtable_) #name_,
#include "nsCSSPropList.h"
#undef CSS_PROP
#define CSS_PROP_SHORTHAND(name_, id_, method_) #name_,
#include "nsCSSPropList.h"
#undef CSS_PROP_SHORTHAND
};


static PRInt32 gTableRefCount;
static nsStaticCaseInsensitiveNameTable* gPropertyTable;

void
nsCSSProps::AddRefTable(void) 
{
  if (0 == gTableRefCount++) {
    NS_ASSERTION(!gPropertyTable, "pre existing array!");
    gPropertyTable = new nsStaticCaseInsensitiveNameTable();
    if (gPropertyTable) {
#ifdef DEBUG
    {
      // let's verify the table...
      for (PRInt32 index = 0; index < eCSSProperty_COUNT; ++index) {
        nsCAutoString temp1(kCSSRawProperties[index]);
        nsCAutoString temp2(kCSSRawProperties[index]);
        ToLowerCase(temp1);
        NS_ASSERTION(temp1.Equals(temp2), "upper case char in table");
        NS_ASSERTION(-1 == temp1.FindChar('_'), "underscore char in table");
      }
    }
#endif      
      gPropertyTable->Init(kCSSRawProperties, eCSSProperty_COUNT); 
    }
  }
}

void
nsCSSProps::ReleaseTable(void) 
{
  if (0 == --gTableRefCount) {
    if (gPropertyTable) {
      delete gPropertyTable;
      gPropertyTable = nsnull;
    }
  }
}

struct CSSPropertyAlias {
  char name[13];
  nsCSSProperty id;
};

static const CSSPropertyAlias gAliases[] = {
  { "-moz-opacity", eCSSProperty_opacity }
};

nsCSSProperty 
nsCSSProps::LookupProperty(const nsACString& aProperty)
{
  NS_ASSERTION(gPropertyTable, "no lookup table, needs addref");

  nsCSSProperty res = nsCSSProperty(gPropertyTable->Lookup(aProperty));
  if (res == eCSSProperty_UNKNOWN) {
    const nsCString& prop = PromiseFlatCString(aProperty);
    for (const CSSPropertyAlias *alias = gAliases,
                            *alias_end = gAliases + NS_ARRAY_LENGTH(gAliases);
         alias < alias_end; ++alias)
      if (nsCRT::strcasecmp(prop.get(), alias->name) == 0) {
        res = alias->id;
        break;
      }
  }
  return res;
}

nsCSSProperty 
nsCSSProps::LookupProperty(const nsAString& aProperty) {
  // This is faster than converting and calling
  // LookupProperty(nsACString&).  The table will do its own
  // converting and avoid a PromiseFlatCString() call.
  NS_ASSERTION(gPropertyTable, "no lookup table, needs addref");
  nsCSSProperty res = nsCSSProperty(gPropertyTable->Lookup(aProperty));
  if (res == eCSSProperty_UNKNOWN) {
    NS_ConvertUTF16toUTF8 prop(aProperty);
    for (const CSSPropertyAlias *alias = gAliases,
                            *alias_end = gAliases + NS_ARRAY_LENGTH(gAliases);
         alias < alias_end; ++alias)
      if (nsCRT::strcasecmp(prop.get(), alias->name) == 0) {
        res = alias->id;
        break;
      }
  }
  return res;
}

const nsAFlatCString& 
nsCSSProps::GetStringValue(nsCSSProperty aProperty)
{
  NS_ASSERTION(gPropertyTable, "no lookup table, needs addref");
  if (gPropertyTable) {
    return gPropertyTable->GetStringValue(PRInt32(aProperty));
  } else {
    static nsDependentCString sNullStr("");
    return sNullStr;
  }
}


/***************************************************************************/

const PRInt32 nsCSSProps::kAppearanceKTable[] = {
  eCSSKeyword_none,                   NS_THEME_NONE,
  eCSSKeyword_button,                 NS_THEME_BUTTON,
  eCSSKeyword_radio,                  NS_THEME_RADIO,
  eCSSKeyword_checkbox,               NS_THEME_CHECKBOX,
  eCSSKeyword_radio_small,            NS_THEME_RADIO_SMALL,
  eCSSKeyword_checkbox_small,         NS_THEME_CHECKBOX_SMALL,
  eCSSKeyword_button_small,           NS_THEME_BUTTON_SMALL,
  eCSSKeyword_button_bevel,           NS_THEME_BUTTON_BEVEL,
  eCSSKeyword_toolbox,                NS_THEME_TOOLBOX,
  eCSSKeyword_toolbar,                NS_THEME_TOOLBAR,
  eCSSKeyword_toolbarbutton,          NS_THEME_TOOLBAR_BUTTON,
  eCSSKeyword_toolbargripper,         NS_THEME_TOOLBAR_GRIPPER,
  eCSSKeyword_dualbutton,             NS_THEME_TOOLBAR_DUAL_BUTTON,
  eCSSKeyword_dualbutton_dropdown,    NS_THEME_TOOLBAR_DUAL_BUTTON_DROPDOWN,
  eCSSKeyword_separator,              NS_THEME_TOOLBAR_SEPARATOR,
  eCSSKeyword_statusbar,              NS_THEME_STATUSBAR,
  eCSSKeyword_statusbarpanel,         NS_THEME_STATUSBAR_PANEL,
  eCSSKeyword_resizerpanel,           NS_THEME_STATUSBAR_RESIZER_PANEL,
  eCSSKeyword_resizer,                NS_THEME_RESIZER,
  eCSSKeyword_listbox,                NS_THEME_LISTBOX,
  eCSSKeyword_listitem,               NS_THEME_LISTBOX_LISTITEM,
  eCSSKeyword_treeview,               NS_THEME_TREEVIEW,
  eCSSKeyword_treeitem,               NS_THEME_TREEVIEW_TREEITEM,
  eCSSKeyword_treetwisty,             NS_THEME_TREEVIEW_TWISTY,
  eCSSKeyword_treetwistyopen,         NS_THEME_TREEVIEW_TWISTY_OPEN,
  eCSSKeyword_treeline,               NS_THEME_TREEVIEW_LINE,
  eCSSKeyword_treeheader,             NS_THEME_TREEVIEW_HEADER,
  eCSSKeyword_treeheadercell,         NS_THEME_TREEVIEW_HEADER_CELL,
  eCSSKeyword_treeheadersortarrow,    NS_THEME_TREEVIEW_HEADER_SORTARROW,
  eCSSKeyword_progressbar,            NS_THEME_PROGRESSBAR,
  eCSSKeyword_progresschunk,          NS_THEME_PROGRESSBAR_CHUNK,
  eCSSKeyword_progressbar_vertical,   NS_THEME_PROGRESSBAR_VERTICAL,
  eCSSKeyword_progresschunk_vertical, NS_THEME_PROGRESSBAR_CHUNK_VERTICAL,
  eCSSKeyword_tab,                    NS_THEME_TAB,
  eCSSKeyword_tab_left_edge,          NS_THEME_TAB_LEFT_EDGE,
  eCSSKeyword_tab_right_edge,         NS_THEME_TAB_RIGHT_EDGE,
  eCSSKeyword_tabpanels,              NS_THEME_TAB_PANELS,
  eCSSKeyword_tabpanel,               NS_THEME_TAB_PANEL,
  eCSSKeyword_tooltip,                NS_THEME_TOOLTIP,
  eCSSKeyword_spinner,                NS_THEME_SPINNER,
  eCSSKeyword_spinner_upbutton,       NS_THEME_SPINNER_UP_BUTTON,
  eCSSKeyword_spinner_downbutton,     NS_THEME_SPINNER_DOWN_BUTTON,
  eCSSKeyword_scrollbar,              NS_THEME_SCROLLBAR,
  eCSSKeyword_scrollbarbutton_up,     NS_THEME_SCROLLBAR_BUTTON_UP,
  eCSSKeyword_scrollbarbutton_down,   NS_THEME_SCROLLBAR_BUTTON_DOWN,
  eCSSKeyword_scrollbarbutton_left,   NS_THEME_SCROLLBAR_BUTTON_LEFT,
  eCSSKeyword_scrollbarbutton_right,  NS_THEME_SCROLLBAR_BUTTON_RIGHT,
  eCSSKeyword_scrollbartrack_horizontal,    NS_THEME_SCROLLBAR_TRACK_HORIZONTAL,
  eCSSKeyword_scrollbartrack_vertical,      NS_THEME_SCROLLBAR_TRACK_VERTICAL,
  eCSSKeyword_scrollbarthumb_horizontal,    NS_THEME_SCROLLBAR_THUMB_HORIZONTAL,
  eCSSKeyword_scrollbarthumb_vertical,      NS_THEME_SCROLLBAR_THUMB_VERTICAL,
  eCSSKeyword_scrollbargripper_horizontal,  NS_THEME_SCROLLBAR_GRIPPER_HORIZONTAL,
  eCSSKeyword_scrollbargripper_vertical,    NS_THEME_SCROLLBAR_GRIPPER_VERTICAL,
  eCSSKeyword_textfield,              NS_THEME_TEXTFIELD,
  eCSSKeyword_caret,                  NS_THEME_TEXTFIELD_CARET,
  eCSSKeyword_menulist,               NS_THEME_DROPDOWN,
  eCSSKeyword_menulistbutton,         NS_THEME_DROPDOWN_BUTTON,
  eCSSKeyword_menulisttext,           NS_THEME_DROPDOWN_TEXT,
  eCSSKeyword_menulisttextfield,      NS_THEME_DROPDOWN_TEXTFIELD,
  eCSSKeyword_slider,                 NS_THEME_SLIDER,
  eCSSKeyword_sliderthumb,            NS_THEME_SLIDER_THUMB,
  eCSSKeyword_sliderthumbstart,       NS_THEME_SLIDER_THUMB_START,
  eCSSKeyword_sliderthumbend,         NS_THEME_SLIDER_THUMB_END,
  eCSSKeyword_sliderthumbtick,        NS_THEME_SLIDER_TICK,
  eCSSKeyword_checkboxcontainer,      NS_THEME_CHECKBOX_CONTAINER,
  eCSSKeyword_radiocontainer,         NS_THEME_RADIO_CONTAINER,
  eCSSKeyword_checkboxlabel,          NS_THEME_CHECKBOX_LABEL,
  eCSSKeyword_radiolabel,             NS_THEME_RADIO_LABEL,
  eCSSKeyword_buttonfocus,            NS_THEME_BUTTON_FOCUS,
  eCSSKeyword_window,                 NS_THEME_WINDOW,
  eCSSKeyword_dialog,                 NS_THEME_DIALOG,
  eCSSKeyword_menubar,                NS_THEME_MENUBAR,
  eCSSKeyword_menupopup,              NS_THEME_MENUPOPUP,
  eCSSKeyword_menuitem,               NS_THEME_MENUITEM,
  -1,-1
};

// Keyword id tables for variant/enum parsing
const PRInt32 nsCSSProps::kAzimuthKTable[] = {
  eCSSKeyword_left_side,    NS_STYLE_AZIMUTH_LEFT_SIDE,
  eCSSKeyword_far_left,     NS_STYLE_AZIMUTH_FAR_LEFT,
  eCSSKeyword_left,         NS_STYLE_AZIMUTH_LEFT,
  eCSSKeyword_center_left,  NS_STYLE_AZIMUTH_CENTER_LEFT,
  eCSSKeyword_center,       NS_STYLE_AZIMUTH_CENTER,
  eCSSKeyword_center_right, NS_STYLE_AZIMUTH_CENTER_RIGHT,
  eCSSKeyword_right,        NS_STYLE_AZIMUTH_RIGHT,
  eCSSKeyword_far_right,    NS_STYLE_AZIMUTH_FAR_RIGHT,
  eCSSKeyword_right_side,   NS_STYLE_AZIMUTH_RIGHT_SIDE,
  eCSSKeyword_behind,       NS_STYLE_AZIMUTH_BEHIND,
  eCSSKeyword_leftwards,    NS_STYLE_AZIMUTH_LEFTWARDS,
  eCSSKeyword_rightwards,   NS_STYLE_AZIMUTH_RIGHTWARDS,
  -1,-1
};

const PRInt32 nsCSSProps::kBackgroundAttachmentKTable[] = {
  eCSSKeyword_fixed, NS_STYLE_BG_ATTACHMENT_FIXED,
  eCSSKeyword_scroll, NS_STYLE_BG_ATTACHMENT_SCROLL,
  -1,-1
};

const PRInt32 nsCSSProps::kBackgroundColorKTable[] = {
  eCSSKeyword_transparent, NS_STYLE_BG_COLOR_TRANSPARENT,
  -1,-1
};

const PRInt32 nsCSSProps::kBackgroundClipKTable[] = {
  eCSSKeyword_border,     NS_STYLE_BG_CLIP_BORDER,
  eCSSKeyword_padding,    NS_STYLE_BG_CLIP_PADDING,
  -1,-1
};

const PRInt32 nsCSSProps::kBackgroundInlinePolicyKTable[] = {
  eCSSKeyword_each_box,     NS_STYLE_BG_INLINE_POLICY_EACH_BOX,
  eCSSKeyword_continuous,   NS_STYLE_BG_INLINE_POLICY_CONTINUOUS,
  eCSSKeyword_bounding_box, NS_STYLE_BG_INLINE_POLICY_BOUNDING_BOX,
  -1,-1
};

const PRInt32 nsCSSProps::kBackgroundOriginKTable[] = {
  eCSSKeyword_border,     NS_STYLE_BG_ORIGIN_BORDER,
  eCSSKeyword_padding,    NS_STYLE_BG_ORIGIN_PADDING,
  eCSSKeyword_content,    NS_STYLE_BG_ORIGIN_CONTENT,
  -1,-1
};

const PRInt32 nsCSSProps::kBackgroundRepeatKTable[] = {
  eCSSKeyword_no_repeat,  NS_STYLE_BG_REPEAT_OFF,
  eCSSKeyword_repeat,     NS_STYLE_BG_REPEAT_XY,
  eCSSKeyword_repeat_x,   NS_STYLE_BG_REPEAT_X,
  eCSSKeyword_repeat_y,   NS_STYLE_BG_REPEAT_Y,
  -1,-1
};

const PRInt32 nsCSSProps::kBackgroundXPositionKTable[] = {
  eCSSKeyword_left,   0,
  eCSSKeyword_center, 50,
  eCSSKeyword_right,  100,
  -1,-1
};

const PRInt32 nsCSSProps::kBackgroundYPositionKTable[] = {
  eCSSKeyword_top,    0,
  eCSSKeyword_center, 50,
  eCSSKeyword_bottom, 100,
  -1,-1
};

const PRInt32 nsCSSProps::kBorderCollapseKTable[] = {
  eCSSKeyword_collapse,  NS_STYLE_BORDER_COLLAPSE,
  eCSSKeyword_separate,  NS_STYLE_BORDER_SEPARATE,
  -1,-1
};

const PRInt32 nsCSSProps::kBorderColorKTable[] = {
  eCSSKeyword_transparent, NS_STYLE_COLOR_TRANSPARENT,
  eCSSKeyword__moz_use_text_color, NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR,
  -1,-1
};

const PRInt32 nsCSSProps::kBorderStyleKTable[] = {
  eCSSKeyword_hidden, NS_STYLE_BORDER_STYLE_HIDDEN,
  eCSSKeyword_dotted, NS_STYLE_BORDER_STYLE_DOTTED,
  eCSSKeyword_dashed, NS_STYLE_BORDER_STYLE_DASHED,
  eCSSKeyword_solid,  NS_STYLE_BORDER_STYLE_SOLID,
  eCSSKeyword_double, NS_STYLE_BORDER_STYLE_DOUBLE,
  eCSSKeyword_groove, NS_STYLE_BORDER_STYLE_GROOVE,
  eCSSKeyword_ridge,  NS_STYLE_BORDER_STYLE_RIDGE,
  eCSSKeyword_inset,  NS_STYLE_BORDER_STYLE_INSET,
  eCSSKeyword_outset, NS_STYLE_BORDER_STYLE_OUTSET,
  eCSSKeyword__moz_bg_inset,  NS_STYLE_BORDER_STYLE_BG_INSET,
  eCSSKeyword__moz_bg_outset, NS_STYLE_BORDER_STYLE_BG_OUTSET,
  eCSSKeyword__moz_bg_solid,  NS_STYLE_BORDER_STYLE_BG_SOLID,
  -1,-1
};

const PRInt32 nsCSSProps::kBorderWidthKTable[] = {
  eCSSKeyword_thin, NS_STYLE_BORDER_WIDTH_THIN,
  eCSSKeyword_medium, NS_STYLE_BORDER_WIDTH_MEDIUM,
  eCSSKeyword_thick, NS_STYLE_BORDER_WIDTH_THICK,
  -1,-1
};

const PRInt32 nsCSSProps::kBoxPropSourceKTable[] = {
  eCSSKeyword_physical,     NS_BOXPROP_SOURCE_PHYSICAL,
  eCSSKeyword_logical,      NS_BOXPROP_SOURCE_LOGICAL,
  -1,-1
};

const PRInt32 nsCSSProps::kBoxSizingKTable[] = {
  eCSSKeyword_content_box,  NS_STYLE_BOX_SIZING_CONTENT,
  eCSSKeyword_border_box,   NS_STYLE_BOX_SIZING_BORDER,
  eCSSKeyword_padding_box,  NS_STYLE_BOX_SIZING_PADDING,
  -1,-1
};

const PRInt32 nsCSSProps::kCaptionSideKTable[] = {
  eCSSKeyword_top,    NS_SIDE_TOP,
  eCSSKeyword_right,  NS_SIDE_RIGHT,
  eCSSKeyword_bottom, NS_SIDE_BOTTOM,
  eCSSKeyword_left,   NS_SIDE_LEFT,
  -1,-1
};

const PRInt32 nsCSSProps::kClearKTable[] = {
  eCSSKeyword_left, NS_STYLE_CLEAR_LEFT,
  eCSSKeyword_right, NS_STYLE_CLEAR_RIGHT,
  eCSSKeyword_both, NS_STYLE_CLEAR_LEFT_AND_RIGHT,
  -1,-1
};

const PRInt32 nsCSSProps::kColorKTable[] = {
  eCSSKeyword_activeborder, nsILookAndFeel::eColor_activeborder,
  eCSSKeyword_activecaption, nsILookAndFeel::eColor_activecaption,
  eCSSKeyword_appworkspace, nsILookAndFeel::eColor_appworkspace,
  eCSSKeyword_background, nsILookAndFeel::eColor_background,
  eCSSKeyword_buttonface, nsILookAndFeel::eColor_buttonface,
  eCSSKeyword_buttonhighlight, nsILookAndFeel::eColor_buttonhighlight,
  eCSSKeyword_buttonshadow, nsILookAndFeel::eColor_buttonshadow,
  eCSSKeyword_buttontext, nsILookAndFeel::eColor_buttontext,
  eCSSKeyword_captiontext, nsILookAndFeel::eColor_captiontext,
  eCSSKeyword_graytext, nsILookAndFeel::eColor_graytext,
  eCSSKeyword_highlight, nsILookAndFeel::eColor_highlight,
  eCSSKeyword_highlighttext, nsILookAndFeel::eColor_highlighttext,
  eCSSKeyword_inactiveborder, nsILookAndFeel::eColor_inactiveborder,
  eCSSKeyword_inactivecaption, nsILookAndFeel::eColor_inactivecaption,
  eCSSKeyword_inactivecaptiontext, nsILookAndFeel::eColor_inactivecaptiontext,
  eCSSKeyword_infobackground, nsILookAndFeel::eColor_infobackground,
  eCSSKeyword_infotext, nsILookAndFeel::eColor_infotext,
  eCSSKeyword_menu, nsILookAndFeel::eColor_menu,
  eCSSKeyword_menutext, nsILookAndFeel::eColor_menutext,
  eCSSKeyword_scrollbar, nsILookAndFeel::eColor_scrollbar,
  eCSSKeyword_threeddarkshadow, nsILookAndFeel::eColor_threeddarkshadow,
  eCSSKeyword_threedface, nsILookAndFeel::eColor_threedface,
  eCSSKeyword_threedhighlight, nsILookAndFeel::eColor_threedhighlight,
  eCSSKeyword_threedlightshadow, nsILookAndFeel::eColor_threedlightshadow,
  eCSSKeyword_threedshadow, nsILookAndFeel::eColor_threedshadow,
  eCSSKeyword_window, nsILookAndFeel::eColor_window,
  eCSSKeyword_windowframe, nsILookAndFeel::eColor_windowframe,
  eCSSKeyword_windowtext, nsILookAndFeel::eColor_windowtext,
  eCSSKeyword__moz_activehyperlinktext, NS_COLOR_MOZ_ACTIVEHYPERLINKTEXT,
  eCSSKeyword__moz_buttondefault, nsILookAndFeel::eColor__moz_buttondefault,
  eCSSKeyword__moz_field, nsILookAndFeel::eColor__moz_field,
  eCSSKeyword__moz_fieldtext, nsILookAndFeel::eColor__moz_fieldtext,
  eCSSKeyword__moz_dialog, nsILookAndFeel::eColor__moz_dialog,
  eCSSKeyword__moz_dialogtext, nsILookAndFeel::eColor__moz_dialogtext,
  eCSSKeyword__moz_dragtargetzone, nsILookAndFeel::eColor__moz_dragtargetzone,
  eCSSKeyword__moz_hyperlinktext, NS_COLOR_MOZ_HYPERLINKTEXT,
  eCSSKeyword__moz_gtk2_hovertext, nsILookAndFeel::eColor__moz_gtk2_hovertext,
  eCSSKeyword__moz_mac_focusring, nsILookAndFeel::eColor__moz_mac_focusring,
  eCSSKeyword__moz_mac_menuselect, nsILookAndFeel::eColor__moz_mac_menuselect,
  eCSSKeyword__moz_mac_menushadow, nsILookAndFeel::eColor__moz_mac_menushadow,
  eCSSKeyword__moz_mac_menutextselect, nsILookAndFeel::eColor__moz_mac_menutextselect,
  eCSSKeyword__moz_mac_accentlightesthighlight, nsILookAndFeel::eColor__moz_mac_accentlightesthighlight,
  eCSSKeyword__moz_mac_accentregularhighlight, nsILookAndFeel::eColor__moz_mac_accentregularhighlight,
  eCSSKeyword__moz_mac_accentface, nsILookAndFeel::eColor__moz_mac_accentface,
  eCSSKeyword__moz_mac_accentlightshadow, nsILookAndFeel::eColor__moz_mac_accentlightshadow,
  eCSSKeyword__moz_mac_accentregularshadow, nsILookAndFeel::eColor__moz_mac_accentregularshadow,
  eCSSKeyword__moz_mac_accentdarkshadow, nsILookAndFeel::eColor__moz_mac_accentdarkshadow,
  eCSSKeyword__moz_mac_accentdarkestshadow, nsILookAndFeel::eColor__moz_mac_accentdarkestshadow,
  eCSSKeyword__moz_visitedhyperlinktext, NS_COLOR_MOZ_VISITEDHYPERLINKTEXT,
  -1,-1
};

const PRInt32 nsCSSProps::kContentKTable[] = {
  eCSSKeyword_open_quote, NS_STYLE_CONTENT_OPEN_QUOTE,
  eCSSKeyword_close_quote, NS_STYLE_CONTENT_CLOSE_QUOTE,
  eCSSKeyword_no_open_quote, NS_STYLE_CONTENT_NO_OPEN_QUOTE,
  eCSSKeyword_no_close_quote, NS_STYLE_CONTENT_NO_CLOSE_QUOTE,
  -1,-1
};

const PRInt32 nsCSSProps::kCursorKTable[] = {
  eCSSKeyword_crosshair, NS_STYLE_CURSOR_CROSSHAIR,
  eCSSKeyword_default, NS_STYLE_CURSOR_DEFAULT,
  eCSSKeyword_pointer, NS_STYLE_CURSOR_POINTER,
  eCSSKeyword_move, NS_STYLE_CURSOR_MOVE,
  eCSSKeyword_e_resize, NS_STYLE_CURSOR_E_RESIZE,
  eCSSKeyword_ne_resize, NS_STYLE_CURSOR_NE_RESIZE,
  eCSSKeyword_nw_resize, NS_STYLE_CURSOR_NW_RESIZE,
  eCSSKeyword_n_resize, NS_STYLE_CURSOR_N_RESIZE,
  eCSSKeyword_se_resize, NS_STYLE_CURSOR_SE_RESIZE,
  eCSSKeyword_sw_resize, NS_STYLE_CURSOR_SW_RESIZE,
  eCSSKeyword_s_resize, NS_STYLE_CURSOR_S_RESIZE,
  eCSSKeyword_w_resize, NS_STYLE_CURSOR_W_RESIZE,
  eCSSKeyword_text, NS_STYLE_CURSOR_TEXT,
  eCSSKeyword_wait, NS_STYLE_CURSOR_WAIT,
  eCSSKeyword_help, NS_STYLE_CURSOR_HELP,
  // CSS 2.1
  eCSSKeyword_progress, NS_STYLE_CURSOR_SPINNING,
  // CSS3 proposed
  eCSSKeyword__moz_copy, NS_STYLE_CURSOR_COPY,
  eCSSKeyword__moz_alias, NS_STYLE_CURSOR_ALIAS,
  eCSSKeyword__moz_context_menu, NS_STYLE_CURSOR_CONTEXT_MENU,
  eCSSKeyword__moz_cell, NS_STYLE_CURSOR_CELL,
  eCSSKeyword__moz_grab, NS_STYLE_CURSOR_GRAB,
  eCSSKeyword__moz_grabbing, NS_STYLE_CURSOR_GRABBING,
  eCSSKeyword__moz_spinning, NS_STYLE_CURSOR_SPINNING,
  eCSSKeyword__moz_count_up, NS_STYLE_CURSOR_COUNT_UP,
  eCSSKeyword__moz_count_down, NS_STYLE_CURSOR_COUNT_DOWN,
  eCSSKeyword__moz_count_up_down, NS_STYLE_CURSOR_COUNT_UP_DOWN,
  eCSSKeyword__moz_zoom_in, NS_STYLE_CURSOR_MOZ_ZOOM_IN,
  eCSSKeyword__moz_zoom_out, NS_STYLE_CURSOR_MOZ_ZOOM_OUT,
  -1,-1
};

const PRInt32 nsCSSProps::kDirectionKTable[] = {
  eCSSKeyword_ltr,      NS_STYLE_DIRECTION_LTR,
  eCSSKeyword_rtl,      NS_STYLE_DIRECTION_RTL,
  -1,-1
};

const PRInt32 nsCSSProps::kDisplayKTable[] = {
  eCSSKeyword_inline,             NS_STYLE_DISPLAY_INLINE,
  eCSSKeyword_block,              NS_STYLE_DISPLAY_BLOCK,
  eCSSKeyword__moz_inline_block,  NS_STYLE_DISPLAY_INLINE_BLOCK,
  eCSSKeyword_list_item,          NS_STYLE_DISPLAY_LIST_ITEM,
  eCSSKeyword__moz_run_in,        NS_STYLE_DISPLAY_RUN_IN,
  eCSSKeyword__moz_compact,       NS_STYLE_DISPLAY_COMPACT,
  eCSSKeyword__moz_marker,        NS_STYLE_DISPLAY_MARKER,
  eCSSKeyword_table,              NS_STYLE_DISPLAY_TABLE,
  eCSSKeyword__moz_inline_table,  NS_STYLE_DISPLAY_INLINE_TABLE,
  eCSSKeyword_table_row_group,    NS_STYLE_DISPLAY_TABLE_ROW_GROUP,
  eCSSKeyword_table_header_group, NS_STYLE_DISPLAY_TABLE_HEADER_GROUP,
  eCSSKeyword_table_footer_group, NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP,
  eCSSKeyword_table_row,          NS_STYLE_DISPLAY_TABLE_ROW,
  eCSSKeyword_table_column_group, NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP,
  eCSSKeyword_table_column,       NS_STYLE_DISPLAY_TABLE_COLUMN,
  eCSSKeyword_table_cell,         NS_STYLE_DISPLAY_TABLE_CELL,
  eCSSKeyword_table_caption,      NS_STYLE_DISPLAY_TABLE_CAPTION,
  eCSSKeyword__moz_box,           NS_STYLE_DISPLAY_BOX,
  eCSSKeyword__moz_inline_box,    NS_STYLE_DISPLAY_INLINE_BOX,
  eCSSKeyword__moz_grid,          NS_STYLE_DISPLAY_GRID,
  eCSSKeyword__moz_inline_grid,   NS_STYLE_DISPLAY_INLINE_GRID,
  eCSSKeyword__moz_grid_group,    NS_STYLE_DISPLAY_GRID_GROUP,
  eCSSKeyword__moz_grid_line,     NS_STYLE_DISPLAY_GRID_LINE,
  eCSSKeyword__moz_stack,         NS_STYLE_DISPLAY_STACK,
  eCSSKeyword__moz_inline_stack,  NS_STYLE_DISPLAY_INLINE_STACK,
  eCSSKeyword__moz_deck,          NS_STYLE_DISPLAY_DECK,
  eCSSKeyword__moz_bulletinboard, NS_STYLE_DISPLAY_BULLETINBOARD,
  eCSSKeyword__moz_popup,         NS_STYLE_DISPLAY_POPUP,
  eCSSKeyword__moz_groupbox,      NS_STYLE_DISPLAY_GROUPBOX,
  eCSSKeyword__moz_page_break,    NS_STYLE_DISPLAY_PAGE_BREAK,
  -1,-1
};

const PRInt32 nsCSSProps::kElevationKTable[] = {
  eCSSKeyword_below,  NS_STYLE_ELEVATION_BELOW,
  eCSSKeyword_level,  NS_STYLE_ELEVATION_LEVEL,
  eCSSKeyword_above,  NS_STYLE_ELEVATION_ABOVE,
  eCSSKeyword_higher, NS_STYLE_ELEVATION_HIGHER,
  eCSSKeyword_lower,  NS_STYLE_ELEVATION_LOWER,
  -1,-1
};

const PRInt32 nsCSSProps::kEmptyCellsKTable[] = {
  eCSSKeyword_show,                 NS_STYLE_TABLE_EMPTY_CELLS_SHOW,
  eCSSKeyword_hide,                 NS_STYLE_TABLE_EMPTY_CELLS_HIDE,
  eCSSKeyword__moz_show_background, NS_STYLE_TABLE_EMPTY_CELLS_SHOW_BACKGROUND,
  -1,-1
};

const PRInt32 nsCSSProps::kFloatKTable[] = {
  eCSSKeyword_left,  NS_STYLE_FLOAT_LEFT,
  eCSSKeyword_right, NS_STYLE_FLOAT_RIGHT,
  -1,-1
};

const PRInt32 nsCSSProps::kFloatEdgeKTable[] = {
  eCSSKeyword_content_box,  NS_STYLE_FLOAT_EDGE_CONTENT,
  eCSSKeyword_border_box,  NS_STYLE_FLOAT_EDGE_BORDER,
  eCSSKeyword_padding_box,  NS_STYLE_FLOAT_EDGE_PADDING,
  eCSSKeyword_margin_box,  NS_STYLE_FLOAT_EDGE_MARGIN,
  -1,-1
};

const PRInt32 nsCSSProps::kFontKTable[] = {
  // CSS2.
  eCSSKeyword_caption, NS_STYLE_FONT_CAPTION,
  eCSSKeyword_icon, NS_STYLE_FONT_ICON,
  eCSSKeyword_menu, NS_STYLE_FONT_MENU,
  eCSSKeyword_message_box, NS_STYLE_FONT_MESSAGE_BOX,
  eCSSKeyword_small_caption, NS_STYLE_FONT_SMALL_CAPTION,
  eCSSKeyword_status_bar, NS_STYLE_FONT_STATUS_BAR,

  // Proposed for CSS3.
  eCSSKeyword__moz_window, NS_STYLE_FONT_WINDOW,
  eCSSKeyword__moz_document, NS_STYLE_FONT_DOCUMENT,
  eCSSKeyword__moz_workspace, NS_STYLE_FONT_WORKSPACE,
  eCSSKeyword__moz_desktop, NS_STYLE_FONT_DESKTOP,
  eCSSKeyword__moz_info, NS_STYLE_FONT_INFO,
  eCSSKeyword__moz_dialog, NS_STYLE_FONT_DIALOG,
  eCSSKeyword__moz_button, NS_STYLE_FONT_BUTTON,
  eCSSKeyword__moz_pull_down_menu, NS_STYLE_FONT_PULL_DOWN_MENU,
  eCSSKeyword__moz_list, NS_STYLE_FONT_LIST,
  eCSSKeyword__moz_field, NS_STYLE_FONT_FIELD,
  -1,-1
};

const PRInt32 nsCSSProps::kFontSizeKTable[] = {
  eCSSKeyword_xx_small, NS_STYLE_FONT_SIZE_XXSMALL,
  eCSSKeyword_x_small, NS_STYLE_FONT_SIZE_XSMALL,
  eCSSKeyword_small, NS_STYLE_FONT_SIZE_SMALL,
  eCSSKeyword_medium, NS_STYLE_FONT_SIZE_MEDIUM,
  eCSSKeyword_large, NS_STYLE_FONT_SIZE_LARGE,
  eCSSKeyword_x_large, NS_STYLE_FONT_SIZE_XLARGE,
  eCSSKeyword_xx_large, NS_STYLE_FONT_SIZE_XXLARGE,
  eCSSKeyword_larger, NS_STYLE_FONT_SIZE_LARGER,
  eCSSKeyword_smaller, NS_STYLE_FONT_SIZE_SMALLER,
  -1,-1
};

const PRInt32 nsCSSProps::kFontStretchKTable[] = {
  eCSSKeyword_wider, NS_STYLE_FONT_STRETCH_WIDER,
  eCSSKeyword_narrower, NS_STYLE_FONT_STRETCH_NARROWER,
  eCSSKeyword_ultra_condensed, NS_STYLE_FONT_STRETCH_ULTRA_CONDENSED,
  eCSSKeyword_extra_condensed, NS_STYLE_FONT_STRETCH_EXTRA_CONDENSED,
  eCSSKeyword_condensed, NS_STYLE_FONT_STRETCH_CONDENSED,
  eCSSKeyword_semi_condensed, NS_STYLE_FONT_STRETCH_SEMI_CONDENSED,
  eCSSKeyword_semi_expanded, NS_STYLE_FONT_STRETCH_SEMI_EXPANDED,
  eCSSKeyword_expanded, NS_STYLE_FONT_STRETCH_EXPANDED,
  eCSSKeyword_extra_expanded, NS_STYLE_FONT_STRETCH_EXTRA_EXPANDED,
  eCSSKeyword_ultra_expanded, NS_STYLE_FONT_STRETCH_ULTRA_EXPANDED,
  -1,-1
};

const PRInt32 nsCSSProps::kFontStyleKTable[] = {
  eCSSKeyword_italic, NS_STYLE_FONT_STYLE_ITALIC,
  eCSSKeyword_oblique, NS_STYLE_FONT_STYLE_OBLIQUE,
  -1,-1
};

const PRInt32 nsCSSProps::kFontVariantKTable[] = {
  eCSSKeyword_small_caps, NS_STYLE_FONT_VARIANT_SMALL_CAPS,
  -1,-1
};

const PRInt32 nsCSSProps::kFontWeightKTable[] = {
  eCSSKeyword_bold, NS_STYLE_FONT_WEIGHT_BOLD,
  eCSSKeyword_bolder, NS_STYLE_FONT_WEIGHT_BOLDER,
  eCSSKeyword_lighter, NS_STYLE_FONT_WEIGHT_LIGHTER,
  -1,-1
};

// XXX What's the point?
const PRInt32 nsCSSProps::kKeyEquivalentKTable[] = {
  -1,-1
};

const PRInt32 nsCSSProps::kListStylePositionKTable[] = {
  eCSSKeyword_inside, NS_STYLE_LIST_STYLE_POSITION_INSIDE,
  eCSSKeyword_outside, NS_STYLE_LIST_STYLE_POSITION_OUTSIDE,
  -1,-1
};

const PRInt32 nsCSSProps::kListStyleKTable[] = {
  eCSSKeyword_disc, NS_STYLE_LIST_STYLE_DISC,
  eCSSKeyword_circle, NS_STYLE_LIST_STYLE_CIRCLE,
  eCSSKeyword_square, NS_STYLE_LIST_STYLE_SQUARE,
  eCSSKeyword_decimal, NS_STYLE_LIST_STYLE_DECIMAL,
  eCSSKeyword_decimal_leading_zero, NS_STYLE_LIST_STYLE_DECIMAL_LEADING_ZERO,
  eCSSKeyword_lower_roman, NS_STYLE_LIST_STYLE_LOWER_ROMAN,
  eCSSKeyword_upper_roman, NS_STYLE_LIST_STYLE_UPPER_ROMAN,
  eCSSKeyword_lower_greek, NS_STYLE_LIST_STYLE_LOWER_GREEK,
  eCSSKeyword_lower_alpha, NS_STYLE_LIST_STYLE_LOWER_ALPHA,
  eCSSKeyword_lower_latin, NS_STYLE_LIST_STYLE_LOWER_LATIN,
  eCSSKeyword_upper_alpha, NS_STYLE_LIST_STYLE_UPPER_ALPHA,
  eCSSKeyword_upper_latin, NS_STYLE_LIST_STYLE_UPPER_LATIN,
  eCSSKeyword_hebrew, NS_STYLE_LIST_STYLE_HEBREW,
  eCSSKeyword_armenian, NS_STYLE_LIST_STYLE_ARMENIAN,
  eCSSKeyword_georgian, NS_STYLE_LIST_STYLE_GEORGIAN,
  eCSSKeyword_cjk_ideographic, NS_STYLE_LIST_STYLE_CJK_IDEOGRAPHIC,
  eCSSKeyword_hiragana, NS_STYLE_LIST_STYLE_HIRAGANA,
  eCSSKeyword_katakana, NS_STYLE_LIST_STYLE_KATAKANA,
  eCSSKeyword_hiragana_iroha, NS_STYLE_LIST_STYLE_HIRAGANA_IROHA,
  eCSSKeyword_katakana_iroha, NS_STYLE_LIST_STYLE_KATAKANA_IROHA,
  eCSSKeyword__moz_cjk_heavenly_stem, NS_STYLE_LIST_STYLE_MOZ_CJK_HEAVENLY_STEM,
  eCSSKeyword__moz_cjk_earthly_branch, NS_STYLE_LIST_STYLE_MOZ_CJK_EARTHLY_BRANCH,
  eCSSKeyword__moz_trad_chinese_informal, NS_STYLE_LIST_STYLE_MOZ_TRAD_CHINESE_INFORMAL,
  eCSSKeyword__moz_trad_chinese_formal, NS_STYLE_LIST_STYLE_MOZ_TRAD_CHINESE_FORMAL,
  eCSSKeyword__moz_simp_chinese_informal, NS_STYLE_LIST_STYLE_MOZ_SIMP_CHINESE_INFORMAL,
  eCSSKeyword__moz_simp_chinese_formal, NS_STYLE_LIST_STYLE_MOZ_SIMP_CHINESE_FORMAL,
  eCSSKeyword__moz_japanese_informal, NS_STYLE_LIST_STYLE_MOZ_JAPANESE_INFORMAL,
  eCSSKeyword__moz_japanese_formal, NS_STYLE_LIST_STYLE_MOZ_JAPANESE_FORMAL,
  eCSSKeyword__moz_arabic_indic, NS_STYLE_LIST_STYLE_MOZ_ARABIC_INDIC,
  eCSSKeyword__moz_persian, NS_STYLE_LIST_STYLE_MOZ_PERSIAN,
  eCSSKeyword__moz_urdu, NS_STYLE_LIST_STYLE_MOZ_URDU,
  eCSSKeyword__moz_devanagari, NS_STYLE_LIST_STYLE_MOZ_DEVANAGARI,
  eCSSKeyword__moz_gurmukhi, NS_STYLE_LIST_STYLE_MOZ_GURMUKHI,
  eCSSKeyword__moz_gujarati, NS_STYLE_LIST_STYLE_MOZ_GUJARATI,
  eCSSKeyword__moz_oriya, NS_STYLE_LIST_STYLE_MOZ_ORIYA,
  eCSSKeyword__moz_kannada, NS_STYLE_LIST_STYLE_MOZ_KANNADA,
  eCSSKeyword__moz_malayalam, NS_STYLE_LIST_STYLE_MOZ_MALAYALAM,
  eCSSKeyword__moz_bengali, NS_STYLE_LIST_STYLE_MOZ_BENGALI,
  eCSSKeyword__moz_tamil, NS_STYLE_LIST_STYLE_MOZ_TAMIL,
  eCSSKeyword__moz_telugu, NS_STYLE_LIST_STYLE_MOZ_TELUGU,
  eCSSKeyword__moz_thai, NS_STYLE_LIST_STYLE_MOZ_THAI,
  eCSSKeyword__moz_lao, NS_STYLE_LIST_STYLE_MOZ_LAO,
  eCSSKeyword__moz_myanmar, NS_STYLE_LIST_STYLE_MOZ_MYANMAR,
  eCSSKeyword__moz_khmer, NS_STYLE_LIST_STYLE_MOZ_KHMER,
  eCSSKeyword__moz_hangul, NS_STYLE_LIST_STYLE_MOZ_HANGUL,
  eCSSKeyword__moz_hangul_consonant, NS_STYLE_LIST_STYLE_MOZ_HANGUL_CONSONANT,
  eCSSKeyword__moz_ethiopic_halehame, NS_STYLE_LIST_STYLE_MOZ_ETHIOPIC_HALEHAME,
  eCSSKeyword__moz_ethiopic_numeric, NS_STYLE_LIST_STYLE_MOZ_ETHIOPIC_NUMERIC,
  eCSSKeyword__moz_ethiopic_halehame_am, NS_STYLE_LIST_STYLE_MOZ_ETHIOPIC_HALEHAME_AM,
  eCSSKeyword__moz_ethiopic_halehame_ti_er, NS_STYLE_LIST_STYLE_MOZ_ETHIOPIC_HALEHAME_TI_ER,
  eCSSKeyword__moz_ethiopic_halehame_ti_et, NS_STYLE_LIST_STYLE_MOZ_ETHIOPIC_HALEHAME_TI_ET,
  -1,-1
};

const PRInt32 nsCSSProps::kOutlineColorKTable[] = {
  eCSSKeyword_invert, NS_STYLE_COLOR_INVERT,
  -1,-1
};

const PRInt32 nsCSSProps::kOverflowKTable[] = {
  eCSSKeyword_visible, NS_STYLE_OVERFLOW_VISIBLE,
  eCSSKeyword_hidden, NS_STYLE_OVERFLOW_HIDDEN,
  eCSSKeyword_scroll, NS_STYLE_OVERFLOW_SCROLL,
  // Deprecated:
  eCSSKeyword__moz_scrollbars_none, NS_STYLE_OVERFLOW_HIDDEN,
  eCSSKeyword__moz_scrollbars_horizontal, NS_STYLE_OVERFLOW_SCROLLBARS_HORIZONTAL,
  eCSSKeyword__moz_scrollbars_vertical, NS_STYLE_OVERFLOW_SCROLLBARS_VERTICAL,
  eCSSKeyword__moz_hidden_unscrollable, NS_STYLE_OVERFLOW_CLIP,
  -1,-1
};

const PRInt32 nsCSSProps::kOverflowSubKTable[] = {
  eCSSKeyword_visible, NS_STYLE_OVERFLOW_VISIBLE,
  eCSSKeyword_hidden, NS_STYLE_OVERFLOW_HIDDEN,
  eCSSKeyword_scroll, NS_STYLE_OVERFLOW_SCROLL,
  // Deprecated:
  eCSSKeyword__moz_hidden_unscrollable, NS_STYLE_OVERFLOW_CLIP,
  -1,-1
};

const PRInt32 nsCSSProps::kPageBreakKTable[] = {
  eCSSKeyword_always, NS_STYLE_PAGE_BREAK_ALWAYS,
  eCSSKeyword_avoid, NS_STYLE_PAGE_BREAK_AVOID,
  eCSSKeyword_left, NS_STYLE_PAGE_BREAK_LEFT,
  eCSSKeyword_right, NS_STYLE_PAGE_BREAK_RIGHT,
  -1,-1
};

const PRInt32 nsCSSProps::kPageBreakInsideKTable[] = {
  eCSSKeyword_avoid, NS_STYLE_PAGE_BREAK_AVOID,
  -1,-1
};

const PRInt32 nsCSSProps::kPageMarksKTable[] = {
  eCSSKeyword_crop, NS_STYLE_PAGE_MARKS_CROP,
  eCSSKeyword_cross, NS_STYLE_PAGE_MARKS_REGISTER,
  -1,-1
};

const PRInt32 nsCSSProps::kPageSizeKTable[] = {
  eCSSKeyword_landscape, NS_STYLE_PAGE_SIZE_LANDSCAPE,
  eCSSKeyword_portrait, NS_STYLE_PAGE_SIZE_PORTRAIT,
  -1,-1
};

const PRInt32 nsCSSProps::kPitchKTable[] = {
  eCSSKeyword_x_low, NS_STYLE_PITCH_X_LOW,
  eCSSKeyword_low, NS_STYLE_PITCH_LOW,
  eCSSKeyword_medium, NS_STYLE_PITCH_MEDIUM,
  eCSSKeyword_high, NS_STYLE_PITCH_HIGH,
  eCSSKeyword_x_high, NS_STYLE_PITCH_X_HIGH,
  -1,-1
};

const PRInt32 nsCSSProps::kPlayDuringKTable[] = {
  eCSSKeyword_mix, NS_STYLE_PLAY_DURING_MIX,
  eCSSKeyword_repeat, NS_STYLE_PLAY_DURING_REPEAT,
  -1,-1
};

const PRInt32 nsCSSProps::kPositionKTable[] = {
  eCSSKeyword_static, NS_STYLE_POSITION_STATIC,
  eCSSKeyword_relative, NS_STYLE_POSITION_RELATIVE,
  eCSSKeyword_absolute, NS_STYLE_POSITION_ABSOLUTE,
  eCSSKeyword_fixed, NS_STYLE_POSITION_FIXED,
  -1,-1
};

const PRInt32 nsCSSProps::kSpeakKTable[] = {
  eCSSKeyword_spell_out, NS_STYLE_SPEAK_SPELL_OUT,
  -1,-1
};

const PRInt32 nsCSSProps::kSpeakHeaderKTable[] = {
  eCSSKeyword_once, NS_STYLE_SPEAK_HEADER_ONCE,
  eCSSKeyword_always, NS_STYLE_SPEAK_HEADER_ALWAYS,
  -1,-1
};

const PRInt32 nsCSSProps::kSpeakNumeralKTable[] = {
  eCSSKeyword_digits, NS_STYLE_SPEAK_NUMERAL_DIGITS,
  eCSSKeyword_continuous, NS_STYLE_SPEAK_NUMERAL_CONTINUOUS,
  -1,-1
};

const PRInt32 nsCSSProps::kSpeakPunctuationKTable[] = {
  eCSSKeyword_code, NS_STYLE_SPEAK_PUNCTUATION_CODE,
  -1,-1
};

const PRInt32 nsCSSProps::kSpeechRateKTable[] = {
  eCSSKeyword_x_slow, NS_STYLE_SPEECH_RATE_X_SLOW,
  eCSSKeyword_slow, NS_STYLE_SPEECH_RATE_SLOW,
  eCSSKeyword_medium, NS_STYLE_SPEECH_RATE_MEDIUM,
  eCSSKeyword_fast, NS_STYLE_SPEECH_RATE_FAST,
  eCSSKeyword_x_fast, NS_STYLE_SPEECH_RATE_X_FAST,
  eCSSKeyword_faster, NS_STYLE_SPEECH_RATE_FASTER,
  eCSSKeyword_slower, NS_STYLE_SPEECH_RATE_SLOWER,
  -1,-1
};

const PRInt32 nsCSSProps::kTableLayoutKTable[] = {
  eCSSKeyword_fixed, NS_STYLE_TABLE_LAYOUT_FIXED,
  -1,-1
};

const PRInt32 nsCSSProps::kTextAlignKTable[] = {
  eCSSKeyword_left, NS_STYLE_TEXT_ALIGN_LEFT,
  eCSSKeyword_right, NS_STYLE_TEXT_ALIGN_RIGHT,
  eCSSKeyword_center, NS_STYLE_TEXT_ALIGN_CENTER,
  eCSSKeyword_justify, NS_STYLE_TEXT_ALIGN_JUSTIFY,
  eCSSKeyword__moz_center, NS_STYLE_TEXT_ALIGN_MOZ_CENTER,
  eCSSKeyword__moz_right, NS_STYLE_TEXT_ALIGN_MOZ_RIGHT,
  eCSSKeyword_start, NS_STYLE_TEXT_ALIGN_DEFAULT,
  -1,-1
};

const PRInt32 nsCSSProps::kTextDecorationKTable[] = {
  eCSSKeyword_underline, NS_STYLE_TEXT_DECORATION_UNDERLINE,
  eCSSKeyword_overline, NS_STYLE_TEXT_DECORATION_OVERLINE,
  eCSSKeyword_line_through, NS_STYLE_TEXT_DECORATION_LINE_THROUGH,
  eCSSKeyword_blink, NS_STYLE_TEXT_DECORATION_BLINK,
  eCSSKeyword__moz_anchor_decoration, NS_STYLE_TEXT_DECORATION_PREF_ANCHORS,
  -1,-1
};

const PRInt32 nsCSSProps::kTextTransformKTable[] = {
  eCSSKeyword_capitalize, NS_STYLE_TEXT_TRANSFORM_CAPITALIZE,
  eCSSKeyword_lowercase, NS_STYLE_TEXT_TRANSFORM_LOWERCASE,
  eCSSKeyword_uppercase, NS_STYLE_TEXT_TRANSFORM_UPPERCASE,
  -1,-1
};

const PRInt32 nsCSSProps::kUnicodeBidiKTable[] = {
  eCSSKeyword_embed, NS_STYLE_UNICODE_BIDI_EMBED,
  eCSSKeyword_bidi_override, NS_STYLE_UNICODE_BIDI_OVERRIDE,
  -1,-1
};

const PRInt32 nsCSSProps::kUserFocusKTable[] = {
  eCSSKeyword_ignore,         NS_STYLE_USER_FOCUS_IGNORE,
  eCSSKeyword_select_all,     NS_STYLE_USER_FOCUS_SELECT_ALL,
  eCSSKeyword_select_before,  NS_STYLE_USER_FOCUS_SELECT_BEFORE,
  eCSSKeyword_select_after,   NS_STYLE_USER_FOCUS_SELECT_AFTER,
  eCSSKeyword_select_same,    NS_STYLE_USER_FOCUS_SELECT_SAME,
  eCSSKeyword_select_menu,    NS_STYLE_USER_FOCUS_SELECT_MENU,
  -1,-1
};

const PRInt32 nsCSSProps::kUserInputKTable[] = {
  eCSSKeyword_enabled,  NS_STYLE_USER_INPUT_ENABLED,
  eCSSKeyword_disabled, NS_STYLE_USER_INPUT_DISABLED,
  -1,-1
};

const PRInt32 nsCSSProps::kUserModifyKTable[] = {
  eCSSKeyword_read_only,  NS_STYLE_USER_MODIFY_READ_ONLY,
  eCSSKeyword_read_write, NS_STYLE_USER_MODIFY_READ_WRITE,
  eCSSKeyword_write_only, NS_STYLE_USER_MODIFY_WRITE_ONLY,
  -1,-1
};

const PRInt32 nsCSSProps::kUserSelectKTable[] = {
  eCSSKeyword_text,       NS_STYLE_USER_SELECT_TEXT,
  eCSSKeyword_element,    NS_STYLE_USER_SELECT_ELEMENT,
  eCSSKeyword_elements,   NS_STYLE_USER_SELECT_ELEMENTS,
  eCSSKeyword_all,        NS_STYLE_USER_SELECT_ALL,
  eCSSKeyword_toggle,     NS_STYLE_USER_SELECT_TOGGLE,
  eCSSKeyword_tri_state,  NS_STYLE_USER_SELECT_TRI_STATE,
  eCSSKeyword__moz_all,   NS_STYLE_USER_SELECT_MOZ_ALL,
  -1,-1
};

const PRInt32 nsCSSProps::kVerticalAlignKTable[] = {
  eCSSKeyword_baseline, NS_STYLE_VERTICAL_ALIGN_BASELINE,
  eCSSKeyword_sub, NS_STYLE_VERTICAL_ALIGN_SUB,
  eCSSKeyword_super, NS_STYLE_VERTICAL_ALIGN_SUPER,
  eCSSKeyword_top, NS_STYLE_VERTICAL_ALIGN_TOP,
  eCSSKeyword_text_top, NS_STYLE_VERTICAL_ALIGN_TEXT_TOP,
  eCSSKeyword_middle, NS_STYLE_VERTICAL_ALIGN_MIDDLE,
  eCSSKeyword_bottom, NS_STYLE_VERTICAL_ALIGN_BOTTOM,
  eCSSKeyword_text_bottom, NS_STYLE_VERTICAL_ALIGN_TEXT_BOTTOM,
  -1,-1
};

const PRInt32 nsCSSProps::kVisibilityKTable[] = {
  eCSSKeyword_visible, NS_STYLE_VISIBILITY_VISIBLE,
  eCSSKeyword_hidden, NS_STYLE_VISIBILITY_HIDDEN,
  eCSSKeyword_collapse, NS_STYLE_VISIBILITY_COLLAPSE,
  -1,-1
};

const PRInt32 nsCSSProps::kVolumeKTable[] = {
  eCSSKeyword_silent, NS_STYLE_VOLUME_SILENT,
  eCSSKeyword_x_soft, NS_STYLE_VOLUME_X_SOFT,
  eCSSKeyword_soft, NS_STYLE_VOLUME_SOFT,
  eCSSKeyword_medium, NS_STYLE_VOLUME_MEDIUM,
  eCSSKeyword_loud, NS_STYLE_VOLUME_LOUD,
  eCSSKeyword_x_loud, NS_STYLE_VOLUME_X_LOUD,
  -1,-1
};

const PRInt32 nsCSSProps::kWhitespaceKTable[] = {
  eCSSKeyword_pre, NS_STYLE_WHITESPACE_PRE,
  eCSSKeyword_nowrap, NS_STYLE_WHITESPACE_NOWRAP,
  eCSSKeyword__moz_pre_wrap, NS_STYLE_WHITESPACE_MOZ_PRE_WRAP,
  -1,-1
};

// Specific keyword tables for XUL.properties
const PRInt32 nsCSSProps::kBoxAlignKTable[] = {
  eCSSKeyword_stretch,  NS_STYLE_BOX_ALIGN_STRETCH,
  eCSSKeyword_start,   NS_STYLE_BOX_ALIGN_START,
  eCSSKeyword_center, NS_STYLE_BOX_ALIGN_CENTER,
  eCSSKeyword_baseline, NS_STYLE_BOX_ALIGN_BASELINE, 
  eCSSKeyword_end, NS_STYLE_BOX_ALIGN_END, 
  -1,-1
};

const PRInt32 nsCSSProps::kBoxDirectionKTable[] = {
  eCSSKeyword_normal,  NS_STYLE_BOX_DIRECTION_NORMAL,
  eCSSKeyword_reverse,   NS_STYLE_BOX_DIRECTION_REVERSE,
  -1,-1
};

const PRInt32 nsCSSProps::kBoxOrientKTable[] = {
  eCSSKeyword_horizontal,  NS_STYLE_BOX_ORIENT_HORIZONTAL,
  eCSSKeyword_vertical,   NS_STYLE_BOX_ORIENT_VERTICAL,
  eCSSKeyword_inline_axis, NS_STYLE_BOX_ORIENT_HORIZONTAL,
  eCSSKeyword_block_axis, NS_STYLE_BOX_ORIENT_VERTICAL, 
  -1,-1
};

const PRInt32 nsCSSProps::kBoxPackKTable[] = {
  eCSSKeyword_start,  NS_STYLE_BOX_PACK_START,
  eCSSKeyword_center,   NS_STYLE_BOX_PACK_CENTER,
  eCSSKeyword_end, NS_STYLE_BOX_PACK_END,
  eCSSKeyword_justify, NS_STYLE_BOX_PACK_JUSTIFY, 
  -1,-1
};

#ifdef MOZ_SVG
// keyword tables for SVG properties

const PRInt32 nsCSSProps::kDominantBaselineKTable[] = {
  eCSSKeyword_use_script, NS_STYLE_DOMINANT_BASELINE_USE_SCRIPT,
  eCSSKeyword_no_change, NS_STYLE_DOMINANT_BASELINE_NO_CHANGE,
  eCSSKeyword_reset_size, NS_STYLE_DOMINANT_BASELINE_RESET_SIZE,
  eCSSKeyword_alphabetic, NS_STYLE_DOMINANT_BASELINE_ALPHABETIC,
  eCSSKeyword_hanging, NS_STYLE_DOMINANT_BASELINE_HANGING,
  eCSSKeyword_ideographic, NS_STYLE_DOMINANT_BASELINE_IDEOGRAPHIC,
  eCSSKeyword_mathematical, NS_STYLE_DOMINANT_BASELINE_MATHEMATICAL,
  eCSSKeyword_central, NS_STYLE_DOMINANT_BASELINE_CENTRAL,
  eCSSKeyword_middle, NS_STYLE_DOMINANT_BASELINE_MIDDLE,
  eCSSKeyword_text_after_edge, NS_STYLE_DOMINANT_BASELINE_TEXT_AFTER_EDGE,
  eCSSKeyword_text_before_edge, NS_STYLE_DOMINANT_BASELINE_TEXT_BEFORE_EDGE,
  -1, -1
};

const PRInt32 nsCSSProps::kFillRuleKTable[] = {
  eCSSKeyword_nonzero, NS_STYLE_FILL_RULE_NONZERO,
  eCSSKeyword_evenodd, NS_STYLE_FILL_RULE_EVENODD,
  -1, -1
};

const PRInt32 nsCSSProps::kPointerEventsKTable[] = {
  eCSSKeyword_visiblepainted, NS_STYLE_POINTER_EVENTS_VISIBLEPAINTED,
  eCSSKeyword_visiblefill, NS_STYLE_POINTER_EVENTS_VISIBLEFILL,
  eCSSKeyword_visiblestroke, NS_STYLE_POINTER_EVENTS_VISIBLESTROKE,
  eCSSKeyword_visible, NS_STYLE_POINTER_EVENTS_VISIBLE,
  eCSSKeyword_painted, NS_STYLE_POINTER_EVENTS_PAINTED,
  eCSSKeyword_fill, NS_STYLE_POINTER_EVENTS_FILL,
  eCSSKeyword_stroke, NS_STYLE_POINTER_EVENTS_STROKE,
  eCSSKeyword_all, NS_STYLE_POINTER_EVENTS_ALL,
  eCSSKeyword_none, NS_STYLE_POINTER_EVENTS_NONE,
  -1, -1
};

const PRInt32 nsCSSProps::kShapeRenderingKTable[] = {
  eCSSKeyword_optimizespeed, NS_STYLE_SHAPE_RENDERING_OPTIMIZESPEED,
  eCSSKeyword_crispedges, NS_STYLE_SHAPE_RENDERING_CRISPEDGES,
  eCSSKeyword_geometricprecision, NS_STYLE_SHAPE_RENDERING_GEOMETRICPRECISION,
  -1, -1
};

const PRInt32 nsCSSProps::kStrokeLinecapKTable[] = {
  eCSSKeyword_butt, NS_STYLE_STROKE_LINECAP_BUTT,
  eCSSKeyword_round, NS_STYLE_STROKE_LINECAP_ROUND,
  eCSSKeyword_square, NS_STYLE_STROKE_LINECAP_SQUARE,
  -1, -1
};

const PRInt32 nsCSSProps::kStrokeLinejoinKTable[] = {
  eCSSKeyword_miter, NS_STYLE_STROKE_LINEJOIN_MITER,
  eCSSKeyword_round, NS_STYLE_STROKE_LINEJOIN_ROUND,
  eCSSKeyword_bevel, NS_STYLE_STROKE_LINEJOIN_BEVEL,
  -1, -1
};

const PRInt32 nsCSSProps::kTextAnchorKTable[] = {
  eCSSKeyword_start, NS_STYLE_TEXT_ANCHOR_START,
  eCSSKeyword_middle, NS_STYLE_TEXT_ANCHOR_MIDDLE,
  eCSSKeyword_end, NS_STYLE_TEXT_ANCHOR_END,
  -1, -1
};

const PRInt32 nsCSSProps::kTextRenderingKTable[] = {
  eCSSKeyword_optimizespeed, NS_STYLE_TEXT_RENDERING_OPTIMIZESPEED,
  eCSSKeyword_optimizelegibility, NS_STYLE_TEXT_RENDERING_OPTIMIZELEGIBILITY,
  eCSSKeyword_geometricprecision, NS_STYLE_TEXT_RENDERING_GEOMETRICPRECISION,
  -1, -1
};

#endif

PRInt32 
nsCSSProps::SearchKeywordTableInt(PRInt32 aValue, const PRInt32 aTable[])
{
  PRInt32 i = 1;
  for (;;) {
    if (aTable[i] == -1 && aTable[i-1] == -1) {
      break;
    }
    if (aValue == aTable[i]) {
      return PRInt32(aTable[i-1]);
    }
    i += 2;
  }
  return -1;
}

const nsAFlatCString&
nsCSSProps::SearchKeywordTable(PRInt32 aValue, const PRInt32 aTable[])
{
  PRInt32 i = SearchKeywordTableInt(aValue, aTable);
  if (i < 0) {
    static nsDependentCString sNullStr("");
    return sNullStr;
  } else {
    return nsCSSKeywords::GetStringValue(nsCSSKeyword(i));
  }
}

/* static */ const PRInt32* const
nsCSSProps::kKeywordTableTable[eCSSProperty_COUNT_no_shorthands] = {
  #define CSS_PROP(name_, id_, method_, datastruct_, member_, type_, iscoord_, kwtable_) kwtable_,
  #include "nsCSSPropList.h"
  #undef CSS_PROP
};

const nsAFlatCString& 
nsCSSProps::LookupPropertyValue(nsCSSProperty aProp, PRInt32 aValue)
{
  NS_ASSERTION(aProp >= 0 && aProp < eCSSProperty_COUNT, "property out of range");

  const PRInt32* kwtable = nsnull;
  if (aProp < eCSSProperty_COUNT_no_shorthands)
    kwtable = kKeywordTableTable[aProp];

  if (kwtable)
    return SearchKeywordTable(aValue, kwtable);

  static nsDependentCString sNullStr("");
  return sNullStr;
}

PRBool nsCSSProps::GetColorName(PRInt32 aPropValue, nsCString &aStr)
{
  PRBool rv = PR_FALSE;
  PRInt32 keyword = -1;

  // first get the keyword corresponding to the property Value from the color table
  keyword = SearchKeywordTableInt(aPropValue, kColorKTable);

  // next get the name as a string from the keywords table
  if (keyword >= 0) {
    nsCSSKeywords::AddRefTable();
    aStr = nsCSSKeywords::GetStringValue((nsCSSKeyword)keyword);
    nsCSSKeywords::ReleaseTable();
    rv = PR_TRUE;
  }  
  return rv;
}

// define array of all CSS property types
const nsCSSType nsCSSProps::kTypeTable[eCSSProperty_COUNT_no_shorthands] = {
    #define CSS_PROP(name_, id_, method_, datastruct_, member_, type_, iscoord_, kwtable_) type_,
    #include "nsCSSPropList.h"
    #undef CSS_PROP
};

const nsStyleStructID nsCSSProps::kSIDTable[eCSSProperty_COUNT_no_shorthands] = {
    #define CSS_PROP_FONT(name_, id_, method_, datastruct_, member_, type_, iscoord_, kwtable_) eStyleStruct_Font,
    #define CSS_PROP_COLOR(name_, id_, method_, datastruct_, member_, type_, iscoord_, kwtable_) eStyleStruct_Color,
    #define CSS_PROP_BACKGROUND(name_, id_, method_, datastruct_, member_, type_, iscoord_, kwtable_) eStyleStruct_Background,
    #define CSS_PROP_LIST(name_, id_, method_, datastruct_, member_, type_, iscoord_, kwtable_) eStyleStruct_List,
    #define CSS_PROP_POSITION(name_, id_, method_, datastruct_, member_, type_, iscoord_, kwtable_) eStyleStruct_Position,
    #define CSS_PROP_TEXT(name_, id_, method_, datastruct_, member_, type_, iscoord_, kwtable_) eStyleStruct_Text,
    #define CSS_PROP_TEXTRESET(name_, id_, method_, datastruct_, member_, type_, iscoord_, kwtable_) eStyleStruct_TextReset,
    #define CSS_PROP_DISPLAY(name_, id_, method_, datastruct_, member_, type_, iscoord_, kwtable_) eStyleStruct_Display,
    #define CSS_PROP_VISIBILITY(name_, id_, method_, datastruct_, member_, type_, iscoord_, kwtable_) eStyleStruct_Visibility,
    #define CSS_PROP_CONTENT(name_, id_, method_, datastruct_, member_, type_, iscoord_, kwtable_) eStyleStruct_Content,
    #define CSS_PROP_QUOTES(name_, id_, method_, datastruct_, member_, type_, iscoord_, kwtable_) eStyleStruct_Quotes,
    #define CSS_PROP_USERINTERFACE(name_, id_, method_, datastruct_, member_, type_, iscoord_, kwtable_) eStyleStruct_UserInterface,
    #define CSS_PROP_UIRESET(name_, id_, method_, datastruct_, member_, type_, iscoord_, kwtable_) eStyleStruct_UIReset,
    #define CSS_PROP_TABLE(name_, id_, method_, datastruct_, member_, type_, iscoord_, kwtable_) eStyleStruct_Table,
    #define CSS_PROP_TABLEBORDER(name_, id_, method_, datastruct_, member_, type_, iscoord_, kwtable_) eStyleStruct_TableBorder,
    #define CSS_PROP_MARGIN(name_, id_, method_, datastruct_, member_, type_, iscoord_, kwtable_) eStyleStruct_Margin,
    #define CSS_PROP_PADDING(name_, id_, method_, datastruct_, member_, type_, iscoord_, kwtable_) eStyleStruct_Padding,
    #define CSS_PROP_BORDER(name_, id_, method_, datastruct_, member_, type_, iscoord_, kwtable_) eStyleStruct_Border,
    #define CSS_PROP_OUTLINE(name_, id_, method_, datastruct_, member_, type_, iscoord_, kwtable_) eStyleStruct_Outline,
    #define CSS_PROP_XUL(name_, id_, method_, datastruct_, member_, type_, iscoord_, kwtable_) eStyleStruct_XUL,
    #ifdef MOZ_SVG
    #define CSS_PROP_SVG(name_, id_, method_, datastruct_, member_, type_, iscoord_, kwtable_) eStyleStruct_SVG,
    #define CSS_PROP_SVGRESET(name_, id_, method_, datastruct_, member_, type_, iscoord_, kwtable_) eStyleStruct_SVGReset,
    #endif /* defined(MOZ_SVG) */
    #define CSS_PROP_COLUMN(name_, id_, method_, datastruct_, member_, type_, iscoord_, kwtable_) eStyleStruct_Column,
    // This shouldn't matter, but we need something to go here.
    #define CSS_PROP_BACKENDONLY(name_, id_, method_, datastruct_, member_, type_, iscoord_, kwtable_) nsStyleStructID(-1),

    #include "nsCSSPropList.h"

    #undef CSS_PROP_FONT
    #undef CSS_PROP_COLOR
    #undef CSS_PROP_BACKGROUND
    #undef CSS_PROP_LIST
    #undef CSS_PROP_POSITION
    #undef CSS_PROP_TEXT
    #undef CSS_PROP_TEXTRESET
    #undef CSS_PROP_DISPLAY
    #undef CSS_PROP_VISIBILITY
    #undef CSS_PROP_CONTENT
    #undef CSS_PROP_QUOTES
    #undef CSS_PROP_USERINTERFACE
    #undef CSS_PROP_UIRESET
    #undef CSS_PROP_TABLE
    #undef CSS_PROP_TABLEBORDER
    #undef CSS_PROP_MARGIN
    #undef CSS_PROP_PADDING
    #undef CSS_PROP_BORDER
    #undef CSS_PROP_OUTLINE
    #undef CSS_PROP_XUL
    #ifdef MOZ_SVG
    #undef CSS_PROP_SVG
    #undef CSS_PROP_SVGRESET
    #endif /* undefd(MOZ_SVG) */
    #undef CSS_PROP_COLUMN
    #undef CSS_PROP_BACKENDONLY
};

static const nsCSSProperty gMozBorderRadiusSubpropTable[] = {
  // Code relies on these being in topleft-topright-bottomright-bottomleft
  // order.
  eCSSProperty__moz_border_radius_topLeft,
  eCSSProperty__moz_border_radius_topRight,
  eCSSProperty__moz_border_radius_bottomRight,
  eCSSProperty__moz_border_radius_bottomLeft,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gMozOutlineRadiusSubpropTable[] = {
  // Code relies on these being in topleft-topright-bottomright-bottomleft
  // order.
  eCSSProperty__moz_outline_radius_topLeft,
  eCSSProperty__moz_outline_radius_topRight,
  eCSSProperty__moz_outline_radius_bottomRight,
  eCSSProperty__moz_outline_radius_bottomLeft,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBackgroundSubpropTable[] = {
  eCSSProperty_background_color,
  eCSSProperty_background_image,
  eCSSProperty_background_repeat,
  eCSSProperty_background_attachment,
  eCSSProperty_background_x_position,
  eCSSProperty_background_y_position,
  eCSSProperty__moz_background_clip, // XXX Added LDB.
  eCSSProperty__moz_background_origin, // XXX Added LDB.
  eCSSProperty__moz_background_inline_policy, // XXX Added LDB.
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBackgroundPositionSubpropTable[] = {
  eCSSProperty_background_x_position,
  eCSSProperty_background_y_position,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderSubpropTable[] = {
  eCSSProperty_border_top_width,
  eCSSProperty_border_right_width,
  eCSSProperty_border_bottom_width,
  eCSSProperty_border_left_width,
  eCSSProperty_border_top_style,
  eCSSProperty_border_right_style,
  eCSSProperty_border_bottom_style,
  eCSSProperty_border_left_style,
  eCSSProperty_border_top_color,
  eCSSProperty_border_right_color,
  eCSSProperty_border_bottom_color,
  eCSSProperty_border_left_color,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderBottomSubpropTable[] = {
  // nsCSSDeclaration.cpp outputs the subproperties in this order.
  eCSSProperty_border_bottom_width,
  eCSSProperty_border_bottom_style,
  eCSSProperty_border_bottom_color,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderColorSubpropTable[] = {
  // Code relies on these being in top-right-bottom-left order.
  eCSSProperty_border_top_color,
  eCSSProperty_border_right_color,
  eCSSProperty_border_bottom_color,
  eCSSProperty_border_left_color,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderLeftSubpropTable[] = {
  // nsCSSDeclaration.cpp outputs the subproperties in this order.
  eCSSProperty_border_left_width,
  eCSSProperty_border_left_style,
  eCSSProperty_border_left_color,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderRightSubpropTable[] = {
  // nsCSSDeclaration.cpp outputs the subproperties in this order.
  eCSSProperty_border_right_width,
  eCSSProperty_border_right_style,
  eCSSProperty_border_right_color,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderSpacingSubpropTable[] = {
  eCSSProperty_border_x_spacing,
  eCSSProperty_border_y_spacing,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderStyleSubpropTable[] = {
  // Code relies on these being in top-right-bottom-left order.
  eCSSProperty_border_top_style,
  eCSSProperty_border_right_style,
  eCSSProperty_border_bottom_style,
  eCSSProperty_border_left_style,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderTopSubpropTable[] = {
  // nsCSSDeclaration.cpp outputs the subproperties in this order.
  eCSSProperty_border_top_width,
  eCSSProperty_border_top_style,
  eCSSProperty_border_top_color,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderWidthSubpropTable[] = {
  // Code relies on these being in top-right-bottom-left order.
  eCSSProperty_border_top_width,
  eCSSProperty_border_right_width,
  eCSSProperty_border_bottom_width,
  eCSSProperty_border_left_width,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gCueSubpropTable[] = {
  eCSSProperty_cue_after,
  eCSSProperty_cue_before,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gFontSubpropTable[] = {
  eCSSProperty_font_family,
  eCSSProperty_font_style,
  eCSSProperty_font_variant,
  eCSSProperty_font_weight,
  eCSSProperty_font_size,
  eCSSProperty_line_height,
  eCSSProperty_font_size_adjust, // XXX Added LDB.
  eCSSProperty_font_stretch, // XXX Added LDB.
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gListStyleSubpropTable[] = {
  eCSSProperty_list_style_type,
  eCSSProperty_list_style_image,
  eCSSProperty_list_style_position,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gMarginSubpropTable[] = {
  // Code relies on these being in top-right-bottom-left order.
  eCSSProperty_margin_top,
  eCSSProperty_margin_right_value,
  eCSSProperty_margin_bottom,
  eCSSProperty_margin_left_value,
  // extras:
  eCSSProperty_margin_left_ltr_source,
  eCSSProperty_margin_left_rtl_source,
  eCSSProperty_margin_right_ltr_source,
  eCSSProperty_margin_right_rtl_source,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gMarginLeftSubpropTable[] = {
  // nsCSSParser::ParseDirectionalBoxProperty depends on this order
  eCSSProperty_margin_left_value,
  eCSSProperty_margin_left_ltr_source,
  eCSSProperty_margin_left_rtl_source,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gMarginRightSubpropTable[] = {
  // nsCSSParser::ParseDirectionalBoxProperty depends on this order
  eCSSProperty_margin_right_value,
  eCSSProperty_margin_right_ltr_source,
  eCSSProperty_margin_right_rtl_source,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gMozMarginStartSubpropTable[] = {
  // nsCSSParser::ParseDirectionalBoxProperty depends on this order
  eCSSProperty_margin_start_value,
  eCSSProperty_margin_left_ltr_source,
  eCSSProperty_margin_right_rtl_source,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gMozMarginEndSubpropTable[] = {
  // nsCSSParser::ParseDirectionalBoxProperty depends on this order
  eCSSProperty_margin_end_value,
  eCSSProperty_margin_right_ltr_source,
  eCSSProperty_margin_left_rtl_source,
  eCSSProperty_UNKNOWN
};


static const nsCSSProperty gMozOutlineSubpropTable[] = {
  // nsCSSDeclaration.cpp outputs the subproperties in this order.
  eCSSProperty__moz_outline_color,
  eCSSProperty__moz_outline_style,
  eCSSProperty__moz_outline_width,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gOverflowSubpropTable[] = {
  eCSSProperty_overflow_x,
  eCSSProperty_overflow_y,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gPaddingSubpropTable[] = {
  // Code relies on these being in top-right-bottom-left order.
  eCSSProperty_padding_top,
  eCSSProperty_padding_right_value,
  eCSSProperty_padding_bottom,
  eCSSProperty_padding_left_value,
  // extras:
  eCSSProperty_padding_left_ltr_source,
  eCSSProperty_padding_left_rtl_source,
  eCSSProperty_padding_right_ltr_source,
  eCSSProperty_padding_right_rtl_source,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gPaddingLeftSubpropTable[] = {
  // nsCSSParser::ParseDirectionalBoxProperty depends on this order
  eCSSProperty_padding_left_value,
  eCSSProperty_padding_left_ltr_source,
  eCSSProperty_padding_left_rtl_source,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gPaddingRightSubpropTable[] = {
  // nsCSSParser::ParseDirectionalBoxProperty depends on this order
  eCSSProperty_padding_right_value,
  eCSSProperty_padding_right_ltr_source,
  eCSSProperty_padding_right_rtl_source,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gMozPaddingStartSubpropTable[] = {
  // nsCSSParser::ParseDirectionalBoxProperty depends on this order
  eCSSProperty_padding_start_value,
  eCSSProperty_padding_left_ltr_source,
  eCSSProperty_padding_right_rtl_source,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gMozPaddingEndSubpropTable[] = {
  // nsCSSParser::ParseDirectionalBoxProperty depends on this order
  eCSSProperty_padding_end_value,
  eCSSProperty_padding_right_ltr_source,
  eCSSProperty_padding_left_rtl_source,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gPauseSubpropTable[] = {
  eCSSProperty_pause_after,
  eCSSProperty_pause_before,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gPlayDuringSubpropTable[] = {
  eCSSProperty_play_during_uri,
  eCSSProperty_play_during_flags,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gSizeSubpropTable[] = {
  eCSSProperty_size_width,
  eCSSProperty_size_height,
  eCSSProperty_UNKNOWN
};

const nsCSSProperty *const
nsCSSProps::kSubpropertyTable[eCSSProperty_COUNT - eCSSProperty_COUNT_no_shorthands] = {
    #define CSS_PROP_SHORTHAND(name_, id_, method_) g##method_##SubpropTable,
    #include "nsCSSPropList.h"
    #undef CSS_PROP_SHORTHAND
};
