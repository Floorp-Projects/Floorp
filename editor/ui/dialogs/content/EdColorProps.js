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

//Cancel() is in EdDialogCommon.js
// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;

  dialog = new Object;
  if (!dialog)
  {
    dump("Failed to create dialog object!!!\n");
    window.close();
  }

  dialog.ColorPreview = document.getElementById("ColorPreview");
  dialog.NormalText = document.getElementById("NormalText");
  dialog.LinkText = document.getElementById("LinkText");
  dialog.ActiveLinkText = document.getElementById("ActiveLinkText");
  dialog.FollowedLinkText = document.getElementById("FollowedLinkText");

  doSetOKCancel(onOK, null);

  InitDialog();

  //.focus();
}

function InitDialog()
{
}

function GetColor(ColorPickerID, ColorWellID)
{
  var color = getColorAndSetColorWell(ColorPickerID, ColorWellID);
dump("GetColor, color="+color+"\n");
  if (!color)
  {
    // No color was clicked on,
    //  so user clicked on "Don't Specify Color" button
    color = "inherit";
    dump("Don't specify color\n");
  }
  var colorString = "color:"+color;
  switch( ColorPickerID )
  {
    case "textCP":
      dialog.textColor = color;
      dialog.NormalText.setAttribute("style",colorString);
      break;
    case "linkCP":
      dialog.linkColor = color;
      dialog.LinkText.setAttribute("style",colorString);
      break;
    case "followedCP":
      dialog.followedLinkColor = color;
      dialog.FollowedLinkText.setAttribute("style",colorString);
      break;
    case "activeCP":
      dialog.activeLinkColor = color;
      dialog.ActiveLinkText.setAttribute("style",colorString);
      break;
    case "backgroundCP":
      dialog.backgroundColor = color;
      dialog.ColorPreview.setAttribute("bgcolor",color);
      break;
  }
}

function RemoveColor(ColorWellID)
{
}

function onOK()
{

 return true; // do close the window
}
