/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 */

/* Insert Source HTML dialog */
var srcInput;

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;
  doSetOKCancel(onOK, onCancel);
  var okButton = document.getElementById("ok");
  if (okButton)
  {
    okButton.removeAttribute("default");
    okButton.setAttribute("label",GetString("Insert"));
  }
  // Create dialog object to store controls for easy access
  srcInput = document.getElementById("srcInput");

  var selection = editorShell.GetContentsAs("text/html", 35);
  selection = (selection.replace(/<body[^>]*>/,"")).replace(/<\/body>/,"");
  if (selection != "")
    srcInput.value = selection;

  // Set initial focus
  srcInput.focus();
  // Note: We can't set the caret location in a multiline textbox
  SetWindowLocation();
}

function onOK()
{
  if (srcInput.value != "")
    editorShell.InsertSource(srcInput.value);
  else {
    dump("Null value -- not inserting in HTML Source dialog\n");
    return false;
  }
  SaveWindowLocation();

  return true;
}

