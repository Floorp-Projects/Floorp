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

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;

  doSetOKCancel(onOK, onCancel); // Map OK/Cancel to relevant functions

  gDialog.urlInput = document.getElementById("urlInput");
  gDialog.targetInput = document.getElementById("targetInput");
  gDialog.altInput = document.getElementById("altInput");
  gDialog.commonInput = document.getElementById("commonInput");

  gDialog.hsHref = window.arguments[0].getAttribute("hsHref");
  if (gDialog.hsHref != '')
    gDialog.urlInput.value = gDialog.hsHref;

  gDialog.hsAlt = window.arguments[0].getAttribute("hsAlt");
  if (gDialog.hsAlt != '')
    gDialog.altInput.value = gDialog.hsAlt;

  gDialog.hsTarget = window.arguments[0].getAttribute("hsTarget");
  if (gDialog.hsTarget != ''){
    gDialog.targetInput.value = gDialog.hsTarget;
    len = gDialog.commonInput.length;
    for (i=0; i<len; i++){
      if (gDialog.hsTarget == gDialog.commonInput.options[i].value)
        gDialog.commonInput.options[i].selected = "true";
    }
  }

  SetTextboxFocus(gDialog.urlInput);

  SetWindowLocation();
}

function onOK()
{
  dump(window.arguments[0].id+"\n");
  window.arguments[0].setAttribute("hsHref", gDialog.urlInput.value);
  window.arguments[0].setAttribute("hsAlt", gDialog.altInput.value);
  window.arguments[0].setAttribute("hsTarget", gDialog.targetInput.value);

  SaveWindowLocation();

  window.close();
}

function changeTarget() {
  gDialog.targetInput.value=gDialog.commonInput.value;
}

function chooseFile()
{
  // Get a local file, converted into URL format

  fileName = GetLocalFileURL("html");
  if (fileName && fileName != "") {
    gDialog.urlInput.value = fileName;
  }

  // Put focus into the input field
  SetTextboxFocus(gDialog.urlInput);
}
