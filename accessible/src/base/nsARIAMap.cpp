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
 *  XXX Should we store attribute names in this table as atoms instead of strings?
 *  Definition of nsRoleMapEntry and nsStateMapEntry contains comments explaining this table.
 *
 *  When no nsIAccessibleRole neum mapping exists for an ARIA role, the
 *  role will be exposed via the object attribute "xml-roles".
 *  In addition, in MSAA, the unmapped role will also be exposed as a BSTR string role.
 *
 *  There are no nsIAccessibleRole enums for the following landmark roles:
 *    banner, contentinfo, main, navigation, note, search, secondary, seealso, breadcrumbs
 */ 

static const nsStateMapEntry kEndEntry = {0, 0, 0};  // To fill in array of state mappings

nsRoleMapEntry nsARIAMap::gWAIRoleMap[] = 
{
  {"alert", nsIAccessibleRole::ROLE_ALERT, eNameLabelOrTitle, eNoValue, kNoReqStates, kEndEntry},
  {"alertdialog", nsIAccessibleRole::ROLE_ALERT, eNameOkFromChildren, eNoValue, kNoReqStates, kEndEntry},
  {"application", nsIAccessibleRole::ROLE_APPLICATION, eNameLabelOrTitle, eNoValue, kNoReqStates, kEndEntry},
  {"button", nsIAccessibleRole::ROLE_PUSHBUTTON, eNameOkFromChildren, eNoValue, kNoReqStates,
            {"disabled", kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {"pressed", kBoolState, nsIAccessibleStates::STATE_PRESSED},
            {"pressed", "mixed", nsIAccessibleStates::STATE_MIXED}, kEndEntry},
  {"checkbox", nsIAccessibleRole::ROLE_CHECKBUTTON, eNameOkFromChildren, eNoValue, nsIAccessibleStates::STATE_CHECKABLE,
            {"disabled", kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {"checked", kBoolState, nsIAccessibleStates::STATE_CHECKED},
            {"checked", "mixed", nsIAccessibleStates::STATE_MIXED},
            {"readonly", kBoolState, nsIAccessibleStates::STATE_READONLY}, kEndEntry},
  {"columnheader", nsIAccessibleRole::ROLE_COLUMNHEADER, eNameOkFromChildren, eNoValue, kNoReqStates,
            {"disabled", kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {"selected", kBoolState, nsIAccessibleStates::STATE_SELECTED | nsIAccessibleStates::STATE_SELECTABLE},
            {"selected", "false", nsIAccessibleStates::STATE_SELECTABLE},
            {"readonly", kBoolState, nsIAccessibleStates::STATE_READONLY}, kEndEntry},
  {"combobox", nsIAccessibleRole::ROLE_COMBOBOX, eNameLabelOrTitle, eHasValueMinMax, nsIAccessibleStates::STATE_COLLAPSED,
            {"disabled", kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {"readonly", kBoolState, nsIAccessibleStates::STATE_READONLY},
            {"expanded", kBoolState, nsIAccessibleStates::STATE_EXPANDED}, kEndEntry},
  {"description", nsIAccessibleRole::ROLE_TEXT_CONTAINER, eNameOkFromChildren, eNoValue, kNoReqStates, kEndEntry},
  {"dialog", nsIAccessibleRole::ROLE_DIALOG, eNameLabelOrTitle, eNoValue, kNoReqStates, kEndEntry},
  {"document", nsIAccessibleRole::ROLE_DOCUMENT, eNameLabelOrTitle, eNoValue, kNoReqStates, kEndEntry},
  {"grid", nsIAccessibleRole::ROLE_TABLE, eNameLabelOrTitle, eNoValue, nsIAccessibleStates::STATE_FOCUSABLE,
            {"disabled", kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {"multiselectable", kBoolState, nsIAccessibleStates::STATE_MULTISELECTABLE | nsIAccessibleStates::STATE_EXTSELECTABLE},
            {"readonly", kBoolState, nsIAccessibleStates::STATE_READONLY}, kEndEntry},
  {"gridcell", nsIAccessibleRole::ROLE_CELL, eNameOkFromChildren, eNoValue, kNoReqStates,
            {"disabled", kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {"expanded", kBoolState, nsIAccessibleStates::STATE_EXPANDED},
            {"expanded", "false", nsIAccessibleStates::STATE_COLLAPSED},
            {"selected", kBoolState, nsIAccessibleStates::STATE_SELECTED | nsIAccessibleStates::STATE_SELECTABLE},
            {"selected", "false", nsIAccessibleStates::STATE_SELECTABLE},
            {"readonly", kBoolState, nsIAccessibleStates::STATE_READONLY}, kEndEntry},
  {"group", nsIAccessibleRole::ROLE_GROUPING, eNameLabelOrTitle, eNoValue, kNoReqStates, kEndEntry},
  {"heading", nsIAccessibleRole::ROLE_HEADING, eNameLabelOrTitle, eNoValue, kNoReqStates, kEndEntry},
  {"img", nsIAccessibleRole::ROLE_GRAPHIC, eNameLabelOrTitle, eNoValue, kNoReqStates, kEndEntry},
  {"label", nsIAccessibleRole::ROLE_LABEL, eNameOkFromChildren, eNoValue, kNoReqStates, kEndEntry},
  {"link", nsIAccessibleRole::ROLE_LINK, eNameOkFromChildren, eNoValue, nsIAccessibleStates::STATE_LINKED,
            {"disabled", kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE}, kEndEntry},
  {"list", nsIAccessibleRole::ROLE_LIST, eNameLabelOrTitle, eNoValue, kNoReqStates,
            {"readonly", kBoolState, nsIAccessibleStates::STATE_READONLY},
            {"multiselectable", kBoolState, nsIAccessibleStates::STATE_MULTISELECTABLE | nsIAccessibleStates::STATE_EXTSELECTABLE}, kEndEntry},
  {"listbox", nsIAccessibleRole::ROLE_LIST, eNameLabelOrTitle, eNoValue, kNoReqStates,
            {"disabled", kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {"readonly", kBoolState, nsIAccessibleStates::STATE_READONLY},
            {"multiselectable", kBoolState, nsIAccessibleStates::STATE_MULTISELECTABLE | nsIAccessibleStates::STATE_EXTSELECTABLE}, kEndEntry},
  {"listitem", nsIAccessibleRole::ROLE_LISTITEM, eNameOkFromChildren, eNoValue, kNoReqStates,
            {"selected", kBoolState, nsIAccessibleStates::STATE_SELECTED | nsIAccessibleStates::STATE_SELECTABLE},
            {"selected", "false", nsIAccessibleStates::STATE_SELECTABLE},
            {"checked", kBoolState, nsIAccessibleStates::STATE_CHECKED | nsIAccessibleStates::STATE_CHECKABLE},
            {"checked", "mixed", nsIAccessibleStates::STATE_MIXED | nsIAccessibleStates::STATE_CHECKABLE},
            {"checked", "false", nsIAccessibleStates::STATE_CHECKABLE}, kEndEntry},
  {"menu", nsIAccessibleRole::ROLE_MENUPOPUP, eNameLabelOrTitle, eNoValue, kNoReqStates,
            {"disabled", kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE}, kEndEntry},
  {"menubar", nsIAccessibleRole::ROLE_MENUBAR, eNameLabelOrTitle, eNoValue, kNoReqStates,
            {"disabled", kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE}, kEndEntry},
  {"menuitem", nsIAccessibleRole::ROLE_MENUITEM, eNameOkFromChildren, eNoValue, kNoReqStates,
            {"disabled", kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {"checked", kBoolState, nsIAccessibleStates::STATE_CHECKED | nsIAccessibleStates::STATE_CHECKABLE},
            {"checked", "mixed", nsIAccessibleStates::STATE_MIXED | nsIAccessibleStates::STATE_CHECKABLE},
            {"checked", "false", nsIAccessibleStates::STATE_CHECKABLE}, kEndEntry},
  {"menuitemcheckbox", nsIAccessibleRole::ROLE_MENUITEM, eNameOkFromChildren, eNoValue, nsIAccessibleStates::STATE_CHECKABLE,
            {"disabled", kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {"checked", kBoolState, nsIAccessibleStates::STATE_CHECKED },
            {"checked", "mixed", nsIAccessibleStates::STATE_MIXED}, kEndEntry},
  {"menuitemradio", nsIAccessibleRole::ROLE_MENUITEM, eNameOkFromChildren, eNoValue, nsIAccessibleStates::STATE_CHECKABLE,
            {"disabled", kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {"checked", kBoolState, nsIAccessibleStates::STATE_CHECKED }, kEndEntry},
  {"option", nsIAccessibleRole::ROLE_LISTITEM, eNameOkFromChildren, eNoValue, kNoReqStates,
            {"disabled", kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {"selected", kBoolState, nsIAccessibleStates::STATE_SELECTED | nsIAccessibleStates::STATE_SELECTABLE},
            {"selected", "false", nsIAccessibleStates::STATE_SELECTABLE},
            {"checked", kBoolState, nsIAccessibleStates::STATE_CHECKED | nsIAccessibleStates::STATE_CHECKABLE},
            {"checked", "mixed", nsIAccessibleStates::STATE_MIXED | nsIAccessibleStates::STATE_CHECKABLE},
            {"checked", "false", nsIAccessibleStates::STATE_CHECKABLE}, kEndEntry},
  {"progressbar", nsIAccessibleRole::ROLE_PROGRESSBAR, eNameLabelOrTitle, eHasValueMinMax, nsIAccessibleStates::STATE_READONLY,
            {"disabled", kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE}, kEndEntry},
  {"radio", nsIAccessibleRole::ROLE_RADIOBUTTON, eNameOkFromChildren, eNoValue, kNoReqStates,
            {"disabled", kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {"checked", kBoolState, nsIAccessibleStates::STATE_CHECKED}, kEndEntry},
  {"radiogroup", nsIAccessibleRole::ROLE_GROUPING, eNameLabelOrTitle, eNoValue, kNoReqStates,
            {"disabled", kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE}, kEndEntry},
  {"region", nsIAccessibleRole::ROLE_PANE, eNameLabelOrTitle, eNoValue, kNoReqStates, kEndEntry},
  {"row", nsIAccessibleRole::ROLE_ROW, eNameOkFromChildren, eNoValue, kNoReqStates,
            {"disabled", kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {"selected", kBoolState, nsIAccessibleStates::STATE_SELECTED | nsIAccessibleStates::STATE_SELECTABLE},
            {"selected", "false", nsIAccessibleStates::STATE_SELECTABLE},
            {"expanded", kBoolState, nsIAccessibleStates::STATE_EXPANDED},
            {"expanded", "false", nsIAccessibleStates::STATE_COLLAPSED}, kEndEntry},
  {"rowheader", nsIAccessibleRole::ROLE_ROWHEADER, eNameOkFromChildren, eNoValue, kNoReqStates,
            {"disabled", kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {"selected", kBoolState, nsIAccessibleStates::STATE_SELECTED | nsIAccessibleStates::STATE_SELECTABLE},
            {"selected", "false", nsIAccessibleStates::STATE_SELECTABLE},
            {"readonly", kBoolState, nsIAccessibleStates::STATE_READONLY}, kEndEntry},
  {"section", nsIAccessibleRole::ROLE_SECTION, eNameLabelOrTitle, eNoValue, kNoReqStates, kEndEntry},
  {"separator", nsIAccessibleRole::ROLE_SEPARATOR, eNameLabelOrTitle, eNoValue, kNoReqStates, kEndEntry},
  {"slider", nsIAccessibleRole::ROLE_SLIDER, eNameLabelOrTitle, eHasValueMinMax, kNoReqStates,
            {"disabled", kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {"readonly", kBoolState, nsIAccessibleStates::STATE_READONLY}, kEndEntry},
  {"spinbutton", nsIAccessibleRole::ROLE_SPINBUTTON, eNameLabelOrTitle, eHasValueMinMax, kNoReqStates,
            {"disabled", kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {"readonly", kBoolState, nsIAccessibleStates::STATE_READONLY}, kEndEntry},
  {"status", nsIAccessibleRole::ROLE_STATUSBAR, eNameLabelOrTitle, eNoValue, kNoReqStates, kEndEntry},
  {"tab", nsIAccessibleRole::ROLE_PAGETAB, eNameOkFromChildren, eNoValue, kNoReqStates,
            {"disabled", kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE}, kEndEntry},
  {"tablist", nsIAccessibleRole::ROLE_PAGETABLIST, eNameLabelOrTitle, eNoValue, kNoReqStates, kEndEntry},
  {"tabpanel", nsIAccessibleRole::ROLE_PROPERTYPAGE, eNameLabelOrTitle, eNoValue, kNoReqStates, kEndEntry},
  {"textbox", nsIAccessibleRole::ROLE_ENTRY, eNameLabelOrTitle, eNoValue, kNoReqStates,
            // Manually map EXT_STATE_SINGLE_LINE and EXT_STATE_MULTI_LINE FROM aaa:multiline
            {"secret", kBoolState, nsIAccessibleStates::STATE_PROTECTED},
            {"disabled", kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {"readonly", kBoolState, nsIAccessibleStates::STATE_READONLY}, kEndEntry},
  {"toolbar", nsIAccessibleRole::ROLE_TOOLBAR, eNameLabelOrTitle, eNoValue, kNoReqStates,
            {"disabled", kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE}, kEndEntry},
  {"tooltip", nsIAccessibleRole::ROLE_TOOLTIP, eNameOkFromChildren, eNoValue, kNoReqStates, kEndEntry},
  {"tree", nsIAccessibleRole::ROLE_OUTLINE, eNameLabelOrTitle, eNoValue, kNoReqStates,
            {"disabled", kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {"readonly", kBoolState, nsIAccessibleStates::STATE_READONLY},
            {"multiselectable", kBoolState, nsIAccessibleStates::STATE_MULTISELECTABLE | nsIAccessibleStates::STATE_EXTSELECTABLE}, kEndEntry},
  {"treegrid", nsIAccessibleRole::ROLE_TREE_TABLE, eNameLabelOrTitle, eNoValue, kNoReqStates,
            {"disabled", kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {"readonly", kBoolState, nsIAccessibleStates::STATE_READONLY},
            {"multiselectable", kBoolState, nsIAccessibleStates::STATE_MULTISELECTABLE | nsIAccessibleStates::STATE_EXTSELECTABLE}, kEndEntry},
  {"treeitem", nsIAccessibleRole::ROLE_OUTLINEITEM, eNameOkFromChildren, eNoValue, kNoReqStates,
            {"disabled", kBoolState, nsIAccessibleStates::STATE_UNAVAILABLE},
            {"selected", kBoolState, nsIAccessibleStates::STATE_SELECTED | nsIAccessibleStates::STATE_SELECTABLE},
            {"selected", "false", nsIAccessibleStates::STATE_SELECTABLE},
            {"expanded", kBoolState, nsIAccessibleStates::STATE_EXPANDED},
            {"expanded", "false", nsIAccessibleStates::STATE_COLLAPSED},
            {"checked", kBoolState, nsIAccessibleStates::STATE_CHECKED | nsIAccessibleStates::STATE_CHECKABLE},
            {"checked", "mixed", nsIAccessibleStates::STATE_MIXED | nsIAccessibleStates::STATE_CHECKABLE},
            {"checked", "false", nsIAccessibleStates::STATE_CHECKABLE},},
  {nsnull, nsIAccessibleRole::ROLE_NOTHING, eNameLabelOrTitle, eNoValue, kNoReqStates, kEndEntry} // Last item
};

/**
 * Universal states:
 * The following state rules are applied to any accessible element,
 * whether there is an ARIA role or not:
 */
nsStateMapEntry nsARIAMap::gWAIUnivStateMap[] = {
  {"required", kBoolState, nsIAccessibleStates::STATE_REQUIRED},
  {"invalid",  kBoolState, nsIAccessibleStates::STATE_INVALID},
  {"haspopup", kBoolState, nsIAccessibleStates::STATE_HASPOPUP},
  {"busy",     "true",     nsIAccessibleStates::STATE_BUSY},
  {"busy",     "error",    nsIAccessibleStates::STATE_INVALID},
  kEndEntry
};

