/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

var gPrefs;
var gAddButton;
var gOKButton;
var gRemoveButton;
var gHeaderInputElement;
var gArrayHdrs;
var gHdrsList;
var gContainer;
var gFilterBundle=null;
var gCustomBundle=null;

function onLoad()
{
    gPrefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
    var hdrs;
    try
    {
       hdrs = gPrefs.getCharPref("mailnews.customHeaders");
    }
    catch(ex)
    {
      hdrs =null;
    }
    gHeaderInputElement = document.getElementById("headerInput");
    gHeaderInputElement.focus();

    gHdrsList = document.getElementById("headerList");
    gArrayHdrs = new Array();
    gAddButton = document.getElementById("addButton");
    gRemoveButton = document.getElementById("removeButton");
    gOKButton = document.getElementById("ok");

    initializeDialog(hdrs);

    doSetOKCancel(onOk, null);

    updateAddButton(true);
    updateRemoveButton();

    moveToAlertPosition();
}

function initializeDialog(hdrs)
{
  if (hdrs)
  {
    hdrs = hdrs.replace(/\s+/g,'');  //remove white spaces before splitting
    gArrayHdrs = hdrs.split(":");
    for (var i = 0; i< gArrayHdrs.length; i++) 
      if (!gArrayHdrs[i])
        gArrayHdrs.splice(i,1);  //remove any null elements
    initializeRows();
  }
}

function initializeRows()
{
  for (var i = 0; i< gArrayHdrs.length; i++) 
    addRow(TrimString(gArrayHdrs[i]));
}

function onTextInput()
{
  // enable the add button if the user has started to type text
  updateAddButton( (gHeaderInputElement.value == "") );
}

function enterKeyPressed()
{
   // if the add button is currently the default action then add the text
  if (gHeaderInputElement.value != "" && !gAddButton.disabled)
  {
    onAddHeader();
  } 
  else
  {
    // otherwise, the default action for the dialog is the OK button
    if (! gOKButton.disabled) 
      doOKButton();
  }
}

function onOk()
{  
  if (gArrayHdrs.length)
  {
    var hdrs;
    if (gArrayHdrs.length == 1)
      hdrs = gArrayHdrs;
    else
      hdrs = gArrayHdrs.join(": ");
    gPrefs.setCharPref("mailnews.customHeaders", hdrs);
    // flush prefs to disk, in case we crash, to avoid dataloss and problems with filters that use the custom headers
    var prefService = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefService);
    prefService.savePrefFile(null);
  }
  else
  {
    try
    {
      gPrefs.clearUserPref("mailnews.customHeaders"); //clear the pref, no custom headers 
    }
    catch(ex) {}  //will throw an exception if there is no "mailnews.customHeaders" in prefs.js
  }
  window.close();
}

function customHeaderOverflow()
{
  var nsMsgSearchAttrib = Components.interfaces.nsMsgSearchAttrib;
  if (gArrayHdrs.length >= (nsMsgSearchAttrib.kNumMsgSearchAttributes - nsMsgSearchAttrib.OtherHeader - 1))
  {
    if (!gFilterBundle)
      gFilterBundle = document.getElementById("bundle_filter");

    var alertText = gFilterBundle.getString("customHeaderOverflow");
    window.alert(alertText);
    return true;
  }
  return false;
}

function onAddHeader()
{
  var newHdr = TrimString(gHeaderInputElement.value);

  if (!isRFC2822Header(newHdr))  // if user entered an invalid rfc822 header field name, bail out.
  {
    if (!gCustomBundle)
      gCustomBundle = document.getElementById("bundle_custom");

    var alertText = gCustomBundle.getString("colonInHeaderName");
    window.alert(alertText);
    return;
  }

  gHeaderInputElement.value = "";
  if (!newHdr || customHeaderOverflow())
    return;
  if (!duplicateHdrExists(newHdr))
  {
    gArrayHdrs[gArrayHdrs.length] = newHdr;
    var newItem = addRow(newHdr);
    gHdrsList.selectItem (newItem); // make sure the new entry is selected in the tree
    // now disable the add button
    updateAddButton(true);
    gHeaderInputElement.focus(); // refocus the input field for the next custom header
  }
}

function isRFC2822Header(hdr)
{
  var charCode;
  for (var i=0; i< hdr.length; i++)
  {
    charCode = hdr.charCodeAt(i);
    //58 is for colon and 33 and 126 are us-ascii bounds that should be used for header field name, as per rfc2822

    if (charCode < 33 || charCode == 58 || charCode > 126) 
      return false;
  }
  return true;
}

function duplicateHdrExists(hdr)
{
  for (var i=0;i<gArrayHdrs.length; i++) 
  {
    if (gArrayHdrs[i] == hdr)
      return true;
  }
  return false;
}
 
function onRemoveHeader()
{
  var listitem = gHdrsList.selectedItems[0]
  if (!listitem) return;
  gHdrsList.removeChild(listitem);
  var selectedHdr = GetListItemAttributeStr(listitem);
  var j=0;
  for (var i=0;i<gArrayHdrs.length; i++) 
  {
    if (gArrayHdrs[i] == selectedHdr)
    {
      gArrayHdrs.splice(i,1);
      break;
    }
  }
}

function GetListItemAttributeStr(listitem)
{
   if (listitem)
     return TrimString(listitem.getAttribute("label"));
 
   return "";
}

function addRow(newHdr)
{
  var listitem = document.createElement("listitem");
  listitem.setAttribute("label", newHdr);
  gHdrsList.appendChild(listitem); 
  return listitem;  
}

function updateAddButton(aDisable)
{
  // only update the button if we absolutely have to...
  if (aDisable != gAddButton.disabled)
  {
    gAddButton.disabled = aDisable;
    if (aDisable)
    {
      gOKButton.setAttribute('default', true); 
      gAddButton.removeAttribute('default');
    }
    else
    {
      gOKButton.removeAttribute('default');
      gAddButton.setAttribute('default', true); 
    }
  }
}

function updateRemoveButton()
{
  var headerSelected = (gHdrsList.selectedItems.length > 0);
  gRemoveButton.disabled = !headerSelected;
}

//Remove whitespace from both ends of a string
function TrimString(string)
{
  if (!string) return "";
  return string.replace(/(^\s+)|(\s+$)/g, '')
}
