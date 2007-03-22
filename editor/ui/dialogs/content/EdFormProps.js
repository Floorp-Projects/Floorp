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
 * The Original Code is Form Properties Dialog.
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

var gForm;
var insertNew;
var formElement;
var formActionWarning;

function Startup()
{
  var editor = GetCurrentEditor();
  if (!editor)
  {
    dump("Failed to get active editor!\n");
    window.close();
    return;
  }

  gForm = {
    Name:     document.getElementById("FormName"),
    Action:   document.getElementById("FormAction"),
    Method:   document.getElementById("FormMethod"),
    EncType:  document.getElementById("FormEncType"),
    Target:   document.getElementById("FormTarget")
  }
  gDialog.MoreSection = document.getElementById("MoreSection");
  gDialog.MoreFewerButton = document.getElementById("MoreFewerButton");
  gDialog.RemoveForm = document.getElementById("RemoveForm")

  // Get a single selected form element
  const kTagName = "form";
  try {
    formElement = editor.getSelectedElement(kTagName);
    if (!formElement)
      formElement = editor.getElementOrParentByTagName(kTagName, editor.selection.anchorNode);
    if (!formElement)
      formElement = editor.getElementOrParentByTagName(kTagName, editor.selection.focusNode);
  } catch (e) {}

  if (formElement)
  {
    // We found an element and don't need to insert one
    insertNew = false;
    formActionWarning = formElement.hasAttribute("action");
  }
  else
  {
    insertNew = true;
    formActionWarning = true;

    // We don't have an element selected,
    //  so create one with default attributes
    try {
      formElement = editor.createElementWithDefaults(kTagName);
    } catch (e) {}

    if (!formElement)
    {
      dump("Failed to get selected element or create a new one!\n");
      window.close();
      return;
    }
    // Hide button removing existing form
    gDialog.RemoveForm.hidden = true;
  }

  // Make a copy to use for AdvancedEdit
  globalElement = formElement.cloneNode(false);

  InitDialog();

  InitMoreFewer();

  SetTextboxFocus(gForm.Name);

  SetWindowLocation();
}

function InitDialog()
{
  for (var attribute in gForm)
    gForm[attribute].value = globalElement.getAttribute(attribute);
}

function RemoveForm()
{
  RemoveBlockContainer(formElement);
  SaveWindowLocation();
  window.close();
}

function ValidateData()
{
  for (var attribute in gForm)
  {
    if (gForm[attribute].value)
      globalElement.setAttribute(attribute, gForm[attribute].value);
    else
      globalElement.removeAttribute(attribute);
  }
  return true;
}

function onAccept()
{
  if (formActionWarning && !gForm.Action.value)
  {
    AlertWithTitle(GetString("Alert"), GetString("NoFormAction"));
    gForm.Action.focus();
    formActionWarning = false;
    return false;
  }
  // All values are valid - copy to actual element in doc or
  //   element created to insert
  ValidateData();

  var editor = GetCurrentEditor();

  editor.cloneAttributes(formElement, globalElement);

  if (insertNew)
    InsertElementAroundSelection(formElement);

  SaveWindowLocation();

  return true;
}
