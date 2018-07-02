/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * methods for dealing with CSS properties and tables of the keyword
 * values they accept
 */

#include "nsCSSProps.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Casting.h"

#include "nsCSSKeywords.h"
#include "nsLayoutUtils.h"
#include "nsStyleConsts.h"
#include "nsIWidget.h"
#include "nsThemeConstants.h"  // For system widget appearance types

#include "mozilla/dom/Animation.h"
#include "mozilla/dom/AnimationEffectBinding.h" // for PlaybackDirection
#include "mozilla/LookAndFeel.h" // for system colors

#include "nsString.h"
#include "nsStaticNameTable.h"

#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs.h"

using namespace mozilla;

typedef nsCSSProps::KTableEntry KTableEntry;

// required to make the symbol external, so that TestCSSPropertyLookup.cpp can link with it
extern const char* const kCSSRawProperties[];

// define an array of all CSS properties
const char* const kCSSRawProperties[eCSSProperty_COUNT_with_aliases] = {
#define CSS_PROP_LONGHAND(name_, ...) #name_,
#define CSS_PROP_SHORTHAND(name_, ...) #name_,
#define CSS_PROP_ALIAS(name_, ...) #name_,
#include "mozilla/ServoCSSPropList.h"
#undef CSS_PROP_ALIAS
#undef CSS_PROP_SHORTHAND
#undef CSS_PROP_LONGHAND
};

using namespace mozilla;

static int32_t gPropertyTableRefCount;
static nsStaticCaseInsensitiveNameTable* gPropertyTable;
static nsStaticCaseInsensitiveNameTable* gFontDescTable;
static nsStaticCaseInsensitiveNameTable* gCounterDescTable;
static nsDataHashtable<nsCStringHashKey,nsCSSPropertyID>* gPropertyIDLNameTable;

static const char* const kCSSRawFontDescs[] = {
#define CSS_FONT_DESC(name_, method_) #name_,
#include "nsCSSFontDescList.h"
#undef CSS_FONT_DESC
};

static const char* const kCSSRawCounterDescs[] = {
#define CSS_COUNTER_DESC(name_, method_) #name_,
#include "nsCSSCounterDescList.h"
#undef CSS_COUNTER_DESC
};

// We need eCSSAliasCount so we can make gAliases nonzero size when there
// are no aliases.
enum {
  eCSSAliasCount = eCSSProperty_COUNT_with_aliases - eCSSProperty_COUNT
};

// The names are in kCSSRawProperties.
static nsCSSPropertyID gAliases[eCSSAliasCount != 0 ? eCSSAliasCount : 1] = {
#define CSS_PROP_ALIAS(aliasname_, aliasid_, propid_, aliasmethod_, pref_)  \
  eCSSProperty_##propid_ ,
#include "mozilla/ServoCSSPropList.h"
#undef CSS_PROP_ALIAS
};

static nsStaticCaseInsensitiveNameTable*
CreateStaticTable(const char* const aRawTable[], int32_t aLength)
{
  auto table = new nsStaticCaseInsensitiveNameTable(aRawTable, aLength);
#ifdef DEBUG
  // Partially verify the entries.
  for (int32_t index = 0; index < aLength; ++index) {
    nsAutoCString temp(aRawTable[index]);
    MOZ_ASSERT(-1 == temp.FindChar('_'),
               "underscore char in case insensitive name table");
  }
#endif
  return table;
}

void
nsCSSProps::AddRefTable(void)
{
  if (0 == gPropertyTableRefCount++) {
    MOZ_ASSERT(!gPropertyTable, "pre existing array!");
    MOZ_ASSERT(!gFontDescTable, "pre existing array!");
    MOZ_ASSERT(!gCounterDescTable, "pre existing array!");
    MOZ_ASSERT(!gPropertyIDLNameTable, "pre existing array!");

    gPropertyTable = CreateStaticTable(
        kCSSRawProperties, eCSSProperty_COUNT_with_aliases);
    gFontDescTable = CreateStaticTable(kCSSRawFontDescs, eCSSFontDesc_COUNT);
    gCounterDescTable = CreateStaticTable(
        kCSSRawCounterDescs, eCSSCounterDesc_COUNT);

    gPropertyIDLNameTable = new nsDataHashtable<nsCStringHashKey,nsCSSPropertyID>;
    for (nsCSSPropertyID p = nsCSSPropertyID(0);
         size_t(p) < ArrayLength(kIDLNameTable);
         p = nsCSSPropertyID(p + 1)) {
      if (kIDLNameTable[p]) {
        gPropertyIDLNameTable->Put(nsDependentCString(kIDLNameTable[p]), p);
      }
    }

    static bool prefObserversInited = false;
    if (!prefObserversInited) {
      prefObserversInited = true;
      for (const PropertyPref* pref = kPropertyPrefTable;
           pref->mPropID != eCSSProperty_UNKNOWN; pref++) {
        bool* enabled = &gPropertyEnabled[pref->mPropID];
        Preferences::AddBoolVarCache(enabled, pref->mPref);
      }
    }
  }
}

#undef  DEBUG_SHORTHANDS_CONTAINING

void
nsCSSProps::ReleaseTable(void)
{
  if (0 == --gPropertyTableRefCount) {
    delete gPropertyTable;
    gPropertyTable = nullptr;

    delete gFontDescTable;
    gFontDescTable = nullptr;

    delete gCounterDescTable;
    gCounterDescTable = nullptr;

    delete gPropertyIDLNameTable;
    gPropertyIDLNameTable = nullptr;
  }
}

/* static */ bool
nsCSSProps::IsCustomPropertyName(const nsAString& aProperty)
{
  return aProperty.Length() >= CSS_CUSTOM_NAME_PREFIX_LENGTH &&
         StringBeginsWith(aProperty, NS_LITERAL_STRING("--"));
}

nsCSSPropertyID
nsCSSProps::LookupProperty(const nsAString& aProperty, EnabledState aEnabled)
{
  if (IsCustomPropertyName(aProperty)) {
    return eCSSPropertyExtra_variable;
  }

  // This is faster than converting and calling
  // LookupProperty(nsACString&).  The table will do its own
  // converting and avoid a PromiseFlatCString() call.
  MOZ_ASSERT(gPropertyTable, "no lookup table, needs addref");
  nsCSSPropertyID res = nsCSSPropertyID(gPropertyTable->Lookup(aProperty));
  if (MOZ_LIKELY(res < eCSSProperty_COUNT)) {
    if (res != eCSSProperty_UNKNOWN && !IsEnabled(res, aEnabled)) {
      res = eCSSProperty_UNKNOWN;
    }
    return res;
  }
  MOZ_ASSERT(eCSSAliasCount != 0,
             "'res' must be an alias at this point so we better have some!");
  // We intentionally don't support CSSEnabledState::eInUASheets or
  // CSSEnabledState::eInChrome for aliases yet because it's unlikely
  // there will be a need for it.
  if (IsEnabled(res) || aEnabled == CSSEnabledState::eIgnoreEnabledState) {
    res = gAliases[res - eCSSProperty_COUNT];
    MOZ_ASSERT(0 <= res && res < eCSSProperty_COUNT,
               "aliases must not point to other aliases");
    if (IsEnabled(res) || aEnabled == CSSEnabledState::eIgnoreEnabledState) {
      return res;
    }
  }
  return eCSSProperty_UNKNOWN;
}

nsCSSPropertyID
nsCSSProps::LookupPropertyByIDLName(const nsACString& aPropertyIDLName,
                                    EnabledState aEnabled)
{
  nsCSSPropertyID res;
  if (!gPropertyIDLNameTable->Get(aPropertyIDLName, &res)) {
    return eCSSProperty_UNKNOWN;
  }
  MOZ_ASSERT(res < eCSSProperty_COUNT);
  if (!IsEnabled(res, aEnabled)) {
    return eCSSProperty_UNKNOWN;
  }
  return res;
}

nsCSSPropertyID
nsCSSProps::LookupPropertyByIDLName(const nsAString& aPropertyIDLName,
                                    EnabledState aEnabled)
{
  MOZ_ASSERT(gPropertyIDLNameTable, "no lookup table, needs addref");
  return LookupPropertyByIDLName(NS_ConvertUTF16toUTF8(aPropertyIDLName),
                                 aEnabled);
}

nsCSSFontDesc
nsCSSProps::LookupFontDesc(const nsAString& aFontDesc)
{
  MOZ_ASSERT(gFontDescTable, "no lookup table, needs addref");
  nsCSSFontDesc which = nsCSSFontDesc(gFontDescTable->Lookup(aFontDesc));

  if (which == eCSSFontDesc_Display &&
      !StaticPrefs::layout_css_font_display_enabled()) {
    which = eCSSFontDesc_UNKNOWN;
  }
  return which;
}

const nsCString&
nsCSSProps::GetStringValue(nsCSSPropertyID aProperty)
{
  MOZ_ASSERT(gPropertyTable, "no lookup table, needs addref");
  if (gPropertyTable) {
    return gPropertyTable->GetStringValue(int32_t(aProperty));
  } else {
    static nsDependentCString sNullStr("");
    return sNullStr;
  }
}

const nsCString&
nsCSSProps::GetStringValue(nsCSSFontDesc aFontDescID)
{
  MOZ_ASSERT(gFontDescTable, "no lookup table, needs addref");
  if (gFontDescTable) {
    return gFontDescTable->GetStringValue(int32_t(aFontDescID));
  } else {
    static nsDependentCString sNullStr("");
    return sNullStr;
  }
}

const nsCString&
nsCSSProps::GetStringValue(nsCSSCounterDesc aCounterDesc)
{
  MOZ_ASSERT(gCounterDescTable, "no lookup table, needs addref");
  if (gCounterDescTable) {
    return gCounterDescTable->GetStringValue(int32_t(aCounterDesc));
  } else {
    static nsDependentCString sNullStr("");
    return sNullStr;
  }
}

/***************************************************************************/

const KTableEntry nsCSSProps::kTransformStyleKTable[] = {
  { eCSSKeyword_flat, NS_STYLE_TRANSFORM_STYLE_FLAT },
  { eCSSKeyword_preserve_3d, NS_STYLE_TRANSFORM_STYLE_PRESERVE_3D },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kImageLayerRepeatKTable[] = {
  { eCSSKeyword_no_repeat,  StyleImageLayerRepeat::NoRepeat },
  { eCSSKeyword_repeat,     StyleImageLayerRepeat::Repeat },
  { eCSSKeyword_repeat_x,   StyleImageLayerRepeat::RepeatX },
  { eCSSKeyword_repeat_y,   StyleImageLayerRepeat::RepeatY },
  { eCSSKeyword_round,      StyleImageLayerRepeat::Round},
  { eCSSKeyword_space,      StyleImageLayerRepeat::Space},
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kBorderImageRepeatKTable[] = {
  { eCSSKeyword_stretch, StyleBorderImageRepeat::Stretch },
  { eCSSKeyword_repeat, StyleBorderImageRepeat::Repeat },
  { eCSSKeyword_round, StyleBorderImageRepeat::Round },
  { eCSSKeyword_space, StyleBorderImageRepeat::Space },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kBorderStyleKTable[] = {
  { eCSSKeyword_none,   NS_STYLE_BORDER_STYLE_NONE },
  { eCSSKeyword_hidden, NS_STYLE_BORDER_STYLE_HIDDEN },
  { eCSSKeyword_dotted, NS_STYLE_BORDER_STYLE_DOTTED },
  { eCSSKeyword_dashed, NS_STYLE_BORDER_STYLE_DASHED },
  { eCSSKeyword_solid,  NS_STYLE_BORDER_STYLE_SOLID },
  { eCSSKeyword_double, NS_STYLE_BORDER_STYLE_DOUBLE },
  { eCSSKeyword_groove, NS_STYLE_BORDER_STYLE_GROOVE },
  { eCSSKeyword_ridge,  NS_STYLE_BORDER_STYLE_RIDGE },
  { eCSSKeyword_inset,  NS_STYLE_BORDER_STYLE_INSET },
  { eCSSKeyword_outset, NS_STYLE_BORDER_STYLE_OUTSET },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kBoxShadowTypeKTable[] = {
  { eCSSKeyword_inset, uint8_t(StyleBoxShadowType::Inset) },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kCursorKTable[] = {
  // CSS 2.0
  { eCSSKeyword_auto, NS_STYLE_CURSOR_AUTO },
  { eCSSKeyword_crosshair, NS_STYLE_CURSOR_CROSSHAIR },
  { eCSSKeyword_default, NS_STYLE_CURSOR_DEFAULT },
  { eCSSKeyword_pointer, NS_STYLE_CURSOR_POINTER },
  { eCSSKeyword_move, NS_STYLE_CURSOR_MOVE },
  { eCSSKeyword_e_resize, NS_STYLE_CURSOR_E_RESIZE },
  { eCSSKeyword_ne_resize, NS_STYLE_CURSOR_NE_RESIZE },
  { eCSSKeyword_nw_resize, NS_STYLE_CURSOR_NW_RESIZE },
  { eCSSKeyword_n_resize, NS_STYLE_CURSOR_N_RESIZE },
  { eCSSKeyword_se_resize, NS_STYLE_CURSOR_SE_RESIZE },
  { eCSSKeyword_sw_resize, NS_STYLE_CURSOR_SW_RESIZE },
  { eCSSKeyword_s_resize, NS_STYLE_CURSOR_S_RESIZE },
  { eCSSKeyword_w_resize, NS_STYLE_CURSOR_W_RESIZE },
  { eCSSKeyword_text, NS_STYLE_CURSOR_TEXT },
  { eCSSKeyword_wait, NS_STYLE_CURSOR_WAIT },
  { eCSSKeyword_help, NS_STYLE_CURSOR_HELP },
  // CSS 2.1
  { eCSSKeyword_progress, NS_STYLE_CURSOR_SPINNING },
  // CSS3 basic user interface module
  { eCSSKeyword_copy, NS_STYLE_CURSOR_COPY },
  { eCSSKeyword_alias, NS_STYLE_CURSOR_ALIAS },
  { eCSSKeyword_context_menu, NS_STYLE_CURSOR_CONTEXT_MENU },
  { eCSSKeyword_cell, NS_STYLE_CURSOR_CELL },
  { eCSSKeyword_not_allowed, NS_STYLE_CURSOR_NOT_ALLOWED },
  { eCSSKeyword_col_resize, NS_STYLE_CURSOR_COL_RESIZE },
  { eCSSKeyword_row_resize, NS_STYLE_CURSOR_ROW_RESIZE },
  { eCSSKeyword_no_drop, NS_STYLE_CURSOR_NO_DROP },
  { eCSSKeyword_vertical_text, NS_STYLE_CURSOR_VERTICAL_TEXT },
  { eCSSKeyword_all_scroll, NS_STYLE_CURSOR_ALL_SCROLL },
  { eCSSKeyword_nesw_resize, NS_STYLE_CURSOR_NESW_RESIZE },
  { eCSSKeyword_nwse_resize, NS_STYLE_CURSOR_NWSE_RESIZE },
  { eCSSKeyword_ns_resize, NS_STYLE_CURSOR_NS_RESIZE },
  { eCSSKeyword_ew_resize, NS_STYLE_CURSOR_EW_RESIZE },
  { eCSSKeyword_none, NS_STYLE_CURSOR_NONE },
  { eCSSKeyword_grab, NS_STYLE_CURSOR_GRAB },
  { eCSSKeyword_grabbing, NS_STYLE_CURSOR_GRABBING },
  { eCSSKeyword_zoom_in, NS_STYLE_CURSOR_ZOOM_IN },
  { eCSSKeyword_zoom_out, NS_STYLE_CURSOR_ZOOM_OUT },
  // -moz- prefixed vendor specific
  { eCSSKeyword__moz_grab, NS_STYLE_CURSOR_GRAB },
  { eCSSKeyword__moz_grabbing, NS_STYLE_CURSOR_GRABBING },
  { eCSSKeyword__moz_zoom_in, NS_STYLE_CURSOR_ZOOM_IN },
  { eCSSKeyword__moz_zoom_out, NS_STYLE_CURSOR_ZOOM_OUT },
  { eCSSKeyword_UNKNOWN, -1 }
};

KTableEntry nsCSSProps::kDisplayKTable[] = {
  { eCSSKeyword_none,                StyleDisplay::None },
  { eCSSKeyword_inline,              StyleDisplay::Inline },
  { eCSSKeyword_block,               StyleDisplay::Block },
  { eCSSKeyword_inline_block,        StyleDisplay::InlineBlock },
  { eCSSKeyword_list_item,           StyleDisplay::ListItem },
  { eCSSKeyword_table,               StyleDisplay::Table },
  { eCSSKeyword_inline_table,        StyleDisplay::InlineTable },
  { eCSSKeyword_table_row_group,     StyleDisplay::TableRowGroup },
  { eCSSKeyword_table_header_group,  StyleDisplay::TableHeaderGroup },
  { eCSSKeyword_table_footer_group,  StyleDisplay::TableFooterGroup },
  { eCSSKeyword_table_row,           StyleDisplay::TableRow },
  { eCSSKeyword_table_column_group,  StyleDisplay::TableColumnGroup },
  { eCSSKeyword_table_column,        StyleDisplay::TableColumn },
  { eCSSKeyword_table_cell,          StyleDisplay::TableCell },
  { eCSSKeyword_table_caption,       StyleDisplay::TableCaption },
  // Make sure this is kept in sync with the code in
  // nsCSSFrameConstructor::ConstructXULFrame
  { eCSSKeyword__moz_box,            StyleDisplay::MozBox },
  { eCSSKeyword__moz_inline_box,     StyleDisplay::MozInlineBox },
#ifdef MOZ_XUL
  { eCSSKeyword__moz_grid,           StyleDisplay::MozGrid },
  { eCSSKeyword__moz_inline_grid,    StyleDisplay::MozInlineGrid },
  { eCSSKeyword__moz_grid_group,     StyleDisplay::MozGridGroup },
  { eCSSKeyword__moz_grid_line,      StyleDisplay::MozGridLine },
  { eCSSKeyword__moz_stack,          StyleDisplay::MozStack },
  { eCSSKeyword__moz_inline_stack,   StyleDisplay::MozInlineStack },
  { eCSSKeyword__moz_deck,           StyleDisplay::MozDeck },
  { eCSSKeyword__moz_popup,          StyleDisplay::MozPopup },
  { eCSSKeyword__moz_groupbox,       StyleDisplay::MozGroupbox },
#endif
  { eCSSKeyword_flex,                StyleDisplay::Flex },
  { eCSSKeyword_inline_flex,         StyleDisplay::InlineFlex },
  { eCSSKeyword_ruby,                StyleDisplay::Ruby },
  { eCSSKeyword_ruby_base,           StyleDisplay::RubyBase },
  { eCSSKeyword_ruby_base_container, StyleDisplay::RubyBaseContainer },
  { eCSSKeyword_ruby_text,           StyleDisplay::RubyText },
  { eCSSKeyword_ruby_text_container, StyleDisplay::RubyTextContainer },
  { eCSSKeyword_grid,                StyleDisplay::Grid },
  { eCSSKeyword_inline_grid,         StyleDisplay::InlineGrid },
  // The next 4 entries are controlled by the layout.css.prefixes.webkit pref.
  { eCSSKeyword__webkit_box,         StyleDisplay::WebkitBox },
  { eCSSKeyword__webkit_inline_box,  StyleDisplay::WebkitInlineBox },
  { eCSSKeyword__webkit_flex,        StyleDisplay::Flex },
  { eCSSKeyword__webkit_inline_flex, StyleDisplay::InlineFlex },
  { eCSSKeyword_contents,            StyleDisplay::Contents },
  { eCSSKeyword_flow_root,           StyleDisplay::FlowRoot },
  { eCSSKeyword_UNKNOWN,             -1 }
};

const KTableEntry nsCSSProps::kAlignAllKeywords[] = {
  { eCSSKeyword_auto,          NS_STYLE_ALIGN_AUTO },
  { eCSSKeyword_normal,        NS_STYLE_ALIGN_NORMAL },
  { eCSSKeyword_start,         NS_STYLE_ALIGN_START },
  { eCSSKeyword_end,           NS_STYLE_ALIGN_END },
  { eCSSKeyword_flex_start,    NS_STYLE_ALIGN_FLEX_START },
  { eCSSKeyword_flex_end,      NS_STYLE_ALIGN_FLEX_END },
  { eCSSKeyword_center,        NS_STYLE_ALIGN_CENTER },
  { eCSSKeyword_left,          NS_STYLE_ALIGN_LEFT },
  { eCSSKeyword_right,         NS_STYLE_ALIGN_RIGHT },
  { eCSSKeyword_baseline,      NS_STYLE_ALIGN_BASELINE },
  // Also "first/last baseline"; see nsCSSValue::AppendAlignJustifyValueToString
  { eCSSKeyword_stretch,       NS_STYLE_ALIGN_STRETCH },
  { eCSSKeyword_self_start,    NS_STYLE_ALIGN_SELF_START },
  { eCSSKeyword_self_end,      NS_STYLE_ALIGN_SELF_END },
  { eCSSKeyword_space_between, NS_STYLE_ALIGN_SPACE_BETWEEN },
  { eCSSKeyword_space_around,  NS_STYLE_ALIGN_SPACE_AROUND },
  { eCSSKeyword_space_evenly,  NS_STYLE_ALIGN_SPACE_EVENLY },
  { eCSSKeyword_legacy,        NS_STYLE_ALIGN_LEGACY },
  { eCSSKeyword_safe,          NS_STYLE_ALIGN_SAFE },
  { eCSSKeyword_unsafe,        NS_STYLE_ALIGN_UNSAFE },
  { eCSSKeyword_UNKNOWN,       -1 }
};

const KTableEntry nsCSSProps::kAlignOverflowPosition[] = {
  { eCSSKeyword_unsafe,        NS_STYLE_ALIGN_UNSAFE },
  { eCSSKeyword_safe,          NS_STYLE_ALIGN_SAFE },
  { eCSSKeyword_UNKNOWN,       -1 }
};

const KTableEntry nsCSSProps::kAlignSelfPosition[] = {
  { eCSSKeyword_start,         NS_STYLE_ALIGN_START },
  { eCSSKeyword_end,           NS_STYLE_ALIGN_END },
  { eCSSKeyword_flex_start,    NS_STYLE_ALIGN_FLEX_START },
  { eCSSKeyword_flex_end,      NS_STYLE_ALIGN_FLEX_END },
  { eCSSKeyword_center,        NS_STYLE_ALIGN_CENTER },
  { eCSSKeyword_left,          NS_STYLE_ALIGN_LEFT },
  { eCSSKeyword_right,         NS_STYLE_ALIGN_RIGHT },
  { eCSSKeyword_self_start,    NS_STYLE_ALIGN_SELF_START },
  { eCSSKeyword_self_end,      NS_STYLE_ALIGN_SELF_END },
  { eCSSKeyword_UNKNOWN,       -1 }
};

const KTableEntry nsCSSProps::kAlignLegacy[] = {
  { eCSSKeyword_legacy,        NS_STYLE_ALIGN_LEGACY },
  { eCSSKeyword_UNKNOWN,       -1 }
};

const KTableEntry nsCSSProps::kAlignLegacyPosition[] = {
  { eCSSKeyword_center,        NS_STYLE_ALIGN_CENTER },
  { eCSSKeyword_left,          NS_STYLE_ALIGN_LEFT },
  { eCSSKeyword_right,         NS_STYLE_ALIGN_RIGHT },
  { eCSSKeyword_UNKNOWN,       -1 }
};

const KTableEntry nsCSSProps::kAlignAutoNormalStretchBaseline[] = {
  { eCSSKeyword_auto,          NS_STYLE_ALIGN_AUTO },
  { eCSSKeyword_normal,        NS_STYLE_ALIGN_NORMAL },
  { eCSSKeyword_stretch,       NS_STYLE_ALIGN_STRETCH },
  { eCSSKeyword_baseline,      NS_STYLE_ALIGN_BASELINE },
  // Also "first baseline" & "last baseline"; see CSSParserImpl::ParseAlignEnum
  { eCSSKeyword_UNKNOWN,       -1 }
};

const KTableEntry nsCSSProps::kAlignNormalStretchBaseline[] = {
  { eCSSKeyword_normal,        NS_STYLE_ALIGN_NORMAL },
  { eCSSKeyword_stretch,       NS_STYLE_ALIGN_STRETCH },
  { eCSSKeyword_baseline,      NS_STYLE_ALIGN_BASELINE },
  // Also "first baseline" & "last baseline"; see CSSParserImpl::ParseAlignEnum
  { eCSSKeyword_UNKNOWN,       -1 }
};

const KTableEntry nsCSSProps::kAlignNormalBaseline[] = {
  { eCSSKeyword_normal,        NS_STYLE_ALIGN_NORMAL },
  { eCSSKeyword_baseline,      NS_STYLE_ALIGN_BASELINE },
  // Also "first baseline" & "last baseline"; see CSSParserImpl::ParseAlignEnum
  { eCSSKeyword_UNKNOWN,       -1 }
};

const KTableEntry nsCSSProps::kAlignContentDistribution[] = {
  { eCSSKeyword_stretch,       NS_STYLE_ALIGN_STRETCH },
  { eCSSKeyword_space_between, NS_STYLE_ALIGN_SPACE_BETWEEN },
  { eCSSKeyword_space_around,  NS_STYLE_ALIGN_SPACE_AROUND },
  { eCSSKeyword_space_evenly,  NS_STYLE_ALIGN_SPACE_EVENLY },
  { eCSSKeyword_UNKNOWN,       -1 }
};

const KTableEntry nsCSSProps::kAlignContentPosition[] = {
  { eCSSKeyword_start,         NS_STYLE_ALIGN_START },
  { eCSSKeyword_end,           NS_STYLE_ALIGN_END },
  { eCSSKeyword_flex_start,    NS_STYLE_ALIGN_FLEX_START },
  { eCSSKeyword_flex_end,      NS_STYLE_ALIGN_FLEX_END },
  { eCSSKeyword_center,        NS_STYLE_ALIGN_CENTER },
  { eCSSKeyword_left,          NS_STYLE_ALIGN_LEFT },
  { eCSSKeyword_right,         NS_STYLE_ALIGN_RIGHT },
  { eCSSKeyword_UNKNOWN,       -1 }
};

// <NOTE> these are only used for auto-completion, not parsing:
const KTableEntry nsCSSProps::kAutoCompletionAlignJustifySelf[] = {
  { eCSSKeyword_auto,          NS_STYLE_ALIGN_AUTO },
  { eCSSKeyword_normal,        NS_STYLE_ALIGN_NORMAL },
  { eCSSKeyword_stretch,       NS_STYLE_ALIGN_STRETCH },
  { eCSSKeyword_baseline,      NS_STYLE_ALIGN_BASELINE },
  { eCSSKeyword_last_baseline, NS_STYLE_ALIGN_LAST_BASELINE },
  { eCSSKeyword_start,         NS_STYLE_ALIGN_START },
  { eCSSKeyword_end,           NS_STYLE_ALIGN_END },
  { eCSSKeyword_flex_start,    NS_STYLE_ALIGN_FLEX_START },
  { eCSSKeyword_flex_end,      NS_STYLE_ALIGN_FLEX_END },
  { eCSSKeyword_center,        NS_STYLE_ALIGN_CENTER },
  { eCSSKeyword_left,          NS_STYLE_ALIGN_LEFT },
  { eCSSKeyword_right,         NS_STYLE_ALIGN_RIGHT },
  { eCSSKeyword_self_start,    NS_STYLE_ALIGN_SELF_START },
  { eCSSKeyword_self_end,      NS_STYLE_ALIGN_SELF_END },
  { eCSSKeyword_UNKNOWN,       -1 }
};

const KTableEntry nsCSSProps::kAutoCompletionAlignItems[] = {
  // Intentionally no 'auto' here.
  { eCSSKeyword_normal,        NS_STYLE_ALIGN_NORMAL },
  { eCSSKeyword_stretch,       NS_STYLE_ALIGN_STRETCH },
  { eCSSKeyword_baseline,      NS_STYLE_ALIGN_BASELINE },
  { eCSSKeyword_last_baseline, NS_STYLE_ALIGN_LAST_BASELINE },
  { eCSSKeyword_start,         NS_STYLE_ALIGN_START },
  { eCSSKeyword_end,           NS_STYLE_ALIGN_END },
  { eCSSKeyword_flex_start,    NS_STYLE_ALIGN_FLEX_START },
  { eCSSKeyword_flex_end,      NS_STYLE_ALIGN_FLEX_END },
  { eCSSKeyword_center,        NS_STYLE_ALIGN_CENTER },
  { eCSSKeyword_left,          NS_STYLE_ALIGN_LEFT },
  { eCSSKeyword_right,         NS_STYLE_ALIGN_RIGHT },
  { eCSSKeyword_self_start,    NS_STYLE_ALIGN_SELF_START },
  { eCSSKeyword_self_end,      NS_STYLE_ALIGN_SELF_END },
  { eCSSKeyword_UNKNOWN,       -1 }
};

const KTableEntry nsCSSProps::kAutoCompletionAlignJustifyContent[] = {
  // Intentionally no 'auto' here.
  { eCSSKeyword_normal,        NS_STYLE_ALIGN_NORMAL },
  { eCSSKeyword_baseline,      NS_STYLE_ALIGN_BASELINE },
  { eCSSKeyword_last_baseline, NS_STYLE_ALIGN_LAST_BASELINE },
  { eCSSKeyword_stretch,       NS_STYLE_ALIGN_STRETCH },
  { eCSSKeyword_space_between, NS_STYLE_ALIGN_SPACE_BETWEEN },
  { eCSSKeyword_space_around,  NS_STYLE_ALIGN_SPACE_AROUND },
  { eCSSKeyword_space_evenly,  NS_STYLE_ALIGN_SPACE_EVENLY },
  { eCSSKeyword_start,         NS_STYLE_ALIGN_START },
  { eCSSKeyword_end,           NS_STYLE_ALIGN_END },
  { eCSSKeyword_flex_start,    NS_STYLE_ALIGN_FLEX_START },
  { eCSSKeyword_flex_end,      NS_STYLE_ALIGN_FLEX_END },
  { eCSSKeyword_center,        NS_STYLE_ALIGN_CENTER },
  { eCSSKeyword_left,          NS_STYLE_ALIGN_LEFT },
  { eCSSKeyword_right,         NS_STYLE_ALIGN_RIGHT },
  { eCSSKeyword_UNKNOWN,       -1 }
};
// </NOTE>

const KTableEntry nsCSSProps::kFontSmoothingKTable[] = {
  { eCSSKeyword_auto, NS_FONT_SMOOTHING_AUTO },
  { eCSSKeyword_grayscale, NS_FONT_SMOOTHING_GRAYSCALE },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kFontVariantAlternatesFuncsKTable[] = {
  { eCSSKeyword_stylistic, NS_FONT_VARIANT_ALTERNATES_STYLISTIC },
  { eCSSKeyword_styleset, NS_FONT_VARIANT_ALTERNATES_STYLESET },
  { eCSSKeyword_character_variant, NS_FONT_VARIANT_ALTERNATES_CHARACTER_VARIANT },
  { eCSSKeyword_swash, NS_FONT_VARIANT_ALTERNATES_SWASH },
  { eCSSKeyword_ornaments, NS_FONT_VARIANT_ALTERNATES_ORNAMENTS },
  { eCSSKeyword_annotation, NS_FONT_VARIANT_ALTERNATES_ANNOTATION },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kGridAutoFlowKTable[] = {
  { eCSSKeyword_row, NS_STYLE_GRID_AUTO_FLOW_ROW },
  { eCSSKeyword_column, NS_STYLE_GRID_AUTO_FLOW_COLUMN },
  { eCSSKeyword_dense, NS_STYLE_GRID_AUTO_FLOW_DENSE },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kGridTrackBreadthKTable[] = {
  { eCSSKeyword_min_content, StyleGridTrackBreadth::MinContent },
  { eCSSKeyword_max_content, StyleGridTrackBreadth::MaxContent },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kLineHeightKTable[] = {
  // -moz- prefixed, intended for internal use for single-line controls
  { eCSSKeyword__moz_block_height, NS_STYLE_LINE_HEIGHT_BLOCK_HEIGHT },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kContainKTable[] = {
  { eCSSKeyword_none,    NS_STYLE_CONTAIN_NONE },
  { eCSSKeyword_strict,  NS_STYLE_CONTAIN_STRICT },
  { eCSSKeyword_content, NS_STYLE_CONTAIN_CONTENT },
  { eCSSKeyword_layout,  NS_STYLE_CONTAIN_LAYOUT },
  { eCSSKeyword_style,   NS_STYLE_CONTAIN_STYLE },
  { eCSSKeyword_paint,   NS_STYLE_CONTAIN_PAINT },
  { eCSSKeyword_size,    NS_STYLE_CONTAIN_SIZE },
  { eCSSKeyword_UNKNOWN, -1 }
};

// Same as kBorderStyleKTable except 'hidden'.
const KTableEntry nsCSSProps::kOutlineStyleKTable[] = {
  { eCSSKeyword_none,   NS_STYLE_BORDER_STYLE_NONE },
  { eCSSKeyword_auto,   NS_STYLE_BORDER_STYLE_AUTO },
  { eCSSKeyword_dotted, NS_STYLE_BORDER_STYLE_DOTTED },
  { eCSSKeyword_dashed, NS_STYLE_BORDER_STYLE_DASHED },
  { eCSSKeyword_solid,  NS_STYLE_BORDER_STYLE_SOLID },
  { eCSSKeyword_double, NS_STYLE_BORDER_STYLE_DOUBLE },
  { eCSSKeyword_groove, NS_STYLE_BORDER_STYLE_GROOVE },
  { eCSSKeyword_ridge,  NS_STYLE_BORDER_STYLE_RIDGE },
  { eCSSKeyword_inset,  NS_STYLE_BORDER_STYLE_INSET },
  { eCSSKeyword_outset, NS_STYLE_BORDER_STYLE_OUTSET },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kOverflowKTable[] = {
  { eCSSKeyword_auto, NS_STYLE_OVERFLOW_AUTO },
  { eCSSKeyword_visible, NS_STYLE_OVERFLOW_VISIBLE },
  { eCSSKeyword_hidden, NS_STYLE_OVERFLOW_HIDDEN },
  { eCSSKeyword_scroll, NS_STYLE_OVERFLOW_SCROLL },
  // Deprecated:
  { eCSSKeyword__moz_scrollbars_none, NS_STYLE_OVERFLOW_HIDDEN },
  { eCSSKeyword__moz_scrollbars_horizontal, NS_STYLE_OVERFLOW_SCROLLBARS_HORIZONTAL },
  { eCSSKeyword__moz_scrollbars_vertical, NS_STYLE_OVERFLOW_SCROLLBARS_VERTICAL },
  { eCSSKeyword__moz_hidden_unscrollable, NS_STYLE_OVERFLOW_CLIP },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kOverflowClipBoxKTable[] = {
  { eCSSKeyword_padding_box, NS_STYLE_OVERFLOW_CLIP_BOX_PADDING_BOX },
  { eCSSKeyword_content_box, NS_STYLE_OVERFLOW_CLIP_BOX_CONTENT_BOX },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kOverflowSubKTable[] = {
  { eCSSKeyword_auto, NS_STYLE_OVERFLOW_AUTO },
  { eCSSKeyword_visible, NS_STYLE_OVERFLOW_VISIBLE },
  { eCSSKeyword_hidden, NS_STYLE_OVERFLOW_HIDDEN },
  { eCSSKeyword_scroll, NS_STYLE_OVERFLOW_SCROLL },
  // Deprecated:
  { eCSSKeyword__moz_hidden_unscrollable, NS_STYLE_OVERFLOW_CLIP },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kRadialGradientSizeKTable[] = {
  { eCSSKeyword_closest_side,    NS_STYLE_GRADIENT_SIZE_CLOSEST_SIDE },
  { eCSSKeyword_closest_corner,  NS_STYLE_GRADIENT_SIZE_CLOSEST_CORNER },
  { eCSSKeyword_farthest_side,   NS_STYLE_GRADIENT_SIZE_FARTHEST_SIDE },
  { eCSSKeyword_farthest_corner, NS_STYLE_GRADIENT_SIZE_FARTHEST_CORNER },
  { eCSSKeyword_UNKNOWN,         -1 }
};

const KTableEntry nsCSSProps::kOverscrollBehaviorKTable[] = {
  { eCSSKeyword_auto,       StyleOverscrollBehavior::Auto },
  { eCSSKeyword_contain,    StyleOverscrollBehavior::Contain },
  { eCSSKeyword_none,       StyleOverscrollBehavior::None },
  { eCSSKeyword_UNKNOWN,    -1 }
};

const KTableEntry nsCSSProps::kScrollSnapTypeKTable[] = {
  { eCSSKeyword_none,      NS_STYLE_SCROLL_SNAP_TYPE_NONE },
  { eCSSKeyword_mandatory, NS_STYLE_SCROLL_SNAP_TYPE_MANDATORY },
  { eCSSKeyword_proximity, NS_STYLE_SCROLL_SNAP_TYPE_PROXIMITY },
  { eCSSKeyword_UNKNOWN,   -1 }
};

KTableEntry nsCSSProps::kTextAlignKTable[] = {
  { eCSSKeyword_left, NS_STYLE_TEXT_ALIGN_LEFT },
  { eCSSKeyword_right, NS_STYLE_TEXT_ALIGN_RIGHT },
  { eCSSKeyword_center, NS_STYLE_TEXT_ALIGN_CENTER },
  { eCSSKeyword_justify, NS_STYLE_TEXT_ALIGN_JUSTIFY },
  { eCSSKeyword__moz_center, NS_STYLE_TEXT_ALIGN_MOZ_CENTER },
  { eCSSKeyword__moz_right, NS_STYLE_TEXT_ALIGN_MOZ_RIGHT },
  { eCSSKeyword__moz_left, NS_STYLE_TEXT_ALIGN_MOZ_LEFT },
  { eCSSKeyword_start, NS_STYLE_TEXT_ALIGN_START },
  { eCSSKeyword_end, NS_STYLE_TEXT_ALIGN_END },
  { eCSSKeyword_unsafe, NS_STYLE_TEXT_ALIGN_UNSAFE },
  { eCSSKeyword_match_parent, NS_STYLE_TEXT_ALIGN_MATCH_PARENT },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kTextDecorationLineKTable[] = {
  { eCSSKeyword_none, NS_STYLE_TEXT_DECORATION_LINE_NONE },
  { eCSSKeyword_underline, NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE },
  { eCSSKeyword_overline, NS_STYLE_TEXT_DECORATION_LINE_OVERLINE },
  { eCSSKeyword_line_through, NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH },
  { eCSSKeyword_blink, NS_STYLE_TEXT_DECORATION_LINE_BLINK },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kTextDecorationStyleKTable[] = {
  { eCSSKeyword__moz_none, NS_STYLE_TEXT_DECORATION_STYLE_NONE },
  { eCSSKeyword_solid, NS_STYLE_TEXT_DECORATION_STYLE_SOLID },
  { eCSSKeyword_double, NS_STYLE_TEXT_DECORATION_STYLE_DOUBLE },
  { eCSSKeyword_dotted, NS_STYLE_TEXT_DECORATION_STYLE_DOTTED },
  { eCSSKeyword_dashed, NS_STYLE_TEXT_DECORATION_STYLE_DASHED },
  { eCSSKeyword_wavy, NS_STYLE_TEXT_DECORATION_STYLE_WAVY },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kTextEmphasisStyleShapeKTable[] = {
  { eCSSKeyword_dot, NS_STYLE_TEXT_EMPHASIS_STYLE_DOT },
  { eCSSKeyword_circle, NS_STYLE_TEXT_EMPHASIS_STYLE_CIRCLE },
  { eCSSKeyword_double_circle, NS_STYLE_TEXT_EMPHASIS_STYLE_DOUBLE_CIRCLE },
  { eCSSKeyword_triangle, NS_STYLE_TEXT_EMPHASIS_STYLE_TRIANGLE },
  { eCSSKeyword_sesame, NS_STYLE_TEXT_EMPHASIS_STYLE_SESAME} ,
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kTextOverflowKTable[] = {
  { eCSSKeyword_clip, NS_STYLE_TEXT_OVERFLOW_CLIP },
  { eCSSKeyword_ellipsis, NS_STYLE_TEXT_OVERFLOW_ELLIPSIS },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kTouchActionKTable[] = {
  { eCSSKeyword_none,         NS_STYLE_TOUCH_ACTION_NONE },
  { eCSSKeyword_auto,         NS_STYLE_TOUCH_ACTION_AUTO },
  { eCSSKeyword_pan_x,        NS_STYLE_TOUCH_ACTION_PAN_X },
  { eCSSKeyword_pan_y,        NS_STYLE_TOUCH_ACTION_PAN_Y },
  { eCSSKeyword_manipulation, NS_STYLE_TOUCH_ACTION_MANIPULATION },
  { eCSSKeyword_UNKNOWN,      -1 }
};

const KTableEntry nsCSSProps::kTransitionTimingFunctionKTable[] = {
  { eCSSKeyword_ease, NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE },
  { eCSSKeyword_linear, NS_STYLE_TRANSITION_TIMING_FUNCTION_LINEAR },
  { eCSSKeyword_ease_in, NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE_IN },
  { eCSSKeyword_ease_out, NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE_OUT },
  { eCSSKeyword_ease_in_out, NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE_IN_OUT },
  { eCSSKeyword_step_start, NS_STYLE_TRANSITION_TIMING_FUNCTION_STEP_START },
  { eCSSKeyword_step_end, NS_STYLE_TRANSITION_TIMING_FUNCTION_STEP_END },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kVerticalAlignKTable[] = {
  { eCSSKeyword_baseline, NS_STYLE_VERTICAL_ALIGN_BASELINE },
  { eCSSKeyword_sub, NS_STYLE_VERTICAL_ALIGN_SUB },
  { eCSSKeyword_super, NS_STYLE_VERTICAL_ALIGN_SUPER },
  { eCSSKeyword_top, NS_STYLE_VERTICAL_ALIGN_TOP },
  { eCSSKeyword_text_top, NS_STYLE_VERTICAL_ALIGN_TEXT_TOP },
  { eCSSKeyword_middle, NS_STYLE_VERTICAL_ALIGN_MIDDLE },
  { eCSSKeyword__moz_middle_with_baseline, NS_STYLE_VERTICAL_ALIGN_MIDDLE_WITH_BASELINE },
  { eCSSKeyword_bottom, NS_STYLE_VERTICAL_ALIGN_BOTTOM },
  { eCSSKeyword_text_bottom, NS_STYLE_VERTICAL_ALIGN_TEXT_BOTTOM },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kWidthKTable[] = {
  { eCSSKeyword__moz_max_content, NS_STYLE_WIDTH_MAX_CONTENT },
  { eCSSKeyword__moz_min_content, NS_STYLE_WIDTH_MIN_CONTENT },
  { eCSSKeyword__moz_fit_content, NS_STYLE_WIDTH_FIT_CONTENT },
  { eCSSKeyword__moz_available, NS_STYLE_WIDTH_AVAILABLE },
  { eCSSKeyword_UNKNOWN, -1 }
};

// This must be the same as kWidthKTable, but just with 'content' added:
const KTableEntry nsCSSProps::kFlexBasisKTable[] = {
  { eCSSKeyword__moz_max_content, NS_STYLE_WIDTH_MAX_CONTENT },
  { eCSSKeyword__moz_min_content, NS_STYLE_WIDTH_MIN_CONTENT },
  { eCSSKeyword__moz_fit_content, NS_STYLE_WIDTH_FIT_CONTENT },
  { eCSSKeyword__moz_available,   NS_STYLE_WIDTH_AVAILABLE },
  { eCSSKeyword_content,          NS_STYLE_FLEX_BASIS_CONTENT },
  { eCSSKeyword_UNKNOWN, -1 }
};
static_assert(ArrayLength(nsCSSProps::kFlexBasisKTable) ==
              ArrayLength(nsCSSProps::kWidthKTable) + 1,
              "kFlexBasisKTable should have the same entries as "
              "kWidthKTable, plus one more for 'content'");

// Specific keyword tables for XUL.properties

// keyword tables for SVG properties

const KTableEntry nsCSSProps::kClipPathGeometryBoxKTable[] = {
  { eCSSKeyword_content_box, StyleGeometryBox::ContentBox },
  { eCSSKeyword_padding_box, StyleGeometryBox::PaddingBox },
  { eCSSKeyword_border_box, StyleGeometryBox::BorderBox },
  { eCSSKeyword_margin_box, StyleGeometryBox::MarginBox },
  { eCSSKeyword_fill_box, StyleGeometryBox::FillBox },
  { eCSSKeyword_stroke_box, StyleGeometryBox::StrokeBox },
  { eCSSKeyword_view_box, StyleGeometryBox::ViewBox },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kShapeRadiusKTable[] = {
  { eCSSKeyword_closest_side, StyleShapeRadius::ClosestSide },
  { eCSSKeyword_farthest_side, StyleShapeRadius::FarthestSide },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kFilterFunctionKTable[] = {
  { eCSSKeyword_blur, NS_STYLE_FILTER_BLUR },
  { eCSSKeyword_brightness, NS_STYLE_FILTER_BRIGHTNESS },
  { eCSSKeyword_contrast, NS_STYLE_FILTER_CONTRAST },
  { eCSSKeyword_grayscale, NS_STYLE_FILTER_GRAYSCALE },
  { eCSSKeyword_invert, NS_STYLE_FILTER_INVERT },
  { eCSSKeyword_opacity, NS_STYLE_FILTER_OPACITY },
  { eCSSKeyword_saturate, NS_STYLE_FILTER_SATURATE },
  { eCSSKeyword_sepia, NS_STYLE_FILTER_SEPIA },
  { eCSSKeyword_hue_rotate, NS_STYLE_FILTER_HUE_ROTATE },
  { eCSSKeyword_drop_shadow, NS_STYLE_FILTER_DROP_SHADOW },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kShapeOutsideShapeBoxKTable[] = {
  { eCSSKeyword_content_box, StyleGeometryBox::ContentBox },
  { eCSSKeyword_padding_box, StyleGeometryBox::PaddingBox },
  { eCSSKeyword_border_box, StyleGeometryBox::BorderBox },
  { eCSSKeyword_margin_box, StyleGeometryBox::MarginBox },
  { eCSSKeyword_UNKNOWN, -1 }
};

int32_t
nsCSSProps::FindIndexOfKeyword(nsCSSKeyword aKeyword,
                               const KTableEntry aTable[])
{
  if (eCSSKeyword_UNKNOWN == aKeyword) {
    // NOTE: we can have keyword tables where eCSSKeyword_UNKNOWN is used
    // not only for the sentinel, but also in the middle of the table to
    // knock out values that have been disabled by prefs, e.g. kDisplayKTable.
    // So we deal with eCSSKeyword_UNKNOWN up front to avoid returning a valid
    // index in the loop below.
    return -1;
  }
  for (int32_t i = 0; ; ++i) {
    const KTableEntry& entry = aTable[i];
    if (entry.IsSentinel()) {
      break;
    }
    if (aKeyword == entry.mKeyword) {
      return i;
    }
  }
  return -1;
}

bool
nsCSSProps::FindKeyword(nsCSSKeyword aKeyword, const KTableEntry aTable[],
                        int32_t& aResult)
{
  int32_t index = FindIndexOfKeyword(aKeyword, aTable);
  if (index >= 0) {
    aResult = aTable[index].mValue;
    return true;
  }
  return false;
}

nsCSSKeyword
nsCSSProps::ValueToKeywordEnum(int32_t aValue, const KTableEntry aTable[])
{
#ifdef DEBUG
  typedef decltype(aTable[0].mValue) table_value_type;
  NS_ASSERTION(table_value_type(aValue) == aValue, "Value out of range");
#endif
  for (int32_t i = 0; ; ++i) {
    const KTableEntry& entry = aTable[i];
    if (entry.IsSentinel()) {
      break;
    }
    if (aValue == entry.mValue) {
      return entry.mKeyword;
    }
  }
  return eCSSKeyword_UNKNOWN;
}

const nsCString&
nsCSSProps::ValueToKeyword(int32_t aValue, const KTableEntry aTable[])
{
  nsCSSKeyword keyword = ValueToKeywordEnum(aValue, aTable);
  if (keyword == eCSSKeyword_UNKNOWN) {
    static nsDependentCString sNullStr("");
    return sNullStr;
  } else {
    return nsCSSKeywords::GetStringValue(keyword);
  }
}

const CSSPropFlags nsCSSProps::kFlagsTable[eCSSProperty_COUNT] = {
#define CSS_PROP_LONGHAND(name_, id_, method_, flags_, ...) flags_,
#define CSS_PROP_SHORTHAND(name_, id_, method_, flags_, ...) flags_,
#include "mozilla/ServoCSSPropList.h"
#undef CSS_PROP_SHORTHAND
#undef CSS_PROP_LONGHAND
};

/* static */ bool
nsCSSProps::gPropertyEnabled[eCSSProperty_COUNT_with_aliases] = {
  // If the property has any "ENABLED_IN" flag set, it is disabled by
  // default. Note that, if a property has pref, whatever its default
  // value is, it will later be changed in nsCSSProps::AddRefTable().
  // If the property has "ENABLED_IN" flags but doesn't have a pref,
  // it is an internal property which is disabled elsewhere.
  #define IS_ENABLED_BY_DEFAULT(flags_) \
    (!((flags_) & (CSSPropFlags::EnabledMask | CSSPropFlags::Inaccessible)))

  #define CSS_PROP_LONGHAND(name_, id_, method_, flags_, ...) \
    IS_ENABLED_BY_DEFAULT(flags_),
  #define CSS_PROP_SHORTHAND(name_, id_, method_, flags_, ...) \
    IS_ENABLED_BY_DEFAULT(flags_),
  #define CSS_PROP_ALIAS(...) true,
  #include "mozilla/ServoCSSPropList.h"
  #undef CSS_PROP_ALIAS
  #undef CSS_PROP_SHORTHAND
  #undef CSS_PROP_LONGHAND

  #undef IS_ENABLED_BY_DEFAULT
};

#include "../../dom/base/PropertyUseCounterMap.inc"

/* static */ const UseCounter
nsCSSProps::gPropertyUseCounter[eCSSProperty_COUNT_no_shorthands] = {
  #define CSS_PROP_PUBLIC_OR_PRIVATE(publicname_, privatename_) privatename_
  // Need an extra level of macro nesting to force expansion of method_
  // params before they get pasted.
  #define CSS_PROP_USE_COUNTER(method_) \
    static_cast<UseCounter>(USE_COUNTER_FOR_CSS_PROPERTY_##method_),
  #define CSS_PROP_LONGHAND(name_, id_, method_, ...) \
    CSS_PROP_USE_COUNTER(method_)
  #include "mozilla/ServoCSSPropList.h"
  #undef CSS_PROP_LONGHAND
  #undef CSS_PROP_USE_COUNTER
  #undef CSS_PROP_PUBLIC_OR_PRIVATE
};

#include "nsCSSPropsGenerated.inc"
