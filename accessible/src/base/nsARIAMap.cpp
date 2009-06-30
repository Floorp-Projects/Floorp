/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
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
 * The Initial Developer of the Original Code is IBM Corporation
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Aaron Leventhal <aleventh@us.ibm.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsARIAMap.h"
#include "nsIAccessibleRole.h"
#include "nsIAccessibleStates.h"

/**
 *  This list of WAI-defined roles are currently hardcoded.
 *  Eventually we will most likely be loading an RDF resource that contains this information
 *  Using RDF will also allow for role extensibility. See bug 280138.
 *
 *  Definition of nsRoleMapEntry and nsStateMapEntry contains comments explaining this table.
 *
 *  When no nsIAccessibleRole enum mapping exists for an ARIA role, the
 *  role will be exposed via the object attribute "xml-roles".
 *  In addition, in MSAA, the unmapped role will also be exposed as a BSTR string role.
 *
 *  There are no nsIAccessibleRole enums for the following landmark roles:
 *    banner, contentinfo, main, navigation, note, search, secondary, seealso, breadcrumbs
 */

nsRoleMapEntry nsARIAMap::gWAIRoleMap[] = 
{
  {
    "alert",
    nsIAccessibleRole::ROLE_ALERT,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  {
    "alertdialog",
    nsIAccessibleRole::ROLE_DIALOG,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  {
    "application",
    nsIAccessibleRole::ROLE_APPLICATION,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  {
    "article",
    nsIAccessibleRole::ROLE_DOCUMENT,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    nsIAccessibleStates::STATE_READONLY
  },
  {
    "button",
    nsIAccessibleRole::ROLE_PUSHBUTTON,
    kUseMapRole,
    eNoValue,
    eClickAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIAPressed
  },
  {
    "checkbox",
    nsIAccessibleRole::ROLE_CHECKBUTTON,
    kUseMapRole,
    eNoValue,
    eCheckUncheckAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIACheckableMixed,
    eARIAReadonly
  },
  {
    "columnheader",
    nsIAccessibleRole::ROLE_COLUMNHEADER,
    kUseMapRole,
    eNoValue,
    eSortAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIASelectable,
    eARIAReadonly
  },
  {
    "combobox",
    nsIAccessibleRole::ROLE_COMBOBOX,
    kUseMapRole,
    eHasValueMinMax,
    eOpenCloseAction,
    eNoLiveAttr,
    nsIAccessibleStates::STATE_COLLAPSED | nsIAccessibleStates::STATE_HASPOPUP,
    eARIAAutoComplete,
    eARIAReadonly
  },
  {
    "dialog",
    nsIAccessibleRole::ROLE_DIALOG,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  {
    "document",
    nsIAccessibleRole::ROLE_DOCUMENT,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    nsIAccessibleStates::STATE_READONLY
  },
  {
    "grid",
    nsIAccessibleRole::ROLE_TABLE,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    nsIAccessibleStates::STATE_FOCUSABLE,
    eARIAMultiSelectable,
    eARIAReadonly
  },
  {
    "gridcell",
    nsIAccessibleRole::ROLE_GRID_CELL,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIASelectable,
    eARIAReadonly
  },
  {
    "group",
    nsIAccessibleRole::ROLE_GROUPING,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  {
    "heading",
    nsIAccessibleRole::ROLE_HEADING,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  {
    "img",
    nsIAccessibleRole::ROLE_GRAPHIC,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  {
    "label",
    nsIAccessibleRole::ROLE_LABEL,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  {
    "link",
    nsIAccessibleRole::ROLE_LINK,
    kUseMapRole,
    eNoValue,
    eJumpAction,
    eNoLiveAttr,
    nsIAccessibleStates::STATE_LINKED
  },
  {
    "list",
    nsIAccessibleRole::ROLE_LIST,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    nsIAccessibleStates::STATE_READONLY,
    eARIAMultiSelectable
  },
  {
    "listbox",
    nsIAccessibleRole::ROLE_LISTBOX,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIAMultiSelectable,
    eARIAReadonly
  },
  {
    "listitem",
    nsIAccessibleRole::ROLE_LISTITEM,
    kUseMapRole,
    eNoValue,
    eNoAction, // XXX: should depend on state, parent accessible
    eNoLiveAttr,
    nsIAccessibleStates::STATE_READONLY,
    eARIASelectable,
    eARIACheckedMixed
  },
  {
    "log",
    nsIAccessibleRole::ROLE_NOTHING,
    kUseNativeRole,
    eNoValue,
    eNoAction,
    ePoliteLiveAttr,
    kNoReqStates
  },
  {
    "marquee",
    nsIAccessibleRole::ROLE_NOTHING,
    kUseNativeRole,
    eNoValue,
    eNoAction,
    eOffLiveAttr,
    kNoReqStates
  },
  {
    "math",
    nsIAccessibleRole::ROLE_FLAT_EQUATION,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  {
    "menu",
    nsIAccessibleRole::ROLE_MENUPOPUP,
    kUseMapRole,
    eNoValue,
    eNoAction, // XXX: technically accessibles of menupopup role haven't
               // any action, but menu can be open or close.
    eNoLiveAttr,
    kNoReqStates
  },
  {
    "menubar",
    nsIAccessibleRole::ROLE_MENUBAR,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  {
    "menuitem",
    nsIAccessibleRole::ROLE_MENUITEM,
    kUseMapRole,
    eNoValue,
    eClickAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIACheckedMixed
  },
  {
    "menuitemcheckbox",
    nsIAccessibleRole::ROLE_CHECK_MENU_ITEM,
    kUseMapRole,
    eNoValue,
    eClickAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIACheckableMixed
  },
  {
    "menuitemradio",
    nsIAccessibleRole::ROLE_RADIO_MENU_ITEM,
    kUseMapRole,
    eNoValue,
    eClickAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIACheckableBool
  },
  {
    "option",
    nsIAccessibleRole::ROLE_OPTION,
    kUseMapRole,
    eNoValue,
    eSelectAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIASelectable,
    eARIACheckedMixed
  },
  {
    "presentation",
    nsIAccessibleRole::ROLE_NOTHING,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  {
    "progressbar",
    nsIAccessibleRole::ROLE_PROGRESSBAR,
    kUseMapRole,
    eHasValueMinMax,
    eNoAction,
    eNoLiveAttr,
    nsIAccessibleStates::STATE_READONLY
  },
  {
    "radio",
    nsIAccessibleRole::ROLE_RADIOBUTTON,
    kUseMapRole,
    eNoValue,
    eSelectAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIACheckableBool
  },
  {
    "radiogroup",
    nsIAccessibleRole::ROLE_GROUPING,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  {
    "region",
    nsIAccessibleRole::ROLE_PANE,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  {
    "row",
    nsIAccessibleRole::ROLE_ROW,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIASelectable
  },
  {
    "rowheader",
    nsIAccessibleRole::ROLE_ROWHEADER,
    kUseMapRole,
    eNoValue,
    eSortAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIASelectable,
    eARIAReadonly
  },
  {
    "section",
    nsIAccessibleRole::ROLE_SECTION,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  {
    "separator",
    nsIAccessibleRole::ROLE_SEPARATOR,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  {
    "slider",
    nsIAccessibleRole::ROLE_SLIDER,
    kUseMapRole,
    eHasValueMinMax,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIAReadonly
  },
  {
    "spinbutton",
    nsIAccessibleRole::ROLE_SPINBUTTON,
    kUseMapRole,
    eHasValueMinMax,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIAReadonly
  },
  {
    "status",
    nsIAccessibleRole::ROLE_STATUSBAR,
    kUseMapRole,
    eNoValue,
    eNoAction,
    ePoliteLiveAttr,
    kNoReqStates
  },
  {
    "tab",
    nsIAccessibleRole::ROLE_PAGETAB,
    kUseMapRole,
    eNoValue,
    eSwitchAction,
    eNoLiveAttr,
    kNoReqStates
  },
  {
    "tablist",
    nsIAccessibleRole::ROLE_PAGETABLIST,
    kUseMapRole,
    eNoValue,
    eNoAction,
    ePoliteLiveAttr,
    kNoReqStates
  },
  {
    "tabpanel",
    nsIAccessibleRole::ROLE_PROPERTYPAGE,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  {
    "textbox",
    nsIAccessibleRole::ROLE_ENTRY,
    kUseMapRole,
    eNoValue,
    eActivateAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIAAutoComplete,
    eARIAMultiline,
    eARIAReadonlyOrEditable
  },
  {
    "timer",
    nsIAccessibleRole::ROLE_NOTHING,
    kUseNativeRole,
    eNoValue,
    eNoAction,
    eOffLiveAttr,
    kNoReqStates
  },
  {
    "toolbar",
    nsIAccessibleRole::ROLE_TOOLBAR,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  {
    "tooltip",
    nsIAccessibleRole::ROLE_TOOLTIP,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates
  },
  {
    "tree",
    nsIAccessibleRole::ROLE_OUTLINE,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIAReadonly,
    eARIAMultiSelectable
  },
  {
    "treegrid",
    nsIAccessibleRole::ROLE_TREE_TABLE,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kNoReqStates,
    eARIAReadonly,
    eARIAMultiSelectable
  },
  {
    "treeitem",
    nsIAccessibleRole::ROLE_OUTLINEITEM,
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

PRUint32 nsARIAMap::gWAIRoleMapLength = NS_ARRAY_LENGTH(nsARIAMap::gWAIRoleMap);

nsRoleMapEntry nsARIAMap::gLandmarkRoleMap = {
  "",
  nsIAccessibleRole::ROLE_NOTHING,
  kUseNativeRole,
  eNoValue,
  eNoAction,
  eNoLiveAttr,
  kNoReqStates
};

nsRoleMapEntry nsARIAMap::gEmptyRoleMap = {
  "",
  nsIAccessibleRole::ROLE_NOTHING,
  kUseMapRole,
  eNoValue,
  eNoAction,
  eNoLiveAttr,
  kNoReqStates
};

nsStateMapEntry nsARIAMap::gWAIStateMap[] = {
  // eARIANone
  nsStateMapEntry(),

  // eARIAAutoComplete
  nsStateMapEntry(&nsAccessibilityAtoms::aria_autocomplete,
                  "inline", 0, nsIAccessibleStates::EXT_STATE_SUPPORTS_AUTOCOMPLETION,
                  "list", nsIAccessibleStates::STATE_HASPOPUP, nsIAccessibleStates::EXT_STATE_SUPPORTS_AUTOCOMPLETION,
                  "both", nsIAccessibleStates::STATE_HASPOPUP, nsIAccessibleStates::EXT_STATE_SUPPORTS_AUTOCOMPLETION),

  // eARIABusy
  nsStateMapEntry(&nsAccessibilityAtoms::aria_busy,
                  "true", nsIAccessibleStates::STATE_BUSY, 0,
                  "error", nsIAccessibleStates::STATE_INVALID, 0),

  // eARIACheckableBool
  nsStateMapEntry(&nsAccessibilityAtoms::aria_checked, kBoolType,
                  nsIAccessibleStates::STATE_CHECKABLE,
                  nsIAccessibleStates::STATE_CHECKED, 0,
                  0, 0, PR_TRUE),

  // eARIACheckableMixed
  nsStateMapEntry(&nsAccessibilityAtoms::aria_checked, kMixedType,
                  nsIAccessibleStates::STATE_CHECKABLE,
                  nsIAccessibleStates::STATE_CHECKED, 0,
                  0, 0, PR_TRUE),

  // eARIACheckedMixed
  nsStateMapEntry(&nsAccessibilityAtoms::aria_checked, kMixedType,
                  nsIAccessibleStates::STATE_CHECKABLE,
                  nsIAccessibleStates::STATE_CHECKED, 0),

  // eARIADisabled
  nsStateMapEntry(&nsAccessibilityAtoms::aria_disabled, kBoolType, 0,
                  nsIAccessibleStates::STATE_UNAVAILABLE, 0),

  // eARIAExpanded
  nsStateMapEntry(&nsAccessibilityAtoms::aria_expanded, kBoolType, 0,
                  nsIAccessibleStates::STATE_EXPANDED, 0,
                  nsIAccessibleStates::STATE_COLLAPSED, 0),

  // eARIAHasPopup
  nsStateMapEntry(&nsAccessibilityAtoms::aria_haspopup, kBoolType, 0,
                  nsIAccessibleStates::STATE_HASPOPUP, 0),

  // eARIAInvalid
  nsStateMapEntry(&nsAccessibilityAtoms::aria_invalid, kBoolType, 0,
                  nsIAccessibleStates::STATE_INVALID, 0),

  // eARIAMultiline
  nsStateMapEntry(&nsAccessibilityAtoms::aria_multiline, kBoolType, 0,
                  0, nsIAccessibleStates::EXT_STATE_MULTI_LINE,
                  0, nsIAccessibleStates::EXT_STATE_SINGLE_LINE, PR_TRUE),

  // eARIAMultiSelectable
  nsStateMapEntry(&nsAccessibilityAtoms::aria_multiselectable, kBoolType, 0,
                  nsIAccessibleStates::STATE_MULTISELECTABLE | nsIAccessibleStates::STATE_EXTSELECTABLE, 0),

  // eARIAPressed
  nsStateMapEntry(&nsAccessibilityAtoms::aria_pressed, kMixedType,
                  nsIAccessibleStates::STATE_CHECKABLE,
                  nsIAccessibleStates::STATE_PRESSED, 0),

  // eARIAReadonly
  nsStateMapEntry(&nsAccessibilityAtoms::aria_readonly, kBoolType, 0,
                  nsIAccessibleStates::STATE_READONLY, 0),

  // eARIAReadonlyOrEditable
  nsStateMapEntry(&nsAccessibilityAtoms::aria_readonly, kBoolType, 0,
                  nsIAccessibleStates::STATE_READONLY, 0,
                  0, nsIAccessibleStates::EXT_STATE_EDITABLE, PR_TRUE),

  // eARIARequired
  nsStateMapEntry(&nsAccessibilityAtoms::aria_required, kBoolType, 0,
                  nsIAccessibleStates::STATE_REQUIRED, 0),

  // eARIASelectable
  nsStateMapEntry(&nsAccessibilityAtoms::aria_selected, kBoolType,
                  nsIAccessibleStates::STATE_SELECTABLE,
                  nsIAccessibleStates::STATE_SELECTED, 0,
                  0, 0, PR_TRUE)
};

/**
 * Universal states:
 * The following state rules are applied to any accessible element,
 * whether there is an ARIA role or not:
 */
eStateMapEntryID nsARIAMap::gWAIUnivStateMap[] = {
  eARIARequired,
  eARIAInvalid,
  eARIAHasPopup,
  eARIABusy,
  eARIADisabled,
  eARIAExpanded,
  eARIANone
};


/**
 * ARIA attribute map for attribute characteristics
 * 
 * @note ARIA attributes that don't have any flags are not included here
 */
nsAttributeCharacteristics nsARIAMap::gWAIUnivAttrMap[] = {
  {&nsAccessibilityAtoms::aria_activedescendant,  ATTR_BYPASSOBJ                 },
  {&nsAccessibilityAtoms::aria_atomic,                             ATTR_VALTOKEN },
  {&nsAccessibilityAtoms::aria_busy,                               ATTR_VALTOKEN },
  {&nsAccessibilityAtoms::aria_checked,           ATTR_BYPASSOBJ | ATTR_VALTOKEN }, /* exposes checkable  obj attr */
  {&nsAccessibilityAtoms::aria_controls,          ATTR_BYPASSOBJ                 },
  {&nsAccessibilityAtoms::aria_describedby,       ATTR_BYPASSOBJ                 },
  {&nsAccessibilityAtoms::aria_disabled,          ATTR_BYPASSOBJ | ATTR_VALTOKEN },
  {&nsAccessibilityAtoms::aria_dropeffect,                         ATTR_VALTOKEN },
  {&nsAccessibilityAtoms::aria_expanded,          ATTR_BYPASSOBJ | ATTR_VALTOKEN },
  {&nsAccessibilityAtoms::aria_flowto,            ATTR_BYPASSOBJ                 },  
  {&nsAccessibilityAtoms::aria_grabbed,                            ATTR_VALTOKEN },
  {&nsAccessibilityAtoms::aria_haspopup,          ATTR_BYPASSOBJ | ATTR_VALTOKEN },
  {&nsAccessibilityAtoms::aria_invalid,           ATTR_BYPASSOBJ | ATTR_VALTOKEN },
  {&nsAccessibilityAtoms::aria_labelledby,        ATTR_BYPASSOBJ                 },
  {&nsAccessibilityAtoms::aria_live,                               ATTR_VALTOKEN },
  {&nsAccessibilityAtoms::aria_multiline,         ATTR_BYPASSOBJ | ATTR_VALTOKEN },
  {&nsAccessibilityAtoms::aria_multiselectable,   ATTR_BYPASSOBJ | ATTR_VALTOKEN },
  {&nsAccessibilityAtoms::aria_owns,              ATTR_BYPASSOBJ                 },
  {&nsAccessibilityAtoms::aria_pressed,           ATTR_BYPASSOBJ | ATTR_VALTOKEN },
  {&nsAccessibilityAtoms::aria_readonly,          ATTR_BYPASSOBJ | ATTR_VALTOKEN },
  {&nsAccessibilityAtoms::aria_relevant,          ATTR_BYPASSOBJ                 },
  {&nsAccessibilityAtoms::aria_required,          ATTR_BYPASSOBJ | ATTR_VALTOKEN },
  {&nsAccessibilityAtoms::aria_selected,          ATTR_BYPASSOBJ | ATTR_VALTOKEN },
  {&nsAccessibilityAtoms::aria_sort,                               ATTR_VALTOKEN },
  {&nsAccessibilityAtoms::aria_valuenow,          ATTR_BYPASSOBJ                 },
  {&nsAccessibilityAtoms::aria_valuemin,          ATTR_BYPASSOBJ                 },
  {&nsAccessibilityAtoms::aria_valuemax,          ATTR_BYPASSOBJ                 },
  {&nsAccessibilityAtoms::aria_valuetext,         ATTR_BYPASSOBJ                 }
};

PRUint32 nsARIAMap::gWAIUnivAttrMapLength = NS_ARRAY_LENGTH(nsARIAMap::gWAIUnivAttrMap);


////////////////////////////////////////////////////////////////////////////////
// nsStateMapEntry

nsStateMapEntry:: nsStateMapEntry(nsIAtom **aAttrName, eStateValueType aType,
                                  PRUint32 aPermanentState,
                                  PRUint32 aTrueState, PRUint32 aTrueExtraState,
                                  PRUint32 aFalseState, PRUint32 aFalseExtraState,
                                  PRBool aDefinedIfAbsent) :
  attributeName(aAttrName), isToken(PR_TRUE), permanentState(aPermanentState)
{
  value1 = "false";
  state1 = aFalseState;
  extraState1 = aFalseExtraState;

  if (aType == kMixedType) {
    value2 = "mixed";
    state2 = nsIAccessibleStates::STATE_MIXED;
    extraState2 = 0;
  }

  defaultState = aTrueState;
  defaultExtraState = aTrueExtraState;

  definedIfAbsent = aDefinedIfAbsent;
}

nsStateMapEntry::nsStateMapEntry(nsIAtom **aAttrName,
                                 const char *aValue1,
                                 PRUint32 aState1, PRUint32 aExtraState1,
                                 const char *aValue2,
                                 PRUint32 aState2, PRUint32 aExtraState2,
                                 const char *aValue3,
                                 PRUint32 aState3, PRUint32 aExtraState3) :
  attributeName(aAttrName), isToken(PR_FALSE), permanentState(0),
  value1(aValue1), state1(aState1), extraState1(aExtraState1),
  value2(aValue2), state2(aState2), extraState2(aExtraState2),
  value3(aValue3), state3(aState3), extraState3(aExtraState3),
  defaultState(0), defaultExtraState(0), definedIfAbsent(PR_FALSE)
{
}

PRBool
nsStateMapEntry::MapToStates(nsIContent *aContent,
                             PRUint32 *aState, PRUint32 *aExtraState,
                             eStateMapEntryID aStateMapEntryID)
{
  // Return true if we should continue.
  if (aStateMapEntryID == eARIANone)
    return PR_FALSE;

  const nsStateMapEntry& entry = nsARIAMap::gWAIStateMap[aStateMapEntryID];

  if (entry.isToken) {
    // If attribute is considered as defined when it's absent then let's act
    // attribute value is "false" supposedly.
    PRBool hasAttr = aContent->HasAttr(kNameSpaceID_None, *entry.attributeName);
    if (entry.definedIfAbsent && !hasAttr) {
      if (entry.permanentState)
        *aState |= entry.permanentState;
      if (entry.state1)
        *aState |= entry.state1;
      if (aExtraState && entry.extraState1)
        *aExtraState |= entry.extraState1;

      return PR_TRUE;
    }

    // We only have attribute state mappings for NMTOKEN (and boolean) based
    // ARIA attributes. According to spec, a value of "undefined" is to be
    // treated equivalent to "", or the absence of the attribute. We bail out
    // for this case here.
    // Note: If this method happens to be called with a non-token based
    // attribute, for example: aria-label="" or aria-label="undefined", we will
    // bail out and not explore a state mapping, which is safe.
    if (!hasAttr ||
        aContent->AttrValueIs(kNameSpaceID_None, *entry.attributeName,
                              nsAccessibilityAtoms::_empty, eCaseMatters) ||
        aContent->AttrValueIs(kNameSpaceID_None, *entry.attributeName,
                              nsAccessibilityAtoms::_undefined, eCaseMatters)) {

      if (entry.permanentState)
        *aState &= ~entry.permanentState;
      return PR_TRUE;
    }

    if (entry.permanentState)
      *aState |= entry.permanentState;
  }

  nsAutoString attrValue;
  if (!aContent->GetAttr(kNameSpaceID_None, *entry.attributeName, attrValue))
    return PR_TRUE;

  // Apply states for matched value. If no values was matched then apply default
  // states.
  PRBool applyDefaultStates = PR_TRUE;
  if (entry.value1) {
    if (attrValue.EqualsASCII(entry.value1)) {
      applyDefaultStates = PR_FALSE;

      if (entry.state1)
        *aState |= entry.state1;

      if (aExtraState && entry.extraState1)
        *aExtraState |= entry.extraState1;

    } else if (entry.value2) {
      if (attrValue.EqualsASCII(entry.value2)) {
        applyDefaultStates = PR_FALSE;

        if (entry.state2)
          *aState |= entry.state2;

        if (aExtraState && entry.extraState2)
          *aExtraState |= entry.extraState2;

      } else if (entry.value3) {
        if (attrValue.EqualsASCII(entry.value3)) {
          applyDefaultStates = PR_FALSE;

          if (entry.state3)
            *aState |= entry.state3;

          if (aExtraState && entry.extraState3)
            *aExtraState |= entry.extraState3;
        }
      }
    }
  }

  if (applyDefaultStates) {
    if (entry.defaultState)
      *aState |= entry.defaultState;
    if (entry.defaultExtraState && aExtraState)
      *aExtraState |= entry.defaultExtraState;
  }

  return PR_TRUE;
}
