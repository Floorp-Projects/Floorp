/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is Mozilla Communicator client code, released March
 * 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Ben Goodger
 */

/*** =================== INITIALISATION CODE =================== ***/

// globals 
var signonviewer        = null;
var signonList          = [];
var rejectList          = [];
var nopreviewList       = [];
var nocaptureList       = [];
var goneSS              = ""; // signon
var goneIS              = ""; // ignored site
var goneFR              = ""; // form preview
var goneNC              = ""; // nocapture

// function : <SignonViewer.js>::Startup();
// purpose  : initialises interface, calls init functions for each page
function Startup()
{
  signonviewer = Components.classes["component://netscape/signonviewer/signonviewer-world"].createInstance();
  signonviewer = signonviewer.QueryInterface(Components.interfaces.nsISignonViewer);

  doSetOKCancel(onOK, null);  // init ok event handler

  LoadSignons();
  LoadReject();
  LoadNopreview();
}

/*** =================== SAVED SIGNONS CODE =================== ***/

// function : <SignonViewer.js>::LoadSignons();
// purpose  : reads signons from interface and loads into tree
function LoadSignons()
{
  signonList = signonviewer.GetSignonValue();
  var delim = signonList[0];
  signonList = signonList.split(delim);
  for(var i = 1; i < signonList.length; i++)
  {
    var currSignon = TrimString(signonList[i]);
    // TEMP HACK until morse fixes signon viewer functions
    currSignon = RemoveHTMLFormatting(currSignon);
    var site = currSignon.substring(0,currSignon.lastIndexOf(":"));
    var user = currSignon.substring(currSignon.lastIndexOf(":")+1,currSignon.length);
    AddItem("savesignonlist",[site,user],"signon_",i-1);
  }
}

/*** =================== IGNORED SIGNONS CODE =================== ***/

// function : <SignonViewer.js>::LoadReject();
// purpose  : reads rejected sites from interface and loads into tree
function LoadReject()
{
  rejectList = signonviewer.GetRejectValue();
  var delim = rejectList[0];
  rejectList = rejectList.split(delim);
  for(var i = 1; i < rejectList.length; i++)
  {
    var currSignon = TrimString(rejectList[i]);
    // TEMP HACK until morse fixes signon viewer functions
    currSignon = RemoveHTMLFormatting(currSignon);
    var site = currSignon.substring(0,currSignon.lastIndexOf(":"));
    var user = currSignon.substring(currSignon.lastIndexOf(":")+1,currSignon.length);
    AddItem("ignoredlist",[site,user],"reject_",i-1);
  }
}

/*** =================== NO PREVIEW FORMS CODE =================== ***/

// function : <SignonViewer.js>::LoadNopreview();
// purpose  : reads non-previewed forms from interface and loads into tree
function LoadNopreview()
{
  nopreviewList = signonviewer.GetNopreviewValue();
  var delim = nopreviewList[0];
  nopreviewList = nopreviewList.split(delim);
  for(var i = 1; i < nopreviewList.length; i++)
  {
    var currSignon = TrimString(nopreviewList[i]);
    // TEMP HACK until morse fixes signon viewer functions
    currSignon = RemoveHTMLFormatting(currSignon);
    var form = currSignon.substring(currSignon.lastIndexOf(":")+1,currSignon.length);
    AddItem("nopreviewlist",[form],"nopreview_",i-1);
  }
}

function onOK()
{
  var result = "|goneS|"+goneSS+"|goneR|"+goneIS;
  result += "|goneC|"+goneNC+"|goneP|"+goneFR+"|";
  signonviewer.SetValue(result, window);
  return true;
}

/*** =================== UTILITY FUNCTIONS =================== ***/

// function : <SignonViewer.js>::RemoveHTMLFormatting();
// purpose  : removes HTML formatting from input stream      
function RemoveHTMLFormatting(which)
{
  var ignoreon = false;
  var rv = "";
  for(var i = 0; i < which.length; i++)
  {
    if(which.charAt(i) == "<")
      ignoreon = true;
    if(which.charAt(i) == ">") {
      ignoreon = false;
      continue;
    }
    if(ignoreon)
      continue;
    rv += which.charAt(i);
  }
  return rv;
}

/*** =================== TREE MANAGEMENT CODE =================== ***/

// function : <SignonViewer.js>::AddItem();
// purpose  : utility function for adding items to a tree.
function AddItem(children,cells,prefix,idfier)
{
  var kids = document.getElementById(children);
  var item  = document.createElement("treeitem");
  var row   = document.createElement("treerow");
  for(var i = 0; i < cells.length; i++)
  {
    var cell  = document.createElement("treecell");
    cell.setAttribute("value", cells[i]);
    row.appendChild(cell);
  }
  item.appendChild(row);
  item.setAttribute("id",prefix + idfier);
  kids.appendChild(item);
}

// function : <SignonViewer.js>::DeleteItemSelected();
// purpose  : deletes all the signons that are selected
function DeleteItemSelected(tree, prefix, kids) {
  var delnarray = [];
  var rv = "";
  var cookietree = document.getElementById(tree);
  selitems = cookietree.selectedItems;
  for(var i = 0; i < selitems.length; i++) 
  { 
    delnarray[i] = document.getElementById(selitems[i].getAttribute("id"));
    var itemid = parseInt(selitems[i].getAttribute("id").substring(prefix.length,selitems[i].getAttribute("id").length));
    rv += (itemid + ",");
  }
  for(var i = 0; i < delnarray.length; i++) 
  { 
    document.getElementById(kids).removeChild(delnarray[i]);
  }
  return rv;
}