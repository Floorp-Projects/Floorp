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

const FOLDERS = 1;
const MESSAGES = 2;
var dialog;

function onLoad()
{
  var arguments = window.arguments[0];

  dialog = {};

  dialog.OKButton = document.documentElement.getButton("accept");

  dialog.nameField = document.getElementById("name");
  dialog.nameField.focus();

  // call this when OK is pressed
  dialog.okCallback = arguments.okCallback;

  // pre select the folderPicker, based on what they selected in the folder pane
  dialog.picker = document.getElementById("msgNewFolderPicker");
  MsgFolderPickerOnLoad("msgNewFolderPicker");

  // can folders contain both folders and messages?
  if (arguments.dualUseFolders) {
    dialog.folderType = FOLDERS | MESSAGES;
  } else {

    // show the section which allows us to select the folder type to create
    var newFolderTypeBox = document.getElementById("newFolderTypeBox");
    newFolderTypeBox.removeAttribute("hidden");

    // set our folder type by calling the default selected type's oncommand
    var selectedFolderType = document.getElementById("folderGroup").selectedItem;
    eval(selectedFolderType.getAttribute("oncommand"));
  }

  moveToAlertPosition();
  doEnabling();
  doSetOKCancel(onOK, onCancel);
}

function onOK()
{
  var name = dialog.nameField.value;
  var uri = dialog.picker.getAttribute("uri");

  // do name validity check?

  // make sure name ends in  "/" if folder to create can only contain folders
  if ((dialog.folderType == FOLDERS) && name.charAt(name.length-1) != "/")
    dialog.okCallback(name + "/", uri);
  else
    dialog.okCallback(name, uri);

  return true;
}

function onCancel()
{
  // close the window
  return true;
}

function onFoldersOnly()
{
  dialog.folderType = FOLDERS;
}

function onMessagesOnly()
{
  dialog.folderType = MESSAGES;
}

function doEnabling()
{
  if (dialog.nameField.value && dialog.picker.getAttribute("uri")) {
    if (dialog.OKButton.disabled)
      dialog.OKButton.disabled = false;
  } else {
    if (!dialog.OKButton.disabled)
      dialog.OKButton.disabled = true;
  }
}

