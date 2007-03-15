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

/** 
 * Pass in keyToEdit as a window argument to turn this dialog into an edit
 * tag dialog.
 */
function onLoad()
{
  var arguments = window.arguments[0];

  dialog = {};

  dialog.OKButton = document.documentElement.getButton("accept");
  dialog.nameField = document.getElementById("name");
  dialog.nameField.focus();

  // call this when OK is pressed
  dialog.okCallback = arguments.okCallback;
  if (arguments.keyToEdit)
    initializeForEditing(arguments.keyToEdit);

  moveToAlertPosition();
  doEnabling();
}

/** 
 * Turn the new tag dialog into an edit existing tag dialog
 */
function initializeForEditing(aTagKey)
{
  dialog.editTagKey = aTagKey;
  
  // Change the title of the dialog
  var messengerBundle = document.getElementById("bundle_messenger");
  document.title = messengerBundle.getString("editTagTitle");
  
  // override the OK button
  document.documentElement.setAttribute("ondialogaccept", "return onOKEditTag();");
  
  // extract the color and name for the current tag
  var tagService = Components.classes["@mozilla.org/messenger/tagservice;1"]
                     .getService(Components.interfaces.nsIMsgTagService);
  document.getElementById("tagColorPicker").color = tagService.getColorForKey(aTagKey);
  dialog.nameField.value = tagService.getTagForKey(aTagKey);
}

/**
 * on OK handler for editing a new tag. 
 */
function onOKEditTag()
{
  var tagService = Components.classes["@mozilla.org/messenger/tagservice;1"]
                     .getService(Components.interfaces.nsIMsgTagService);
  // get the tag name of the current key we are editing
  var existingTagName = tagService.getTagForKey(dialog.editTagKey);
  
  // it's ok if the name didn't change
  if (existingTagName != dialog.nameField.value)
  {
    // don't let the user edit a tag to the name of another existing tag
    if (tagService.getKeyForTag(dialog.nameField.value))
    {
      alertForExistingTag();
      return false; // abort the OK
    }
              
    tagService.setTagForKey(dialog.editTagKey, dialog.nameField.value);
  }
    
  tagService.setColorForKey(dialog.editTagKey, document.getElementById("tagColorPicker").color);
  return dialog.okCallback();
}

/**
 * on OK handler for creating a new tag. Alerts the user if a tag with 
 * the name already exists.
 */
function onOKNewTag()
{
  var name = dialog.nameField.value;

  var tagService = Components.classes["@mozilla.org/messenger/tagservice;1"].getService(Components.interfaces.nsIMsgTagService);
  
  if (tagService.getKeyForTag(name))
  {
    alertForExistingTag();
    return false;
  } 
  else
    return dialog.okCallback(name, document.getElementById("tagColorPicker").color);
}

/**
 * Alerts the user that they are trying to create a tag with a name that
 * already exists.
 */
function alertForExistingTag()
{
  var messengerBundle = document.getElementById("bundle_messenger");
  var alertText = messengerBundle.getString("tagExists");
  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                .getService(Components.interfaces.nsIPromptService);
  promptService.alert(window, document.title, alertText);
}

function doEnabling()
{
  dialog.OKButton.disabled = !dialog.nameField.value;
}
