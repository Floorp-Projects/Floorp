/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the new tag dialog
 *
 * The Initial Developer of the Original Code is
 * David Bienvenu.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  David Bienvenu <bienvenu@nventure.com> 
 *  Karsten Düsterloh <mnyromyr@tprac.de>
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

  moveToAlertPosition();
  doEnabling();
}

function onOK()
{
  var name = dialog.nameField.value;

  var tagService = Components.classes["@mozilla.org/messenger/tagservice;1"].getService(Components.interfaces.nsIMsgTagService); 
  // do name validity check? Has to be non-empty, and not existing already
  try
  {
    var key = tagService.getKeyForTag(name); 
    // above will throw an error if tag doesn't exist. So if it doesn't throw an error,
    // the tag exists, so alert the user and return false.
  }
  catch (ex) {return dialog.okCallback(name, "")}
  
  var messengerBundle = document.getElementById("bundle_messenger");
  var alertText = messengerBundle.getString("tagExists");
  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                .getService(Components.interfaces.nsIPromptService);
  promptService.alert(window, document.title, alertText);
  return false;
}

function doEnabling()
{
  dialog.OKButton.disabled = !dialog.nameField.value;
}
