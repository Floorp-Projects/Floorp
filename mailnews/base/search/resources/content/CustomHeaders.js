/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Original Contributor(s): Navin Gupta <naving@netscape.com>
 */

var gPrefs;
var addButton;
var removeButton;
var gHeaderInputElement;
var gArrayHdrs;
var gHdrsList;
var gTree;
var gContainer;

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
    gHdrsList = document.getElementById("headerList");
    gTree = document.getElementById("headerTree");
    gArrayHdrs = new Array();
    addButton = document.getElementById("addButton");
    removeButton = document.getElementById("removeButton");

    initializeDialog(hdrs);

    doSetOKCancel(onOk, null);

    updateButtons();
    moveToAlertPosition();
}

function initializeDialog(hdrs)
{
  if (hdrs)
  {
    gArrayHdrs = hdrs.split(": ");
    initializeRows();
  }
}

function initializeRows()
{
  for (var i = 0; i< gArrayHdrs.length; i++) 
    addRow(TrimString(gArrayHdrs[i]));
}

function onOk()
{ 
  if (gArrayHdrs.length)
  {
    var hdrs = gArrayHdrs.join(": ");
    gPrefs.setCharPref("mailnews.customHeaders", hdrs);
  }
  window.close();
}

function onAddHeader()
{
  var newHdr = TrimString(gHeaderInputElement.value);
  gHeaderInputElement.value = "";
  if (!newHdr) return;
  if (!duplicateHdrExists(newHdr))
  {
    gArrayHdrs[gArrayHdrs.length] = newHdr;
    addRow(newHdr);
  }
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
  var treeItem = gTree.selectedItems[0]
  if (!treeItem) return;
  gHdrsList.removeChild(treeItem);
  var selectedHdr = GetTreeItemAttributeStr(treeItem);
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

function GetTreeItemAttributeStr(treeItem)
{
   if (treeItem)
     return TrimString(treeItem.firstChild.firstChild.getAttribute("label"));
 
   return "";
}

function addRow(newHdr)
{
  var treeitem = document.createElement("treeitem");
  var row = document.createElement("treerow");
  var treecell = document.createElement("treecell");
  treecell.setAttribute("flex", "1");
  treecell.setAttribute("label", newHdr);
  row.appendChild(treecell);
  treeitem.appendChild(row);
  gHdrsList.appendChild(treeitem);
  
}

function updateButtons()
{
    var headerSelected = (gTree.selectedItems.length > 0);
    removeButton.disabled = !headerSelected;
}

//Remove whitespace from both ends of a string
function TrimString(string)
{
   if (!string) return "";
   return string.replace(/(^\s+)|(\s+$)/g, '')
}
