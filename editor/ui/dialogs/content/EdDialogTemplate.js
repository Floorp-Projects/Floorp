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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
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
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


//Cancel() is in EdDialogCommon.js
var insertNew = true;
var tagname = "TAG NAME"

// dialog initialization code
function Startup()
{
  if (!GetCurrentEditor())
  {
    window.close();
    return;
  }
  // gDialog is declared in EdDialogCommon.js
  // Set commonly-used widgets like this:
  gDialog.fooButton = document.getElementById("fooButton");

  initDialog();
  
  // Set window location relative to parent window (based on persisted attributes)
  SetWindowLocation();

  // Set focus to first widget in dialog, e.g.:
  SetTextboxFocus(gDialog.fooButton);
}

function InitDialog() 
{
  // Initialize all dialog widgets here,
  // e.g., get attributes from an element for property dialog
}

function onAccept()
{
  // Validate all user data and set attributes and possibly insert new element here
  // If there's an error the user must correct, return false to keep dialog open.
  
  SaveWindowLocation();
  return true; // do close the window
}
