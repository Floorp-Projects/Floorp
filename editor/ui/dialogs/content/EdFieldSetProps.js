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
 * The Original Code is Field Set Properties Dialog.
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
var fieldsetElement;
var newLegend;
var legendElement;

// dialog initialization code

function Startup()
{
  if (!InitEditorShell())
    return;

  gDialog.editText = document.getElementById("EditText");
  gDialog.legendText = document.getElementById("LegendText");
  gDialog.legendAlign = document.getElementById("LegendAlign");
  gDialog.RemoveFieldSet = document.getElementById("RemoveFieldSet");

  // Get a single selected field set element
  var tagName = "fieldset";
  fieldsetElement = editorShell.GetSelectedElement(tagName);
  if (!fieldsetElement)
    fieldsetElement = editorShell.GetElementOrParentByTagName(tagName, editorShell.editorSelection.anchorNode);
  if (!fieldsetElement)
    fieldsetElement = editorShell.GetElementOrParentByTagName(tagName, editorShell.editorSelection.focusNode);

  if (fieldsetElement)
    // We found an element and don't need to insert one
    insertNew = false;
  else
  {
    insertNew = true;

    // We don't have an element selected,
    //  so create one with default attributes

    fieldsetElement = editorShell.CreateElementWithDefaults(tagName);
    if (!fieldsetElement)
    {
      dump("Failed to get selected element or create a new one!\n");
      window.close();
      return;
    }
    // Hide button removing existing fieldset
    gDialog.RemoveFieldSet.setAttribute("hidden", "true");
  }

  legendElement = fieldsetElement.firstChild;
  if (legendElement && legendElement.localName == "LEGEND")
  {
    newLegend = false;
    editorShell.SelectElement(legendElement);
    gDialog.legendText.value = GetSelectionAsText();
    if (/</.test(legendElement.innerHTML))
    {
      gDialog.editText.checked = false;
      gDialog.editText.disabled = false;
      gDialog.legendText.disabled = true;
      gDialog.editText.addEventListener("command", onEditText, false);
      gDialog.RemoveFieldSet.focus();
    }
    else
      SetTextboxFocus(gDialog.legendText);
  }
  else
  {
    newLegend = true;

    // We don't have an element selected,
    //  so create one with default attributes

    legendElement = editorShell.CreateElementWithDefaults("legend");
    if (!legendElement)
    {
      dump("Failed to get selected element or create a new one!\n");
      window.close();
      return;
    }
    SetTextboxFocus(gDialog.legendText);
  }

  // Make a copy to use for AdvancedEdit
  globalElement = legendElement.cloneNode(false);

  InitDialog();

  SetWindowLocation();
}

function InitDialog()
{
  gDialog.legendAlign.value = GetHTMLOrCSSStyleValue(globalElement, "align", "caption-side");
}

function onEditText()
{
  gDialog.editText.removeEventListener("command", onEditText, false);
  AlertWithTitle(GetString("Alert"), GetString("EditTextWarning"));
}

function RemoveFieldSet()
{
  editorShell.BeginBatchChanges();
  try {
    if (!newLegend)
      editorShell.DeleteElement(legendElement);
    // This really needs to call the C++ function RemoveBlockContainer
    // which inserts any <BR>s needed
    RemoveElementKeepingChildren(fieldsetElement);
  } finally {
    editorShell.EndBatchChanges();
  }
  SaveWindowLocation();
  window.close();
}

function ValidateData()
{
  if (gDialog.legendAlign.value)
    globalElement.setAttribute("align", gDialog.legendAlign.value);
  else
    globalElement.removeAttribute("align");
  return true;
}

function onAccept()
{
  // All values are valid - copy to actual element in doc
  ValidateData();

  editorShell.BeginBatchChanges();

  try {
    if (gDialog.editText.checked)
    {
      if (gDialog.legendText.value)
      {
        var editor = editorShell.editor;
        if (newLegend)
          editorShell.InsertElement(legendElement, fieldsetElement, 0, true);
        else while (legendElement.firstChild)
          editor.deleteNode(legendElement.firstChild);
        editor.insertNode(editorShell.editorDocument.createTextNode(gDialog.legendText.value), legendElement, 0);
      }
      else if (!newLegend)
        editorShell.DeleteElement(legendElement);
    }
 
    if (insertNew)
      InsertElementAroundSelection(fieldsetElement);
    else
      editorShell.SelectElement(fieldsetElement);

    editorShell.CloneAttributes(legendElement, globalElement);
  }
  finally {
    editorShell.EndBatchChanges();
  }

  SaveWindowLocation();

  return true;
}

