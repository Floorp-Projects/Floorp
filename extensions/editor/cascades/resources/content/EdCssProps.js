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
 * The Original Code is CaScadeS, a stylesheet editor for Composer.
 *
 * The Initial Developer of the Original Code is
 * Daniel Glazman.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original author: Daniel Glazman <daniel@glazman.org>
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

const SHEET        = 1;
const STYLE_RULE   = 2;
const IMPORT_RULE  = 3;
const MEDIA_RULE   = 4;
const CHARSET_RULE = 5;
const PAGE_RULE    = 6;
const OWNER_NODE   = 7;

const COMPATIBILITY_TAB = 1;
const GENERAL_TAB       = 2;
const TEXT_TAB          = 3;
const BACKGROUND_TAB    = 4;
const BORDER_TAB        = 5;
const BOX_TAB           = 6;
const AURAL_TAB         = 7;

const GENERIC_SELECTOR      = 0;
const TYPE_ELEMENT_SELECTOR = 1;
const CLASS_SELECTOR        = 2;

const kUrlString = "url(";

const kAsyncTimeout = 1500; // 1.5 second

var objectsArray = null;
var gTimerID;
var gAsyncLoadingTimerID;

var gHaveDocumentUrl = false;
var predefSelector = "";

var gInsertIndex = -1;

// * dialog initialization code
function Startup()
{
  if (InitEditorShell) {
    if (!InitEditorShell())
      return;
  }
  else if (GetCurrentEditor && !GetCurrentEditor()) {
    window.close();
    return;
  }

  // gDialog is declared in EdDialogCommon.js
  // Set commonly-used widgets like this:
  gDialog.selectedTab = TEXT_TAB;
  gDialog.sheetsTreechildren            = document.getElementById("stylesheetsTree");
  gDialog.sheetsTree                    = document.getElementById("sheetsTree");
  gDialog.sheetInfoTab                  = document.getElementById("sheetInfoTab");
  gDialog.atimportButton                = document.getElementById("atimportButton");
  gDialog.atmediaButton                 = document.getElementById("atmediaButton");
  gDialog.linkButton                    = document.getElementById("linkButton");
  gDialog.styleButton                   = document.getElementById("styleButton");
  gDialog.ruleButton                    = document.getElementById("ruleButton");
  gDialog.removeButton                  = document.getElementById("removeButton");
  gDialog.upButton                      = document.getElementById("upButton");
  gDialog.downButton                    = document.getElementById("downButton");

  gDialog.selectedTab = COMPATIBILITY_TAB;
  gDialog.sheetInfoTabPanelTitle        = document.getElementById("sheetInfoTabPanelTitle");
  gDialog.textTab                       = document.getElementById("textTab");
  gDialog.brownFoxLabel                 = document.getElementById("brownFoxLabel");
  gDialog.backgroundImageInput          = document.getElementById("backgroundImageInput");
  gDialog.backgroundPreview             = document.getElementById("backgroundPreview");
  gDialog.sheetTabbox                   = document.getElementById("sheetTabbox");
  gDialog.backgroundColorInput          = document.getElementById("backgroundColorInput");
  gDialog.textColorInput                = document.getElementById("textColorInput");
  gDialog.backgroundRepeatMenulist      = document.getElementById("backgroundRepeatMenulist");
  gDialog.backgroundAttachmentCheckbox  = document.getElementById("backgroundAttachmentCheckbox");
  gDialog.xBackgroundPositionRadiogroup = document.getElementById("xBackgroundPositionRadiogroup");
  gDialog.yBackgroundPositionRadiogroup = document.getElementById("yBackgroundPositionRadiogroup");
  gDialog.fontFamilyRadiogroup          = document.getElementById("fontFamilyRadiogroup");
  gDialog.customFontFamilyInput         = document.getElementById("customFontFamilyInput");
  gDialog.predefFontFamilyMenulist      = document.getElementById("predefFontFamilyMenulist");
  gDialog.fontSizeInput                 = document.getElementById("fontSizeInput");
  gDialog.lineHeightInput               = document.getElementById("lineHeightInput");
  gDialog.textUnderlineCheckbox         = document.getElementById("underlineTextDecorationCheckbox");
  gDialog.textOverlineCheckbox          = document.getElementById("overlineTextDecorationCheckbox");
  gDialog.textLinethroughCheckbox       = document.getElementById("linethroughTextDecorationCheckbox");
  gDialog.textBlinkCheckbox             = document.getElementById("blinkTextDecorationCheckbox");
  gDialog.noDecorationCheckbox          = document.getElementById("noneTextDecorationCheckbox");

  gDialog.topBorderStyleMenulist        = document.getElementById("topBorderStyleMenulist");
  gDialog.topBorderWidthInput           = document.getElementById("topBorderWidthInput");
  gDialog.topBorderColorInput           = document.getElementById("topBorderColorInput");

  gDialog.leftBorderStyleMenulist       = document.getElementById("leftBorderStyleMenulist");
  gDialog.leftBorderWidthInput          = document.getElementById("leftBorderWidthInput");
  gDialog.leftBorderColorInput          = document.getElementById("leftBorderColorInput");

  gDialog.rightBorderStyleMenulist      = document.getElementById("rightBorderStyleMenulist");
  gDialog.rightBorderWidthInput         = document.getElementById("rightBorderWidthInput");
  gDialog.rightBorderColorInput         = document.getElementById("rightBorderColorInput");

  gDialog.bottomBorderStyleMenulist     = document.getElementById("bottomBorderStyleMenulist");
  gDialog.bottomBorderWidthInput        = document.getElementById("bottomBorderWidthInput");
  gDialog.bottomBorderColorInput        = document.getElementById("bottomBorderColorInput");

  gDialog.allFourBordersSame            = document.getElementById("allFourBordersSame");
  gDialog.borderPreview                 = document.getElementById("borderPreview");

  gDialog.volumeScrollbar               = document.getElementById("volumeScrollbar");
  gDialog.volumeMenulist                = document.getElementById("volumeMenulist");
  gDialog.muteVolumeCheckbox            = document.getElementById("muteVolumeCheckbox");

  gDialog.opacityScrollbar              = document.getElementById("opacityScrollbar");
  gDialog.opacityLabel                  = document.getElementById("opacityLabel");

  gDialog.sheetInfoTabGridRows          = document.getElementById("sheetInfoTabGridRows");
  gDialog.sheetInfoTabGrid              = document.getElementById("sheetInfoTabGrid");

  gDialog.expertMode = true;
  gDialog.modified = false;
  gDialog.selectedIndex = -1;

  gHaveDocumentUrl = GetDocumentBaseUrl();

  // Initialize all dialog widgets here,
  // e.g., get attributes from an element for property dialog
  InitSheetsTree(gDialog.sheetsTreechildren);
  
  // Set window location relative to parent window (based on persisted attributes)
  SetWindowLocation();
}

// * Toggles on/off expert mode. In expert mode, all buttons are enabled
//   including buttons for stylesheet creation. When the mode is off, only
//   the "create rule" button is enabled, a stylesheet being created to contain
//   the new rule if necessary
function toggleExpertMode()
{
  // toggle the boolean
  gDialog.expertMode = !gDialog.expertMode;
  if (gDialog.expertMode) {
    if (gDialog.selectedIndex == -1) {
      // if expert mode is on but no selection in the tree, only
      // sheet creation buttons are enabled
      UpdateButtons(false, false, true, true, false, false);
    }
    else {
      // if expert mode is on and we have something selected in the tree,
      // the state of the buttons depend on the type of the selection
      var external  = objectsArray[gDialog.selectedIndex].external;
      var type      = objectsArray[gDialog.selectedIndex].type;
      UpdateButtons(!external, !external, true, true, !external, (!external || (type == SHEET)) );
    }
  }
  else {
    // if we're not in expert mode, allow only rule creation button
    UpdateButtons(!gDialog.atimportButton.hasAttribute("disabled"),
                  !gDialog.atmediaButton.hasAttribute("disabled"),
                  !gDialog.linkButton.hasAttribute("disabled"),
                  !gDialog.styleButton.hasAttribute("disabled"),
                  !gDialog.ruleButton.hasAttribute("disabled"),
                  !gDialog.removeButton.hasAttribute("disabled"));
  }
}

// * This function recreates the contents of the STYLE elements and
//   of the stylesheets local to the filesystem
// XXXX : this function is about to disappear due to last code cleanup
function FlushChanges()
{
  if (gDialog.modified) {
    // let's make sure the editor is going to require save on exit
    getCurrentEditor().incrementModificationCount(1);
  }
  // Validate all user data and set attributes and possibly insert new element here
  // If there's an error the user must correct, return false to keep dialog open.
  var sheet;
  for (var i = 0; i < objectsArray.length; i++) {
    if (objectsArray[i].modified && !objectsArray[i].external &&
        objectsArray[i].type == SHEET) {
      /* let's serialize this stylesheet ! */
      sheet = objectsArray[i].cssElt;
      if (sheet.ownerNode.nodeName.toLowerCase() == "link")
        SerializeExternalSheet(sheet, null);
      else
        SerializeEmbeddedSheet(sheet);
    }
  }
  SaveWindowLocation();
  return true; // do close the window
}

// * removes all the content in a tree
//   param XULElement sheetsTree
function CleanSheetsTree(sheetsTreeChildren)
{
  // we need to clear the selection in the tree otherwise the onselect
  // action on the tree will be fired when we removed the selected entry
  ClearTreeSelection(gDialog.sheetsTree);

  var elt = sheetsTreeChildren.firstChild;
  while (elt) {
    var tmp = elt.nextSibling;
    sheetsTreeChildren.removeChild(elt);
    elt = tmp;
  }
}

function AddSheetEntryToTree(sheetsTree, ownerNode)
{
  if (ownerNode.nodeType == Node.ELEMENT_NODE) {
    var ownerTag  = ownerNode.nodeName.toLowerCase()
    var relType   = ownerNode.getAttribute("rel");
    if (relType) relType = relType.toLowerCase();
    if (ownerTag == "style" ||
        (ownerTag == "link" && relType.indexOf("stylesheet") != -1)) {

      var treeitem  = document.createElementNS(XUL_NS, "treeitem");
      var treerow   = document.createElementNS(XUL_NS, "treerow");
      var treecell  = document.createElementNS(XUL_NS, "treecell");

      // what kind of owner node do we have here ?
      // a style element indicates an embedded stylesheet,
      // while a link element indicates an external stylesheet;
      // the case of an XML Processing Instruction is not handled, we
      // are supposed to be in HTML 4
      var external = false;
      if (ownerTag == "style") {
        treecell.setAttribute("label", "internal stylesheet");
      }
      else if (ownerTag == "link") {
        // external stylesheet, let's present its URL to user
        treecell.setAttribute("label", ownerNode.href);
        external = true;
        if ( /(\w*):.*/.test(ownerNode.href) ) {
          if (RegExp.$1 == "file") {
            external = false;
          }
        }
        else
          external = false;
      }
      // add a new entry to the tree
      var o = newObject( treeitem, external, SHEET, ownerNode.sheet, false, 0 );
      PushInObjectsArray(o);
      
      treerow.appendChild(treecell);
      treeitem.appendChild(treerow);
      treeitem.setAttribute("container", "true");
      // add enties to the tree for the rules in the current stylesheet
      var rules = null;
      if (ownerNode.sheet)
        rules = ownerNode.sheet.cssRules;
      AddRulesToTreechildren(treeitem, rules, external, 1);

      sheetsTree.appendChild(treeitem);
    }
  }
}

function PushInObjectsArray(o)
{
  if (gInsertIndex == -1)
    objectsArray.push(o);
  else {
    objectsArray.splice(gInsertIndex, 0, o);
    gInsertIndex++;
  }
}

// * populates the tree in the dialog with entries
//   corresponding to all stylesheets and css rules attached to
//   document
//   param XULElement sheetsTree
function InitSheetsTree(sheetsTree)
{
  // remove all entries in the tree
  CleanSheetsTree(sheetsTree);
  // Look for the stylesheets attached to the current document
  // Get them from the STYLE and LINK elements because of async sheet loading :
  // the LINK element is always here while the corresponding sheet might be
  // delayed by network
  var headNode = GetHeadElement();
  if ( headNode && headNode.hasChildNodes() ) {
    var ssn = headNode.childNodes.length;
    objectsArray = new Array();
    if (ssn) {
      var i;
      gInsertIndex = -1;
      for (i=0; i<ssn; i++) {
        var ownerNode = headNode.childNodes[i];
        AddSheetEntryToTree(sheetsTree, ownerNode); 
      }
    }
  }
}

// * create a new "object" corresponding to an entry in the tree
//   unfortunately, it's still impossible to attach a JS object to
//   a treecell :-(
//   param XULElement xulElt
//   param boolean external
//   param integer type
//   param (DOMNode|DOMCSSStyleSheet|DOMCSSRule) cssElt
//   param boolean modified
//   param integer depth
function newObject( xulElt, external, type, cssElt, modified, depth)
{
  return {xulElt:xulElt,
          external:external,
          type:type,
          cssElt:cssElt,
          modified:modified,
          depth:depth};
}

function AddStyleRuleToTreeChildren(rule, external, depth)
{
  var subtreeitem  = document.createElementNS(XUL_NS, "treeitem");
  var subtreerow   = document.createElementNS(XUL_NS, "treerow");
  var subtreecell  = document.createElementNS(XUL_NS, "treecell");
  // show the selector attached to the rule
  subtreecell.setAttribute("label", rule.selectorText);
  var o = newObject( subtreeitem, external, STYLE_RULE, rule, false, depth );
  PushInObjectsArray(o);
  if (external) {
    subtreecell.setAttribute("properties", "external");
  }
  subtreerow.appendChild(subtreecell);
  subtreeitem.appendChild(subtreerow);

  return subtreeitem;
}

function AddPageRuleToTreeChildren(rule, external, depth)
{
  var subtreeitem  = document.createElementNS(XUL_NS, "treeitem");
  var subtreerow   = document.createElementNS(XUL_NS, "treerow");
  var subtreecell  = document.createElementNS(XUL_NS, "treecell");
  // show the selector attached to the rule
  subtreecell.setAttribute("label", "@page " + rule.selectorText);
  var o = newObject( subtreeitem, external, PAGE_RULE, rule, false, depth );
  PushInObjectsArray(o);
  if (external) {
    subtreecell.setAttribute("properties", "external");
  }
  subtreerow.appendChild(subtreecell);
  subtreeitem.appendChild(subtreerow);

  return subtreeitem;
}

function AddImportRuleToTreeChildren(rule, external, depth)
{
  var subtreeitem  = document.createElementNS(XUL_NS, "treeitem");
  var subtreerow   = document.createElementNS(XUL_NS, "treerow");
  var subtreecell  = document.createElementNS(XUL_NS, "treecell");
  // show "@import" and the URL
  subtreecell.setAttribute("label", "@import "+rule.href, external);
  var o = newObject( subtreeitem, external, IMPORT_RULE, rule, false, depth );
  PushInObjectsArray(o);
  if (external) {
    subtreecell.setAttribute("properties", "external");
  }
  subtreerow.appendChild(subtreecell);
  subtreeitem.appendChild(subtreerow);
  subtreeitem.setAttribute("container", "true");
  if (rule.styleSheet) {
    // if we have a stylesheet really imported, let's browse it too
    // increasing the depth and marking external
    AddRulesToTreechildren(subtreeitem , rule.styleSheet.cssRules, true, depth+1);
  }
  return subtreeitem;
}

// * adds subtreeitems for the CSS rules found
//   into rules; in case of an at-rule, the method calls itself with
//   a subtreeitem, the rules in the at-rule, a boolean specifying if
//   the rules are external to the document or not, and an increased
//   depth.
//   param XULElement  treeItem
//   param CSSRuleList rules
//   param boolean     external
//   param integer     depth
function AddRulesToTreechildren(treeItem, rules, external, depth)
{
  // is there any rule in the stylesheet ; earlay way out if not
  if (rules && rules.length) {
    var subtreechildren = document.createElementNS(XUL_NS, "treechildren");
    var j, o;
    var subtreeitem, subtreerow, subtreecell;
    // let's browse all the rules
    for (j=0; j< rules.length; j++) {
      switch (rules[j].type) {
        case CSSRule.STYLE_RULE:
          // this is a CSSStyleRule
          subtreeitem  = AddStyleRuleToTreeChildren(rules[j], external, depth);
          subtreechildren.appendChild(subtreeitem);
          break;
        case CSSRule.PAGE_RULE:
          // this is a CSSStyleRule
          subtreeitem  = AddPageRuleToTreeChildren(rules[j], external, depth);
          subtreechildren.appendChild(subtreeitem);
          break;
        case CSSRule.IMPORT_RULE:
          // this is CSSImportRule for @import
          subtreeitem  = AddImportRuleToTreeChildren(rules[j], external, depth);
          subtreechildren.appendChild(subtreeitem);
          break;
        case CSSRule.MEDIA_RULE:
          // this is a CSSMediaRule @media
          subtreeitem  = document.createElementNS(XUL_NS, "treeitem");
          subtreerow   = document.createElementNS(XUL_NS, "treerow");
          subtreecell  = document.createElementNS(XUL_NS, "treecell");
          // show "@media" and media list
          subtreecell.setAttribute("label", "@media "+rules[j].media.mediaText, external);
          o = newObject( subtreeitem, external, MEDIA_RULE, rules[j], false, depth );
          PushInObjectsArray(o);
          if (external) {
            subtreecell.setAttribute("properties", "external");
          }
          subtreerow.appendChild(subtreecell);
          subtreeitem.appendChild(subtreerow);
          subtreeitem.setAttribute("container", "true");
          // let's browse the rules attached to this CSSMediaRule, keeping the
          // current external and increasing depth
          AddRulesToTreechildren(subtreeitem, rules[j].cssRules, external, depth+1);
          subtreechildren.appendChild(subtreeitem);
          break;
        case CSSRule.CHARSET_RULE:
          // this is a CSSCharsetRule
          subtreeitem  = document.createElementNS(XUL_NS, "treeitem");
          subtreerow   = document.createElementNS(XUL_NS, "treerow");
          subtreecell  = document.createElementNS(XUL_NS, "treecell");
          // show "@charset" and the encoding
          subtreecell.setAttribute("label", "@charset "+rules[j].encoding, external);
          o = newObject( subtreeitem, external, CHARSET_RULE, rules[j], false, depth );
          PushInObjectsArray(o);
          if (external) {
            subtreecell.setAttribute("properties", "external");
          }
          subtreerow.appendChild(subtreecell);
          subtreeitem.appendChild(subtreerow);
          subtreechildren.appendChild(subtreeitem);
          break;          
      }
    }
    treeItem.appendChild(subtreechildren);
  }
}

function GetSelectedItemData()
{
  // get the selected tree item (if any)
  var selectedItem =  getSelectedItem(gDialog.sheetsTree);
  gDialog.selectedIndex  = -1;
  gDialog.selectedObject = null;

  // look for the object in objectsArray corresponding to the
  // selectedItem
  var i, l = objectsArray.length;
  if (selectedItem) {
    for (i=0; i<l; i++) {
      if (objectsArray[i].xulElt == selectedItem) {
        gDialog.selectedIndex  = i;
        gDialog.treeItem       = objectsArray[i].xulElt;
        gDialog.externalObject = objectsArray[i].external;
        gDialog.selectedType   = objectsArray[i].type;
        gDialog.selectedObject = objectsArray[i].cssElt;
        gDialog.modifiedObject = objectsArray[i].modified;
        gDialog.depthObject    = objectsArray[i].depth;
        break;
      }
    }
  }
}

// * selects either a tree item (sheet, rule) or a tab
//   in the former case, the parameter is null ; in the latter,
//   the parameter is a string containing the name of the selected tab
//   param String tab
function onSelectCSSTreeItem(tab)
{
  // convert the tab string into a tab id
  if      (tab == "general")       tab = GENERAL_TAB;
  else if (tab == "compatibility") tab = COMPATIBILITY_TAB;
  else if (tab == "text")          tab = TEXT_TAB;
  else if (tab == "background")    tab = BACKGROUND_TAB;
  else if (tab == "border")        tab = BORDER_TAB;
  else if (tab == "box")           tab = BOX_TAB;
  else if (tab == "aural")         tab = AURAL_TAB;

  GetSelectedItemData();

  if (gDialog.selectedIndex == -1) {
    // there is no tree item selected, let's fallback to the Info tab
    // but there is nothing we can display in that tab...
    if (tab != COMPATIBILITY_TAB)
      gDialog.sheetTabbox.selectedTab = gDialog.sheetInfoTab;
    return;
  }

  var external  = objectsArray[gDialog.selectedIndex].external;
  var type      = objectsArray[gDialog.selectedIndex].type;
  var cssObject = gDialog.selectedObject;
  // Let's update the buttons depending on what kind of object is
  // selected in the tree
  UpdateButtons(!external, !external, true, true, !external, (!external || (type == SHEET)) );

  if (gDialog.selectedType != STYLE_RULE) {
    // user did not select a CSSStyleRule, let's fallback to Info tab
    tab = GENERAL_TAB;
  }
  if (!tab) {
    // this method gets called by a selection in the tree. Is there a
    // tab already selected ? If yes, keep it; if no, fallback to the
    // Info tab
    tab = gDialog.selectedTab ? gDialog.selectedTab : GENERAL_TAB;
  }
  switch (tab) {
    case COMPATIBILITY_TAB:
      return;
      break;
    case TEXT_TAB:
      // we have to update the text preview, let's remove its style attribute
      gDialog.brownFoxLabel.removeAttribute("style");      
      InitTextTabPanel();
      return;
      break;
    case BACKGROUND_TAB:
      InitBackgroundTabPanel();
      return;
      break;
    case BORDER_TAB:
      InitBorderTabPanel();
      return;
      break;
    case BOX_TAB:
      InitBoxTabPanel();
      return;
      break;
    case AURAL_TAB:
      InitAuralTabPanel();
      return;
      break;
  }

  // if we are here, the Info tab is our choice
  gDialog.selectedTab = GENERAL_TAB;
  gDialog.sheetTabbox.selectedTab = gDialog.sheetInfoTab;
  
  var gridrows = gDialog.sheetInfoTabGridRows;
  var grid     = gDialog.sheetInfoTabGrid;

  if (gridrows) {
    // first, remove all information present in the Info tab
    grid.removeChild(gridrows);
  }

  gridrows = document.createElementNS(XUL_NS, "rows");
  gDialog.sheetInfoTabGridRows = gridrows;
  gridrows.setAttribute("id", "sheetInfoTabGridRows");
  grid.appendChild(gridrows);
  grid.removeAttribute("style");

  switch (type) {
    case OWNER_NODE:
      // this case is a workaround when we have a LINK element but the
      // corresponding stylesheet is not loaded yet
      if (gDialog.selectedObject.sheet) {
        var index = gDialog.selectedIndex;
        sheetLoadedTimeoutCallback(index);
        onSelectCSSTreeItem(tab);
        return;
      }
      break;
    case SHEET:
      if (cssObject) {
        var alternate = "";
        if (external &&
            cssObject.ownerNode.getAttribute("rel").toLowerCase().indexOf("alternate") != -1) {
          alternate = " (alternate)";
        }
        gDialog.sheetInfoTabPanelTitle.setAttribute("value", "Stylesheet"+alternate);
        AddLabelToInfobox(gridrows, "Type:", cssObject.type, null, false);
        AddCheckboxToInfobox(gridrows, "Disabled:", "check to disable stylesheet (cannot be saved)",
                             cssObject.disabled, "onStylesheetDisabledChange");
        var href;
        if (cssObject.ownerNode.nodeName.toLowerCase() == "link") {
          href = cssObject.href;
        }
        else {
          href = "none (embedded into the document)";
        }
        AddLabelToInfobox(gridrows, "URL:", href, null, false);
        var mediaList = cssObject.media.mediaText;
        if (!mediaList || mediaList == "") {
          mediaList = "all";
        }
        AddEditableZoneToInfobox(gridrows, "Media list:", mediaList, "onStylesheetMediaChange", false);
        if (cssObject.title) {
          AddEditableZoneToInfobox(gridrows, "Title:", cssObject.title, "onStylesheetTitleChange", false);
        }

        if (cssObject.cssRules) {
          AddLabelToInfobox(gridrows, "Number of rules:", cssObject.cssRules.length, null, false);
        }
        if (!external && cssObject.ownerNode.nodeName.toLowerCase() == "style")
          // we can export only embedded stylesheets
          AddSingleButtonToInfobox(gridrows, "Export stylesheet and switch to exported version", "onExportStylesheet")
      }
      else
        AddLabelToInfobox(gridrows, "Unable to retrieve stylesheet", null, null, false);
      break;
    case STYLE_RULE:
    case PAGE_RULE:
      var h = document.defaultView.getComputedStyle(grid.parentNode, "").getPropertyValue("height");
      // let's prepare the grid for the case the rule has a laaaaarge number
      // of property declarations
      grid.setAttribute("style", "overflow: auto; height: "+h);
      gDialog.sheetInfoTabPanelTitle.setAttribute("value",
                  (type == STYLE_RULE) ? "Style rule" : "Page rule" );

      AddLabelToInfobox(gridrows, "Selector:", cssObject.selectorText, null, false);
      if (cssObject.style.length) {
        AddLabelToInfobox(gridrows, "Declarations:", " ", "Importance", false);
        for (var i=0; i<cssObject.style.length; i++) {
          AddDeclarationToInfobox(gridrows, cssObject, i, false);
        }
      }
      // TO BE DONE : need a way of changing the importance of a given declaration
      break;
    case MEDIA_RULE:
      gDialog.sheetInfoTabPanelTitle.setAttribute("value", "Media rule");
      AddLabelToInfobox(gridrows, "Media list:", cssObject.media.mediaText, null, false);
      AddLabelToInfobox(gridrows, "Number of rules:", cssObject.cssRules.length, null, false);
      break;
    case IMPORT_RULE:
      gDialog.sheetInfoTabPanelTitle.setAttribute("value", "Import rule");
      AddLabelToInfobox(gridrows, "URL:", cssObject.href, null, false);
      AddLabelToInfobox(gridrows, "Media list:", cssObject.media.mediaText, null, false);
      break;
    // TO BE DONE : @charset and other exotic rules
  }
}

// * updates the UI buttons with the given states
//   param boolean importState
//   param mediaState
//   param boolean linkState
//   param boolean styleState
//   param boolean ruleState
//   param boolean removeState
function UpdateButtons(importState, mediaState, linkState, styleState, ruleState, removeState)
{
  if (!gDialog.expertMode) {
    importState = false;
    mediaState = false;
    linkState = false;
    styleState = false;
    ruleState = true;
  }
  if (!gDialog.selectedObject) {
    importState = false;
    mediaState = false;
    if (gDialog.selectedIndex != -1)
      ruleState = false;
  }
  EnableUI(gDialog.atimportButton, importState);
  EnableUI(gDialog.atmediaButton, mediaState);
  EnableUI(gDialog.linkButton, linkState);
  EnableUI(gDialog.styleButton, styleState);
  EnableUI(gDialog.ruleButton, ruleState);
  EnableUI(gDialog.removeButton, removeState);
  EnableUI(gDialog.upButton, removeState);
  EnableUI(gDialog.downButton, removeState);
}

// * adds a button to the given treerow
//   param XULElement rows
//   param String label
//   param String callback
function AddSingleButtonToInfobox(rows, label, callback)
{
  var row = document.createElementNS(XUL_NS, "row");
  row.setAttribute("align", "center");
  var spacer = document.createElementNS(XUL_NS, "spacer");
  var hbox = document.createElementNS(XUL_NS, "hbox");
  var button = document.createElementNS(XUL_NS, "button");
  button.setAttribute("label", label);
  button.setAttribute("class", "align-right");
  button.setAttribute("oncommand", callback+"();");
  row.appendChild(spacer);
  hbox.appendChild(button);
  row.appendChild(hbox);
  rows.appendChild(row);
}

// * adds a textarea to the given treerow, allows to assign the
//   initial value and specify that it should acquire the focus
//   param XULElement rows
//   param String label
//   param String value
//   param String callback
//   param boolean focus
function AddEditableZoneToInfobox(rows, label, value, callback, focus)
{
  var labelLabel = document.createElementNS(XUL_NS, "label");
  var row = document.createElementNS(XUL_NS, "row");
  row.setAttribute("align", "center");
  labelLabel.setAttribute("value", label);
  row.appendChild(labelLabel);

  var textbox = document.createElementNS(XUL_NS, "textbox");
  if (callback != "") 
    textbox.setAttribute("oninput", callback+"(this)");
  textbox.setAttribute("value", value);
  textbox.setAttribute("size", 20);
  row.appendChild(textbox);
  rows.appendChild(row);
  if (focus)
    SetTextboxFocus(textbox);
  return textbox;
}

// * adds a radiogroup to the given treerow
//   param XULElement rows
//   param String label
function AddRadioGroupToInfoBox(rows, label)
{
  var row = document.createElementNS(XUL_NS, "row");
  row.setAttribute("align", "baseline");

  var labelLabel = document.createElementNS(XUL_NS, "label");
  labelLabel.setAttribute("value", label);
  row.appendChild(labelLabel);

  var radiogroup = document.createElementNS(XUL_NS, "radiogroup");
  row.appendChild(radiogroup);
  rows.appendChild(row);
  return radiogroup;
}

// * adds a radio button to a previously created radiogroup
//   param XULElement radiogroup
//   param String label
//   param String callback
//   param integer selectorType
//   param boolean selected
function AddRadioToRadioGroup(radiogroup, label, callback, selectorType, selected)
{
  var radio = document.createElementNS(XUL_NS, "radio");
  radio.setAttribute("label",     label);
  radio.setAttribute("oncommand", callback + "(" + selectorType + ");" );
  if (selected)
    radio.setAttribute("selected", "true");
  radiogroup.appendChild(radio);
}

// * adds a label and a checkbox to a given treerows
//   param XULElement rows
//   param String label
//   param String checkboxLabel
//   param String value
//   param String callback
function AddCheckboxToInfobox(rows, label, checkboxLabel, value, callback)
{
  var row = document.createElementNS(XUL_NS, "row");
  row.setAttribute("align", "center");

  var labelLabel = document.createElementNS(XUL_NS, "label");
  labelLabel.setAttribute("value", label);
  row.appendChild(labelLabel);

  var checkbox = document.createElementNS(XUL_NS, "checkbox");
  checkbox.setAttribute("label", checkboxLabel);
  checkbox.setAttribute("checked", value);
  checkbox.setAttribute("oncommand", callback+"()");
  row.appendChild(checkbox);

  rows.appendChild(row);
}

// * adds a label to a given treerows; strong indicates a bold font
//   param XULElement rows
//   param String label
//   param String value
//   param boolean strong
function AddLabelToInfobox(rows, firstLabel, flexibleLabel, lastLabel, strong)
{
  var labelLabel = document.createElementNS(XUL_NS, "label");
  var row = document.createElementNS(XUL_NS, "row");
  row.setAttribute("align", "center");
  labelLabel.setAttribute("value", firstLabel);
  if (strong) {
    labelLabel.setAttribute("class", "titleLabel");
  }
  row.appendChild(labelLabel);

  if (flexibleLabel) {
    var valueLabel = document.createElementNS(XUL_NS, "label");
    valueLabel.setAttribute("value", flexibleLabel);
    if (strong) {
      valueLabel.setAttribute("class", "titleLabel");
    }
    row.appendChild(valueLabel);
  }

  if (lastLabel) {
    valueLabel = document.createElementNS(XUL_NS, "label");
    valueLabel.setAttribute("value", lastLabel);
    if (strong) {
      valueLabel.setAttribute("class", "titleLabel");
    }
    row.appendChild(valueLabel);
  }

  rows.appendChild(row);
}

// * adds a declaration's importance to a given treerows
//   param XULElement rows
//   param String label
//   param String value
//   param String importance
function AddDeclarationToInfobox(rows, cssObject, i, importance)
{
  var labelLabel = document.createElementNS(XUL_NS, "label");
  var row = document.createElementNS(XUL_NS, "row");
  row.setAttribute("align", "center");
  labelLabel.setAttribute("value", "");
  row.appendChild(labelLabel);

  var valueLabel = document.createElementNS(XUL_NS, "label");
  valueLabel.setAttribute("value", GetDeclarationText(cssObject, i));
  row.appendChild(valueLabel);

  var importanceLabel = document.createElementNS(XUL_NS, "checkbox");
  if (GetDeclarationImportance(cssObject, i) == "important") {
    importanceLabel.setAttribute("checked", true);
  }
  importanceLabel.setAttribute("oncommand", "TogglePropertyImportance(\"" + cssObject.style.item(i) + "\")" );
  row.appendChild(importanceLabel);

  rows.appendChild(row);
}

function TogglePropertyImportance(property)
{
  var cssObject = gDialog.selectedObject;
  dump("IMPORTANCE = " + cssObject.style.getPropertyPriority(property) + "\n");
  var newImportance =  (cssObject.style.getPropertyPriority(property) == "important") ? "" : "important" ;
  dump("NEW IMPORTANCE = " + newImportance + "\n");
  cssObject.style.setProperty(property, cssObject.style.getPropertyValue(property), newImportance);
}

// * retrieves the index-nth style declaration in a rule
//   param DOMCSSRule styleRule
//   param integer index
//   return String
function GetDeclarationText(styleRule, index)
{
  var pName = styleRule.style.item(index);
  return pName + ": " + styleRule.style.getPropertyValue(pName);
}

// * retrieves the stylesheet containing the selected tree entry
//   return integer
function GetSheetContainer()
{
  var index = gDialog.selectedIndex;
  while (index >= 0 && objectsArray[index].type != SHEET) {
    index--;
  }
  return index;
}

// * declares that the stylesheet containing the selected tree entry
//   has been modified
function SetModifiedFlagOnStylesheet()
{
  var index = GetSheetContainer();
  if (index != -1) {
    objectsArray[index].modified = true;
    gDialog.modified = true;
  }
}

// * we are about to put some info about the selected entry into
//   the Info tab
//   return XULElement
function PrepareInfoGridForCreation()
{
  gDialog.sheetTabbox.selectedTab = gDialog.sheetInfoTab;
  
  var gridrows = gDialog.sheetInfoTabGridRows;
  var grid     = gDialog.sheetInfoTabGrid;

  if (gridrows) {
    grid.removeChild(gridrows);
  }

  gridrows = document.createElementNS(XUL_NS, "rows");
  gDialog.sheetInfoTabGridRows = gridrows;
  gridrows.setAttribute("id", "sheetInfoTabGridRows");
  grid.appendChild(gridrows);
  grid.removeAttribute("style");
  return gridrows;
}

// * user wants to create a @import rule
function CreateNewAtimportRule()
{
  var gridrows = PrepareInfoGridForCreation();

  gDialog.newType      = IMPORT_RULE;
  gDialog.newMediaList = "";
  gDialog.newURL       = "";
  gDialog.sheetInfoTabPanelTitle.setAttribute("value", "New Import At-rule");
  AddEditableZoneToInfobox(gridrows, "URL:",        gDialog.newURL,    "onNewURLChange", true);
  AddEditableZoneToInfobox(gridrows, "Media list:", gDialog.newMediaList, "onNewMediaListChange", false);
  AddSingleButtonToInfobox(gridrows, "Create Import At-rule", "onConfirmCreateNewObject", false);
}

// * user wants to create a new style rule
function CreateNewStyleRule()
{
  var gridrows = PrepareInfoGridForCreation();

  gDialog.newExternal  = false;
  gDialog.newType      = STYLE_RULE;
  gDialog.newSelector  = "";
  gDialog.sheetInfoTabPanelTitle.setAttribute("value", "New Style Rule");
  gDialog.newSelectorType = CLASS_SELECTOR;

  var radiogroup = AddRadioGroupToInfoBox(gridrows, "Create a new:");
  // offer choice between class selector and type element selector
  AddRadioToRadioGroup(radiogroup, "named style (enter class name below)",
                       "onCreationStyleRuleTypeChange", CLASS_SELECTOR, true);
  AddRadioToRadioGroup(radiogroup, "style applied to all elements of type (enter type below)",
                       "onCreationStyleRuleTypeChange", TYPE_ELEMENT_SELECTOR, false);
  // oh, and in expert mode, allow of course any selector
  if (gDialog.expertMode) {
    AddRadioToRadioGroup(radiogroup, "style applied to all elements matching the following selector",
                         "onCreationStyleRuleTypeChange", GENERIC_SELECTOR, false);
  }
  AddEditableZoneToInfobox(gridrows, " ", gDialog.newSelector,  "onNewSelectorChange", true);

  AddSingleButtonToInfobox(gridrows, "Create Style Rule", "onConfirmCreateNewObject");
}

// * user changed the type of style rule (s)he wants to create
//   param integer type
function onCreationStyleRuleTypeChange(type)
{
  gDialog.newSelectorType = type;
}

// * user wants to create a new embedded stylesheet
function CreateNewStyleElement()
{
  var gridrows = PrepareInfoGridForCreation();

  gDialog.newExternal  = false;
  gDialog.newType      = SHEET;
  gDialog.newMediaList = "";
  gDialog.newTitle     = "";
  gDialog.sheetInfoTabPanelTitle.setAttribute("value", "New Stylesheet");
  AddLabelToInfobox(gridrows, "Type:", "text/css", null, false);
  AddEditableZoneToInfobox(gridrows, "Media list:", gDialog.newMediaList, "onNewMediaListChange", true);
  AddEditableZoneToInfobox(gridrows, "Title:",      gDialog.newTitle,  "onNewTitleChange", false);

  AddSingleButtonToInfobox(gridrows, "Create Stylesheet", "onConfirmCreateNewObject")
}

// * user wants to attach an external stylesheet
function CreateNewLinkedSheet()
{
  var gridrows = PrepareInfoGridForCreation();

  gDialog.newExternal  = true;
  gDialog.newType      = SHEET;
  gDialog.newAlternate = false;
  gDialog.newURL       = "";
  gDialog.newMediaList = "";
  gDialog.newTitle     = "";
  gDialog.sheetInfoTabPanelTitle.setAttribute("value", "New Linked Stylesheet");
  AddLabelToInfobox(gridrows, "Type:", "text/css", null, false);
  // alternate stylesheet ?
  AddCheckboxToInfobox(gridrows, "Alternate:", "check to create alternate stylesheet", gDialog.newAlternate,
                       "onNewAlternateChange");
  gDialog.URLtextbox = AddEditableZoneToInfobox(gridrows, "URL:",        gDialog.newURL,    "onNewURLChange", true);
  AddSingleButtonToInfobox(gridrows, "Choose file", "onChooseLocalFile")

  AddEditableZoneToInfobox(gridrows, "Media list:", gDialog.newMediaList, "onNewMediaListChange", false);
  AddEditableZoneToInfobox(gridrows, "Title:",      gDialog.newTitle,  "onNewTitleChange", false);

  AddSingleButtonToInfobox(gridrows, "Create Stylesheet", "onConfirmCreateNewObject")

  // the two following labels are unfortunately useful...
  AddLabelToInfobox(gridrows, "", "(Warning : save document *before* attaching local stylesheet)", null, false);
  AddLabelToInfobox(gridrows, "", "(use Refresh button if stylesheet is not immediately downloaded)", null, false);
}

// * forget about everything, and let's redo it
function Restart()
{
  // delete all objects we keep track of
  var l = objectsArray.length;
  for (var i=0; i<l; i++) {
    delete objectsArray[i];
  }
  delete objectsArray;

  // now, let's clear the tree
  var gridrows = gDialog.sheetInfoTabGridRows;
  if (gridrows) {
    var elt = gridrows.lastChild;
    while (elt) {
      var tmp = elt.previousSibling;
      gridrows.removeChild(elt);
      elt = tmp;
    }
  }

  // switch to default Info tab
  gDialog.selectedTab = GENERAL_TAB;
  gDialog.sheetInfoTabPanelTitle.setAttribute("value", "");

  // let's recreate the tree
  InitSheetsTree(gDialog.sheetsTreechildren);
  // and update the buttons
  UpdateButtons(false, false, true, true, false, false);
}

// * does less than Restart(). We only regenerate the tree, keeping track
//   of the selection
function Refresh()
{
  var index = gDialog.selectedIndex;
  Restart();
  if (index != -1)
    selectTreeItem(objectsArray[index].xulElt);
}

/* CALLBACKS */

// * user toggled the "Alternate Stylesheet" checkbox
function onNewAlternateChange()
{
  gDialog.newAlternate = !gDialog.newAlternate;
}

// * user changed the URL of an external stylesheet; elt is the textarea
//   param XULElement elt
function onNewURLChange(elt)
{
  gDialog.newURL = elt.value;
}

// * user changed the selector for a rule; elt is the textarea
//   param XULElement elt
function onNewSelectorChange(elt)
{
  gDialog.newSelector = elt.value;
}

// * user changed the list of medias for a new rule; elt is the textarea
//   param XULElement elt
function onNewMediaListChange(elt)
{
  gDialog.newMediaList = elt.value;
}

// * user changed the title of a new stylesheet; elt is the textarea
//   param XULElement elt
function onNewTitleChange(elt)
{
  gDialog.newTitle = elt.value;
}

// * user disables/enabled the stylesheet
function onStylesheetDisabledChange()
{
  gDialog.selectedObject.disabled = !gDialog.selectedObject.disabled;
}

// * user changed the title of an existing stylesheet; elt is the textarea
//   param XULElement elt
function onStylesheetTitleChange(elt)
{
  var titleText = elt.value;
  gDialog.selectedObject.ownerNode.setAttribute("title", titleText);
  SetModifiedFlagOnStylesheet();
}

// * user changed the list of medias of an existing stylesheet; elt is the textarea
//   param XULElement elt
function onStylesheetMediaChange(elt)
{
  var mediaText= elt.value;
  gDialog.selectedObject.ownerNode.setAttribute("media", mediaText);
  SetModifiedFlagOnStylesheet();
}

function GetSubtreeChildren(elt)
{
  var subtreechildren = null;
  if (!elt) return null;
  if (elt.hasChildNodes()) {
    subtreechildren = elt.lastChild;
    while (subtreechildren && subtreechildren .nodeName.toLowerCase() != "treechildren") {
      subtreechildren = subtreechildren .previousSibling;
    }
  }
  if (!subtreechildren) {
    subtreechildren = document.createElementNS(XUL_NS, "treechildren");
    elt.appendChild(subtreechildren);
  }
  return subtreechildren;
}

// * here, we create a new sheet or rule
function onConfirmCreateNewObject()
{
  // first, let's declare we modify the document
  gDialog.modified = true;
  var selector;

  // if we are requested to create a style rule in expert mode,
  // let's find the last embedded stylesheet
  if (!gDialog.expertMode && gDialog.newType == STYLE_RULE) {
    var indexLastEmbeddedStylesheet = -1;
    for (var i = objectsArray.length-1; i >= 0 ; i--) {
      if (objectsArray[i].type == SHEET && ! objectsArray[i].external) {
        indexLastEmbeddedStylesheet = i;
        break;
      }
    }
    if (indexLastEmbeddedStylesheet != -1) {
      gDialog.selectedIndex = indexLastEmbeddedStylesheet;
    }
    else {
      // there is no stylesheet ! let's create one that will contain our rule
      gDialog.newExternal  = false;
      gDialog.newMediaList = "";
      gDialog.newTitle     = "";
      gDialog.newType      = SHEET;
      var selectorType     = gDialog.newSelectorType;
      selector             = gDialog.newSelector;
      onConfirmCreateNewObject();

      // now, create the rule...
      gDialog.newType         = STYLE_RULE;
      gDialog.newSelectorType = selectorType;
      gDialog.newSelector     = selector;
    }
  }
  var containerIndex, sheetIndex;
  var cssObject;
  var l;
  var ruleIndex;
  var newSheetOwnerNode;
  var headNode;
  var newCssRule;
  switch (gDialog.newType) {
    case STYLE_RULE:
      if (gDialog.newSelector != "") {
        containerIndex = gDialog.selectedIndex;
        while (objectsArray[containerIndex].type != SHEET &&
               objectsArray[containerIndex].type != MEDIA_RULE)
          containerIndex--;

        switch (gDialog.newSelectorType) {
          case TYPE_ELEMENT_SELECTOR:
          case GENERIC_SELECTOR:
            selector = gDialog.newSelector;
            break;
          case CLASS_SELECTOR:
            selector = "." + gDialog.newSelector;
            break;
        }
        cssObject = objectsArray[containerIndex].cssElt;
        l = cssObject.cssRules.length;
        cssObject.insertRule(selector + " { }", l);

        if (cssObject.cssRules.length > l) {
          // hmmm, there's always the bad case of a wrong rule, dropped by the
          // parser ; that's why we need to check we really inserted something

          /* find inserted rule's index in objectsArray */
          var depth    = objectsArray[containerIndex].depth;
          var external = objectsArray[containerIndex].external;

          ruleIndex = containerIndex + 1;
          while (ruleIndex < objectsArray.length &&
                 objectsArray[ruleIndex].depth > depth) {
            ruleIndex++;
          }
          var subtreechildren = GetSubtreeChildren(objectsArray[containerIndex].xulElt);
          gInsertIndex = ruleIndex;
          var subtreeitem = AddStyleRuleToTreeChildren(cssObject.cssRules[l], external, depth);
          subtreechildren.appendChild(subtreeitem);
          selectTreeItem(subtreeitem);
          SetModifiedFlagOnStylesheet();
        }
      }
      break;
    case IMPORT_RULE:
      if (gDialog.newURL != "") {

        containerIndex     = GetSheetContainer();
        // **must** clear the selection before changing the tree
        ClearTreeSelection(gDialog.sheetsTree);

        var containerCssObject = objectsArray[containerIndex].cssElt;
        var containerDepth     = objectsArray[containerIndex].depth;
        var containerExternal  = objectsArray[containerIndex].external;

        var cssRuleIndex = -1;
        if (containerCssObject.cssRules)
          for (i=0; i < containerCssObject.cssRules.length; i++)
            if (containerCssObject.cssRules[i].type != CSSRule.IMPORT_RULE &&
                containerCssObject.cssRules[i].type != CSSRule.CHARSET_RULE) {
              cssRuleIndex = i;
              break;
            }
        if (cssRuleIndex == -1) {
          // no rule in the sheet for the moment or only charset and import rules
          containerCssObject.insertRule('@import url("'+ gDialog.newURL + '") ' + gDialog.newMediaList + ";",
                                        containerCssObject.cssRules.length);
          newCssRule = containerCssObject.cssRules[containerCssObject.cssRules.length - 1];

          subtreechildren = GetSubtreeChildren(objectsArray[containerIndex].xulElt);

          gInsertIndex = ruleIndex;
          subtreeitem = AddImportRuleToTreeChildren(newCssRule,
                                                    containerExternal,
                                                    containerDepth + 1);
          
          subtreechildren.appendChild(subtreeitem);
          ruleIndex = FindObjectIndexInObjectsArray(newCssRule);
        }
        else {
          // cssRuleIndex is the index of the first not charset and not import rule in the sheet
          ruleIndex = FindObjectIndexInObjectsArray(containerCssObject.cssRules[cssRuleIndex]);
          // and ruleIndex represents the index of the corresponding object in objectsArray
          var refObject  = objectsArray[ruleIndex];

          containerCssObject.insertRule('@import url("'+ gDialog.newURL + '") ' + gDialog.newMediaList + ";",
                                        cssRuleIndex);
          newCssRule = containerCssObject.cssRules[cssRuleIndex];
          gInsertIndex = ruleIndex;
          subtreeitem = AddImportRuleToTreeChildren(newCssRule, containerExternal, containerDepth + 1);

          var refNode = refObject.xulElt;
          refNode.parentNode.insertBefore(subtreeitem, refNode);
        }

          
        selectTreeItem(subtreeitem);
        SetModifiedFlagOnStylesheet();
        if (gAsyncLoadingTimerID)
          clearTimeout(gAsyncLoadingTimerID);
        if (!newCssRule.styleSheet)
          gAsyncLoadingTimerID = setTimeout("sheetLoadedTimeoutCallback(" + ruleIndex + ")", kAsyncTimeout);
      }
      break;
    case SHEET:
      gInsertIndex = -1;

      ClearTreeSelection(gDialog.sheetsTree);

      if (gDialog.newExternal && gDialog.newURL != "") {
        subtreeitem  = document.createElementNS(XUL_NS, "treeitem");
        var subtreerow   = document.createElementNS(XUL_NS, "treerow");
        var subtreecell  = document.createElementNS(XUL_NS, "treecell");
        subtreeitem.setAttribute("container", "true");
        subtreerow.appendChild(subtreecell);
        subtreeitem.appendChild(subtreerow);
        gDialog.sheetsTreechildren.appendChild(subtreeitem);

        newSheetOwnerNode = getCurrentEditor().document.createElement("link");
        newSheetOwnerNode.setAttribute("type", "text/css");
        newSheetOwnerNode.setAttribute("href", gDialog.newURL);
        if (gDialog.newAlternate) {
          newSheetOwnerNode.setAttribute("rel", "alternate stylesheet");
        }
        else {
          newSheetOwnerNode.setAttribute("rel", "stylesheet");
        }
        if (gDialog.newMediaList != "") {
          newSheetOwnerNode.setAttribute("media", gDialog.newMediaList);
        }
        if (gDialog.newTitle != "") {
          newSheetOwnerNode.setAttribute("title", gDialog.newTitle);
        }
        headNode = GetHeadElement();
        headNode.appendChild(newSheetOwnerNode);

        subtreecell.setAttribute("label", gDialog.newURL);
        external = true;
        if ( /(\w*):.*/.test(gDialog.newURL) ) {
          if (RegExp.$1 == "file") {
            external = false;
          }
        }
        if (external)
          subtreecell.setAttribute("properties", "external");

        if (!newSheetOwnerNode.sheet) {
          /* hack due to asynchronous load of external stylesheet */        
          var o = newObject( subtreeitem, external, OWNER_NODE, newSheetOwnerNode, false, 0 );
          PushInObjectsArray(o)
          if (gAsyncLoadingTimerID)
            clearTimeout(gAsyncLoadingTimerID);
          sheetIndex = objectsArray.length - 1;
          
          gAsyncLoadingTimerID = setTimeout("sheetLoadedTimeoutCallback(" + sheetIndex + ")", kAsyncTimeout);
        }
        else {
          o = newObject( subtreeitem, external, SHEET, newSheetOwnerNode.sheet, false, 0 );
          PushInObjectsArray(o)
          AddRulesToTreechildren(subtreeitem, newSheetOwnerNode.sheet.cssRules, external, 1);
        }
      }
      else if (!gDialog.newExternal) {
        newSheetOwnerNode = getCurrentEditor().document.createElement("style");
        newSheetOwnerNode.setAttribute("type", "text/css");
        if (gDialog.newMediaList != "") {
          newSheetOwnerNode.setAttribute("media", gDialog.newMediaList);
        }
        if (gDialog.newTitle != "") {
          newSheetOwnerNode.setAttribute("title", gDialog.newTitle);
        }
        headNode = GetHeadElement();
        headNode.appendChild(newSheetOwnerNode);
        AddSheetEntryToTree(gDialog.sheetsTreechildren, newSheetOwnerNode);

        selectTreeItem(objectsArray[objectsArray.length - 1].xulElt);
      }
      selectTreeItem(subtreeitem);
      break;
  }
}

// * we need that to refresh the tree after async sheet load
//   param integer index
function sheetLoadedTimeoutCallback(index)
{
  var subtreeitem = objectsArray[index].xulElt;
  gInsertIndex = index+1;
  ClearTreeSelection(gDialog.sheetsTree);
  if (objectsArray[index].type == OWNER_NODE && objectsArray[index].cssElt.sheet != null) {
    var sheet = objectsArray[index].cssElt.sheet;
    AddRulesToTreechildren(subtreeitem , sheet.cssRules, objectsArray[index].external,
                           objectsArray[index].depth+1);
    objectsArray[index].type = SHEET;
    objectsArray[index].cssElt = sheet;
  }
  else if (objectsArray[index].type == IMPORT_RULE && objectsArray[index].cssElt.styleSheet != null) {
    AddRulesToTreechildren(subtreeitem , objectsArray[index].cssElt.styleSheet.cssRules, true,
                           objectsArray[index].depth+1)
  }
  else
    return;
  selectTreeItem(subtreeitem);
}

// * gets the object's index corresponding to an entry in the tree
//   param XULElement object
//   return integer
function FindObjectIndexInObjectsArray(object)
{
  var i, l = objectsArray.length;
  for (i=0; i<l; i++)
    if (objectsArray[i].cssElt == object)
      return i;
  return -1;
}

// * removes the selected entry from the tree and deletes the corresponding
//   object ; does some magic if we remove a container like a stylesheet or
//   an @import rule to remove all the contained rules
function RemoveObject()
{
  GetSelectedItemData();
  var objectIndex = gDialog.selectedIndex;
  if (objectIndex == -1) return;
  var depth = gDialog.depthObject;

  var ruleIndex, i, ruleIndexInTree, toSplice;

  switch (gDialog.selectedType) {
    case SHEET:
      var ownerNode = gDialog.selectedObject.ownerNode;
      ownerNode.parentNode.removeChild(ownerNode);

      for (i=objectIndex+1; i<objectsArray.length && objectsArray[i].depth > depth; i++);
      toSplice = i - objectIndex;
      break;

    case IMPORT_RULE:
    case MEDIA_RULE:
      for (ruleIndexInTree=objectIndex-1; objectsArray[ruleIndexInTree].depth >= depth; ruleIndexInTree--);

      objectsArray[ruleIndexInTree].modified = true;
      ruleIndex = getRuleIndexInRulesList(gDialog.selectedObject.parentStyleSheet.cssRules,
                                          gDialog.selectedObject);
      if (ruleIndex != -1) {
        gDialog.selectedObject.parentStyleSheet.deleteRule(ruleIndex);
      }
      for (i=objectIndex+1; i<objectsArray.length && objectsArray[i].depth > depth; i++);
      toSplice = i - objectIndex;
      break;

    case STYLE_RULE:
      for (ruleIndexInTree=objectIndex-1; objectsArray[ruleIndexInTree].depth; ruleIndexInTree--);
      objectsArray[ruleIndexInTree].modified = true;
      if (gDialog.selectedObject.parentRule) {
        /* this style rule is contained in an at-rule */
        /* need to remove the rule only from the at-rule listing it */
        ruleIndex = getRuleIndexInRulesList(gDialog.selectedObject.parentRule.cssRules,
                                            gDialog.selectedObject);
        if (ruleIndex != -1) {
          gDialog.selectedObject.parentRule.deleteRule(ruleIndex);
        }
      }
      else {
        ruleIndex = getRuleIndexInRulesList(gDialog.selectedObject.parentStyleSheet.cssRules,
                                            gDialog.selectedObject);
        if (ruleIndex != -1) {
          gDialog.selectedObject.parentStyleSheet.deleteRule(ruleIndex);
        }
      }
      toSplice = 1;
      break;
  }
  // let's remove the whole treeitem
  gDialog.treeItem.parentNode.removeChild(gDialog.treeItem);
  // and then remove the objects from our array
  objectsArray.splice(objectIndex, toSplice);
  // can we select an item ?
  if (objectsArray.length)
    selectTreeItem(objectsArray[Math.min(objectIndex, objectsArray.length - 1)].xulElt);
}

// * moves a sheet/rule up in the tree
function MoveObjectUp()
{
  GetSelectedItemData();
  var index = gDialog.selectedIndex;
  if (index <= 0) return;
  var sheetIndex = GetSheetContainer();
  
  switch (gDialog.selectedType) {
  case SHEET:
    var ownerNode = gDialog.selectedObject.ownerNode;
    ClearTreeSelection(gDialog.sheetsTree)
    index--;
    while (index && objectsArray[index].type != SHEET)
      index--;
    if (index == -1) return;
    ownerNode.parentNode.insertBefore(ownerNode, objectsArray[index].cssElt.ownerNode);
    Restart();
    selectTreeItem(objectsArray[index].xulElt);
    gDialog.modified = true;
    break;

  case OWNER_NODE:
    ownerNode = gDialog.selectedObject;
    ClearTreeSelection(gDialog.sheetsTree)
    index--;
    while (index && objectsArray[index].type != SHEET)
      index--;
    if (index == -1) return;
    ownerNode.parentNode.insertBefore(ownerNode, objectsArray[index].cssElt.ownerNode);
    Restart();
    selectTreeItem(objectsArray[index].xulElt);
    gDialog.modified = true;
    break;

  case STYLE_RULE:
  case PAGE_RULE:
    var rule = gDialog.selectedObject;
    objectsArray[sheetIndex].modified = true;

    ClearTreeSelection(gDialog.sheetsTree)
    var ruleText = rule.cssText;
    var subtreeitem;
    var newRule;
    if (rule.parentRule) {
      var ruleIndex = getRuleIndexInRulesList(rule.parentRule.cssRules, rule);
      var parentRule = rule.parentRule;
      
      if (ruleIndex == -1) return;

      if (!ruleIndex) {
        // we have to move the rule just before its parent rule
        parentRule.deleteRule(0);
        var parentRuleIndex;

        parentRuleIndex = getRuleIndexInRulesList(parentRule.parentStyleSheet.cssRules, parentRule);
        parentRule.parentStyleSheet.insertRule(ruleText, parentRuleIndex);
        newRule = parentRule.parentStyleSheet.cssRules[parentRuleIndex];
      }
      else {
        // we just move the rule in its parentRule
        parentRule.deleteRule(ruleIndex);
        parentRule.insertRule(ruleText, ruleIndex - 1);
        newRule = parentRule.cssRules.item(ruleIndex - 1);
      }
      // remove the tree entry
      objectsArray[index].xulElt.parentNode.removeChild(objectsArray[index].xulElt);
      // delete the object
      objectsArray.splice(index, 1);
      // position the insertion index
      gInsertIndex = index - 1;
      subtreeitem =  AddStyleRuleToTreeChildren(newRule,
                                                objectsArray[index-1].external,
                                                objectsArray[index-1].depth);
      // make the new tree entry
      objectsArray[index].xulElt.parentNode.insertBefore(subtreeitem,
                                                           objectsArray[index].xulElt);
      selectTreeItem(subtreeitem);
        
    }
    else {
      // standard case, the parent of the rule is the stylesheet itself
      ruleIndex = getRuleIndexInRulesList(rule.parentStyleSheet.cssRules, rule);
      var refStyleSheet = rule.parentStyleSheet;
      if (ruleIndex == -1) return;
      if (ruleIndex) {
        // we just move the rule in the sheet
        refStyleSheet.deleteRule(ruleIndex);
        var targetRule = refStyleSheet.cssRules.item(ruleIndex - 1);

        if (targetRule.type == CSSRule.MEDIA_RULE) {
          targetRule.insertRule(ruleText, targetRule.cssRules.length);
          var targetRuleIndex = FindObjectIndexInObjectsArray(targetRule);
          newRule = targetRule.cssRules.item(targetRule.cssRules.length - 1);
          var subtreechildren = GetSubtreeChildren(objectsArray[targetRuleIndex].xulElt);

          // in this case, the target treeitem is at same location but one level deeper
          // remove the tree entry
          objectsArray[index].xulElt.parentNode.removeChild(objectsArray[index].xulElt);
          // delete the object
          objectsArray.splice(index, 1);
          // position the insertion index
          gInsertIndex = index;
          subtreeitem = AddStyleRuleToTreeChildren(newRule,
                                                   objectsArray[targetRuleIndex].external,
                                                   objectsArray[targetRuleIndex].depth + 1);
          // make the new tree entry
          subtreechildren.appendChild(subtreeitem);
          selectTreeItem(subtreeitem);
          return;
        }
        else if (targetRule.type != CSSRule.IMPORT_RULE &&
                 targetRule.type != CSSRule.CHARSET_RULE) {
          // we can move the rule before its predecessor only if that one is
          // not an @import rule nor an @charset rule
          refStyleSheet.insertRule(ruleText, ruleIndex - 1);
          newRule = refStyleSheet.cssRules[ruleIndex - 1];
          // remove the tree entry
          objectsArray[index].xulElt.parentNode.removeChild(objectsArray[index].xulElt);
          // delete the object
          objectsArray.splice(index, 1);
          // position the insertion index
          gInsertIndex = index - 1;
          subtreeitem =  AddStyleRuleToTreeChildren(newRule,
                                                    objectsArray[index-1].external,
                                                    objectsArray[index-1].depth);
          // make the new tree entry
          objectsArray[index].xulElt.parentNode.insertBefore(subtreeitem,
                                                             objectsArray[index].xulElt);
          selectTreeItem(subtreeitem);
          return;
        }
      }
      // At this point, we need to move the rule from one sheet to another one, if it
      // exists...
      // First, let's if there is another candidate stylesheet before the current one
      // for the style rule's ownership
      var refSheetIndex = FindObjectIndexInObjectsArray(refStyleSheet);
      sheetIndex = refSheetIndex;

      if (!sheetIndex) return; // early way out
      sheetIndex--;
      while (sheetIndex && (objectsArray[sheetIndex].type != SHEET ||
                            objectsArray[sheetIndex])) {
        sheetIndex--;
      }
      if (sheetIndex == -1) return; // no embedded or local stylesheet available
      var newStyleSheet = objectsArray[sheetIndex].cssElt;
      // we need to check the type of the last rule in the sheet, if it exists
      if (newStyleSheet.cssRules.length &&
          newStyleSheet.cssRules[newStyleSheet.cssRules.length - 1].type == CSSRule.MEDIA_RULE) {
        // this media rule is our target
        var refMediaRule      = newStyleSheet.cssRules[newStyleSheet.cssRules.length - 1];
        var refMediaRuleIndex = FindObjectIndexInObjectsArray(refMediaRule );
        var refRuleIndex      = refMediaRuleIndex++;
        while (refRuleIndex < objectsArray.length && objectsArray[refRuleIndex].depth > 1)
          refRuleIndex++;
        // refRuleIndex represents where we should insert the new object
      }
      else {
        // we just append the rule to the list of rules of the sheet
      }
    }
    break;
  }
}

// * moves a sheet/rule down in the tree
function MoveObjectDown()
{
  /* NOT YET IMPLEMENTED */
  var objectIndex = FindObjectIndexInObjectsArray(gDialog.selectedObject);
  if (objectIndex == -1) return;
}

// * opens a file picker and returns the file:/// URL in gDialog.newURL
function onChooseLocalFile() {
  // Get a local file, converted into URL format
  var fileName = getLocalFileURL(true);
  if (fileName)
  {
    gDialog.URLtextbox.setAttribute("value", fileName);
    gDialog.newURL = fileName;
  }
}

// * opens a file picker for a *.css filename, exports the selected stylesheet
//   in that file, and replace the selected embedded sheet by its external, but
//   local to the filesystem, new counterpart
function onExportStylesheet() {
  var fileName = getLocalFileURL(false);
  SerializeExternalSheet(gDialog.selectedObject, fileName);
  var ownerNode = gDialog.selectedObject.ownerNode;
  var newSheetOwnerNode = ownerNode.ownerDocument.createElement("link");
  newSheetOwnerNode.setAttribute("type", "text/css");
  newSheetOwnerNode.setAttribute("href", fileName);
  newSheetOwnerNode.setAttribute("rel", "stylesheet");
  var mediaList = ownerNode.getAttribute("media");
  if (mediaList && mediaList != "")
    newSheetOwnerNode.setAttribute("media", mediaList);
  ownerNode.parentNode.insertBefore(newSheetOwnerNode, ownerNode);
  ownerNode.parentNode.removeChild(ownerNode);

  // we still have to wait for async sheet loading's completion
  if (gAsyncLoadingTimerID)
    clearTimeout(gAsyncLoadingTimerID);
  gAsyncLoadingTimerID = setTimeout("Refresh()", 500);
}

function getCurrentEditor() {
  if (InitEditorShell)
    return editorShell.editor;
  else
    return GetCurrentEditor();
}

function AddCSSLevelChoice(rows)
{
  var labelLabel = document.createElementNS(XUL_NS, "label");
  var row = document.createElementNS(XUL_NS, "row");
  row.setAttribute("align", "center");
  labelLabel.setAttribute("value", "CSS Level");
  
  row.appendChild(labelLabel);

  var level;
  for (level = 1; level < 4; level++) {
    var levelCheckbox = document.createElementNS(XUL_NS, "checkbox");
    levelCheckbox.setAttribute("label", level);
    row.appendChild(levelCheckbox);
  }

  rows.appendChild(row);
}
