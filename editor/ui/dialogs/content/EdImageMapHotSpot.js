/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Dan Haddix
 */

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
