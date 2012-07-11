/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsARIAMap.h"

#include "nsCoreUtils.h"
#include "Role.h"
#include "States.h"

#include "nsIContent.h"
#include "nsWhitespaceTokenizer.h"

using namespace mozilla;
using namespace mozilla::a11y;
using namespace mozilla::a11y::aria;

/**
 *  This list of WAI-defined roles are currently hardcoded.
 *  Eventually we will most likely be loading an RDF resource that contains this information
 *  Using RDF will also allow for role extensibility. See bug 280138.
 *
 *  Definition of nsRoleMapEntry contains comments explaining this table.
 *
 *  When no nsIAccessibleRole enum mapping exists for an ARIA role, the
 *  role will be exposed via the object attribute "xml-roles".
 *  In addition, in MSAA, the unmapped role will also be exposed as a BSTR string role.
 *
 *  There are no nsIAccessibleRole enums for the following landmark roles:
 *    banner, contentinfo, main, navigation, note, search, secondary, seealso, breadcrumbs
 */

static nsRoleMapEntry sWAIRoleMaps[] =
{
  { // alert
    &nsGkAtoms::alert,
    roles::ALERT,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  { // alertdialog
    &nsGkAtoms::alertdialog,
    roles::DIALOG,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  { // application
    &nsGkAtoms::application,
    roles::APPLICATION,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  { // article
    &nsGkAtoms::article,
    roles::DOCUMENT,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates,
    eReadonlyUntilEditable
  },
  { // button
    &nsGkAtoms::button,
    roles::PUSHBUTTON,
    kUseMapRole,
    eNoValue,
    ePressAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIAPressed
  },
  { // checkbox
    &nsGkAtoms::checkbox,
    roles::CHECKBUTTON,
    kUseMapRole,
    eNoValue,
    eCheckUncheckAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIACheckableMixed,
    eARIAReadonly
  },
  { // columnheader
    &nsGkAtoms::columnheader,
    roles::COLUMNHEADER,
    kUseMapRole,
    eNoValue,
    eSortAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIASelectable,
    eARIAReadonly
  },
  { // combobox
    &nsGkAtoms::combobox,
    roles::COMBOBOX,
    kUseMapRole,
    eNoValue,
    eOpenCloseAction,
    eNoLiveAttr,
    states::COLLAPSED | states::HASPOPUP,
    eARIAAutoComplete,
    eARIAReadonly
  },
  { // dialog
    &nsGkAtoms::dialog,
    roles::DIALOG,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  { // directory
    &nsGkAtoms::directory,
    roles::LIST,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  { // document
    &nsGkAtoms::document,
    roles::DOCUMENT,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates,
    eReadonlyUntilEditable
  },
  { // form
    &nsGkAtoms::form,
    roles::FORM,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  { // grid
    &nsGkAtoms::grid,
    roles::TABLE,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    states::FOCUSABLE,
    eARIAMultiSelectable,
    eARIAReadonly
  },
  { // gridcell
    &nsGkAtoms::gridcell,
    roles::GRID_CELL,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIASelectable,
    eARIAReadonly
  },
  { // group
    &nsGkAtoms::group,
    roles::GROUPING,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  { // heading
    &nsGkAtoms::heading,
    roles::HEADING,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  { // img
    &nsGkAtoms::img,
    roles::GRAPHIC,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  { // link
    &nsGkAtoms::link,
    roles::LINK,
    kUseMapRole,
    eNoValue,
    eJumpAction,
    eNoLiveAttr,
    states::LINKED
  },
  { // list
    &nsGkAtoms::list,
    roles::LIST,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    states::READONLY
  },
  { // listbox
    &nsGkAtoms::listbox,
    roles::LISTBOX,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIAMultiSelectable,
    eARIAReadonly
  },
  { // listitem
    &nsGkAtoms::listitem,
    roles::LISTITEM,
    kUseMapRole,
    eNoValue,
    eNoAction, // XXX: should depend on state, parent accessible
    eNoLiveAttr,
    states::READONLY
  },
  { // log
    &nsGkAtoms::log_,
    roles::NOTHING,
    kUseNativeRole,
    eNoValue,
    eNoAction,
    ePoliteLiveAttr,
    kNoReqStates
  },
  { // marquee
    &nsGkAtoms::marquee,
    roles::ANIMATION,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eOffLiveAttr,
    kNoReqStates
  },
  { // math
    &nsGkAtoms::math,
    roles::FLAT_EQUATION,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  { // menu
    &nsGkAtoms::menu,
    roles::MENUPOPUP,
    kUseMapRole,
    eNoValue,
    eNoAction, // XXX: technically accessibles of menupopup role haven't
               // any action, but menu can be open or close.
    eNoLiveAttr,
    kNoReqStates
  },
  { // menubar
    &nsGkAtoms::menubar,
    roles::MENUBAR,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  { // menuitem
    &nsGkAtoms::menuitem,
    roles::MENUITEM,
    kUseMapRole,
    eNoValue,
    eClickAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIACheckedMixed
  },
  { // menuitemcheckbox
    &nsGkAtoms::menuitemcheckbox,
    roles::CHECK_MENU_ITEM,
    kUseMapRole,
    eNoValue,
    eClickAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIACheckableMixed
  },
  { // menuitemradio
    &nsGkAtoms::menuitemradio,
    roles::RADIO_MENU_ITEM,
    kUseMapRole,
    eNoValue,
    eClickAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIACheckableBool
  },
  { // note
    &nsGkAtoms::note_,
    roles::NOTE,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  { // option
    &nsGkAtoms::option,
    roles::OPTION,
    kUseMapRole,
    eNoValue,
    eSelectAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIASelectable,
    eARIACheckedMixed
  },
  { // presentation
    &nsGkAtoms::presentation,
    roles::NOTHING,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  { // progressbar
    &nsGkAtoms::progressbar,
    roles::PROGRESSBAR,
    kUseMapRole,
    eHasValueMinMax,
    eNoAction,
    eNoLiveAttr,
    states::READONLY,
    eIndeterminateIfNoValue
  },
  { // radio
    &nsGkAtoms::radio,
    roles::RADIOBUTTON,
    kUseMapRole,
    eNoValue,
    eSelectAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIACheckableBool
  },
  { // radiogroup
    &nsGkAtoms::radiogroup,
    roles::GROUPING,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  { // region
    &nsGkAtoms::region,
    roles::PANE,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  { // row
    &nsGkAtoms::row,
    roles::ROW,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIASelectable
  },
  { // rowheader
    &nsGkAtoms::rowheader,
    roles::ROWHEADER,
    kUseMapRole,
    eNoValue,
    eSortAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIASelectable,
    eARIAReadonly
  },
  { // scrollbar
    &nsGkAtoms::scrollbar,
    roles::SCROLLBAR,
    kUseMapRole,
    eHasValueMinMax,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIAOrientation,
    eARIAReadonly
  },
  { // separator
    &nsGkAtoms::separator_,
    roles::SEPARATOR,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIAOrientation
  },
  { // slider
    &nsGkAtoms::slider,
    roles::SLIDER,
    kUseMapRole,
    eHasValueMinMax,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIAOrientation,
    eARIAReadonly
  },
  { // spinbutton
    &nsGkAtoms::spinbutton,
    roles::SPINBUTTON,
    kUseMapRole,
    eHasValueMinMax,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIAReadonly
  },
  { // status
    &nsGkAtoms::status,
    roles::STATUSBAR,
    kUseMapRole,
    eNoValue,
    eNoAction,
    ePoliteLiveAttr,
    kNoReqStates
  },
  { // tab
    &nsGkAtoms::tab,
    roles::PAGETAB,
    kUseMapRole,
    eNoValue,
    eSwitchAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIASelectable
  },
  { // tablist
    &nsGkAtoms::tablist,
    roles::PAGETABLIST,
    kUseMapRole,
    eNoValue,
    eNoAction,
    ePoliteLiveAttr,
    kNoReqStates
  },
  { // tabpanel
    &nsGkAtoms::tabpanel,
    roles::PROPERTYPAGE,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  { // textbox
    &nsGkAtoms::textbox,
    roles::ENTRY,
    kUseMapRole,
    eNoValue,
    eActivateAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIAAutoComplete,
    eARIAMultiline,
    eARIAReadonlyOrEditable
  },
  { // timer
    &nsGkAtoms::timer,
    roles::NOTHING,
    kUseNativeRole,
    eNoValue,
    eNoAction,
    eOffLiveAttr,
    kNoReqStates
  },
  { // toolbar
    &nsGkAtoms::toolbar,
    roles::TOOLBAR,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  { // tooltip
    &nsGkAtoms::tooltip,
    roles::TOOLTIP,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  { // tree
    &nsGkAtoms::tree,
    roles::OUTLINE,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIAReadonly,
    eARIAMultiSelectable
  },
  { // treegrid
    &nsGkAtoms::treegrid,
    roles::TREE_TABLE,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIAReadonly,
    eARIAMultiSelectable
  },
  { // treeitem
    &nsGkAtoms::treeitem,
    roles::OUTLINEITEM,
    kUseMapRole,
    eNoValue,
    eActivateAction, // XXX: should expose second 'expand/collapse' action based
                     // on states
    eNoLiveAttr,
    kNoReqStates,
    eARIASelectable,
    eARIACheckedMixed
  }
};

static nsRoleMapEntry sLandmarkRoleMap = {
  &nsGkAtoms::_empty,
  roles::NOTHING,
  kUseNativeRole,
  eNoValue,
  eNoAction,
  eNoLiveAttr,
  kNoReqStates
};

nsRoleMapEntry nsARIAMap::gEmptyRoleMap = {
  &nsGkAtoms::_empty,
  roles::NOTHING,
  kUseMapRole,
  eNoValue,
  eNoAction,
  eNoLiveAttr,
  kNoReqStates
};

/**
 * Universal (Global) states:
 * The following state rules are applied to any accessible element,
 * whether there is an ARIA role or not:
 */
static const EStateRule sWAIUnivStateMap[] = {
  eARIABusy,
  eARIADisabled,
  eARIAExpanded,  // Currently under spec review but precedent exists
  eARIAHasPopup,  // Note this is technically a "property"
  eARIAInvalid,
  eARIARequired,  // XXX not global, Bug 553117
  eARIANone
};


/**
 * ARIA attribute map for attribute characteristics
 * 
 * @note ARIA attributes that don't have any flags are not included here
 */
nsAttributeCharacteristics nsARIAMap::gWAIUnivAttrMap[] = {
  {&nsGkAtoms::aria_activedescendant,  ATTR_BYPASSOBJ                 },
  {&nsGkAtoms::aria_atomic,                             ATTR_VALTOKEN },
  {&nsGkAtoms::aria_busy,                               ATTR_VALTOKEN },
  {&nsGkAtoms::aria_checked,           ATTR_BYPASSOBJ | ATTR_VALTOKEN }, /* exposes checkable obj attr */
  {&nsGkAtoms::aria_controls,          ATTR_BYPASSOBJ                 },
  {&nsGkAtoms::aria_describedby,       ATTR_BYPASSOBJ                 },
  {&nsGkAtoms::aria_disabled,          ATTR_BYPASSOBJ | ATTR_VALTOKEN },
  {&nsGkAtoms::aria_dropeffect,                         ATTR_VALTOKEN },
  {&nsGkAtoms::aria_expanded,          ATTR_BYPASSOBJ | ATTR_VALTOKEN },
  {&nsGkAtoms::aria_flowto,            ATTR_BYPASSOBJ                 },  
  {&nsGkAtoms::aria_grabbed,                            ATTR_VALTOKEN },
  {&nsGkAtoms::aria_haspopup,          ATTR_BYPASSOBJ | ATTR_VALTOKEN },
  {&nsGkAtoms::aria_hidden,                             ATTR_VALTOKEN },/* always expose obj attr */
  {&nsGkAtoms::aria_invalid,           ATTR_BYPASSOBJ | ATTR_VALTOKEN },
  {&nsGkAtoms::aria_label,             ATTR_BYPASSOBJ                 },
  {&nsGkAtoms::aria_labelledby,        ATTR_BYPASSOBJ                 },
  {&nsGkAtoms::aria_level,             ATTR_BYPASSOBJ                 }, /* handled via groupPosition */
  {&nsGkAtoms::aria_live,                               ATTR_VALTOKEN },
  {&nsGkAtoms::aria_multiline,         ATTR_BYPASSOBJ | ATTR_VALTOKEN },
  {&nsGkAtoms::aria_multiselectable,   ATTR_BYPASSOBJ | ATTR_VALTOKEN },
  {&nsGkAtoms::aria_owns,              ATTR_BYPASSOBJ                 },
  {&nsGkAtoms::aria_orientation,                        ATTR_VALTOKEN },
  {&nsGkAtoms::aria_posinset,          ATTR_BYPASSOBJ                 }, /* handled via groupPosition */
  {&nsGkAtoms::aria_pressed,           ATTR_BYPASSOBJ | ATTR_VALTOKEN },
  {&nsGkAtoms::aria_readonly,          ATTR_BYPASSOBJ | ATTR_VALTOKEN },
  {&nsGkAtoms::aria_relevant,          ATTR_BYPASSOBJ                 },
  {&nsGkAtoms::aria_required,          ATTR_BYPASSOBJ | ATTR_VALTOKEN },
  {&nsGkAtoms::aria_selected,          ATTR_BYPASSOBJ | ATTR_VALTOKEN },
  {&nsGkAtoms::aria_setsize,           ATTR_BYPASSOBJ                 }, /* handled via groupPosition */
  {&nsGkAtoms::aria_sort,                               ATTR_VALTOKEN },
  {&nsGkAtoms::aria_valuenow,          ATTR_BYPASSOBJ                 },
  {&nsGkAtoms::aria_valuemin,          ATTR_BYPASSOBJ                 },
  {&nsGkAtoms::aria_valuemax,          ATTR_BYPASSOBJ                 },
  {&nsGkAtoms::aria_valuetext,         ATTR_BYPASSOBJ                 }
};

PRUint32
nsARIAMap::gWAIUnivAttrMapLength = NS_ARRAY_LENGTH(nsARIAMap::gWAIUnivAttrMap);

nsRoleMapEntry*
aria::GetRoleMap(nsINode* aNode)
{
  nsIContent* content = nsCoreUtils::GetRoleContent(aNode);
  nsAutoString roles;
  if (!content ||
      !content->GetAttr(kNameSpaceID_None, nsGkAtoms::role, roles) ||
      roles.IsEmpty()) {
    // We treat role="" as if the role attribute is absent (per aria spec:8.1.1)
    return nsnull;
  }

  nsWhitespaceTokenizer tokenizer(roles);
  while (tokenizer.hasMoreTokens()) {
    // Do a binary search through table for the next role in role list
    const nsDependentSubstring role = tokenizer.nextToken();
    PRUint32 low = 0;
    PRUint32 high = ArrayLength(sWAIRoleMaps);
    while (low < high) {
      PRUint32 idx = (low + high) / 2;
      PRInt32 compare = Compare(role, sWAIRoleMaps[idx].ARIARoleString());
      if (compare == 0)
        return sWAIRoleMaps + idx;

      if (compare < 0)
        high = idx;
      else
        low = idx + 1;
    }
  }

  // Always use some entry if there is a non-empty role string
  // To ensure an accessible object is created
  return &sLandmarkRoleMap;
}

PRUint64
aria::UniversalStatesFor(mozilla::dom::Element* aElement)
{
  PRUint64 state = 0;
  PRUint32 index = 0;
  while (MapToState(sWAIUnivStateMap[index], aElement, &state))
    index++;

  return state;
}
