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

  colorPage = new Object;
  if (!colorPage)
  {
    dump("Failed to create colorPage object!!!\n");
    window.close();
  }

  colorPage.ColorPreview = document.getElementById("ColorPreview");
  colorPage.NormalText = document.getElementById("NormalText");
  colorPage.LinkText = document.getElementById("LinkText");
  colorPage.ActiveLinkText = document.getElementById("ActiveLinkText");
  colorPage.FollowedLinkText = document.getElementById("FollowedLinkText");

//  colorPage. = documentgetElementById("");

  tabPanel = document.getElementById("tabPanel");

  // Set the starting Tab:
  var tabName = window.arguments[1];
  currentTab = document.getElementById(tabName);
  if (!currentTab)
    currentTab = document.getElementById("GeneralTab");

  if (!tabPanel || !currentTab)
  {
    dump("Not all dialog controls were found!!!\n");
    window.close;
  }

  // Trigger setting of style for the selected tab widget
  currentTab.setAttribute("selected", "true");
  // Activate the corresponding panel
  var index = 0;
  switch(tabName)
  {
    case "ColorTab":
      index = 1;
      break;
    case "MetaTab":
      index = 2;
      break;
  }
  tabPanel.setAttribute("index",index);

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
      colorPage.textColor = color;
      colorPage.NormalText.setAttribute("style",colorString);
      break;
    case "linkCP":
      colorPage.linkColor = color;
      colorPage.LinkText.setAttribute("style",colorString);
      break;
    case "followedCP":
      colorPage.followedLinkColor = color;
      colorPage.FollowedLinkText.setAttribute("style",colorString);
      break;
    case "activeCP":
      colorPage.activeLinkColor = color;
      colorPage.ActiveLinkText.setAttribute("style",colorString);
      break;
    case "backgroundCP":
      colorPage.backgroundColor = color;
      colorPage.ColorPreview.setAttribute("bgcolor",color);
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
