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

var objectsArray = null;
var gTimerID;
var gAsyncLoadingTimerID;

const SHEET        = 1;
const STYLE_RULE   = 2;
const IMPORT_RULE  = 3;
const MEDIA_RULE   = 4;
const CHARSET_RULE = 5;
const PAGE_RULE    = 6;
const OWNER_NODE   = 7;

const GENERAL_TAB    = 1;
const TEXT_TAB       = 2;
const BACKGROUND_TAB = 3;
const BORDER_TAB     = 4;
const BOX_TAB        = 5;
const AURAL_TAB      = 6;

const GENERIC_SELECTOR      = 0;
const TYPE_ELEMENT_SELECTOR = 1;
const CLASS_SELECTOR        = 2;

const kUrlString = "url(";

var gHaveDocumentUrl = false;
var predefSelector = "";

// * Debug only...
function doDump(text, value) {
  dump("===> " + text + " : " + value + "\n");
}

// * dialog initialization code
function Startup()
{
  // let's just check we have an editor... We don't store it
  // because we prefer calling GetCurrentEditor() every time
  if (!GetCurrentEditor()) {
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

  gDialog.selectedTab = GENERAL_TAB;
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
function FlushChanges()
{
  if (gDialog.modified) {
    // let's make sure the editor is going to require save on exit
    GetCurrentEditor().incrementModificationCount(1);
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
  gDialog.sheetsTree.treeBoxObject.selection.clearSelection();

  var elt = sheetsTreeChildren.firstChild;
  while (elt) {
    var tmp = elt.nextSibling;
    sheetsTreeChildren.removeChild(elt);
    elt = tmp;
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
  var o;
  if ( headNode && headNode.hasChildNodes() ) {
    var ssn = headNode.childNodes.length;
    objectsArray = new Array();
    if (ssn) {
      var i;
      for (i=0; i<ssn; i++) {
        var ownerNode = headNode.childNodes[i];
        if (ownerNode.nodeType == Node.ELEMENT_NODE) {
          var ownerTag  = ownerNode.nodeName.toLowerCase()
          var relType   = ownerNode.getAttribute("rel");
          if (ownerTag == "style" ||
              (ownerTag == "link" && 
               (relType == "stylesheet" || relType == "alternate stylesheet"))) {

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
            o = newObject( treeitem, external, SHEET, ownerNode.sheet, false, 0 );
            objectsArray.push(o);

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
          subtreeitem  = document.createElementNS(XUL_NS, "treeitem");
          subtreerow   = document.createElementNS(XUL_NS, "treerow");
          subtreecell  = document.createElementNS(XUL_NS, "treecell");
          // show the selector attached to the rule
          subtreecell.setAttribute("label", rules[j].selectorText);
          o = newObject( subtreeitem, external, STYLE_RULE, rules[j], false, depth );
          objectsArray.push(o);
          if (external) {
            subtreecell.setAttribute("properties", "external");
          }
          subtreerow.appendChild(subtreecell);
          subtreeitem.appendChild(subtreerow);
          subtreechildren.appendChild(subtreeitem);
          break;
        case CSSRule.IMPORT_RULE:
          // this is CSSImportRule for @import
          subtreeitem  = document.createElementNS(XUL_NS, "treeitem");
          subtreerow   = document.createElementNS(XUL_NS, "treerow");
          subtreecell  = document.createElementNS(XUL_NS, "treecell");
          // show "@import" and the URL
          subtreecell.setAttribute("label", "@import "+rules[j].href, external);
          o = newObject( subtreeitem, external, IMPORT_RULE, rules[j], false, depth );
          objectsArray.push(o);
          if (external) {
            subtreecell.setAttribute("properties", "external");
          }
          subtreerow.appendChild(subtreecell);
          subtreeitem.appendChild(subtreerow);
          subtreeitem.setAttribute("container", "true");
          if (rules[j].styleSheet)
            // if we have a stylesheet really imported, let's browse it too
            // increasing the depth and marking external
            AddRulesToTreechildren(subtreeitem, rules[j].styleSheet.cssRules, true, depth+1);
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
          objectsArray.push(o);
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
          objectsArray.push(o);
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

// * selects either a tree item (sheet, rule) or a tab
//   in the former case, the parameter is null ; in the latter,
//   the parameter is a string containing the name of the selected tab
//   param String tab
function onSelectCSSTreeItem(tab)
{
  // convert the tab string into a tab id
  if      (tab == "general")     tab = GENERAL_TAB;
  else if (tab == "text")        tab = TEXT_TAB;
  else if (tab == "background")  tab = BACKGROUND_TAB;
  else if (tab == "border")      tab = BORDER_TAB;
  else if (tab == "box")         tab = BOX_TAB;
  else if (tab == "aural")       tab = AURAL_TAB;

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
  if (gDialog.selectedIndex == -1) {
    // there is no tree item selected, let's fallback to the Info tab
    // but there is nothing we can display in that tab...
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
        Restart();
        selectTreeItem(objectsArray[index].xulElt);
        onSelectCSSTreeItem(tab);
        return;
      }
      break;
    case SHEET:
      var alternate = "";
      if (external &&
          cssObject.ownerNode.getAttribute("rel").toLowerCase() == "alternate stylesheet") {
        alternate = " (alternate)";
      }
      gDialog.sheetInfoTabPanelTitle.setAttribute("value", "Stylesheet"+alternate);
      AddLabelToInfobox(gridrows, "Type:", cssObject.type, false);
      AddCheckboxToInfobox(gridrows, "Disabled:", "check to disable stylesheet (cannot be saved)",
                           cssObject.disabled, "onStylesheetDisabledChange");
      var href;
      if (cssObject.ownerNode.nodeName.toLowerCase() == "link") {
        href = cssObject.href;
      }
      else {
        href = "none (embedded into the document)";
      }
      AddLabelToInfobox(gridrows, "URL:", href, false);
      var mediaList = cssObject.media.mediaText;
      if (!mediaList || mediaList == "") {
        mediaList = "all";
      }
      AddEditableZoneToInfobox(gridrows, "Media list:", mediaList, "onStylesheetMediaChange", false);
      if (cssObject.title) {
        AddEditableZoneToInfobox(gridrows, "Title:", cssObject.title, "onStylesheetTitleChange", false);
      }
      if (cssObject.cssRules) {
        AddLabelToInfobox(gridrows, "Number of rules:", cssObject.cssRules.length, false);
      }
      if (!external && cssObject.ownerNode.nodeName.toLowerCase() == "style")
        // we can export only embedded stylesheets
        AddSingleButtonToInfobox(gridrows, "Export stylesheet and switch to exported version", "onExportStylesheet")
      break;
    case STYLE_RULE:
      var h = document.defaultView.getComputedStyle(grid.parentNode, "").getPropertyValue("height");
      // let's prepare the grid for the case the rule has a laaaaarge number
      // of property declarations
      grid.setAttribute("style", "overflow: auto; height: "+h);
      gDialog.sheetInfoTabPanelTitle.setAttribute("value", "Style rule");

      AddLabelToInfobox(gridrows, "Selector:", cssObject.selectorText, false);
      if (cssObject.style.length) {
        AddDeclarationToInfobox(gridrows, "Declarations:", GetDeclarationText(cssObject, 0),
                                GetDeclarationImportance(cssObject, 0), false);
        for (i=1; i<cssObject.style.length; i++) {
          AddDeclarationToInfobox(gridrows, "", GetDeclarationText(cssObject, i),
                                  GetDeclarationImportance(cssObject, i), false);
        }
      }
      // TO BE DONE : we need a way of changing the importance of a given declaration
      break;
    case MEDIA_RULE:
      gDialog.sheetInfoTabPanelTitle.setAttribute("value", "Media rule");
      AddLabelToInfobox(gridrows, "Media list:", cssObject.media.mediaText, false);
      AddLabelToInfobox(gridrows, "Number of declarations:", cssObject.cssRules.length, false);
      break;
    case IMPORT_RULE:
      gDialog.sheetInfoTabPanelTitle.setAttribute("value", "Import rule");
      AddLabelToInfobox(gridrows, "URL:", cssObject.href, false);
      AddLabelToInfobox(gridrows, "Media list:", cssObject.media.mediaText, false);
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
function AddLabelToInfobox(rows, label, value, strong)
{
  var labelLabel = document.createElementNS(XUL_NS, "label");
  var row = document.createElementNS(XUL_NS, "row");
  row.setAttribute("align", "center");
  labelLabel.setAttribute("value", label);
  if (strong) {
    labelLabel.setAttribute("class", "titleLabel");
  }
  row.appendChild(labelLabel);

  if (value) {
    var valueLabel = document.createElementNS(XUL_NS, "label");
    valueLabel.setAttribute("value", value);
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
function AddDeclarationToInfobox(rows, label, value, importance)
{
  var labelLabel = document.createElementNS(XUL_NS, "label");
  var row = document.createElementNS(XUL_NS, "row");
  row.setAttribute("align", "center");
  labelLabel.setAttribute("value", label);
  row.appendChild(labelLabel);

  var valueLabel = document.createElementNS(XUL_NS, "label");
  valueLabel.setAttribute("value", value);
  row.appendChild(valueLabel);

  var importanceLabel = document.createElementNS(XUL_NS, "label");
  importanceLabel.setAttribute("value", importance);
  if (importance == "!important") {
    importanceLabel.setAttribute("class", "bangImportant");
  }
  row.appendChild(importanceLabel);

  rows.appendChild(row);
}

// * retrieves the index of the selected entry in a tree
//   param XULElement tree
//   return integer
function getSelectedItem(tree)
{
  if (tree.treeBoxObject.selection.count == 1)
    return tree.contentView.getItemAtIndex(tree.currentIndex);
  else
    return null;
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

// * retrieves the importance of the index-nth style declaration in a rule
//   param DOMCSSRule styleRule
//   param integer index
//   return String
function GetDeclarationImportance(styleRule, index)
{
  var pName = styleRule.style.item(index);
  return styleRule.style.getPropertyPriority(pName);
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
    gDialog.newSelectorType = GENERIC_SELECTOR;
  }
  else {
    gDialog.newSelectorType = CLASS_SELECTOR;
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
  AddLabelToInfobox(gridrows, "Type:", "text/css", false);
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
  AddLabelToInfobox(gridrows, "Type:", "text/css", false);
  // alternate stylesheet ?
  AddCheckboxToInfobox(gridrows, "Alternate:", "check to create alternate stylesheet", gDialog.newAlternate,
                       "onNewAlternateChange");
  gDialog.URLtextbox = AddEditableZoneToInfobox(gridrows, "URL:",        gDialog.newURL,    "onNewURLChange", true);
  AddSingleButtonToInfobox(gridrows, "Choose file", "onChooseLocalFile")

  AddEditableZoneToInfobox(gridrows, "Media list:", gDialog.newMediaList, "onNewMediaListChange", false);
  AddEditableZoneToInfobox(gridrows, "Title:",      gDialog.newTitle,  "onNewTitleChange", false);

  AddSingleButtonToInfobox(gridrows, "Create Stylesheet", "onConfirmCreateNewObject")

  // the two following labels are unfortunately useful...
  AddLabelToInfobox(gridrows, "", "(Warning : save document *before* attaching local stylesheet)", false);
  AddLabelToInfobox(gridrows, "", "(use Refresh button if stylesheet is not immediately downloaded)", false);
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
  var containerIndex;
  var cssObject;
  var l;
  var ruleIndex;
  var newSheetOwnerNode;
  var headNode;
  switch (gDialog.newType) {
    case STYLE_RULE:
      if (gDialog.newSelector != "") {
        containerIndex = gDialog.selectedIndex;
        while (objectsArray[containerIndex].type != SHEET &&
               objectsArray[containerIndex].type != MEDIA_RULE)
          containerIndex--;
        cssObject = objectsArray[containerIndex].cssElt;
        l = cssObject.cssRules.length;
        switch (gDialog.newSelectorType) {
          case TYPE_ELEMENT_SELECTOR:
          case GENERIC_SELECTOR:
            selector = gDialog.newSelector;
            break;
          case CLASS_SELECTOR:
            selector = "." + gDialog.newSelector;
            break;
        }
        cssObject.insertRule(selector + " { }", l);
        /* find inserted rule's index in objectsArray */
        var depth = objectsArray[containerIndex].depth;
        ruleIndex = containerIndex + 1;
        while (ruleIndex < objectsArray.length &&
               objectsArray[ruleIndex].depth > depth) {
          ruleIndex++;
        }
        Restart();
        selectTreeItem(objectsArray[ruleIndex].xulElt);
        objectsArray[containerIndex].modified = true;
      }
      break;
    case IMPORT_RULE:
      if (gDialog.newURL != "") {
        containerIndex = gDialog.selectedIndex;
        while (objectsArray[containerIndex].type != SHEET &&
               objectsArray[containerIndex].type != MEDIA_RULE)
          containerIndex--;
        cssObject = objectsArray[containerIndex].cssElt;
        l = cssObject.cssRules.length;
        ruleIndex = containerIndex + 1;
        while (ruleIndex < objectsArray.length &&
               (objectsArray[ruleIndex].type == CHARSET_RULE ||
                objectsArray[ruleIndex].type == IMPORT_RULE))
          ruleIndex++;
        cssObject.insertRule('@import url("'+ gDialog.newURL + '") ' + gDialog.newMediaList + ";",
                             ruleIndex - containerIndex - 1);
        doDump("RULEINDEX", ruleIndex);
        doDump("OBJECTSARRAY", objectsArray.length);
        Restart();
        doDump("OBJECTSARRAY", objectsArray.length);
        selectTreeItem(objectsArray[ruleIndex].xulElt);
        SetModifiedFlagOnStylesheet();
      }
      break;
    case SHEET:
      if (gDialog.newExternal && gDialog.newURL != "") {
        newSheetOwnerNode = GetCurrentEditor().document.createElement("link");
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

        if (!newSheetOwnerNode.sheet) {
          /* hack due to asynchronous load of external stylesheet */        
          var subtreeitem  = document.createElementNS(XUL_NS, "treeitem");
          var subtreerow   = document.createElementNS(XUL_NS, "treerow");
          var subtreecell  = document.createElementNS(XUL_NS, "treecell");
          subtreecell.setAttribute("label", gDialog.newURL);
          var external = true;
          if ( / (\w*):.* /.test(gDialog.newURL) ) {
            if (RegExp.$1 != "file") {
              external = false;
            }
          }
          var o = newObject( subtreeitem, external, OWNER_NODE, newSheetOwnerNode, false, 0 );
          objectsArray.push(o);
          subtreeitem.setAttribute("container", "true");
          subtreecell.setAttribute("properties", "external");
          subtreerow.appendChild(subtreecell);
          subtreeitem.appendChild(subtreerow);
          gDialog.sheetsTreechildren.appendChild(subtreeitem);
          if (gAsyncLoadingTimerID)
            clearTimeout(gAsyncLoadingTimerID);
          var sheetIndex = objectsArray.length - 1;
          gAsyncLoadingTimerID = setTimeout("sheetLoadedTimeoutCallback(" + sheetIndex + ")", 500);
        }
        else {
          var index = objectsArray.length;
          Restart();
          selectTreeItem(objectsArray[index].xulElt);
          objectsArray[index].modified = true;
        }
      }
      else if (!gDialog.newExternal) {
        newSheetOwnerNode = GetCurrentEditor().document.createElement("style");
        newSheetOwnerNode.setAttribute("type", "text/css");
        if (gDialog.newMediaList != "") {
          newSheetOwnerNode.setAttribute("media", gDialog.newMediaList);
        }
        if (gDialog.newTitle != "") {
          newSheetOwnerNode.setAttribute("title", gDialog.newTitle);
        }
        headNode = GetHeadElement();
        headNode.appendChild(newSheetOwnerNode);
        FlushChanges();
        Restart();
        selectTreeItem(objectsArray[objectsArray.length - 1].xulElt);
      }
      break;
  }
}

// * we need that to refresh the tree after async sheet load
//   param integer index
function sheetLoadedTimeoutCallback(index) {
  if (objectsArray[index].cssElt.sheet != null) {
    Restart();
    selectTreeItem(objectsArray[index].xulElt);
    objectsArray[index].modified = true;
  }
}

// * selects a entry in the tree
//   param XULElement aItem
function selectTreeItem(aItem)
{
  /* first make sure item's containers are open */
  var tmp = aItem.parentNode;
  while (tmp && tmp.nodeName != "tree") {
    if (tmp.nodeName == "treeitem")
      tmp.setAttribute("open", "true");
    tmp = tmp.parentNode;
  }

  /* then select the item */
  var itemIndex = gDialog.sheetsTree.contentView.getIndexOfItem(aItem);
  gDialog.sheetsTree.treeBoxObject.selection.select(itemIndex);
  /* and make sure it is visible in the clipping area of the tree */
  gDialog.sheetsTree.treeBoxObject.ensureRowIsVisible(itemIndex);
}

// * gets a rule's index in its DOMCSSRuleList container
//   param DOMCSSRuleList rulelist
//   param DOMCSSRule rule
//   return integer
function getRuleIndexInRulesList(rulelist, rule)
{
  if (!rule || !rulelist)
    return -1;

  var i;
  for (i=0; i<rulelist.length; i++) {
    if (rulelist.item(i) == rule) {
      return i;
    }
  }
  return -1;
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
  var objectIndex = FindObjectIndexInObjectsArray(gDialog.selectedObject);
  if (objectIndex == -1) return;

  var ruleIndex;
  switch (gDialog.selectedType) {
    case SHEET:
      var ownerNode = gDialog.selectedObject.ownerNode;
      ownerNode.parentNode.removeChild(ownerNode);
      Restart();
      if (objectIndex < objectsArray.length )
        selectTreeItem(objectsArray[objectIndex].xulElt);
      break;
    case IMPORT_RULE:
      var ruleIndexInTree = FindObjectIndexInObjectsArray(gDialog.selectedObject.parentStyleSheet);
      objectsArray[ruleIndexInTree].modified = true;
      ruleIndex = getRuleIndexInRulesList(gDialog.selectedObject.parentStyleSheet.cssRules,
                                          gDialog.selectedObject);
      if (ruleIndex != -1) {
        gDialog.selectedObject.parentStyleSheet.deleteRule(ruleIndex);
      }
      Restart();
      objectsArray[ruleIndexInTree].modified = true;
      if (objectIndex < objectsArray.length )
        selectTreeItem(objectsArray[objectIndex].xulElt);
      break;
    case STYLE_RULE:
      ruleIndexInTree = FindObjectIndexInObjectsArray(gDialog.selectedObject.parentStyleSheet);
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
      objectsArray[objectIndex].xulElt.parentNode.removeChild(objectsArray[objectIndex].xulElt);
      objectsArray.splice(objectIndex, 1);
      selectTreeItem(objectsArray[Math.min(objectIndex, objectsArray.length - 1)].xulElt);
      break;
  }
}

// * moves a sheet/rule up in the tree
function MoveObjectUp()
{
  /* NOT YET IMPLEMENTED */
  var objectIndex = FindObjectIndexInObjectsArray(gDialog.selectedObject);
  if (objectIndex == -1) return;
  doDump("position", objectIndex);
}

// * moves a sheet/rule down in the tree
function MoveObjectDown()
{
  /* NOT YET IMPLEMENTED */
  var objectIndex = FindObjectIndexInObjectsArray(gDialog.selectedObject);
  if (objectIndex == -1) return;
  doDump("position", objectIndex);
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

// * ok, now let's try if the exported sheet is here
//   param integer index
function sheetExportedTimeoutCallback(index) {
  if (objectsArray[index].cssElt.sheet != null) {
    // yep, found it ! Let's update everything
    Restart();
    selectTreeItem(objectsArray[index].xulElt);
    objectsArray[index].modified = true;
  }
}

// * opens a file picker and returns a file: URL. If openOnly is true,
//   the filepicker's mode is "open"; it is "save" instead and that allows
//   to pick new filenames
//   param boolean openOnly
function getLocalFileURL(openOnly)
{
  var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
  var fileType = "*";
  if (openOnly)
    fp.init(window, "Select a CSS file", nsIFilePicker.modeOpen);
  else
    fp.init(window, "Select a CSS file", nsIFilePicker.modeSave);
  
  // Default or last filter is "All Files"
  fp.appendFilters(nsIFilePicker.filterAll);

  // set the file picker's current directory to last-opened location saved in prefs
  SetFilePickerDirectory(fp, fileType);

  /* doesn't handle *.shtml files */
  try {
    var ret = fp.show();
    if (ret == nsIFilePicker.returnCancel)
      return null;
  }
  catch (ex) {
    dump("filePicker.chooseInputFile threw an exception\n");
    return null;
  }
  // SaveFilePickerDirectory(fp, fileType);
  
  var ioService = GetIOService();
  return fp.file ? ioService.getURLSpecFromFile(fp.file) : null;
}
