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

var gTimerID;

const styleStr  = "style";

var textColor, backgroundColor;
var gHaveDocumentUrl = false;
var predefSelector = "";

var gPanel         = "";
var gOriginalStyle = "";

// const COMPATIBILITY_TAB = 1;
const GENERAL_TAB       = 2;
const TEXT_TAB          = 3;
const BACKGROUND_TAB    = 4;
const BORDER_TAB        = 5;
const BOX_TAB           = 6;
const AURAL_TAB         = 7;

function doDump(text, value) {
  dump("===> " + text + " : " + value + "\n");
}

// dialog initialization code
function Startup()
{
  // are we in a pre-1.3 Mozilla ?
  if (typeof window.InitEditorShell == "function") {
    // yes, so let's get an editorshell
    if (!InitEditorShell())
      return;
  }
  else if (typeof window.GetCurrentEditor != "function" || !GetCurrentEditor()) {
    window.close();
    return;
  }

  var title                     = "Styles";
  gDialog.selectionBased        = false;
  gDialog.isSelectionCollapsed  = false;
  gDialog.selectionInSingleLine = false;

  // if a title was provided at window creation, let's use it
  if ("arguments" in window && window.arguments.length >= 3) {
    gPanel       = window.arguments[0];
    title        = window.arguments[1];
    gDialog.selectionBased = window.arguments[2];
  }
  var w = document.getElementById("specificCssPropsWindow");
  if (!w || !gPanel) // we should never hit this but let's be paranoid...
    return;
  w.setAttribute("title", title);

  // gDialog is declared in EdDialogCommon.js
  // Set commonly-used widgets like this:
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

  gDialog.modified = false;
  gDialog.selectedIndex = -1;

  var elt;
  if (gDialog.selectionBased)
  {
    var editor = GetCurrentEditor();
    var selection = editor.selection;
    gDialog.isSelectionCollapsed = selection.isCollapsed;
    if (gDialog.isSelectionCollapsed) {
      elt = selection.focusNode;
      while (!editor.nodeIsBlock(elt))
        elt = elt.parentNode;
    }
    else
    {
      var range = selection.getRangeAt(0);
      var startNode = range.startContainer;
      var startOffset = range.startOffset;
      if (startNode.nodeType != Node.TEXT_NODE)
        elt = startNode.childNodes.item(startOffset);
      else
        elt = startNode;
    }
  }
  else
  {
    elt = window.opener.gContextMenuFiringDocumentElement;
  }

  if (!elt) return;

  if (elt.nodeType == Node.TEXT_NODE)
    elt = elt.parentNode;

  if (gDialog.selectionBased &&
      gDialog.isSelectionCollapsed &&
      elt.nodeName.toLowerCase() == "body")
  {
    // the selection is collapsed in a line directly contained in the BODY
    gDialog.selectionInSingleLine = true;
    elt = GetCurrentEditor().document.createElement("div");
  }

  gDialog.selectedObject = elt;
  gDialog.selectedHasStyle = elt.hasAttribute("style");
  if (gDialog.selectedHasStyle)
    gDialog.selectedStyle = elt.getAttribute("style");
  gDialog.selectedSheet  = null;

  InitDialog();

  if (gDialog.selectionBased && !gDialog.isSelectionCollapsed)
  {
    gDialog.selectedObject = GetCurrentEditor().document.createElement("div");
  }

  // Set window location relative to parent window (based on persisted attributes)
  SetWindowLocation();

  // Set focus to first widget in dialog, e.g.:
}

function DisableTabIfNotSpecified(id, tabs, firstTab)
{
  if (tabs.indexOf(id) == -1) {
    /* disable the corresponding tab */
    var elt = document.getElementById( id + "Tab" );
    elt.setAttribute("collapsed", "true");
  }
  else
    firstTab = id;
  return firstTab;
}

function InitDialog()
{
  switch (gPanel) {
    case "border":
      InitBorderTabPanel();
      break;
    case "text":
      InitTextTabPanel();
      break;
    case "background":
      InitBackgroundTabPanel();
      break;
    case "box":
      InitBoxTabPanel();
      break;
    case "aural":
      InitAuralTabPanel();
      break;
    default:
      break;
  }
}

function getSelectedBlock()
{
  var anchorNode = editorShell.editorSelection.anchorNode;
  if (!anchorNode) return null;
  var node;
  if (anchorNode.firstChild)
  {
    // Start at actual selected node
    var offset = editorShell.editorSelection.anchorOffset;
    // Note: If collapsed, offset points to element AFTER caret,
    //  thus node may be null
    node = anchorNode.childNodes.item(offset);
  }
  if (!node)
    node = anchorNode;

  while (!nodeIsBlock(node) && node.nodeName.toLowerCase() != "body") {
    node = node.parentNode;
  }
  if (node.nodeName.toLowerCase() == "body") {
    node = null;
  }
  return node;
}

function nodeIsBlock(node)
{
  // HR doesn't count because it's not a container
  return !node || (node.localName != 'HR' && editorShell.NodeIsBlock(node));
}

function SetModifiedFlagOnStylesheet()
{
  /* STUB */
}

function FlushChanges()
{
  var editor = GetCurrentEditor();
  editor.incrementModificationCount(1);

  if (gDialog.selectionBased)
  {
    var selection = editor.selection;

    // we still have to update the selection, the preview is not "live",
    // with the styles hold by gDialog.selectedObject

    var elt;
    if (gDialog.isSelectionCollapsed)
    {
      // do we have to create a block here or not ?
      editor.beginTransaction();
      if (gDialog.selectionInSingleLine)
      {
        // yes, need to create a div around the line
        editor.setParagraphFormat("div");
      }
      elt = selection.focusNode;
      while (!editor.nodeIsBlock(elt))
        elt = elt.parentNode;
      editor.setAttribute(elt, styleStr, gDialog.selectedObject.getAttribute(styleStr));
      editor.endTransaction();
    }
    else
    {
      // nope, the selection is already in a block, nothing to do here
      // the preview is live
      // let's reuse Absolute Positioning code here
      editor.makeComplexBlock(gDialog.selectedObject.getAttribute(styleStr));
    }
  }
}

function CancelChanges()
{
  if (gDialog.selectedHasStyle)
    gDialog.selectedObject.setAttribute("style", gDialog.selectedStyle);
  else
    gDialog.selectedObject.removeAttribute("style");
}
