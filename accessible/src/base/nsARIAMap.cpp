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

static const nsStateMapEntry kEndEntry = {nsnull, 0, 0};  // To fill in array of state mappings

nsRoleMapEntry nsARIAMap::gWAIRoleMap[] = 
{
  {"alert", nsIAccessibleRole::ROLE_ALERT, eNameLabelOrTitle, eNoValue, kNoReqStates, kEndEntry},
  {"alertdialog", nsIAccessibleRole::ROLE_ALERT, eNameOkFromChildren, eNoValue, kNoReqStates, kEndEntry},
  {"application", nsIAccessibleRole::ROLE_APPLICATION, eNameLabelOrTitle, eNoValue, kNoReqStates, kEndEntry},
  {"button", nsIAccessibleRole::ROLE_PUSHBUTTON, eNameOkFromChildren, eNoValue, kNoReqStates,
            {&nsAccessibilityAtoms::aria_disabled, kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {&nsAccessibilityAtoms::aria_pressed, kBoolState, nsIAccessibleStates::STATE_PRESSED},
            {&nsAccessibilityAtoms::aria_pressed, "mixed", nsIAccessibleStates::STATE_MIXED}, kEndEntry},
  {"checkbox", nsIAccessibleRole::ROLE_CHECKBUTTON, eNameOkFromChildren, eNoValue, nsIAccessibleStates::STATE_CHECKABLE,
            {&nsAccessibilityAtoms::aria_disabled, kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {&nsAccessibilityAtoms::aria_checked, kBoolState, nsIAccessibleStates::STATE_CHECKED},
            {&nsAccessibilityAtoms::aria_checked, "mixed", nsIAccessibleStates::STATE_MIXED},
            {&nsAccessibilityAtoms::aria_readonly, kBoolState, nsIAccessibleStates::STATE_READONLY}, kEndEntry},
  {"columnheader", nsIAccessibleRole::ROLE_COLUMNHEADER, eNameOkFromChildren, eNoValue, kNoReqStates,
            {&nsAccessibilityAtoms::aria_disabled, kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {&nsAccessibilityAtoms::aria_selected, kBoolState, nsIAccessibleStates::STATE_SELECTED | nsIAccessibleStates::STATE_SELECTABLE},
            {&nsAccessibilityAtoms::aria_selected, "false", nsIAccessibleStates::STATE_SELECTABLE},
            {&nsAccessibilityAtoms::aria_readonly, kBoolState, nsIAccessibleStates::STATE_READONLY}, kEndEntry},
  {"combobox", nsIAccessibleRole::ROLE_COMBOBOX, eNameLabelOrTitle, eHasValueMinMax,
               nsIAccessibleStates::STATE_COLLAPSED | nsIAccessibleStates::STATE_HASPOPUP,
            // Manually map EXT_STATE_SUPPORTS_AUTOCOMPLETION aria-autocomplete
            {&nsAccessibilityAtoms::aria_disabled, kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {&nsAccessibilityAtoms::aria_readonly, kBoolState, nsIAccessibleStates::STATE_READONLY},
            {&nsAccessibilityAtoms::aria_expanded, kBoolState, nsIAccessibleStates::STATE_EXPANDED}, kEndEntry},
  {"description", nsIAccessibleRole::ROLE_TEXT_CONTAINER, eNameOkFromChildren, eNoValue, kNoReqStates, kEndEntry},
  {"dialog", nsIAccessibleRole::ROLE_DIALOG, eNameLabelOrTitle, eNoValue, kNoReqStates, kEndEntry},
  {"document", nsIAccessibleRole::ROLE_DOCUMENT, eNameLabelOrTitle, eNoValue, kNoReqStates, kEndEntry},
  {"grid", nsIAccessibleRole::ROLE_TABLE, eNameLabelOrTitle, eNoValue, nsIAccessibleStates::STATE_FOCUSABLE,
            {&nsAccessibilityAtoms::aria_disabled, kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {&nsAccessibilityAtoms::aria_multiselectable, kBoolState, nsIAccessibleStates::STATE_MULTISELECTABLE | nsIAccessibleStates::STATE_EXTSELECTABLE},
            {&nsAccessibilityAtoms::aria_readonly, kBoolState, nsIAccessibleStates::STATE_READONLY}, kEndEntry},
  {"gridcell", nsIAccessibleRole::ROLE_CELL, eNameOkFromChildren, eNoValue, kNoReqStates,
            {&nsAccessibilityAtoms::aria_disabled, kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {&nsAccessibilityAtoms::aria_expanded, kBoolState, nsIAccessibleStates::STATE_EXPANDED},
            {&nsAccessibilityAtoms::aria_expanded, "false", nsIAccessibleStates::STATE_COLLAPSED},
            {&nsAccessibilityAtoms::aria_selected, kBoolState, nsIAccessibleStates::STATE_SELECTED | nsIAccessibleStates::STATE_SELECTABLE},
            {&nsAccessibilityAtoms::aria_selected, "false", nsIAccessibleStates::STATE_SELECTABLE},
            {&nsAccessibilityAtoms::aria_readonly, kBoolState, nsIAccessibleStates::STATE_READONLY}, kEndEntry},
  {"group", nsIAccessibleRole::ROLE_GROUPING, eNameLabelOrTitle, eNoValue, kNoReqStates, kEndEntry},
  {"heading", nsIAccessibleRole::ROLE_HEADING, eNameLabelOrTitle, eNoValue, kNoReqStates, kEndEntry},
  {"img", nsIAccessibleRole::ROLE_GRAPHIC, eNameLabelOrTitle, eNoValue, kNoReqStates, kEndEntry},
  {"label", nsIAccessibleRole::ROLE_LABEL, eNameOkFromChildren, eNoValue, kNoReqStates, kEndEntry},
  {"link", nsIAccessibleRole::ROLE_LINK, eNameOkFromChildren, eNoValue, nsIAccessibleStates::STATE_LINKED,
            {&nsAccessibilityAtoms::aria_disabled, kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE}, kEndEntry},
  {"list", nsIAccessibleRole::ROLE_LIST, eNameLabelOrTitle, eNoValue, kNoReqStates,
            {&nsAccessibilityAtoms::aria_readonly, kBoolState, nsIAccessibleStates::STATE_READONLY},
            {&nsAccessibilityAtoms::aria_multiselectable, kBoolState, nsIAccessibleStates::STATE_MULTISELECTABLE | nsIAccessibleStates::STATE_EXTSELECTABLE}, kEndEntry},
  {"listbox", nsIAccessibleRole::ROLE_LISTBOX, eNameLabelOrTitle, eNoValue, kNoReqStates,
            {&nsAccessibilityAtoms::aria_disabled, kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {&nsAccessibilityAtoms::aria_readonly, kBoolState, nsIAccessibleStates::STATE_READONLY},
            {&nsAccessibilityAtoms::aria_multiselectable, kBoolState, nsIAccessibleStates::STATE_MULTISELECTABLE | nsIAccessibleStates::STATE_EXTSELECTABLE}, kEndEntry},
  {"listitem", nsIAccessibleRole::ROLE_LISTITEM, eNameOkFromChildren, eNoValue, kNoReqStates,
            {&nsAccessibilityAtoms::aria_selected, kBoolState, nsIAccessibleStates::STATE_SELECTED | nsIAccessibleStates::STATE_SELECTABLE},
            {&nsAccessibilityAtoms::aria_selected, "false", nsIAccessibleStates::STATE_SELECTABLE},
            {&nsAccessibilityAtoms::aria_checked, kBoolState, nsIAccessibleStates::STATE_CHECKED | nsIAccessibleStates::STATE_CHECKABLE},
            {&nsAccessibilityAtoms::aria_checked, "mixed", nsIAccessibleStates::STATE_MIXED | nsIAccessibleStates::STATE_CHECKABLE},
            {&nsAccessibilityAtoms::aria_checked, "false", nsIAccessibleStates::STATE_CHECKABLE}, kEndEntry},
  {"menu", nsIAccessibleRole::ROLE_MENUPOPUP, eNameLabelOrTitle, eNoValue, kNoReqStates,
            {&nsAccessibilityAtoms::aria_disabled, kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE}, kEndEntry},
  {"menubar", nsIAccessibleRole::ROLE_MENUBAR, eNameLabelOrTitle, eNoValue, kNoReqStates,
            {&nsAccessibilityAtoms::aria_disabled, kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE}, kEndEntry},
  {"menuitem", nsIAccessibleRole::ROLE_MENUITEM, eNameOkFromChildren, eNoValue, kNoReqStates,
            {&nsAccessibilityAtoms::aria_disabled, kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {&nsAccessibilityAtoms::aria_checked, kBoolState, nsIAccessibleStates::STATE_CHECKED | nsIAccessibleStates::STATE_CHECKABLE},
            {&nsAccessibilityAtoms::aria_checked, "mixed", nsIAccessibleStates::STATE_MIXED | nsIAccessibleStates::STATE_CHECKABLE},
            {&nsAccessibilityAtoms::aria_checked, "false", nsIAccessibleStates::STATE_CHECKABLE}, kEndEntry},
  {"menuitemcheckbox", nsIAccessibleRole::ROLE_CHECK_MENU_ITEM, eNameOkFromChildren, eNoValue, nsIAccessibleStates::STATE_CHECKABLE,
            {&nsAccessibilityAtoms::aria_disabled, kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {&nsAccessibilityAtoms::aria_checked, kBoolState, nsIAccessibleStates::STATE_CHECKED },
            {&nsAccessibilityAtoms::aria_checked, "mixed", nsIAccessibleStates::STATE_MIXED}, kEndEntry},
  {"menuitemradio", nsIAccessibleRole::ROLE_RADIO_MENU_ITEM, eNameOkFromChildren, eNoValue, nsIAccessibleStates::STATE_CHECKABLE,
            {&nsAccessibilityAtoms::aria_disabled, kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {&nsAccessibilityAtoms::aria_checked, kBoolState, nsIAccessibleStates::STATE_CHECKED }, kEndEntry},
  {"option", nsIAccessibleRole::ROLE_OPTION, eNameOkFromChildren, eNoValue, kNoReqStates,
            {&nsAccessibilityAtoms::aria_disabled, kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {&nsAccessibilityAtoms::aria_selected, kBoolState, nsIAccessibleStates::STATE_SELECTED | nsIAccessibleStates::STATE_SELECTABLE},
            {&nsAccessibilityAtoms::aria_selected, "false", nsIAccessibleStates::STATE_SELECTABLE},
            {&nsAccessibilityAtoms::aria_checked, kBoolState, nsIAccessibleStates::STATE_CHECKED | nsIAccessibleStates::STATE_CHECKABLE},
            {&nsAccessibilityAtoms::aria_checked, "mixed", nsIAccessibleStates::STATE_MIXED | nsIAccessibleStates::STATE_CHECKABLE},
            {&nsAccessibilityAtoms::aria_checked, "false", nsIAccessibleStates::STATE_CHECKABLE}, kEndEntry},
  {"presentation", nsIAccessibleRole::ROLE_NOTHING, eNameLabelOrTitle, eNoValue, kNoReqStates, kEndEntry},
  {"progressbar", nsIAccessibleRole::ROLE_PROGRESSBAR, eNameLabelOrTitle, eHasValueMinMax, nsIAccessibleStates::STATE_READONLY,
            {&nsAccessibilityAtoms::aria_disabled, kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE}, kEndEntry},
  {"radio", nsIAccessibleRole::ROLE_RADIOBUTTON, eNameOkFromChildren, eNoValue, kNoReqStates,
            {&nsAccessibilityAtoms::aria_disabled, kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {&nsAccessibilityAtoms::aria_checked, kBoolState, nsIAccessibleStates::STATE_CHECKED}, kEndEntry},
  {"radiogroup", nsIAccessibleRole::ROLE_GROUPING, eNameLabelOrTitle, eNoValue, kNoReqStates,
            {&nsAccessibilityAtoms::aria_disabled, kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE}, kEndEntry},
  {"region", nsIAccessibleRole::ROLE_PANE, eNameLabelOrTitle, eNoValue, kNoReqStates, kEndEntry},
  {"row", nsIAccessibleRole::ROLE_ROW, eNameOkFromChildren, eNoValue, kNoReqStates,
            {&nsAccessibilityAtoms::aria_disabled, kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {&nsAccessibilityAtoms::aria_selected, kBoolState, nsIAccessibleStates::STATE_SELECTED | nsIAccessibleStates::STATE_SELECTABLE},
            {&nsAccessibilityAtoms::aria_selected, "false", nsIAccessibleStates::STATE_SELECTABLE},
            {&nsAccessibilityAtoms::aria_expanded, kBoolState, nsIAccessibleStates::STATE_EXPANDED},
            {&nsAccessibilityAtoms::aria_expanded, "false", nsIAccessibleStates::STATE_COLLAPSED}, kEndEntry},
  {"rowheader", nsIAccessibleRole::ROLE_ROWHEADER, eNameOkFromChildren, eNoValue, kNoReqStates,
            {&nsAccessibilityAtoms::aria_disabled, kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {&nsAccessibilityAtoms::aria_selected, kBoolState, nsIAccessibleStates::STATE_SELECTED | nsIAccessibleStates::STATE_SELECTABLE},
            {&nsAccessibilityAtoms::aria_selected, "false", nsIAccessibleStates::STATE_SELECTABLE},
            {&nsAccessibilityAtoms::aria_readonly, kBoolState, nsIAccessibleStates::STATE_READONLY}, kEndEntry},
  {"section", nsIAccessibleRole::ROLE_SECTION, eNameLabelOrTitle, eNoValue, kNoReqStates, kEndEntry},
  {"separator", nsIAccessibleRole::ROLE_SEPARATOR, eNameLabelOrTitle, eNoValue, kNoReqStates, kEndEntry},
  {"slider", nsIAccessibleRole::ROLE_SLIDER, eNameLabelOrTitle, eHasValueMinMax, kNoReqStates,
            {&nsAccessibilityAtoms::aria_disabled, kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {&nsAccessibilityAtoms::aria_readonly, kBoolState, nsIAccessibleStates::STATE_READONLY}, kEndEntry},
  {"spinbutton", nsIAccessibleRole::ROLE_SPINBUTTON, eNameLabelOrTitle, eHasValueMinMax, kNoReqStates,
            {&nsAccessibilityAtoms::aria_disabled, kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {&nsAccessibilityAtoms::aria_readonly, kBoolState, nsIAccessibleStates::STATE_READONLY}, kEndEntry},
  {"status", nsIAccessibleRole::ROLE_STATUSBAR, eNameLabelOrTitle, eNoValue, kNoReqStates, kEndEntry},
  {"tab", nsIAccessibleRole::ROLE_PAGETAB, eNameOkFromChildren, eNoValue, kNoReqStates,
            {&nsAccessibilityAtoms::aria_disabled, kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE}, kEndEntry},
  {"tablist", nsIAccessibleRole::ROLE_PAGETABLIST, eNameLabelOrTitle, eNoValue, kNoReqStates, kEndEntry},
  {"tabpanel", nsIAccessibleRole::ROLE_PROPERTYPAGE, eNameLabelOrTitle, eNoValue, kNoReqStates, kEndEntry},
  {"textbox", nsIAccessibleRole::ROLE_ENTRY, eNameLabelOrTitle, eNoValue, kNoReqStates,
            // Manually map EXT_STATE_SINGLE_LINE and EXT_STATE_MULTI_LINE FROM aria-multiline
            // Manually map EXT_STATE_SUPPORTS_AUTOCOMPLETION aria-autocomplete
            {&nsAccessibilityAtoms::aria_autocomplete, "list", nsIAccessibleStates::STATE_HASPOPUP},
            {&nsAccessibilityAtoms::aria_autocomplete, "both", nsIAccessibleStates::STATE_HASPOPUP},
            {&nsAccessibilityAtoms::aria_secret, kBoolState, nsIAccessibleStates::STATE_PROTECTED},
            {&nsAccessibilityAtoms::aria_disabled, kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {&nsAccessibilityAtoms::aria_readonly, kBoolState, nsIAccessibleStates::STATE_READONLY}, kEndEntry},
  {"toolbar", nsIAccessibleRole::ROLE_TOOLBAR, eNameLabelOrTitle, eNoValue, kNoReqStates,
            {&nsAccessibilityAtoms::aria_disabled, kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE}, kEndEntry},
  {"tooltip", nsIAccessibleRole::ROLE_TOOLTIP, eNameOkFromChildren, eNoValue, kNoReqStates, kEndEntry},
  {"tree", nsIAccessibleRole::ROLE_OUTLINE, eNameLabelOrTitle, eNoValue, kNoReqStates,
            {&nsAccessibilityAtoms::aria_disabled, kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {&nsAccessibilityAtoms::aria_readonly, kBoolState, nsIAccessibleStates::STATE_READONLY},
            {&nsAccessibilityAtoms::aria_multiselectable, kBoolState, nsIAccessibleStates::STATE_MULTISELECTABLE | nsIAccessibleStates::STATE_EXTSELECTABLE}, kEndEntry},
  {"treegrid", nsIAccessibleRole::ROLE_TREE_TABLE, eNameLabelOrTitle, eNoValue, kNoReqStates,
            {&nsAccessibilityAtoms::aria_disabled, kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {&nsAccessibilityAtoms::aria_readonly, kBoolState, nsIAccessibleStates::STATE_READONLY},
            {&nsAccessibilityAtoms::aria_multiselectable, kBoolState, nsIAccessibleStates::STATE_MULTISELECTABLE | nsIAccessibleStates::STATE_EXTSELECTABLE}, kEndEntry},
  {"treeitem", nsIAccessibleRole::ROLE_OUTLINEITEM, eNameOkFromChildren, eNoValue, kNoReqStates,
            {&nsAccessibilityAtoms::aria_disabled, kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {&nsAccessibilityAtoms::aria_selected, kBoolState, nsIAccessibleStates::STATE_SELECTED | nsIAccessibleStates::STATE_SELECTABLE},
            {&nsAccessibilityAtoms::aria_selected, "false", nsIAccessibleStates::STATE_SELECTABLE},
            {&nsAccessibilityAtoms::aria_expanded, kBoolState, nsIAccessibleStates::STATE_EXPANDED},
            {&nsAccessibilityAtoms::aria_expanded, "false", nsIAccessibleStates::STATE_COLLAPSED},
            {&nsAccessibilityAtoms::aria_checked, kBoolState, nsIAccessibleStates::STATE_CHECKED | nsIAccessibleStates::STATE_CHECKABLE},
            {&nsAccessibilityAtoms::aria_checked, "mixed", nsIAccessibleStates::STATE_MIXED | nsIAccessibleStates::STATE_CHECKABLE},
            {&nsAccessibilityAtoms::aria_checked, "false", nsIAccessibleStates::STATE_CHECKABLE},},
};

PRUint32 nsARIAMap::gWAIRoleMapLength = NS_ARRAY_LENGTH(nsARIAMap::gWAIRoleMap);

nsRoleMapEntry nsARIAMap::gLandmarkRoleMap = {
  "",
  nsIAccessibleRole::ROLE_NOTHING,
  eNameLabelOrTitle,
  eNoValue,
  kNoReqStates,
  kEndEntry
};

/**
 * Universal states:
 * The following state rules are applied to any accessible element,
 * whether there is an ARIA role or not:
 */
nsStateMapEntry nsARIAMap::gWAIUnivStateMap[] = {
  {&nsAccessibilityAtoms::aria_required, kBoolState, nsIAccessibleStates::STATE_REQUIRED},
  {&nsAccessibilityAtoms::aria_invalid,  kBoolState, nsIAccessibleStates::STATE_INVALID},
  {&nsAccessibilityAtoms::aria_haspopup, kBoolState, nsIAccessibleStates::STATE_HASPOPUP},
  {&nsAccessibilityAtoms::aria_busy,     "true",     nsIAccessibleStates::STATE_BUSY},
  {&nsAccessibilityAtoms::aria_busy,     "error",    nsIAccessibleStates::STATE_INVALID},
  kEndEntry
};

