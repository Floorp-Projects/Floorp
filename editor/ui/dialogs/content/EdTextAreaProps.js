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
 * The Original Code is Text Area Properties Dialog.
 *
 * The Initial Developer of the Original Code is
 * Neil Rashbrook.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Neil Rashbrook <neil@parkwaycc.co.uk>
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

var insertNew;
var textareaElement;

// dialog initialization code

function Startup()
{
  var editor = GetCurrentEditor();
  if (!editor)
  {
    dump("Failed to get active editor!\n");
    window.close();
    return;
  }

  gDialog = {
    accept:             document.documentElement.getButton("accept"),
    textareaName:       document.getElementById("TextAreaName"),
    textareaRows:       document.getElementById("TextAreaRows"),
    textareaCols:       document.getElementById("TextAreaCols"),
    textareaWrap:       document.getElementById("TextAreaWrap"),
    textareaReadOnly:   document.getElementById("TextAreaReadOnly"),
    textareaDisabled:   document.getElementById("TextAreaDisabled"),
    textareaTabIndex:   document.getElementById("TextAreaTabIndex"),
    textareaAccessKey:  document.getElementById("TextAreaAccessKey"),
    textareaValue:      document.getElementById("TextAreaValue"),
    MoreSection:        document.getElementById("MoreSection"),
    MoreFewerButton:    document.getElementById("MoreFewerButton")
  };

  // Get a single selected text area element
  const kTagName = "textarea";
  try {
    textareaElement = editor.getSelectedElement(kTagName);
  } catch (e) {}

  if (textareaElement) {
    // We found an element and don't need to insert one
    insertNew = false;

    gDialog.textareaValue.value = textareaElement.value;
  }
  else
  {
    insertNew = true;

    // We don't have an element selected,
    //  so create one with default attributes
    try {
      textareaElement = editor.createElementWithDefaults(kTagName);
    } catch(e) {}

    if (!textareaElement)
    {
      dump("Failed to get selected element or create a new one!\n");
      window.close();
      return;
    }
    else
      gDialog.textareaValue.value = GetSelectionAsText();
  }

  // Make a copy to use for AdvancedEdit
  globalElement = textareaElement.cloneNode(false);

  InitDialog();

  InitMoreFewer();

  SetTextboxFocus(gDialog.textareaName);
  
  SetWindowLocation();
}

function InitDialog()
{
  gDialog.textareaName.value = globalElement.getAttribute("name");
  gDialog.textareaRows.value = globalElement.getAttribute("rows");
  gDialog.textareaCols.value = globalElement.getAttribute("cols");
  gDialog.textareaWrap.value = GetHTMLOrCSSStyleValue(globalElement, "wrap", "white-space");
  gDialog.textareaReadOnly.checked = globalElement.hasAttribute("readonly");
  gDialog.textareaDisabled.checked = globalElement.hasAttribute("disabled");
  gDialog.textareaTabIndex.value = globalElement.getAttribute("tabindex");
  gDialog.textareaAccessKey.value = globalElement.getAttribute("accesskey");
  onInput();
}

function onInput()
{
  var disabled = !gDialog.textareaName.value || !gDialog.textareaRows.value || !gDialog.textareaCols.value;
  if (gDialog.accept.disabled != disabled)
    gDialog.accept.disabled = disabled;
}

function ValidateData()
{
  var attributes = {
    name: gDialog.textareaName.value,
    rows: gDialog.textareaRows.value,
    cols: gDialog.textareaCols.value,
    wrap: gDialog.textareaWrap.value,
    tabindex: gDialog.textareaTabIndex.value,
    accesskey: gDialog.textareaAccessKey.value
  };
  var flags = {
    readonly: gDialog.textareaReadOnly.checked,
    disabled: gDialog.textareaDisabled.checked
  };
  for (var a in attributes)
  {
    if (attributes[a])
      globalElement.setAttribute(a, attributes[a]);
    else
      globalElement.removeAttribute(a);
  }
  for (var f in flags)
  {
    if (flags[f])
      globalElement.setAttribute(f, "");
    else
      globalElement.removeAttribute(f);
  }
  return true;
}

function onAccept()
{
  // All values are valid - copy to actual element in doc or
  //   element created to insert
  ValidateData();

  var editor = GetCurrentEditor();

  editor.beginTransaction();

  try {
    editor.cloneAttributes(textareaElement, globalElement);

    if (insertNew)
      editor.insertElementAtSelection(textareaElement, true);

    // undoably set value
    var initialText = gDialog.textareaValue.value;
    if (initialText != textareaElement.value) {
      editor.setShouldTxnSetSelection(false);

      while (textareaElement.hasChildNodes())
        editor.deleteNode(textareaElement.lastChild);
      if (initialText) {
        var textNode = editor.document.createTextNode(initialText);
        editor.insertNode(textNode, textareaElement, 0);
      }

      editor.setShouldTxnSetSelection(true);
    }
  } finally {
    editor.endTransaction();
  }

  SaveWindowLocation();

  return true;
}

