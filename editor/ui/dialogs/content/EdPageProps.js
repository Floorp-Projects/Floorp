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
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 */

var textColor;
var linkColor;
var followedLinkColor;
var activeLinkColor;
var backgroundColor;

//Cancel() is in EdDialogCommon.js
// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;

  doSetOKCancel(onOK, null);

  initDialog();
  
  //.focus();
}

function InitDialog()
{
}

function getColor(ColorPickerID, ColorWellID)
{
  var color = getColorAndSetColorWell(ColorPickerID, ColorWellID);
  switch( ColorPickerID )
  {
    case "textCP":
      textColor = color;
      break;
    case "linkCP":
      linkColor = color;
      break;
    case "followedCP":
      followedLinkColor = color;
      break;
    case "activeCP":
      activeLinkColor = color;
      break;
    case "backgroundCP":
      backgroundColor = color;
      break;
  }
}

function onOK()
{

 return true; // do close the window
}
