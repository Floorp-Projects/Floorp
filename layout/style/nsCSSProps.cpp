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
 *   Mats Palmgren <matspal@gmail.com>
 *   Jonathon Jongsma <jonathon.jongsma@collabora.co.uk>, Collabora Ltd.
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

/*
 * methods for dealing with CSS properties and tables of the keyword
 * values they accept
 */

#include "nsCSSProps.h"
#include "nsCSSKeywords.h"
#include "nsStyleConsts.h"
#include "nsIWidget.h"
#include "nsThemeConstants.h"  // For system widget appearance types

#include "nsILookAndFeel.h" // for system colors

#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsStaticNameTable.h"
#include "prlog.h" // for PR_STATIC_ASSERT

// required to make the symbol external, so that TestCSSPropertyLookup.cpp can link with it
extern const char* const kCSSRawProperties[];

// define an array of all CSS properties
const char* const kCSSRawProperties[] = {
#define CSS_PROP(name_, id_, method_, flags_, parsevariant_, kwtable_,       \
                 stylestruct_, stylestructoffset_, animtype_)                \
  #name_,
#include "nsCSSPropList.h"
#undef CSS_PROP
#define CSS_PROP_SHORTHAND(name_, id_, method_, flags_) #name_,
#include "nsCSSPropList.h"
#undef CSS_PROP_SHORTHAND
};


static PRInt32 gTableRefCount;
static nsStaticCaseInsensitiveNameTable* gPropertyTable;
static nsStaticCaseInsensitiveNameTable* gFontDescTable;

/* static */ nsCSSProperty *
  nsCSSProps::gShorthandsContainingTable[eCSSProperty_COUNT_no_shorthands];
/* static */ nsCSSProperty* nsCSSProps::gShorthandsContainingPool = nsnull;

// Keep in sync with enum nsCSSFontDesc in nsCSSProperty.h.
static const char* const kCSSRawFontDescs[] = {
  "font-family",
  "font-style",
  "font-weight",
  "font-stretch",
  "src",
  "unicode-range",
  "-moz-font-feature-settings",
  "-moz-font-language-override"
};

struct PropertyAndCount {
  nsCSSProperty property;
  PRUint32 count;
};

static int
SortPropertyAndCount(const void* s1, const void* s2, void *closure)
{
  const PropertyAndCount *pc1 = static_cast<const PropertyAndCount*>(s1);
  const PropertyAndCount *pc2 = static_cast<const PropertyAndCount*>(s2);
  // Primary sort by count (lowest to highest)
  if (pc1->count != pc2->count)
    return pc1->count - pc2->count;
  // Secondary sort by property index (highest to lowest)
  return pc2->property - pc1->property;
}

void
nsCSSProps::AddRefTable(void)
{
  if (0 == gTableRefCount++) {
    NS_ABORT_IF_FALSE(!gPropertyTable, "pre existing array!");
    NS_ABORT_IF_FALSE(!gFontDescTable, "pre existing array!");

    gPropertyTable = new nsStaticCaseInsensitiveNameTable();
    if (gPropertyTable) {
#ifdef DEBUG
    {
      // let's verify the table...
      for (PRInt32 index = 0; index < eCSSProperty_COUNT; ++index) {
        nsCAutoString temp1(kCSSRawProperties[index]);
        nsCAutoString temp2(kCSSRawProperties[index]);
        ToLowerCase(temp1);
        NS_ABORT_IF_FALSE(temp1.Equals(temp2), "upper case char in prop table");
        NS_ABORT_IF_FALSE(-1 == temp1.FindChar('_'),
                          "underscore char in prop table");
      }
    }
#endif
      gPropertyTable->Init(kCSSRawProperties, eCSSProperty_COUNT);
    }

    gFontDescTable = new nsStaticCaseInsensitiveNameTable();
    if (gFontDescTable) {
#ifdef DEBUG
    {
      // let's verify the table...
      for (PRInt32 index = 0; index < eCSSFontDesc_COUNT; ++index) {
        nsCAutoString temp1(kCSSRawFontDescs[index]);
        nsCAutoString temp2(kCSSRawFontDescs[index]);
        ToLowerCase(temp1);
        NS_ABORT_IF_FALSE(temp1.Equals(temp2), "upper case char in desc table");
        NS_ABORT_IF_FALSE(-1 == temp1.FindChar('_'),
                          "underscore char in desc table");
      }
    }
#endif
      gFontDescTable->Init(kCSSRawFontDescs, eCSSFontDesc_COUNT);
    }

    BuildShorthandsContainingTable();
  }
}

#undef  DEBUG_SHORTHANDS_CONTAINING

PRBool
nsCSSProps::BuildShorthandsContainingTable()
{
  PRUint32 occurrenceCounts[eCSSProperty_COUNT_no_shorthands];
  memset(occurrenceCounts, 0, sizeof(occurrenceCounts));
  PropertyAndCount subpropCounts[eCSSProperty_COUNT -
                                   eCSSProperty_COUNT_no_shorthands];
  for (nsCSSProperty shorthand = eCSSProperty_COUNT_no_shorthands;
       shorthand < eCSSProperty_COUNT;
       shorthand = nsCSSProperty(shorthand + 1)) {
#ifdef DEBUG_SHORTHANDS_CONTAINING
    printf("Considering shorthand property '%s'.\n",
           nsCSSProps::GetStringValue(shorthand).get());
#endif
    PropertyAndCount &subpropCountsEntry =
      subpropCounts[shorthand - eCSSProperty_COUNT_no_shorthands];
    subpropCountsEntry.property = shorthand;
    subpropCountsEntry.count = 0;
    for (const nsCSSProperty* subprops = SubpropertyEntryFor(shorthand);
         *subprops != eCSSProperty_UNKNOWN;
         ++subprops) {
      NS_ABORT_IF_FALSE(0 < *subprops &&
                        *subprops < eCSSProperty_COUNT_no_shorthands,
                        "subproperty must be a longhand");
      ++occurrenceCounts[*subprops];
      ++subpropCountsEntry.count;
    }
  }

  PRUint32 poolEntries = 0;
  for (nsCSSProperty longhand = nsCSSProperty(0);
       longhand < eCSSProperty_COUNT_no_shorthands;
       longhand = nsCSSProperty(longhand + 1)) {
    PRUint32 count = occurrenceCounts[longhand];
    if (count > 0)
      // leave room for terminator
      poolEntries += count + 1;
  }

  gShorthandsContainingPool = new nsCSSProperty[poolEntries];
  if (!gShorthandsContainingPool)
    return PR_FALSE;

  // Initialize all entries to point to their null-terminator.
  {
    nsCSSProperty *poolCursor = gShorthandsContainingPool - 1;
    nsCSSProperty *lastTerminator =
      gShorthandsContainingPool + poolEntries - 1;
    for (nsCSSProperty longhand = nsCSSProperty(0);
         longhand < eCSSProperty_COUNT_no_shorthands;
         longhand = nsCSSProperty(longhand + 1)) {
      PRUint32 count = occurrenceCounts[longhand];
      if (count > 0) {
        poolCursor += count + 1;
        gShorthandsContainingTable[longhand] = poolCursor;
        *poolCursor = eCSSProperty_UNKNOWN;
      } else {
        gShorthandsContainingTable[longhand] = lastTerminator;
      }
    }
    NS_ABORT_IF_FALSE(poolCursor == lastTerminator, "miscalculation");
  }

  // Sort with lowest count at the start and highest at the end, and
  // within counts sort in reverse property index order.
  NS_QuickSort(&subpropCounts, NS_ARRAY_LENGTH(subpropCounts),
               sizeof(subpropCounts[0]), SortPropertyAndCount, nsnull);

  // Fill in all the entries in gShorthandsContainingTable
  for (const PropertyAndCount *shorthandAndCount = subpropCounts,
                           *shorthandAndCountEnd =
                                subpropCounts + NS_ARRAY_LENGTH(subpropCounts);
       shorthandAndCount < shorthandAndCountEnd;
       ++shorthandAndCount) {
#ifdef DEBUG_SHORTHANDS_CONTAINING
    printf("Entering %u subprops for '%s'.\n",
           shorthandAndCount->count,
           nsCSSProps::GetStringValue(shorthandAndCount->property).get());
#endif
    for (const nsCSSProperty* subprops =
           SubpropertyEntryFor(shorthandAndCount->property);
         *subprops != eCSSProperty_UNKNOWN;
         ++subprops) {
      *(--gShorthandsContainingTable[*subprops]) = shorthandAndCount->property;
    }
  }

#ifdef DEBUG_SHORTHANDS_CONTAINING
  for (nsCSSProperty longhand = nsCSSProperty(0);
       longhand < eCSSProperty_COUNT_no_shorthands;
       longhand = nsCSSProperty(longhand + 1)) {
    printf("Property %s is in %d shorthands.\n",
           nsCSSProps::GetStringValue(longhand).get(),
           occurrenceCounts[longhand]);
    for (const nsCSSProperty *shorthands = ShorthandsContaining(longhand);
         *shorthands != eCSSProperty_UNKNOWN;
         ++shorthands) {
      printf("  %s\n", nsCSSProps::GetStringValue(*shorthands).get());
    }
  }
#endif

#ifdef DEBUG
  // Verify that all values that should be are present.
  for (nsCSSProperty shorthand = eCSSProperty_COUNT_no_shorthands;
       shorthand < eCSSProperty_COUNT;
       shorthand = nsCSSProperty(shorthand + 1)) {
    for (const nsCSSProperty* subprops = SubpropertyEntryFor(shorthand);
         *subprops != eCSSProperty_UNKNOWN;
         ++subprops) {
      PRUint32 count = 0;
      for (const nsCSSProperty *shcont = ShorthandsContaining(*subprops);
           *shcont != eCSSProperty_UNKNOWN;
           ++shcont) {
        if (*shcont == shorthand)
          ++count;
      }
      NS_ABORT_IF_FALSE(count == 1,
                        "subproperty of shorthand should have shorthand"
                        " in its ShorthandsContaining() table");
    }
  }

  // Verify that there are no extra values
  for (nsCSSProperty longhand = nsCSSProperty(0);
       longhand < eCSSProperty_COUNT_no_shorthands;
       longhand = nsCSSProperty(longhand + 1)) {
    for (const nsCSSProperty *shorthands = ShorthandsContaining(longhand);
         *shorthands != eCSSProperty_UNKNOWN;
         ++shorthands) {
      PRUint32 count = 0;
      for (const nsCSSProperty* subprops = SubpropertyEntryFor(*shorthands);
           *subprops != eCSSProperty_UNKNOWN;
           ++subprops) {
        if (*subprops == longhand)
          ++count;
      }
      NS_ABORT_IF_FALSE(count == 1,
                        "longhand should be in subproperty table of "
                        "property in its ShorthandsContaining() table");
    }
  }
#endif

  return PR_TRUE;
}

void
nsCSSProps::ReleaseTable(void)
{
  if (0 == --gTableRefCount) {
    delete gPropertyTable;
    gPropertyTable = nsnull;

    delete gFontDescTable;
    gFontDescTable = nsnull;

    delete [] gShorthandsContainingPool;
    gShorthandsContainingPool = nsnull;
  }
}

struct CSSPropertyAlias {
  char name[sizeof("-moz-border-radius-bottomright")];
  nsCSSProperty id;
};

static const CSSPropertyAlias gAliases[] = {
  { "-moz-border-radius", eCSSProperty_border_radius },
  { "-moz-border-radius-bottomleft", eCSSProperty_border_bottom_left_radius },
  { "-moz-border-radius-bottomright", eCSSProperty_border_bottom_right_radius },
  { "-moz-border-radius-topleft", eCSSProperty_border_top_left_radius },
  { "-moz-border-radius-topright", eCSSProperty_border_top_right_radius },
  { "-moz-box-shadow", eCSSProperty_box_shadow },
  // Don't forget to update the sizeof in CSSPropertyAlias above with the
  // longest string when you add stuff here.
};

nsCSSProperty
nsCSSProps::LookupProperty(const nsACString& aProperty)
{
  NS_ABORT_IF_FALSE(gPropertyTable, "no lookup table, needs addref");

  nsCSSProperty res = nsCSSProperty(gPropertyTable->Lookup(aProperty));
  if (res == eCSSProperty_UNKNOWN) {
    for (const CSSPropertyAlias *alias = gAliases,
                            *alias_end = gAliases + NS_ARRAY_LENGTH(gAliases);
         alias < alias_end; ++alias) {
      if (aProperty.LowerCaseEqualsASCII(alias->name)) {
        res = alias->id;
        break;
      }
    }
  }
  return res;
}

nsCSSProperty
nsCSSProps::LookupProperty(const nsAString& aProperty)
{
  // This is faster than converting and calling
  // LookupProperty(nsACString&).  The table will do its own
  // converting and avoid a PromiseFlatCString() call.
  NS_ABORT_IF_FALSE(gPropertyTable, "no lookup table, needs addref");
  nsCSSProperty res = nsCSSProperty(gPropertyTable->Lookup(aProperty));
  if (res == eCSSProperty_UNKNOWN) {
    for (const CSSPropertyAlias *alias = gAliases,
                            *alias_end = gAliases + NS_ARRAY_LENGTH(gAliases);
         alias < alias_end; ++alias) {
      if (aProperty.LowerCaseEqualsASCII(alias->name)) {
        res = alias->id;
        break;
      }
    }
  }
  return res;
}

nsCSSFontDesc
nsCSSProps::LookupFontDesc(const nsACString& aFontDesc)
{
  NS_ABORT_IF_FALSE(gFontDescTable, "no lookup table, needs addref");
  return nsCSSFontDesc(gFontDescTable->Lookup(aFontDesc));
}

nsCSSFontDesc
nsCSSProps::LookupFontDesc(const nsAString& aFontDesc)
{
  NS_ABORT_IF_FALSE(gFontDescTable, "no lookup table, needs addref");
  return nsCSSFontDesc(gFontDescTable->Lookup(aFontDesc));
}

const nsAFlatCString&
nsCSSProps::GetStringValue(nsCSSProperty aProperty)
{
  NS_ABORT_IF_FALSE(gPropertyTable, "no lookup table, needs addref");
  if (gPropertyTable) {
    return gPropertyTable->GetStringValue(PRInt32(aProperty));
  } else {
    static nsDependentCString sNullStr("");
    return sNullStr;
  }
}

const nsAFlatCString&
nsCSSProps::GetStringValue(nsCSSFontDesc aFontDescID)
{
  NS_ABORT_IF_FALSE(gFontDescTable, "no lookup table, needs addref");
  if (gFontDescTable) {
    return gFontDescTable->GetStringValue(PRInt32(aFontDescID));
  } else {
    static nsDependentCString sNullStr("");
    return sNullStr;
  }
}

nsCSSProperty
nsCSSProps::OtherNameFor(nsCSSProperty aProperty)
{
  switch (aProperty) {
    case eCSSProperty_border_left_color_value:
      return eCSSProperty_border_left_color;
    case eCSSProperty_border_left_style_value:
      return eCSSProperty_border_left_style;
    case eCSSProperty_border_left_width_value:
      return eCSSProperty_border_left_width;
    case eCSSProperty_border_right_color_value:
      return eCSSProperty_border_right_color;
    case eCSSProperty_border_right_style_value:
      return eCSSProperty_border_right_style;
    case eCSSProperty_border_right_width_value:
      return eCSSProperty_border_right_width;
    case eCSSProperty_margin_left_value:
      return eCSSProperty_margin_left;
    case eCSSProperty_margin_right_value:
      return eCSSProperty_margin_right;
    case eCSSProperty_padding_left_value:
      return eCSSProperty_padding_left;
    case eCSSProperty_padding_right_value:
      return eCSSProperty_padding_right;
    default:
      NS_ABORT_IF_FALSE(PR_FALSE, "bad caller");
  }
  return eCSSProperty_UNKNOWN;
}

/***************************************************************************/

const PRInt32 nsCSSProps::kAnimationDirectionKTable[] = {
  eCSSKeyword_normal, NS_STYLE_ANIMATION_DIRECTION_NORMAL,
  eCSSKeyword_alternate, NS_STYLE_ANIMATION_DIRECTION_ALTERNATE,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kAnimationFillModeKTable[] = {
  eCSSKeyword_none, NS_STYLE_ANIMATION_FILL_MODE_NONE,
  eCSSKeyword_forwards, NS_STYLE_ANIMATION_FILL_MODE_FORWARDS,
  eCSSKeyword_backwards, NS_STYLE_ANIMATION_FILL_MODE_BACKWARDS,
  eCSSKeyword_both, NS_STYLE_ANIMATION_FILL_MODE_BOTH,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kAnimationIterationCountKTable[] = {
  eCSSKeyword_infinite, NS_STYLE_ANIMATION_ITERATION_COUNT_INFINITE,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kAnimationPlayStateKTable[] = {
  eCSSKeyword_running, NS_STYLE_ANIMATION_PLAY_STATE_RUNNING,
  eCSSKeyword_paused, NS_STYLE_ANIMATION_PLAY_STATE_PAUSED,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kAppearanceKTable[] = {
  eCSSKeyword_none,                   NS_THEME_NONE,
  eCSSKeyword_button,                 NS_THEME_BUTTON,
  eCSSKeyword_radio,                  NS_THEME_RADIO,
  eCSSKeyword_checkbox,               NS_THEME_CHECKBOX,
  eCSSKeyword_button_bevel,           NS_THEME_BUTTON_BEVEL,
  eCSSKeyword_toolbox,                NS_THEME_TOOLBOX,
  eCSSKeyword_toolbar,                NS_THEME_TOOLBAR,
  eCSSKeyword_toolbarbutton,          NS_THEME_TOOLBAR_BUTTON,
  eCSSKeyword_toolbargripper,         NS_THEME_TOOLBAR_GRIPPER,
  eCSSKeyword_dualbutton,             NS_THEME_TOOLBAR_DUAL_BUTTON,
  eCSSKeyword_toolbarbutton_dropdown, NS_THEME_TOOLBAR_BUTTON_DROPDOWN,
  eCSSKeyword_button_arrow_up,        NS_THEME_BUTTON_ARROW_UP,
  eCSSKeyword_button_arrow_down,      NS_THEME_BUTTON_ARROW_DOWN,
  eCSSKeyword_button_arrow_next,      NS_THEME_BUTTON_ARROW_NEXT,
  eCSSKeyword_button_arrow_previous,  NS_THEME_BUTTON_ARROW_PREVIOUS,
  eCSSKeyword_separator,              NS_THEME_TOOLBAR_SEPARATOR,
  eCSSKeyword_splitter,               NS_THEME_SPLITTER,
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
  eCSSKeyword_tabpanels,              NS_THEME_TAB_PANELS,
  eCSSKeyword_tabpanel,               NS_THEME_TAB_PANEL,
  eCSSKeyword_tabscrollarrow_back,    NS_THEME_TAB_SCROLLARROW_BACK,
  eCSSKeyword_tabscrollarrow_forward, NS_THEME_TAB_SCROLLARROW_FORWARD,
  eCSSKeyword_tooltip,                NS_THEME_TOOLTIP,
  eCSSKeyword_spinner,                NS_THEME_SPINNER,
  eCSSKeyword_spinner_upbutton,       NS_THEME_SPINNER_UP_BUTTON,
  eCSSKeyword_spinner_downbutton,     NS_THEME_SPINNER_DOWN_BUTTON,
  eCSSKeyword_spinner_textfield,      NS_THEME_SPINNER_TEXTFIELD,
  eCSSKeyword_scrollbar,              NS_THEME_SCROLLBAR,
  eCSSKeyword_scrollbar_small,        NS_THEME_SCROLLBAR_SMALL,
  eCSSKeyword_scrollbarbutton_up,     NS_THEME_SCROLLBAR_BUTTON_UP,
  eCSSKeyword_scrollbarbutton_down,   NS_THEME_SCROLLBAR_BUTTON_DOWN,
  eCSSKeyword_scrollbarbutton_left,   NS_THEME_SCROLLBAR_BUTTON_LEFT,
  eCSSKeyword_scrollbarbutton_right,  NS_THEME_SCROLLBAR_BUTTON_RIGHT,
  eCSSKeyword_scrollbartrack_horizontal,    NS_THEME_SCROLLBAR_TRACK_HORIZONTAL,
  eCSSKeyword_scrollbartrack_vertical,      NS_THEME_SCROLLBAR_TRACK_VERTICAL,
  eCSSKeyword_scrollbarthumb_horizontal,    NS_THEME_SCROLLBAR_THUMB_HORIZONTAL,
  eCSSKeyword_scrollbarthumb_vertical,      NS_THEME_SCROLLBAR_THUMB_VERTICAL,
  eCSSKeyword_textfield,              NS_THEME_TEXTFIELD,
  eCSSKeyword_textfield_multiline,    NS_THEME_TEXTFIELD_MULTILINE,
  eCSSKeyword_caret,                  NS_THEME_TEXTFIELD_CARET,
  eCSSKeyword_searchfield,            NS_THEME_SEARCHFIELD,
  eCSSKeyword_menulist,               NS_THEME_DROPDOWN,
  eCSSKeyword_menulistbutton,         NS_THEME_DROPDOWN_BUTTON,
  eCSSKeyword_menulisttext,           NS_THEME_DROPDOWN_TEXT,
  eCSSKeyword_menulisttextfield,      NS_THEME_DROPDOWN_TEXTFIELD,
  eCSSKeyword_scale_horizontal,       NS_THEME_SCALE_HORIZONTAL,
  eCSSKeyword_scale_vertical,         NS_THEME_SCALE_VERTICAL,
  eCSSKeyword_scalethumb_horizontal,  NS_THEME_SCALE_THUMB_HORIZONTAL,
  eCSSKeyword_scalethumb_vertical,    NS_THEME_SCALE_THUMB_VERTICAL,
  eCSSKeyword_scalethumbstart,        NS_THEME_SCALE_THUMB_START,
  eCSSKeyword_scalethumbend,          NS_THEME_SCALE_THUMB_END,
  eCSSKeyword_scalethumbtick,         NS_THEME_SCALE_TICK,
  eCSSKeyword_groupbox,               NS_THEME_GROUPBOX,
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
  eCSSKeyword_checkmenuitem,          NS_THEME_CHECKMENUITEM,
  eCSSKeyword_radiomenuitem,          NS_THEME_RADIOMENUITEM,
  eCSSKeyword_menucheckbox,           NS_THEME_MENUCHECKBOX,
  eCSSKeyword_menuradio,              NS_THEME_MENURADIO,
  eCSSKeyword_menuseparator,          NS_THEME_MENUSEPARATOR,
  eCSSKeyword_menuarrow,              NS_THEME_MENUARROW,
  eCSSKeyword_menuimage,              NS_THEME_MENUIMAGE,
  eCSSKeyword_menuitemtext,           NS_THEME_MENUITEMTEXT,
  eCSSKeyword__moz_win_media_toolbox, NS_THEME_WIN_MEDIA_TOOLBOX,
  eCSSKeyword__moz_win_communications_toolbox, NS_THEME_WIN_COMMUNICATIONS_TOOLBOX,
  eCSSKeyword__moz_win_browsertabbar_toolbox,  NS_THEME_WIN_BROWSER_TAB_BAR_TOOLBOX,
  eCSSKeyword__moz_win_glass,         NS_THEME_WIN_GLASS,
  eCSSKeyword__moz_win_borderless_glass,      NS_THEME_WIN_BORDERLESS_GLASS,
  eCSSKeyword__moz_mac_unified_toolbar,       NS_THEME_MOZ_MAC_UNIFIED_TOOLBAR,
  eCSSKeyword__moz_window_titlebar,           NS_THEME_WINDOW_TITLEBAR,
  eCSSKeyword__moz_window_titlebar_maximized, NS_THEME_WINDOW_TITLEBAR_MAXIMIZED,
  eCSSKeyword__moz_window_frame_left,         NS_THEME_WINDOW_FRAME_LEFT,
  eCSSKeyword__moz_window_frame_right,        NS_THEME_WINDOW_FRAME_RIGHT,
  eCSSKeyword__moz_window_frame_bottom,       NS_THEME_WINDOW_FRAME_BOTTOM,
  eCSSKeyword__moz_window_button_close,       NS_THEME_WINDOW_BUTTON_CLOSE,
  eCSSKeyword__moz_window_button_minimize,    NS_THEME_WINDOW_BUTTON_MINIMIZE,
  eCSSKeyword__moz_window_button_maximize,    NS_THEME_WINDOW_BUTTON_MAXIMIZE,
  eCSSKeyword__moz_window_button_restore,     NS_THEME_WINDOW_BUTTON_RESTORE,
  eCSSKeyword__moz_window_button_box,         NS_THEME_WINDOW_BUTTON_BOX,
  eCSSKeyword__moz_window_button_box_maximized, NS_THEME_WINDOW_BUTTON_BOX_MAXIMIZED,
  eCSSKeyword__moz_win_exclude_glass,         NS_THEME_WIN_EXCLUDE_GLASS,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kBackfaceVisibilityKTable[] = {
  eCSSKeyword_visible, NS_STYLE_BACKFACE_VISIBILITY_VISIBLE,
  eCSSKeyword_hidden, NS_STYLE_BACKFACE_VISIBILITY_HIDDEN
};

const PRInt32 nsCSSProps::kBackgroundAttachmentKTable[] = {
  eCSSKeyword_fixed, NS_STYLE_BG_ATTACHMENT_FIXED,
  eCSSKeyword_scroll, NS_STYLE_BG_ATTACHMENT_SCROLL,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kBackgroundInlinePolicyKTable[] = {
  eCSSKeyword_each_box,     NS_STYLE_BG_INLINE_POLICY_EACH_BOX,
  eCSSKeyword_continuous,   NS_STYLE_BG_INLINE_POLICY_CONTINUOUS,
  eCSSKeyword_bounding_box, NS_STYLE_BG_INLINE_POLICY_BOUNDING_BOX,
  eCSSKeyword_UNKNOWN,-1
};

PR_STATIC_ASSERT(NS_STYLE_BG_CLIP_BORDER == NS_STYLE_BG_ORIGIN_BORDER);
PR_STATIC_ASSERT(NS_STYLE_BG_CLIP_PADDING == NS_STYLE_BG_ORIGIN_PADDING);
PR_STATIC_ASSERT(NS_STYLE_BG_CLIP_CONTENT == NS_STYLE_BG_ORIGIN_CONTENT);
const PRInt32 nsCSSProps::kBackgroundOriginKTable[] = {
  eCSSKeyword_border_box, NS_STYLE_BG_ORIGIN_BORDER,
  eCSSKeyword_padding_box, NS_STYLE_BG_ORIGIN_PADDING,
  eCSSKeyword_content_box, NS_STYLE_BG_ORIGIN_CONTENT,
  eCSSKeyword_UNKNOWN,-1
};

// Note: Don't change this table unless you update
// parseBackgroundPosition!

const PRInt32 nsCSSProps::kBackgroundPositionKTable[] = {
  eCSSKeyword_center, NS_STYLE_BG_POSITION_CENTER,
  eCSSKeyword_top, NS_STYLE_BG_POSITION_TOP,
  eCSSKeyword_bottom, NS_STYLE_BG_POSITION_BOTTOM,
  eCSSKeyword_left, NS_STYLE_BG_POSITION_LEFT,
  eCSSKeyword_right, NS_STYLE_BG_POSITION_RIGHT,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kBackgroundRepeatKTable[] = {
  eCSSKeyword_no_repeat,  NS_STYLE_BG_REPEAT_OFF,
  eCSSKeyword_repeat,     NS_STYLE_BG_REPEAT_XY,
  eCSSKeyword_repeat_x,   NS_STYLE_BG_REPEAT_X,
  eCSSKeyword_repeat_y,   NS_STYLE_BG_REPEAT_Y,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kBackgroundSizeKTable[] = {
  eCSSKeyword_contain, NS_STYLE_BG_SIZE_CONTAIN,
  eCSSKeyword_cover,   NS_STYLE_BG_SIZE_COVER,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kBorderCollapseKTable[] = {
  eCSSKeyword_collapse,  NS_STYLE_BORDER_COLLAPSE,
  eCSSKeyword_separate,  NS_STYLE_BORDER_SEPARATE,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kBorderColorKTable[] = {
  eCSSKeyword__moz_use_text_color, NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kBorderImageKTable[] = {
  eCSSKeyword_stretch, NS_STYLE_BORDER_IMAGE_STRETCH,
  eCSSKeyword_repeat, NS_STYLE_BORDER_IMAGE_REPEAT,
  eCSSKeyword_round, NS_STYLE_BORDER_IMAGE_ROUND,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kBorderStyleKTable[] = {
  eCSSKeyword_none,   NS_STYLE_BORDER_STYLE_NONE,
  eCSSKeyword_hidden, NS_STYLE_BORDER_STYLE_HIDDEN,
  eCSSKeyword_dotted, NS_STYLE_BORDER_STYLE_DOTTED,
  eCSSKeyword_dashed, NS_STYLE_BORDER_STYLE_DASHED,
  eCSSKeyword_solid,  NS_STYLE_BORDER_STYLE_SOLID,
  eCSSKeyword_double, NS_STYLE_BORDER_STYLE_DOUBLE,
  eCSSKeyword_groove, NS_STYLE_BORDER_STYLE_GROOVE,
  eCSSKeyword_ridge,  NS_STYLE_BORDER_STYLE_RIDGE,
  eCSSKeyword_inset,  NS_STYLE_BORDER_STYLE_INSET,
  eCSSKeyword_outset, NS_STYLE_BORDER_STYLE_OUTSET,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kBorderWidthKTable[] = {
  eCSSKeyword_thin, NS_STYLE_BORDER_WIDTH_THIN,
  eCSSKeyword_medium, NS_STYLE_BORDER_WIDTH_MEDIUM,
  eCSSKeyword_thick, NS_STYLE_BORDER_WIDTH_THICK,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kBoxPropSourceKTable[] = {
  eCSSKeyword_physical,     NS_BOXPROP_SOURCE_PHYSICAL,
  eCSSKeyword_logical,      NS_BOXPROP_SOURCE_LOGICAL,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kBoxShadowTypeKTable[] = {
  eCSSKeyword_inset, NS_STYLE_BOX_SHADOW_INSET,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kBoxSizingKTable[] = {
  eCSSKeyword_content_box,  NS_STYLE_BOX_SIZING_CONTENT,
  eCSSKeyword_border_box,   NS_STYLE_BOX_SIZING_BORDER,
  eCSSKeyword_padding_box,  NS_STYLE_BOX_SIZING_PADDING,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kCaptionSideKTable[] = {
  eCSSKeyword_top,                  NS_STYLE_CAPTION_SIDE_TOP,
  eCSSKeyword_right,                NS_STYLE_CAPTION_SIDE_RIGHT,
  eCSSKeyword_bottom,               NS_STYLE_CAPTION_SIDE_BOTTOM,
  eCSSKeyword_left,                 NS_STYLE_CAPTION_SIDE_LEFT,
  eCSSKeyword_top_outside,          NS_STYLE_CAPTION_SIDE_TOP_OUTSIDE,
  eCSSKeyword_bottom_outside,       NS_STYLE_CAPTION_SIDE_BOTTOM_OUTSIDE,
  eCSSKeyword_UNKNOWN,              -1
};

const PRInt32 nsCSSProps::kClearKTable[] = {
  eCSSKeyword_none, NS_STYLE_CLEAR_NONE,
  eCSSKeyword_left, NS_STYLE_CLEAR_LEFT,
  eCSSKeyword_right, NS_STYLE_CLEAR_RIGHT,
  eCSSKeyword_both, NS_STYLE_CLEAR_LEFT_AND_RIGHT,
  eCSSKeyword_UNKNOWN,-1
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
  eCSSKeyword__moz_buttonhoverface, nsILookAndFeel::eColor__moz_buttonhoverface,
  eCSSKeyword__moz_buttonhovertext, nsILookAndFeel::eColor__moz_buttonhovertext,
  eCSSKeyword__moz_cellhighlight, nsILookAndFeel::eColor__moz_cellhighlight,
  eCSSKeyword__moz_cellhighlighttext, nsILookAndFeel::eColor__moz_cellhighlighttext,
  eCSSKeyword__moz_eventreerow, nsILookAndFeel::eColor__moz_eventreerow,
  eCSSKeyword__moz_field, nsILookAndFeel::eColor__moz_field,
  eCSSKeyword__moz_fieldtext, nsILookAndFeel::eColor__moz_fieldtext,
  eCSSKeyword__moz_default_background_color, NS_COLOR_MOZ_DEFAULT_BACKGROUND_COLOR,
  eCSSKeyword__moz_default_color, NS_COLOR_MOZ_DEFAULT_COLOR,
  eCSSKeyword__moz_dialog, nsILookAndFeel::eColor__moz_dialog,
  eCSSKeyword__moz_dialogtext, nsILookAndFeel::eColor__moz_dialogtext,
  eCSSKeyword__moz_dragtargetzone, nsILookAndFeel::eColor__moz_dragtargetzone,
  eCSSKeyword__moz_hyperlinktext, NS_COLOR_MOZ_HYPERLINKTEXT,
  eCSSKeyword__moz_html_cellhighlight, nsILookAndFeel::eColor__moz_html_cellhighlight,
  eCSSKeyword__moz_html_cellhighlighttext, nsILookAndFeel::eColor__moz_html_cellhighlighttext,
  eCSSKeyword__moz_mac_chrome_active, nsILookAndFeel::eColor__moz_mac_chrome_active,
  eCSSKeyword__moz_mac_chrome_inactive, nsILookAndFeel::eColor__moz_mac_chrome_inactive,
  eCSSKeyword__moz_mac_focusring, nsILookAndFeel::eColor__moz_mac_focusring,
  eCSSKeyword__moz_mac_menuselect, nsILookAndFeel::eColor__moz_mac_menuselect,
  eCSSKeyword__moz_mac_menushadow, nsILookAndFeel::eColor__moz_mac_menushadow,
  eCSSKeyword__moz_mac_menutextdisable, nsILookAndFeel::eColor__moz_mac_menutextdisable,
  eCSSKeyword__moz_mac_menutextselect, nsILookAndFeel::eColor__moz_mac_menutextselect,
  eCSSKeyword__moz_mac_disabledtoolbartext, nsILookAndFeel::eColor__moz_mac_disabledtoolbartext,
  eCSSKeyword__moz_mac_alternateprimaryhighlight, nsILookAndFeel::eColor__moz_mac_alternateprimaryhighlight,
  eCSSKeyword__moz_mac_secondaryhighlight, nsILookAndFeel::eColor__moz_mac_secondaryhighlight,
  eCSSKeyword__moz_menuhover, nsILookAndFeel::eColor__moz_menuhover,
  eCSSKeyword__moz_menuhovertext, nsILookAndFeel::eColor__moz_menuhovertext,
  eCSSKeyword__moz_menubartext, nsILookAndFeel::eColor__moz_menubartext,
  eCSSKeyword__moz_menubarhovertext, nsILookAndFeel::eColor__moz_menubarhovertext,
  eCSSKeyword__moz_oddtreerow, nsILookAndFeel::eColor__moz_oddtreerow,
  eCSSKeyword__moz_visitedhyperlinktext, NS_COLOR_MOZ_VISITEDHYPERLINKTEXT,
  eCSSKeyword_currentcolor, NS_COLOR_CURRENTCOLOR,
  eCSSKeyword__moz_win_mediatext, nsILookAndFeel::eColor__moz_win_mediatext,
  eCSSKeyword__moz_win_communicationstext, nsILookAndFeel::eColor__moz_win_communicationstext,
  eCSSKeyword__moz_nativehyperlinktext, nsILookAndFeel::eColor__moz_nativehyperlinktext,
  eCSSKeyword__moz_comboboxtext, nsILookAndFeel::eColor__moz_comboboxtext,
  eCSSKeyword__moz_combobox, nsILookAndFeel::eColor__moz_combobox,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kContentKTable[] = {
  eCSSKeyword_open_quote, NS_STYLE_CONTENT_OPEN_QUOTE,
  eCSSKeyword_close_quote, NS_STYLE_CONTENT_CLOSE_QUOTE,
  eCSSKeyword_no_open_quote, NS_STYLE_CONTENT_NO_OPEN_QUOTE,
  eCSSKeyword_no_close_quote, NS_STYLE_CONTENT_NO_CLOSE_QUOTE,
  eCSSKeyword__moz_alt_content, NS_STYLE_CONTENT_ALT_CONTENT,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kCursorKTable[] = {
  // CSS 2.0
  eCSSKeyword_auto, NS_STYLE_CURSOR_AUTO,
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
  // CSS3 basic user interface module
  eCSSKeyword_copy, NS_STYLE_CURSOR_COPY,
  eCSSKeyword_alias, NS_STYLE_CURSOR_ALIAS,
  eCSSKeyword_context_menu, NS_STYLE_CURSOR_CONTEXT_MENU,
  eCSSKeyword_cell, NS_STYLE_CURSOR_CELL,
  eCSSKeyword_not_allowed, NS_STYLE_CURSOR_NOT_ALLOWED,
  eCSSKeyword_col_resize, NS_STYLE_CURSOR_COL_RESIZE,
  eCSSKeyword_row_resize, NS_STYLE_CURSOR_ROW_RESIZE,
  eCSSKeyword_no_drop, NS_STYLE_CURSOR_NO_DROP,
  eCSSKeyword_vertical_text, NS_STYLE_CURSOR_VERTICAL_TEXT,
  eCSSKeyword_all_scroll, NS_STYLE_CURSOR_ALL_SCROLL,
  eCSSKeyword_nesw_resize, NS_STYLE_CURSOR_NESW_RESIZE,
  eCSSKeyword_nwse_resize, NS_STYLE_CURSOR_NWSE_RESIZE,
  eCSSKeyword_ns_resize, NS_STYLE_CURSOR_NS_RESIZE,
  eCSSKeyword_ew_resize, NS_STYLE_CURSOR_EW_RESIZE,
  eCSSKeyword_none, NS_STYLE_CURSOR_NONE,
  // -moz- prefixed vendor specific
  eCSSKeyword__moz_grab, NS_STYLE_CURSOR_GRAB,
  eCSSKeyword__moz_grabbing, NS_STYLE_CURSOR_GRABBING,
  eCSSKeyword__moz_zoom_in, NS_STYLE_CURSOR_MOZ_ZOOM_IN,
  eCSSKeyword__moz_zoom_out, NS_STYLE_CURSOR_MOZ_ZOOM_OUT,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kDirectionKTable[] = {
  eCSSKeyword_ltr,      NS_STYLE_DIRECTION_LTR,
  eCSSKeyword_rtl,      NS_STYLE_DIRECTION_RTL,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kDisplayKTable[] = {
  eCSSKeyword_none,               NS_STYLE_DISPLAY_NONE,
  eCSSKeyword_inline,             NS_STYLE_DISPLAY_INLINE,
  eCSSKeyword_block,              NS_STYLE_DISPLAY_BLOCK,
  eCSSKeyword_inline_block,       NS_STYLE_DISPLAY_INLINE_BLOCK,
  eCSSKeyword_list_item,          NS_STYLE_DISPLAY_LIST_ITEM,
  eCSSKeyword_table,              NS_STYLE_DISPLAY_TABLE,
  eCSSKeyword_inline_table,       NS_STYLE_DISPLAY_INLINE_TABLE,
  eCSSKeyword_table_row_group,    NS_STYLE_DISPLAY_TABLE_ROW_GROUP,
  eCSSKeyword_table_header_group, NS_STYLE_DISPLAY_TABLE_HEADER_GROUP,
  eCSSKeyword_table_footer_group, NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP,
  eCSSKeyword_table_row,          NS_STYLE_DISPLAY_TABLE_ROW,
  eCSSKeyword_table_column_group, NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP,
  eCSSKeyword_table_column,       NS_STYLE_DISPLAY_TABLE_COLUMN,
  eCSSKeyword_table_cell,         NS_STYLE_DISPLAY_TABLE_CELL,
  eCSSKeyword_table_caption,      NS_STYLE_DISPLAY_TABLE_CAPTION,
  // Make sure this is kept in sync with the code in
  // nsCSSFrameConstructor::ConstructXULFrame
  eCSSKeyword__moz_box,           NS_STYLE_DISPLAY_BOX,
  eCSSKeyword__moz_inline_box,    NS_STYLE_DISPLAY_INLINE_BOX,
#ifdef MOZ_XUL
  eCSSKeyword__moz_grid,          NS_STYLE_DISPLAY_GRID,
  eCSSKeyword__moz_inline_grid,   NS_STYLE_DISPLAY_INLINE_GRID,
  eCSSKeyword__moz_grid_group,    NS_STYLE_DISPLAY_GRID_GROUP,
  eCSSKeyword__moz_grid_line,     NS_STYLE_DISPLAY_GRID_LINE,
  eCSSKeyword__moz_stack,         NS_STYLE_DISPLAY_STACK,
  eCSSKeyword__moz_inline_stack,  NS_STYLE_DISPLAY_INLINE_STACK,
  eCSSKeyword__moz_deck,          NS_STYLE_DISPLAY_DECK,
  eCSSKeyword__moz_popup,         NS_STYLE_DISPLAY_POPUP,
  eCSSKeyword__moz_groupbox,      NS_STYLE_DISPLAY_GROUPBOX,
#endif
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kEmptyCellsKTable[] = {
  eCSSKeyword_show,                 NS_STYLE_TABLE_EMPTY_CELLS_SHOW,
  eCSSKeyword_hide,                 NS_STYLE_TABLE_EMPTY_CELLS_HIDE,
  eCSSKeyword__moz_show_background, NS_STYLE_TABLE_EMPTY_CELLS_SHOW_BACKGROUND,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kFloatKTable[] = {
  eCSSKeyword_none,  NS_STYLE_FLOAT_NONE,
  eCSSKeyword_left,  NS_STYLE_FLOAT_LEFT,
  eCSSKeyword_right, NS_STYLE_FLOAT_RIGHT,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kFloatEdgeKTable[] = {
  eCSSKeyword_content_box,  NS_STYLE_FLOAT_EDGE_CONTENT,
  eCSSKeyword_margin_box,  NS_STYLE_FLOAT_EDGE_MARGIN,
  eCSSKeyword_UNKNOWN,-1
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
  eCSSKeyword_UNKNOWN,-1
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
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kFontStretchKTable[] = {
  eCSSKeyword_ultra_condensed, NS_STYLE_FONT_STRETCH_ULTRA_CONDENSED,
  eCSSKeyword_extra_condensed, NS_STYLE_FONT_STRETCH_EXTRA_CONDENSED,
  eCSSKeyword_condensed, NS_STYLE_FONT_STRETCH_CONDENSED,
  eCSSKeyword_semi_condensed, NS_STYLE_FONT_STRETCH_SEMI_CONDENSED,
  eCSSKeyword_normal, NS_STYLE_FONT_STRETCH_NORMAL,
  eCSSKeyword_semi_expanded, NS_STYLE_FONT_STRETCH_SEMI_EXPANDED,
  eCSSKeyword_expanded, NS_STYLE_FONT_STRETCH_EXPANDED,
  eCSSKeyword_extra_expanded, NS_STYLE_FONT_STRETCH_EXTRA_EXPANDED,
  eCSSKeyword_ultra_expanded, NS_STYLE_FONT_STRETCH_ULTRA_EXPANDED,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kFontStyleKTable[] = {
  eCSSKeyword_normal, NS_STYLE_FONT_STYLE_NORMAL,
  eCSSKeyword_italic, NS_STYLE_FONT_STYLE_ITALIC,
  eCSSKeyword_oblique, NS_STYLE_FONT_STYLE_OBLIQUE,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kFontVariantKTable[] = {
  eCSSKeyword_normal, NS_STYLE_FONT_VARIANT_NORMAL,
  eCSSKeyword_small_caps, NS_STYLE_FONT_VARIANT_SMALL_CAPS,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kFontWeightKTable[] = {
  eCSSKeyword_normal, NS_STYLE_FONT_WEIGHT_NORMAL,
  eCSSKeyword_bold, NS_STYLE_FONT_WEIGHT_BOLD,
  eCSSKeyword_bolder, NS_STYLE_FONT_WEIGHT_BOLDER,
  eCSSKeyword_lighter, NS_STYLE_FONT_WEIGHT_LIGHTER,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kIMEModeKTable[] = {
  eCSSKeyword_normal, NS_STYLE_IME_MODE_NORMAL,
  eCSSKeyword_auto, NS_STYLE_IME_MODE_AUTO,
  eCSSKeyword_active, NS_STYLE_IME_MODE_ACTIVE,
  eCSSKeyword_disabled, NS_STYLE_IME_MODE_DISABLED,
  eCSSKeyword_inactive, NS_STYLE_IME_MODE_INACTIVE,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kLineHeightKTable[] = {
  // -moz- prefixed, intended for internal use for single-line controls
  eCSSKeyword__moz_block_height, NS_STYLE_LINE_HEIGHT_BLOCK_HEIGHT,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kListStylePositionKTable[] = {
  eCSSKeyword_inside, NS_STYLE_LIST_STYLE_POSITION_INSIDE,
  eCSSKeyword_outside, NS_STYLE_LIST_STYLE_POSITION_OUTSIDE,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kListStyleKTable[] = {
  eCSSKeyword_none, NS_STYLE_LIST_STYLE_NONE,
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
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kOrientKTable[] = {
  eCSSKeyword_horizontal, NS_STYLE_ORIENT_HORIZONTAL,
  eCSSKeyword_vertical,   NS_STYLE_ORIENT_VERTICAL,
  eCSSKeyword_UNKNOWN,    -1
};

// Same as kBorderStyleKTable except 'hidden'.
const PRInt32 nsCSSProps::kOutlineStyleKTable[] = {
  eCSSKeyword_none,   NS_STYLE_BORDER_STYLE_NONE,
  eCSSKeyword_auto,   NS_STYLE_BORDER_STYLE_AUTO,
  eCSSKeyword_dotted, NS_STYLE_BORDER_STYLE_DOTTED,
  eCSSKeyword_dashed, NS_STYLE_BORDER_STYLE_DASHED,
  eCSSKeyword_solid,  NS_STYLE_BORDER_STYLE_SOLID,
  eCSSKeyword_double, NS_STYLE_BORDER_STYLE_DOUBLE,
  eCSSKeyword_groove, NS_STYLE_BORDER_STYLE_GROOVE,
  eCSSKeyword_ridge,  NS_STYLE_BORDER_STYLE_RIDGE,
  eCSSKeyword_inset,  NS_STYLE_BORDER_STYLE_INSET,
  eCSSKeyword_outset, NS_STYLE_BORDER_STYLE_OUTSET,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kOutlineColorKTable[] = {
#ifdef GFX_HAS_INVERT
  eCSSKeyword_invert, NS_STYLE_COLOR_INVERT,
#else
  eCSSKeyword__moz_use_text_color, NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR,
#endif
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kOverflowKTable[] = {
  eCSSKeyword_auto, NS_STYLE_OVERFLOW_AUTO,
  eCSSKeyword_visible, NS_STYLE_OVERFLOW_VISIBLE,
  eCSSKeyword_hidden, NS_STYLE_OVERFLOW_HIDDEN,
  eCSSKeyword_scroll, NS_STYLE_OVERFLOW_SCROLL,
  // Deprecated:
  eCSSKeyword__moz_scrollbars_none, NS_STYLE_OVERFLOW_HIDDEN,
  eCSSKeyword__moz_scrollbars_horizontal, NS_STYLE_OVERFLOW_SCROLLBARS_HORIZONTAL,
  eCSSKeyword__moz_scrollbars_vertical, NS_STYLE_OVERFLOW_SCROLLBARS_VERTICAL,
  eCSSKeyword__moz_hidden_unscrollable, NS_STYLE_OVERFLOW_CLIP,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kOverflowSubKTable[] = {
  eCSSKeyword_auto, NS_STYLE_OVERFLOW_AUTO,
  eCSSKeyword_visible, NS_STYLE_OVERFLOW_VISIBLE,
  eCSSKeyword_hidden, NS_STYLE_OVERFLOW_HIDDEN,
  eCSSKeyword_scroll, NS_STYLE_OVERFLOW_SCROLL,
  // Deprecated:
  eCSSKeyword__moz_hidden_unscrollable, NS_STYLE_OVERFLOW_CLIP,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kPageBreakKTable[] = {
  eCSSKeyword_auto, NS_STYLE_PAGE_BREAK_AUTO,
  eCSSKeyword_always, NS_STYLE_PAGE_BREAK_ALWAYS,
  eCSSKeyword_avoid, NS_STYLE_PAGE_BREAK_AVOID,
  eCSSKeyword_left, NS_STYLE_PAGE_BREAK_LEFT,
  eCSSKeyword_right, NS_STYLE_PAGE_BREAK_RIGHT,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kPageBreakInsideKTable[] = {
  eCSSKeyword_auto, NS_STYLE_PAGE_BREAK_AUTO,
  eCSSKeyword_avoid, NS_STYLE_PAGE_BREAK_AVOID,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kPageMarksKTable[] = {
  eCSSKeyword_none, NS_STYLE_PAGE_MARKS_NONE,
  eCSSKeyword_crop, NS_STYLE_PAGE_MARKS_CROP,
  eCSSKeyword_cross, NS_STYLE_PAGE_MARKS_REGISTER,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kPageSizeKTable[] = {
  eCSSKeyword_landscape, NS_STYLE_PAGE_SIZE_LANDSCAPE,
  eCSSKeyword_portrait, NS_STYLE_PAGE_SIZE_PORTRAIT,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kPointerEventsKTable[] = {
  eCSSKeyword_none, NS_STYLE_POINTER_EVENTS_NONE,
  eCSSKeyword_visiblepainted, NS_STYLE_POINTER_EVENTS_VISIBLEPAINTED,
  eCSSKeyword_visiblefill, NS_STYLE_POINTER_EVENTS_VISIBLEFILL,
  eCSSKeyword_visiblestroke, NS_STYLE_POINTER_EVENTS_VISIBLESTROKE,
  eCSSKeyword_visible, NS_STYLE_POINTER_EVENTS_VISIBLE,
  eCSSKeyword_painted, NS_STYLE_POINTER_EVENTS_PAINTED,
  eCSSKeyword_fill, NS_STYLE_POINTER_EVENTS_FILL,
  eCSSKeyword_stroke, NS_STYLE_POINTER_EVENTS_STROKE,
  eCSSKeyword_all, NS_STYLE_POINTER_EVENTS_ALL,
  eCSSKeyword_auto, NS_STYLE_POINTER_EVENTS_AUTO,
  eCSSKeyword_UNKNOWN, -1
};

const PRInt32 nsCSSProps::kPositionKTable[] = {
  eCSSKeyword_static, NS_STYLE_POSITION_STATIC,
  eCSSKeyword_relative, NS_STYLE_POSITION_RELATIVE,
  eCSSKeyword_absolute, NS_STYLE_POSITION_ABSOLUTE,
  eCSSKeyword_fixed, NS_STYLE_POSITION_FIXED,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kRadialGradientShapeKTable[] = {
  eCSSKeyword_circle,  NS_STYLE_GRADIENT_SHAPE_CIRCULAR,
  eCSSKeyword_ellipse, NS_STYLE_GRADIENT_SHAPE_ELLIPTICAL,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kRadialGradientSizeKTable[] = {
  eCSSKeyword_closest_side,    NS_STYLE_GRADIENT_SIZE_CLOSEST_SIDE,
  eCSSKeyword_closest_corner,  NS_STYLE_GRADIENT_SIZE_CLOSEST_CORNER,
  eCSSKeyword_farthest_side,   NS_STYLE_GRADIENT_SIZE_FARTHEST_SIDE,
  eCSSKeyword_farthest_corner, NS_STYLE_GRADIENT_SIZE_FARTHEST_CORNER,
  // synonyms
  eCSSKeyword_contain,         NS_STYLE_GRADIENT_SIZE_CLOSEST_SIDE,
  eCSSKeyword_cover,           NS_STYLE_GRADIENT_SIZE_FARTHEST_CORNER,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kResizeKTable[] = {
  eCSSKeyword_none,       NS_STYLE_RESIZE_NONE,
  eCSSKeyword_both,       NS_STYLE_RESIZE_BOTH,
  eCSSKeyword_horizontal, NS_STYLE_RESIZE_HORIZONTAL,
  eCSSKeyword_vertical,   NS_STYLE_RESIZE_VERTICAL,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kStackSizingKTable[] = {
  eCSSKeyword_ignore, NS_STYLE_STACK_SIZING_IGNORE,
  eCSSKeyword_stretch_to_fit, NS_STYLE_STACK_SIZING_STRETCH_TO_FIT,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kTableLayoutKTable[] = {
  eCSSKeyword_auto, NS_STYLE_TABLE_LAYOUT_AUTO,
  eCSSKeyword_fixed, NS_STYLE_TABLE_LAYOUT_FIXED,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kTextAlignKTable[] = {
  eCSSKeyword_left, NS_STYLE_TEXT_ALIGN_LEFT,
  eCSSKeyword_right, NS_STYLE_TEXT_ALIGN_RIGHT,
  eCSSKeyword_center, NS_STYLE_TEXT_ALIGN_CENTER,
  eCSSKeyword_justify, NS_STYLE_TEXT_ALIGN_JUSTIFY,
  eCSSKeyword__moz_center, NS_STYLE_TEXT_ALIGN_MOZ_CENTER,
  eCSSKeyword__moz_right, NS_STYLE_TEXT_ALIGN_MOZ_RIGHT,
  eCSSKeyword__moz_left, NS_STYLE_TEXT_ALIGN_MOZ_LEFT,
  eCSSKeyword_start, NS_STYLE_TEXT_ALIGN_DEFAULT,
  eCSSKeyword_end, NS_STYLE_TEXT_ALIGN_END,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kTextBlinkKTable[] = {
  eCSSKeyword_none, NS_STYLE_TEXT_BLINK_NONE,
  eCSSKeyword_blink, NS_STYLE_TEXT_BLINK_BLINK,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kTextDecorationLineKTable[] = {
  eCSSKeyword_none, NS_STYLE_TEXT_DECORATION_LINE_NONE,
  eCSSKeyword_underline, NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE,
  eCSSKeyword_overline, NS_STYLE_TEXT_DECORATION_LINE_OVERLINE,
  eCSSKeyword_line_through, NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH,
  eCSSKeyword__moz_anchor_decoration, NS_STYLE_TEXT_DECORATION_LINE_PREF_ANCHORS,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kTextDecorationStyleKTable[] = {
  eCSSKeyword__moz_none, NS_STYLE_TEXT_DECORATION_STYLE_NONE,
  eCSSKeyword_solid, NS_STYLE_TEXT_DECORATION_STYLE_SOLID,
  eCSSKeyword_double, NS_STYLE_TEXT_DECORATION_STYLE_DOUBLE,
  eCSSKeyword_dotted, NS_STYLE_TEXT_DECORATION_STYLE_DOTTED,
  eCSSKeyword_dashed, NS_STYLE_TEXT_DECORATION_STYLE_DASHED,
  eCSSKeyword_wavy, NS_STYLE_TEXT_DECORATION_STYLE_WAVY,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kTextOverflowKTable[] = {
  eCSSKeyword_clip, NS_STYLE_TEXT_OVERFLOW_CLIP,
  eCSSKeyword_ellipsis, NS_STYLE_TEXT_OVERFLOW_ELLIPSIS,
  eCSSKeyword_UNKNOWN, -1
};

const PRInt32 nsCSSProps::kTextTransformKTable[] = {
  eCSSKeyword_none, NS_STYLE_TEXT_TRANSFORM_NONE,
  eCSSKeyword_capitalize, NS_STYLE_TEXT_TRANSFORM_CAPITALIZE,
  eCSSKeyword_lowercase, NS_STYLE_TEXT_TRANSFORM_LOWERCASE,
  eCSSKeyword_uppercase, NS_STYLE_TEXT_TRANSFORM_UPPERCASE,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kTransitionTimingFunctionKTable[] = {
  eCSSKeyword_ease, NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE,
  eCSSKeyword_linear, NS_STYLE_TRANSITION_TIMING_FUNCTION_LINEAR,
  eCSSKeyword_ease_in, NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE_IN,
  eCSSKeyword_ease_out, NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE_OUT,
  eCSSKeyword_ease_in_out, NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE_IN_OUT,
  eCSSKeyword_step_start, NS_STYLE_TRANSITION_TIMING_FUNCTION_STEP_START,
  eCSSKeyword_step_end, NS_STYLE_TRANSITION_TIMING_FUNCTION_STEP_END,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kUnicodeBidiKTable[] = {
  eCSSKeyword_normal, NS_STYLE_UNICODE_BIDI_NORMAL,
  eCSSKeyword_embed, NS_STYLE_UNICODE_BIDI_EMBED,
  eCSSKeyword_bidi_override, NS_STYLE_UNICODE_BIDI_OVERRIDE,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kUserFocusKTable[] = {
  eCSSKeyword_none,           NS_STYLE_USER_FOCUS_NONE,
  eCSSKeyword_normal,         NS_STYLE_USER_FOCUS_NORMAL,
  eCSSKeyword_ignore,         NS_STYLE_USER_FOCUS_IGNORE,
  eCSSKeyword_select_all,     NS_STYLE_USER_FOCUS_SELECT_ALL,
  eCSSKeyword_select_before,  NS_STYLE_USER_FOCUS_SELECT_BEFORE,
  eCSSKeyword_select_after,   NS_STYLE_USER_FOCUS_SELECT_AFTER,
  eCSSKeyword_select_same,    NS_STYLE_USER_FOCUS_SELECT_SAME,
  eCSSKeyword_select_menu,    NS_STYLE_USER_FOCUS_SELECT_MENU,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kUserInputKTable[] = {
  eCSSKeyword_none,     NS_STYLE_USER_INPUT_NONE,
  eCSSKeyword_auto,     NS_STYLE_USER_INPUT_AUTO,
  eCSSKeyword_enabled,  NS_STYLE_USER_INPUT_ENABLED,
  eCSSKeyword_disabled, NS_STYLE_USER_INPUT_DISABLED,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kUserModifyKTable[] = {
  eCSSKeyword_read_only,  NS_STYLE_USER_MODIFY_READ_ONLY,
  eCSSKeyword_read_write, NS_STYLE_USER_MODIFY_READ_WRITE,
  eCSSKeyword_write_only, NS_STYLE_USER_MODIFY_WRITE_ONLY,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kUserSelectKTable[] = {
  eCSSKeyword_none,       NS_STYLE_USER_SELECT_NONE,
  eCSSKeyword_auto,       NS_STYLE_USER_SELECT_AUTO,
  eCSSKeyword_text,       NS_STYLE_USER_SELECT_TEXT,
  eCSSKeyword_element,    NS_STYLE_USER_SELECT_ELEMENT,
  eCSSKeyword_elements,   NS_STYLE_USER_SELECT_ELEMENTS,
  eCSSKeyword_all,        NS_STYLE_USER_SELECT_ALL,
  eCSSKeyword_toggle,     NS_STYLE_USER_SELECT_TOGGLE,
  eCSSKeyword_tri_state,  NS_STYLE_USER_SELECT_TRI_STATE,
  eCSSKeyword__moz_all,   NS_STYLE_USER_SELECT_MOZ_ALL,
  eCSSKeyword__moz_none,  NS_STYLE_USER_SELECT_MOZ_NONE,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kVerticalAlignKTable[] = {
  eCSSKeyword_baseline, NS_STYLE_VERTICAL_ALIGN_BASELINE,
  eCSSKeyword_sub, NS_STYLE_VERTICAL_ALIGN_SUB,
  eCSSKeyword_super, NS_STYLE_VERTICAL_ALIGN_SUPER,
  eCSSKeyword_top, NS_STYLE_VERTICAL_ALIGN_TOP,
  eCSSKeyword_text_top, NS_STYLE_VERTICAL_ALIGN_TEXT_TOP,
  eCSSKeyword_middle, NS_STYLE_VERTICAL_ALIGN_MIDDLE,
  eCSSKeyword__moz_middle_with_baseline, NS_STYLE_VERTICAL_ALIGN_MIDDLE_WITH_BASELINE,
  eCSSKeyword_bottom, NS_STYLE_VERTICAL_ALIGN_BOTTOM,
  eCSSKeyword_text_bottom, NS_STYLE_VERTICAL_ALIGN_TEXT_BOTTOM,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kVisibilityKTable[] = {
  eCSSKeyword_visible, NS_STYLE_VISIBILITY_VISIBLE,
  eCSSKeyword_hidden, NS_STYLE_VISIBILITY_HIDDEN,
  eCSSKeyword_collapse, NS_STYLE_VISIBILITY_COLLAPSE,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kWhitespaceKTable[] = {
  eCSSKeyword_normal, NS_STYLE_WHITESPACE_NORMAL,
  eCSSKeyword_pre, NS_STYLE_WHITESPACE_PRE,
  eCSSKeyword_nowrap, NS_STYLE_WHITESPACE_NOWRAP,
  eCSSKeyword_pre_wrap, NS_STYLE_WHITESPACE_PRE_WRAP,
  eCSSKeyword_pre_line, NS_STYLE_WHITESPACE_PRE_LINE,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kWidthKTable[] = {
  eCSSKeyword__moz_max_content, NS_STYLE_WIDTH_MAX_CONTENT,
  eCSSKeyword__moz_min_content, NS_STYLE_WIDTH_MIN_CONTENT,
  eCSSKeyword__moz_fit_content, NS_STYLE_WIDTH_FIT_CONTENT,
  eCSSKeyword__moz_available, NS_STYLE_WIDTH_AVAILABLE,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kWindowShadowKTable[] = {
  eCSSKeyword_none, NS_STYLE_WINDOW_SHADOW_NONE,
  eCSSKeyword_default, NS_STYLE_WINDOW_SHADOW_DEFAULT,
  eCSSKeyword_menu, NS_STYLE_WINDOW_SHADOW_MENU,
  eCSSKeyword_tooltip, NS_STYLE_WINDOW_SHADOW_TOOLTIP,
  eCSSKeyword_sheet, NS_STYLE_WINDOW_SHADOW_SHEET,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kWordwrapKTable[] = {
  eCSSKeyword_normal, NS_STYLE_WORDWRAP_NORMAL,
  eCSSKeyword_break_word, NS_STYLE_WORDWRAP_BREAK_WORD,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kHyphensKTable[] = {
  eCSSKeyword_none, NS_STYLE_HYPHENS_NONE,
  eCSSKeyword_manual, NS_STYLE_HYPHENS_MANUAL,
  eCSSKeyword_auto, NS_STYLE_HYPHENS_AUTO,
  eCSSKeyword_UNKNOWN,-1
};

// Specific keyword tables for XUL.properties
const PRInt32 nsCSSProps::kBoxAlignKTable[] = {
  eCSSKeyword_stretch,  NS_STYLE_BOX_ALIGN_STRETCH,
  eCSSKeyword_start,   NS_STYLE_BOX_ALIGN_START,
  eCSSKeyword_center, NS_STYLE_BOX_ALIGN_CENTER,
  eCSSKeyword_baseline, NS_STYLE_BOX_ALIGN_BASELINE,
  eCSSKeyword_end, NS_STYLE_BOX_ALIGN_END,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kBoxDirectionKTable[] = {
  eCSSKeyword_normal,  NS_STYLE_BOX_DIRECTION_NORMAL,
  eCSSKeyword_reverse,   NS_STYLE_BOX_DIRECTION_REVERSE,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kBoxOrientKTable[] = {
  eCSSKeyword_horizontal,  NS_STYLE_BOX_ORIENT_HORIZONTAL,
  eCSSKeyword_vertical,   NS_STYLE_BOX_ORIENT_VERTICAL,
  eCSSKeyword_inline_axis, NS_STYLE_BOX_ORIENT_HORIZONTAL,
  eCSSKeyword_block_axis, NS_STYLE_BOX_ORIENT_VERTICAL,
  eCSSKeyword_UNKNOWN,-1
};

const PRInt32 nsCSSProps::kBoxPackKTable[] = {
  eCSSKeyword_start,  NS_STYLE_BOX_PACK_START,
  eCSSKeyword_center,   NS_STYLE_BOX_PACK_CENTER,
  eCSSKeyword_end, NS_STYLE_BOX_PACK_END,
  eCSSKeyword_justify, NS_STYLE_BOX_PACK_JUSTIFY,
  eCSSKeyword_UNKNOWN,-1
};

// keyword tables for SVG properties

const PRInt32 nsCSSProps::kDominantBaselineKTable[] = {
  eCSSKeyword_auto, NS_STYLE_DOMINANT_BASELINE_AUTO,
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
  eCSSKeyword_UNKNOWN, -1
};

const PRInt32 nsCSSProps::kFillRuleKTable[] = {
  eCSSKeyword_nonzero, NS_STYLE_FILL_RULE_NONZERO,
  eCSSKeyword_evenodd, NS_STYLE_FILL_RULE_EVENODD,
  eCSSKeyword_UNKNOWN, -1
};

const PRInt32 nsCSSProps::kImageRenderingKTable[] = {
  eCSSKeyword_auto, NS_STYLE_IMAGE_RENDERING_AUTO,
  eCSSKeyword_optimizespeed, NS_STYLE_IMAGE_RENDERING_OPTIMIZESPEED,
  eCSSKeyword_optimizequality, NS_STYLE_IMAGE_RENDERING_OPTIMIZEQUALITY,
  eCSSKeyword__moz_crisp_edges, NS_STYLE_IMAGE_RENDERING_CRISPEDGES,
  eCSSKeyword_UNKNOWN, -1
};

const PRInt32 nsCSSProps::kShapeRenderingKTable[] = {
  eCSSKeyword_auto, NS_STYLE_SHAPE_RENDERING_AUTO,
  eCSSKeyword_optimizespeed, NS_STYLE_SHAPE_RENDERING_OPTIMIZESPEED,
  eCSSKeyword_crispedges, NS_STYLE_SHAPE_RENDERING_CRISPEDGES,
  eCSSKeyword_geometricprecision, NS_STYLE_SHAPE_RENDERING_GEOMETRICPRECISION,
  eCSSKeyword_UNKNOWN, -1
};

const PRInt32 nsCSSProps::kStrokeLinecapKTable[] = {
  eCSSKeyword_butt, NS_STYLE_STROKE_LINECAP_BUTT,
  eCSSKeyword_round, NS_STYLE_STROKE_LINECAP_ROUND,
  eCSSKeyword_square, NS_STYLE_STROKE_LINECAP_SQUARE,
  eCSSKeyword_UNKNOWN, -1
};

const PRInt32 nsCSSProps::kStrokeLinejoinKTable[] = {
  eCSSKeyword_miter, NS_STYLE_STROKE_LINEJOIN_MITER,
  eCSSKeyword_round, NS_STYLE_STROKE_LINEJOIN_ROUND,
  eCSSKeyword_bevel, NS_STYLE_STROKE_LINEJOIN_BEVEL,
  eCSSKeyword_UNKNOWN, -1
};

const PRInt32 nsCSSProps::kTextAnchorKTable[] = {
  eCSSKeyword_start, NS_STYLE_TEXT_ANCHOR_START,
  eCSSKeyword_middle, NS_STYLE_TEXT_ANCHOR_MIDDLE,
  eCSSKeyword_end, NS_STYLE_TEXT_ANCHOR_END,
  eCSSKeyword_UNKNOWN, -1
};

const PRInt32 nsCSSProps::kTextRenderingKTable[] = {
  eCSSKeyword_auto, NS_STYLE_TEXT_RENDERING_AUTO,
  eCSSKeyword_optimizespeed, NS_STYLE_TEXT_RENDERING_OPTIMIZESPEED,
  eCSSKeyword_optimizelegibility, NS_STYLE_TEXT_RENDERING_OPTIMIZELEGIBILITY,
  eCSSKeyword_geometricprecision, NS_STYLE_TEXT_RENDERING_GEOMETRICPRECISION,
  eCSSKeyword_UNKNOWN, -1
};

const PRInt32 nsCSSProps::kColorInterpolationKTable[] = {
  eCSSKeyword_auto, NS_STYLE_COLOR_INTERPOLATION_AUTO,
  eCSSKeyword_srgb, NS_STYLE_COLOR_INTERPOLATION_SRGB,
  eCSSKeyword_linearrgb, NS_STYLE_COLOR_INTERPOLATION_LINEARRGB,
  eCSSKeyword_UNKNOWN, -1
};

PRBool
nsCSSProps::FindKeyword(nsCSSKeyword aKeyword, const PRInt32 aTable[], PRInt32& aResult)
{
  PRInt32 index = 0;
  while (eCSSKeyword_UNKNOWN != nsCSSKeyword(aTable[index])) {
    if (aKeyword == nsCSSKeyword(aTable[index])) {
      aResult = aTable[index+1];
      return PR_TRUE;
    }
    index += 2;
  }
  return PR_FALSE;
}

nsCSSKeyword
nsCSSProps::ValueToKeywordEnum(PRInt32 aValue, const PRInt32 aTable[])
{
  PRInt32 i = 1;
  for (;;) {
    if (aTable[i] == -1 && aTable[i-1] == eCSSKeyword_UNKNOWN) {
      break;
    }
    if (aValue == aTable[i]) {
      return nsCSSKeyword(aTable[i-1]);
    }
    i += 2;
  }
  return eCSSKeyword_UNKNOWN;
}

const nsAFlatCString&
nsCSSProps::ValueToKeyword(PRInt32 aValue, const PRInt32 aTable[])
{
  nsCSSKeyword keyword = ValueToKeywordEnum(aValue, aTable);
  if (keyword == eCSSKeyword_UNKNOWN) {
    static nsDependentCString sNullStr("");
    return sNullStr;
  } else {
    return nsCSSKeywords::GetStringValue(keyword);
  }
}

/* static */ const PRInt32* const
nsCSSProps::kKeywordTableTable[eCSSProperty_COUNT_no_shorthands] = {
  #define CSS_PROP(name_, id_, method_, flags_, parsevariant_, kwtable_,     \
                   stylestruct_, stylestructoffset_, animtype_)              \
    kwtable_,
  #include "nsCSSPropList.h"
  #undef CSS_PROP
};

const nsAFlatCString&
nsCSSProps::LookupPropertyValue(nsCSSProperty aProp, PRInt32 aValue)
{
  NS_ABORT_IF_FALSE(aProp >= 0 && aProp < eCSSProperty_COUNT,
                    "property out of range");

  const PRInt32* kwtable = nsnull;
  if (aProp < eCSSProperty_COUNT_no_shorthands)
    kwtable = kKeywordTableTable[aProp];

  if (kwtable)
    return ValueToKeyword(aValue, kwtable);

  static nsDependentCString sNullStr("");
  return sNullStr;
}

PRBool nsCSSProps::GetColorName(PRInt32 aPropValue, nsCString &aStr)
{
  PRBool rv = PR_FALSE;

  // first get the keyword corresponding to the property Value from the color table
  nsCSSKeyword keyword = ValueToKeywordEnum(aPropValue, kColorKTable);

  // next get the name as a string from the keywords table
  if (keyword != eCSSKeyword_UNKNOWN) {
    nsCSSKeywords::AddRefTable();
    aStr = nsCSSKeywords::GetStringValue(keyword);
    nsCSSKeywords::ReleaseTable();
    rv = PR_TRUE;
  }
  return rv;
}

const nsStyleStructID nsCSSProps::kSIDTable[eCSSProperty_COUNT_no_shorthands] = {
    // Note that this uses the special BackendOnly style struct ID
    // (which does need to be valid for storing in the
    // nsCSSCompressedDataBlock::mStyleBits bitfield).
    #define CSS_PROP(name_, id_, method_, flags_, parsevariant_, kwtable_,   \
                     stylestruct_, stylestructoffset_, animtype_)            \
        eStyleStruct_##stylestruct_,

    #include "nsCSSPropList.h"

    #undef CSS_PROP
};

const nsStyleAnimType
nsCSSProps::kAnimTypeTable[eCSSProperty_COUNT_no_shorthands] = {
#define CSS_PROP(name_, id_, method_, flags_, parsevariant_, kwtable_,       \
                 stylestruct_, stylestructoffset_, animtype_)                \
  animtype_,
#include "nsCSSPropList.h"
#undef CSS_PROP
};

const ptrdiff_t
nsCSSProps::kStyleStructOffsetTable[eCSSProperty_COUNT_no_shorthands] = {
#define CSS_PROP(name_, id_, method_, flags_, parsevariant_, kwtable_,       \
                 stylestruct_, stylestructoffset_, animtype_)                \
  stylestructoffset_,
#include "nsCSSPropList.h"
#undef CSS_PROP
};

const PRUint32 nsCSSProps::kFlagsTable[eCSSProperty_COUNT] = {
#define CSS_PROP(name_, id_, method_, flags_, parsevariant_, kwtable_,       \
                 stylestruct_, stylestructoffset_, animtype_)                \
  flags_,
#include "nsCSSPropList.h"
#undef CSS_PROP
#define CSS_PROP_SHORTHAND(name_, id_, method_, flags_) flags_,
#include "nsCSSPropList.h"
#undef CSS_PROP_SHORTHAND
};

static const nsCSSProperty gAnimationSubpropTable[] = {
  eCSSProperty_animation_duration,
  eCSSProperty_animation_timing_function,
  eCSSProperty_animation_delay,
  eCSSProperty_animation_direction,
  eCSSProperty_animation_fill_mode,
  eCSSProperty_animation_iteration_count,
  // List animation-name last so we serialize it last, in case it has
  // a value that conflicts with one of the other properties.  (See
  // how Declaration::GetValue serializes 'animation'.
  eCSSProperty_animation_name,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderRadiusSubpropTable[] = {
  // Code relies on these being in topleft-topright-bottomright-bottomleft
  // order.
  eCSSProperty_border_top_left_radius,
  eCSSProperty_border_top_right_radius,
  eCSSProperty_border_bottom_right_radius,
  eCSSProperty_border_bottom_left_radius,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gOutlineRadiusSubpropTable[] = {
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
  eCSSProperty_background_position,
  eCSSProperty_background_clip,
  eCSSProperty_background_origin,
  eCSSProperty_background_size,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderSubpropTable[] = {
  eCSSProperty_border_top_width,
  eCSSProperty_border_right_width_value,
  eCSSProperty_border_right_width_ltr_source,
  eCSSProperty_border_right_width_rtl_source,
  eCSSProperty_border_bottom_width,
  eCSSProperty_border_left_width_value,
  eCSSProperty_border_left_width_ltr_source,
  eCSSProperty_border_left_width_rtl_source,
  eCSSProperty_border_top_style,
  eCSSProperty_border_right_style_value,
  eCSSProperty_border_right_style_ltr_source,
  eCSSProperty_border_right_style_rtl_source,
  eCSSProperty_border_bottom_style,
  eCSSProperty_border_left_style_value,
  eCSSProperty_border_left_style_ltr_source,
  eCSSProperty_border_left_style_rtl_source,
  eCSSProperty_border_top_color,
  eCSSProperty_border_right_color_value,
  eCSSProperty_border_right_color_ltr_source,
  eCSSProperty_border_right_color_rtl_source,
  eCSSProperty_border_bottom_color,
  eCSSProperty_border_left_color_value,
  eCSSProperty_border_left_color_ltr_source,
  eCSSProperty_border_left_color_rtl_source,
  eCSSProperty_border_top_colors,
  eCSSProperty_border_right_colors,
  eCSSProperty_border_bottom_colors,
  eCSSProperty_border_left_colors,
  eCSSProperty_border_image,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderBottomSubpropTable[] = {
  // nsCSSDeclaration.cpp outputs the subproperties in this order.
  // It also depends on the color being third.
  eCSSProperty_border_bottom_width,
  eCSSProperty_border_bottom_style,
  eCSSProperty_border_bottom_color,
  eCSSProperty_UNKNOWN
};

PR_STATIC_ASSERT(NS_SIDE_TOP == 0);
PR_STATIC_ASSERT(NS_SIDE_RIGHT == 1);
PR_STATIC_ASSERT(NS_SIDE_BOTTOM == 2);
PR_STATIC_ASSERT(NS_SIDE_LEFT == 3);
static const nsCSSProperty gBorderColorSubpropTable[] = {
  // Code relies on these being in top-right-bottom-left order.
  // Code relies on these matching the NS_SIDE_* constants.
  eCSSProperty_border_top_color,
  eCSSProperty_border_right_color_value,
  eCSSProperty_border_bottom_color,
  eCSSProperty_border_left_color_value,
  // extras:
  eCSSProperty_border_left_color_ltr_source,
  eCSSProperty_border_left_color_rtl_source,
  eCSSProperty_border_right_color_ltr_source,
  eCSSProperty_border_right_color_rtl_source,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderEndColorSubpropTable[] = {
  // nsCSSParser::ParseDirectionalBoxProperty depends on this order
  eCSSProperty_border_end_color_value,
  eCSSProperty_border_right_color_ltr_source,
  eCSSProperty_border_left_color_rtl_source,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderLeftColorSubpropTable[] = {
  // nsCSSParser::ParseDirectionalBoxProperty depends on this order
  eCSSProperty_border_left_color_value,
  eCSSProperty_border_left_color_ltr_source,
  eCSSProperty_border_left_color_rtl_source,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderRightColorSubpropTable[] = {
  // nsCSSParser::ParseDirectionalBoxProperty depends on this order
  eCSSProperty_border_right_color_value,
  eCSSProperty_border_right_color_ltr_source,
  eCSSProperty_border_right_color_rtl_source,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderStartColorSubpropTable[] = {
  // nsCSSParser::ParseDirectionalBoxProperty depends on this order
  eCSSProperty_border_start_color_value,
  eCSSProperty_border_left_color_ltr_source,
  eCSSProperty_border_right_color_rtl_source,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderEndSubpropTable[] = {
  // nsCSSDeclaration.cpp output the subproperties in this order.
  // It also depends on the color being third.
  eCSSProperty_border_end_width_value,
  eCSSProperty_border_end_style_value,
  eCSSProperty_border_end_color_value,
  // extras:
  eCSSProperty_border_right_width_ltr_source,
  eCSSProperty_border_left_width_rtl_source,
  eCSSProperty_border_right_style_ltr_source,
  eCSSProperty_border_left_style_rtl_source,
  eCSSProperty_border_right_color_ltr_source,
  eCSSProperty_border_left_color_rtl_source,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderLeftSubpropTable[] = {
  // nsCSSDeclaration.cpp outputs the subproperties in this order.
  // It also depends on the color being third.
  eCSSProperty_border_left_width_value,
  eCSSProperty_border_left_style_value,
  eCSSProperty_border_left_color_value,
  // extras:
  eCSSProperty_border_left_width_ltr_source,
  eCSSProperty_border_left_width_rtl_source,
  eCSSProperty_border_left_style_ltr_source,
  eCSSProperty_border_left_style_rtl_source,
  eCSSProperty_border_left_color_ltr_source,
  eCSSProperty_border_left_color_rtl_source,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderRightSubpropTable[] = {
  // nsCSSDeclaration.cpp outputs the subproperties in this order.
  // It also depends on the color being third.
  eCSSProperty_border_right_width_value,
  eCSSProperty_border_right_style_value,
  eCSSProperty_border_right_color_value,
  // extras:
  eCSSProperty_border_right_width_ltr_source,
  eCSSProperty_border_right_width_rtl_source,
  eCSSProperty_border_right_style_ltr_source,
  eCSSProperty_border_right_style_rtl_source,
  eCSSProperty_border_right_color_ltr_source,
  eCSSProperty_border_right_color_rtl_source,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderStartSubpropTable[] = {
  // nsCSSDeclaration.cpp outputs the subproperties in this order.
  // It also depends on the color being third.
  eCSSProperty_border_start_width_value,
  eCSSProperty_border_start_style_value,
  eCSSProperty_border_start_color_value,
  // extras:
  eCSSProperty_border_left_width_ltr_source,
  eCSSProperty_border_right_width_rtl_source,
  eCSSProperty_border_left_style_ltr_source,
  eCSSProperty_border_right_style_rtl_source,
  eCSSProperty_border_left_color_ltr_source,
  eCSSProperty_border_right_color_rtl_source,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderStyleSubpropTable[] = {
  // Code relies on these being in top-right-bottom-left order.
  eCSSProperty_border_top_style,
  eCSSProperty_border_right_style_value,
  eCSSProperty_border_bottom_style,
  eCSSProperty_border_left_style_value,
  // extras:
  eCSSProperty_border_left_style_ltr_source,
  eCSSProperty_border_left_style_rtl_source,
  eCSSProperty_border_right_style_ltr_source,
  eCSSProperty_border_right_style_rtl_source,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderLeftStyleSubpropTable[] = {
  // nsCSSParser::ParseDirectionalBoxProperty depends on this order
  eCSSProperty_border_left_style_value,
  eCSSProperty_border_left_style_ltr_source,
  eCSSProperty_border_left_style_rtl_source,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderRightStyleSubpropTable[] = {
  // nsCSSParser::ParseDirectionalBoxProperty depends on this order
  eCSSProperty_border_right_style_value,
  eCSSProperty_border_right_style_ltr_source,
  eCSSProperty_border_right_style_rtl_source,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderStartStyleSubpropTable[] = {
  // nsCSSParser::ParseDirectionalBoxProperty depends on this order
  eCSSProperty_border_start_style_value,
  eCSSProperty_border_left_style_ltr_source,
  eCSSProperty_border_right_style_rtl_source,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderEndStyleSubpropTable[] = {
  // nsCSSParser::ParseDirectionalBoxProperty depends on this order
  eCSSProperty_border_end_style_value,
  eCSSProperty_border_right_style_ltr_source,
  eCSSProperty_border_left_style_rtl_source,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderTopSubpropTable[] = {
  // nsCSSDeclaration.cpp outputs the subproperties in this order.
  // It also depends on the color being third.
  eCSSProperty_border_top_width,
  eCSSProperty_border_top_style,
  eCSSProperty_border_top_color,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderWidthSubpropTable[] = {
  // Code relies on these being in top-right-bottom-left order.
  eCSSProperty_border_top_width,
  eCSSProperty_border_right_width_value,
  eCSSProperty_border_bottom_width,
  eCSSProperty_border_left_width_value,
  // extras:
  eCSSProperty_border_left_width_ltr_source,
  eCSSProperty_border_left_width_rtl_source,
  eCSSProperty_border_right_width_ltr_source,
  eCSSProperty_border_right_width_rtl_source,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderLeftWidthSubpropTable[] = {
  // nsCSSParser::ParseDirectionalBoxProperty depends on this order
  eCSSProperty_border_left_width_value,
  eCSSProperty_border_left_width_ltr_source,
  eCSSProperty_border_left_width_rtl_source,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderRightWidthSubpropTable[] = {
  // nsCSSParser::ParseDirectionalBoxProperty depends on this order
  eCSSProperty_border_right_width_value,
  eCSSProperty_border_right_width_ltr_source,
  eCSSProperty_border_right_width_rtl_source,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderStartWidthSubpropTable[] = {
  // nsCSSParser::ParseDirectionalBoxProperty depends on this order
  eCSSProperty_border_start_width_value,
  eCSSProperty_border_left_width_ltr_source,
  eCSSProperty_border_right_width_rtl_source,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderEndWidthSubpropTable[] = {
  // nsCSSParser::ParseDirectionalBoxProperty depends on this order
  eCSSProperty_border_end_width_value,
  eCSSProperty_border_right_width_ltr_source,
  eCSSProperty_border_left_width_rtl_source,
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
  eCSSProperty__x_system_font,
  eCSSProperty_font_feature_settings,
  eCSSProperty_font_language_override,
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

static const nsCSSProperty gMarginStartSubpropTable[] = {
  // nsCSSParser::ParseDirectionalBoxProperty depends on this order
  eCSSProperty_margin_start_value,
  eCSSProperty_margin_left_ltr_source,
  eCSSProperty_margin_right_rtl_source,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gMarginEndSubpropTable[] = {
  // nsCSSParser::ParseDirectionalBoxProperty depends on this order
  eCSSProperty_margin_end_value,
  eCSSProperty_margin_right_ltr_source,
  eCSSProperty_margin_left_rtl_source,
  eCSSProperty_UNKNOWN
};


static const nsCSSProperty gOutlineSubpropTable[] = {
  // nsCSSDeclaration.cpp outputs the subproperties in this order.
  // It also depends on the color being third.
  eCSSProperty_outline_width,
  eCSSProperty_outline_style,
  eCSSProperty_outline_color,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gColumnsSubpropTable[] = {
  eCSSProperty__moz_column_count,
  eCSSProperty__moz_column_width,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gColumnRuleSubpropTable[] = {
  // nsCSSDeclaration.cpp outputs the subproperties in this order.
  // It also depends on the color being third.
  eCSSProperty__moz_column_rule_width,
  eCSSProperty__moz_column_rule_style,
  eCSSProperty__moz_column_rule_color,
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

static const nsCSSProperty gPaddingStartSubpropTable[] = {
  // nsCSSParser::ParseDirectionalBoxProperty depends on this order
  eCSSProperty_padding_start_value,
  eCSSProperty_padding_left_ltr_source,
  eCSSProperty_padding_right_rtl_source,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gPaddingEndSubpropTable[] = {
  // nsCSSParser::ParseDirectionalBoxProperty depends on this order
  eCSSProperty_padding_end_value,
  eCSSProperty_padding_right_ltr_source,
  eCSSProperty_padding_left_rtl_source,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gTextDecorationSubpropTable[] = {
  eCSSProperty_text_blink,
  eCSSProperty_text_decoration_color,
  eCSSProperty_text_decoration_line,
  eCSSProperty_text_decoration_style,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gTransitionSubpropTable[] = {
  eCSSProperty_transition_property,
  eCSSProperty_transition_duration,
  eCSSProperty_transition_timing_function,
  eCSSProperty_transition_delay,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gMarkerSubpropTable[] = {
  eCSSProperty_marker_start,
  eCSSProperty_marker_mid,
  eCSSProperty_marker_end,
  eCSSProperty_UNKNOWN
};

const nsCSSProperty *const
nsCSSProps::kSubpropertyTable[eCSSProperty_COUNT - eCSSProperty_COUNT_no_shorthands] = {
#define CSS_PROP_DOMPROP_PREFIXED(prop_) prop_
// Need an extra level of macro nesting to force expansion of method_
// params before they get pasted.
#define NSCSSPROPS_INNER_MACRO(method_) g##method_##SubpropTable,
#define CSS_PROP_SHORTHAND(name_, id_, method_, flags_) NSCSSPROPS_INNER_MACRO(method_)
#include "nsCSSPropList.h"
#undef CSS_PROP_SHORTHAND
#undef NSCSSPROPS_INNER_MACRO
#undef CSS_PROP_DOMPROP_PREFIXED
};


#define ENUM_DATA_FOR_PROPERTY(name_, id_, method_, flags_, parsevariant_,   \
                               kwtable_, stylestructoffset_, animtype_)      \
  ePropertyIndex_for_##id_,

// The order of these enums must match the g*Flags arrays in nsRuleNode.cpp.

enum FontCheckCounter {
  #define CSS_PROP_FONT ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_FONT
  ePropertyCount_for_Font
};

enum DisplayCheckCounter {
  #define CSS_PROP_DISPLAY ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_DISPLAY
  ePropertyCount_for_Display
};

enum VisibilityCheckCounter {
  #define CSS_PROP_VISIBILITY ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_VISIBILITY
  ePropertyCount_for_Visibility
};

enum MarginCheckCounter {
  #define CSS_PROP_MARGIN ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_MARGIN
  ePropertyCount_for_Margin
};

enum BorderCheckCounter {
  #define CSS_PROP_BORDER ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_BORDER
  ePropertyCount_for_Border
};

enum PaddingCheckCounter {
  #define CSS_PROP_PADDING ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_PADDING
  ePropertyCount_for_Padding
};

enum OutlineCheckCounter {
  #define CSS_PROP_OUTLINE ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_OUTLINE
  ePropertyCount_for_Outline
};

enum ListCheckCounter {
  #define CSS_PROP_LIST ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_LIST
  ePropertyCount_for_List
};

enum ColorCheckCounter {
  #define CSS_PROP_COLOR ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_COLOR
  ePropertyCount_for_Color
};

enum BackgroundCheckCounter {
  #define CSS_PROP_BACKGROUND ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_BACKGROUND
  ePropertyCount_for_Background
};

enum PositionCheckCounter {
  #define CSS_PROP_POSITION ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_POSITION
  ePropertyCount_for_Position
};

enum TableCheckCounter {
  #define CSS_PROP_TABLE ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_TABLE
  ePropertyCount_for_Table
};

enum TableBorderCheckCounter {
  #define CSS_PROP_TABLEBORDER ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_TABLEBORDER
  ePropertyCount_for_TableBorder
};

enum ContentCheckCounter {
  #define CSS_PROP_CONTENT ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_CONTENT
  ePropertyCount_for_Content
};

enum QuotesCheckCounter {
  #define CSS_PROP_QUOTES ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_QUOTES
  ePropertyCount_for_Quotes
};

enum TextCheckCounter {
  #define CSS_PROP_TEXT ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_TEXT
  ePropertyCount_for_Text
};

enum TextResetCheckCounter {
  #define CSS_PROP_TEXTRESET ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_TEXTRESET
  ePropertyCount_for_TextReset
};

enum UserInterfaceCheckCounter {
  #define CSS_PROP_USERINTERFACE ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_USERINTERFACE
  ePropertyCount_for_UserInterface
};

enum UIResetCheckCounter {
  #define CSS_PROP_UIRESET ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_UIRESET
  ePropertyCount_for_UIReset
};

enum XULCheckCounter {
  #define CSS_PROP_XUL ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_XUL
  ePropertyCount_for_XUL
};

enum SVGCheckCounter {
  #define CSS_PROP_SVG ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_SVG
  ePropertyCount_for_SVG
};

enum SVGResetCheckCounter {
  #define CSS_PROP_SVGRESET ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_SVGRESET
  ePropertyCount_for_SVGReset
};

enum ColumnCheckCounter {
  #define CSS_PROP_COLUMN ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_COLUMN
  ePropertyCount_for_Column
};

#undef ENUM_DATA_FOR_PROPERTY

/* static */ const size_t
nsCSSProps::gPropertyCountInStruct[nsStyleStructID_Length] = {
  #define STYLE_STRUCT(name, checkdata_cb, ctor_args) \
    ePropertyCount_for_##name,
  #include "nsStyleStructList.h"
  #undef STYLE_STRUCT
};

/* static */ const size_t
nsCSSProps::gPropertyIndexInStruct[eCSSProperty_COUNT_no_shorthands] = {

  #define CSS_PROP_BACKENDONLY(name_, id_, method_, flags_, parsevariant_,    \
                               kwtable_)                                      \
      size_t(-1),
  #define CSS_PROP(name_, id_, method_, flags_, parsevariant_, kwtable_,      \
                   stylestruct_, stylestructoffset_, animtype_)               \
    ePropertyIndex_for_##id_,
  #include "nsCSSPropList.h"
  #undef CSS_PROP
  #undef CSS_PROP_BACKENDONLY

};
