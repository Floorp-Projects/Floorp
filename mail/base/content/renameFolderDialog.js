/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 *  The contents of this file are subject to the Netscape Public
 *  License Version 1.1 (the "License"); you may not use this file
 *  except in compliance with the License. You may obtain a copy of
 *  the License at http://www.mozilla.org/NPL/
 *
 *  Software distributed under the License is distributed on an "AS
 *  IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 *  implied. See the License for the specific language governing
 *  rights and limitations under the License.
 *
 *  The Original Code is Mozilla Communicator client code, released
 *  March 31, 1998.
 *
 *  The Initial Developer of the Original Code is Netscape
 *  Communications Corporation. Portions created by Netscape are
 *  Copyright (C) 1998-1999 Netscape Communications Corporation. All
 *  Rights Reserved.
 *
 *  Contributor(s):
 *    Fabian Guisset <hidday@geocities.com>
 */

var dialog;

function onLoad()
{
  var arguments = window.arguments[0];

  dialog = {};

  dialog.OKButton = document.getElementById("ok");

  dialog.nameField = document.getElementById("name");
  dialog.nameField.value = arguments.name;
  dialog.nameField.select();
  dialog.nameField.focus();

  // call this when OK is pressed
  dialog.okCallback = arguments.okCallback;

  // pre select the folderPicker, based on what they selected in the folder pane
  dialog.preselectedFolderURI = arguments.preselectedURI;

  moveToAlertPosition();
  doEnabling();
  doSetOKCancel(onOK, onCancel);
}

function onOK()
{
  dialog.okCallback(dialog.nameField.value, dialog.preselectedFolderURI);

  return true;
}

function onCancel()
{
  // close the window
  return true;
}

function doEnabling()
{
  if (dialog.nameField.value) {
    if (dialog.OKButton.disabled)
      dialog.OKButton.disabled = false;
  } else {
    if (!dialog.OKButton.disabled)
      dialog.OKButton.disabled = true;
  }
}

