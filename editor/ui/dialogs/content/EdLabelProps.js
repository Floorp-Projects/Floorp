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
 * The Original Code is Label Properties Dialog.
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

var labelElement;

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

  gDialog.editText = document.getElementById("EditText");
  gDialog.labelText = document.getElementById("LabelText");
  gDialog.labelFor = document.getElementById("LabelFor");
  gDialog.labelAccessKey = document.getElementById("LabelAccessKey");

  labelElement = window.arguments[0];

  // Make a copy to use for AdvancedEdit
  globalElement = labelElement.cloneNode(false);

  InitDialog();

  try {
    editor.selectElement(labelElement);
    gDialog.labelText.value = GetSelectionAsText();
  } catch (e) {}

  if (/</.test(labelElement.innerHTML))
  {
    gDialog.editText.checked = false;
    gDialog.editText.disabled = false;
    gDialog.labelText.disabled = true;
    gDialog.editText.addEventListener("command", onEditText, false);
    SetTextboxFocus(gDialog.labelFor);
  }
  else
    SetTextboxFocus(gDialog.labelText);

  SetWindowLocation();
}

function InitDialog()
{
  gDialog.labelFor.value = globalElement.getAttribute("for");
  gDialog.labelAccessKey.value = globalElement.getAttribute("accesskey");
}

function onEditText()
{
  gDialog.editText.removeEventListener("command", onEditText, false);
  AlertWithTitle(GetString("Alert"), GetString("EditTextWarning"));
}

function RemoveLabel()
{
  // RemoveTextProperty might work
  // but only because the label was selected to get its text
  RemoveElementKeepingChildren(labelElement);
  SaveWindowLocation();
  window.close();
}

function ValidateData()
{
  if (gDialog.labelFor.value)
    globalElement.setAttribute("for", gDialog.labelFor.value);
  else
    globalElement.removeAttribute("for");
  if (gDialog.labelAccessKey.value)
    globalElement.setAttribute("accesskey", gDialog.labelAccessKey.value);
  else
    globalElement.removeAttribute("accesskey");
  return true;
}

function onAccept()
{
  // All values are valid - copy to actual element in doc
  ValidateData();

  var editor = GetCurrentEditor();

  editor.beginTransaction();

  try {
    if (gDialog.editText.checked)
    {
      editor.setShouldTxnSetSelection(false);

      while (labelElement.firstChild)
        editor.deleteNode(labelElement.firstChild);
      if (gDialog.labelText.value) {
        var textNode = editor.document.createTextNode(gDialog.labelText.value);
        editor.insertNode(textNode, labelElement, 0);
        editor.selectElement(labelElement);
      }

      editor.setShouldTxnSetSelection(true);
    }

    editor.cloneAttributes(labelElement, globalElement);
  } catch(e) {}

  editor.endTransaction();

  SaveWindowLocation();

  return true;
}

