/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

var dialog;

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;

  doSetOKCancel(onOK, onCancel); // Map OK/Cancel to relevant functions

  // Create dialog object to store controls for easy access
  dialog = new Object;
  dialog.urlInput = document.getElementById("urlInput");
  dialog.targetInput = document.getElementById("targetInput");
  dialog.altInput = document.getElementById("altInput");
  dialog.commonInput = document.getElementById("commonInput");

  dialog.hsHref = window.arguments[0].getAttribute("hsHref");
  if (dialog.hsHref != '')
    dialog.urlInput.value = dialog.hsHref;

  dialog.hsAlt = window.arguments[0].getAttribute("hsAlt");
  if (dialog.hsAlt != '')
    dialog.altInput.value = dialog.hsAlt;

  dialog.hsTarget = window.arguments[0].getAttribute("hsTarget");
  if (dialog.hsTarget != ''){
    dialog.targetInput.value = dialog.hsTarget;
    len = dialog.commonInput.length;
    for (i=0; i<len; i++){
      if (dialog.hsTarget == dialog.commonInput.options[i].value)
        dialog.commonInput.options[i].selected = "true";
    }
  }

  SetTextboxFocus(dialog.urlInput);

  SetWindowLocation();
}

function onOK()
{
  dump(window.arguments[0].id+"\n");
  window.arguments[0].setAttribute("hsHref", dialog.urlInput.value);
  window.arguments[0].setAttribute("hsAlt", dialog.altInput.value);
  window.arguments[0].setAttribute("hsTarget", dialog.targetInput.value);

  SaveWindowLocation();

  window.close();
}

function changeTarget() {
  dialog.targetInput.value=dialog.commonInput.value;
}

function chooseFile()
{
  // Get a local file, converted into URL format

  fileName = GetLocalFileURL("html");
  if (fileName && fileName != "") {
    dialog.urlInput.value = fileName;
  }

  // Put focus into the input field
  SetTextboxFocus(dialog.urlInput);
}
