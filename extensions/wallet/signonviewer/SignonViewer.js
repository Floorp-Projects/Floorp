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
 *   Ben "Count XULula" Goodger
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
var goneNP              = ""; // nopreview
var goneNC              = ""; // nocapture
var deleted_signons_count = 1;
var deleted_rejects_count = 1;
var deleted_nopreviews_count = 1;
var deleted_nocaptures_count = 1;

// function : <SignonViewer.js>::Startup();
// purpose  : initialises interface, calls init functions for each page
function Startup()
{
  signonviewer = Components.classes["@mozilla.org/signonviewer/signonviewer-world;1"].createInstance();
  signonviewer = signonviewer.QueryInterface(Components.interfaces.nsISignonViewer);

  doSetOKCancel(onOK, null);  // init ok event handler

  // remove wallet functions (unless overruled by the "wallet.enabled" pref)
  try {
    pref = Components.classes['@mozilla.org/preferences;1'];
    pref = pref.getService();
    pref = pref.QueryInterface(Components.interfaces.nsIPref);
    try {
      if (!pref.GetBoolPref("wallet.enabled")) {
        var element;
        element = document.getElementById("nopreview");
        element.setAttribute("style","display: none;" );
        element = document.getElementById("nocapture");
        element.setAttribute("style","display: none;" );
      }
    } catch(e) {
      dump("wallet.enabled pref is missing from all.js");
    }
  } catch (ex) {
    dump("failed to get prefs service!\n");
    pref = null;
  }

  var tab = window.arguments[0];
  if (tab == "S") {
    element = document.getElementById("signonTab");
    element.setAttribute("selected","true" );
    element = document.getElementById("signonTab");
    element.setAttribute("style","display: inline;" );
    element = document.getElementById("signonSitesTab");
    element.setAttribute("style","display: inline;" );
    element = document.getElementById("panel");
    element.setAttribute("index","0" );
  } else if (tab == "W") {
    element = document.getElementById("signonviewer");
    element.setAttribute("title", element.getAttribute("alttitle"));
    element = document.getElementById("nopreview");
    element.setAttribute("selected","true" );
    element = document.getElementById("nopreview");
    element.setAttribute("style","display: inline;" );
    element = document.getElementById("nocapture");
    element.setAttribute("style","display: inline;" );
    element = document.getElementById("panel");
    element.setAttribute("index","2" );
  } else {
    /* invalid value for argument */
  }


  if (!LoadSignons()) {
    return; /* user failed to unlock the database */
  }
  LoadReject();
  LoadNopreview();
  LoadNocapture();
}

/*** =================== SAVED SIGNONS CODE =================== ***/

// function : <SignonViewer.js>::LoadSignons();
// purpose  : reads signons from interface and loads into tree
function LoadSignons()
{
  signonList = signonviewer.GetSignonValue();
  if (signonList.length == 1) {
    /* user supplied invalid database key */
    window.close();
    return false;
  }

  var delim = signonList[0];
  signonList = signonList.split(delim);
  for(var i = 1; i < signonList.length; i++)
  {
    var currSignon = TrimString(signonList[i]);
    // TEMP HACK until morse fixes signon viewer functions
    currSignon = RemoveHTMLFormatting(currSignon);
    /* parameter passed in is url:username */
    var site = currSignon.substring(0,currSignon.lastIndexOf(":"));
    var user = currSignon.substring(currSignon.lastIndexOf(":")+1,currSignon.length);
    if (user == "") {
      /* no username passed in, parse if out of url */
      var uri =
        Components.classes["@mozilla.org/network/standard-url;1"]
          .createInstance(Components.interfaces.nsIURI); 
      uri.spec = site; 
      if (user.username) {
        user = uri.username;
      } else {
        user = "<>";
      }
    }
    AddItem("savesignonlist",[site,user],"signon_",i-1);
  }
  if (deleted_signons_count >= signonList.length) {
    document.getElementById("removeAllSignons").setAttribute("disabled","true");
  }
  return true;
}

// function : <SignonViewer.js>::DeleteSignon();
// purpose  : deletes a particular signon
function DeleteSignon()
{
  deleted_signons_count += document.getElementById("signonstree").selectedItems.length;
  goneSS += DeleteItemSelected('signonstree','signon_','savesignonlist');
  DoButtonEnabling("signonstree");
  if (deleted_signons_count >= signonList.length) {
    document.getElementById("removeAllSignons").setAttribute("disabled","true");
  }
}

// function : <SignonViewer.js>::DeleteAllSignons();
// purpose  : deletes all the signons
function DeleteAllSignons() {
  // delete selected item
  goneSS += DeleteAllItems(signonList.length, "signon_", "savesignonlist");
  if( !document.getElementById("removeSignon").disabled ) {
    document.getElementById("removeSignon").setAttribute("disabled", "true")
  }
  document.getElementById("removeAllSignons").setAttribute("disabled","true");
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
    AddItem("ignoredlist",[site],"reject_",i-1);
  }
  if (deleted_rejects_count >= rejectList.length) {
    document.getElementById("removeAllSites").setAttribute("disabled","true");
  }
}

// function : <SignonViewer.js>::DeleteIgnoredSite();
// purpose  : deletes ignored site(s)
function DeleteIgnoredSite()
{
  deleted_rejects_count += document.getElementById("ignoretree").selectedItems.length;
  goneIS += DeleteItemSelected('ignoretree','reject_','ignoredlist');
  DoButtonEnabling("ignoretree");
  if (deleted_rejects_count >= rejectList.length) {
    document.getElementById("removeAllSites").setAttribute("disabled","true");
  }
}

// function : <SignonViewer.js>::DeleteAllSites();
// purpose  : deletes all the ignored sites
function DeleteAllSites() {
  // delete selected item
  goneIS += DeleteAllItems(rejectList.length, "reject_", "ignoredlist");
  if( !document.getElementById("removeIgnoredSite").disabled ) {
    document.getElementById("removeIgnoredSite").setAttribute("disabled", "true")
  }
  document.getElementById("removeAllSites").setAttribute("disabled","true");
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
    AddItem("nopreviewlist",[currSignon],"nopreview_",i-1);
  }
  if (deleted_nopreviews_count >= nopreviewList.length) {
    document.getElementById("removeAllNopreviews").setAttribute("disabled","true");
  }
}

// function : <SignonViewer>::DeleteNoPreviewForm()
// purpose  : deletes no-preview entry(s)
function DeleteNoPreviewForm()
{
  deleted_nopreviews_count += document.getElementById("nopreviewtree").selectedItems.length;
  goneNP += DeleteItemSelected('nopreviewtree','nopreview_','nopreviewlist');
  DoButtonEnabling("nopreviewtree");
  if (deleted_nopreviews_count >= nopreviewList.length) {
    document.getElementById("removeAllNopreviews").setAttribute("disabled","true");
  }
}

// function : <SignonViewer.js>::DeleteAllNopreviews();
// purpose  : deletes all the nopreview sites
function DeleteAllNopreviews() {
  // delete selected item
  goneNP += DeleteAllItems(nopreviewList.length, "nopreview_", "nopreviewlist");
  if( !document.getElementById("removeNoPreview").disabled ) {
    document.getElementById("removeNoPreview").setAttribute("disabled", "true")
  }
  document.getElementById("removeAllNopreviews").setAttribute("disabled","true");
}

/*** =================== NO CAPTURE FORMS CODE =================== ***/

// function : <SignonViewer.js>::LoadNocapture();
// purpose  : reads non-captured forms from interface and loads into tree
function LoadNocapture()
{
  nocaptureList = signonviewer.GetNocaptureValue();
  var delim = nocaptureList[0];
  nocaptureList = nocaptureList.split(delim);
  for(var i = 1; i < nocaptureList.length; i++)
  {
    var currSignon = TrimString(nocaptureList[i]);
    // TEMP HACK until morse fixes signon viewer functions
    currSignon = RemoveHTMLFormatting(currSignon);
    AddItem("nocapturelist",[currSignon],"nocapture_",i-1);
  }
  if (deleted_nocaptures_count >= nocaptureList.length) {
    document.getElementById("removeAllNocaptures").setAttribute("disabled","true");
  }
}

// function : <SignonViewer>::DeleteNoCaptureForm()
// purpose  : deletes no-capture entry(s)
function DeleteNoCaptureForm()
{
  deleted_nocaptures_count += document.getElementById("nocapturetree").selectedItems.length;
  goneNC += DeleteItemSelected('nocapturetree','nocapture_','nocapturelist');
  DoButtonEnabling("nocapturetree");
  if (deleted_nocaptures_count >= nocaptureList.length) {
    document.getElementById("removeAllNocaptures").setAttribute("disabled","true");
  }
}

// function : <SignonViewer.js>::DeleteAllNocaptures();
// purpose  : deletes all the nocapture sites
function DeleteAllNocaptures() {
  // delete selected item
  goneNC += DeleteAllItems(nocaptureList.length, "nocapture_", "nocapturelist");
  if( !document.getElementById("removeNoCapture").disabled ) {
    document.getElementById("removeNoCapture").setAttribute("disabled", "true")
  }
  document.getElementById("removeAllNocaptures").setAttribute("disabled","true");
}

/*** =================== GENERAL CODE =================== ***/

// function : <SignonViewer>::onOK()
// purpose  : dialog confirm & tidy up.
function onOK()
{
  var result = "|goneS|"+goneSS+"|goneR|"+goneIS;
  result += "|goneC|"+goneNC+"|goneP|"+goneNP+"|";
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
    cell.setAttribute("class", "propertylist");
    cell.setAttribute("value", cells[i])
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

// function : <SignonViewer.js>::DeleteAllItems();
// purpose  : deletes all the items
function DeleteAllItems(length, prefix, kids) {
  var delnarray = [];
  var rv = "";
  for(var i = 0; i < length; i++) 
  { 
    if (document.getElementById(prefix+i) != null) {
      document.getElementById(kids).removeChild(document.getElementById(prefix+i));
      rv += (i + ",");
    }
  }
  return rv;
}

// function: <SignonViewer.js>::HandleKeyPress();
// purpose : handles keypress events 
function HandleEvent( event, page )
{
  // click event
  if( event.type == "click" ) {
    var node = event.target;
    while ( node.nodeName != "tree" )
      node = node.parentNode;
    var tree = node;
    DoButtonEnabling( node.id );
  }
  
  // keypress event
  if( event.type == "keypress" && event.keyCode == 46 ) {
    switch( page ) {
    case 0:
      DeleteSignon();
      break;
    case 1:
      DeleteIgnoredSite();
      break;
    case 2:
      DeleteNoPreviewForm();
      break;
    case 2:
      DeleteNoCaptureForm();
      break;
    default:
      break;
    }
  }
}

function DoButtonEnabling( treeid )
{
  var tree = document.getElementById( treeid );
  switch( treeid ) {
  case "signonstree":
    var button = document.getElementById("removeSignon");
    break;
  case "ignoretree":
    var button = document.getElementById("removeIgnoredSite");
    break;
  case "nopreviewtree":
    var button = document.getElementById("removeNoPreview");
    break;
  case "nocapturetree":
    var button = document.getElementById("removeNoCapture");
    break;
  default:
    break;
  }
  if( button.getAttribute("disabled") && tree.selectedItems.length )
    button.removeAttribute("disabled", "true");
  else if( !button.getAttribute("disabled") && !tree.selectedItems.length )
    button.setAttribute("disabled","true");
}

// Remove whitespace from both ends of a string
function TrimString(string)
{
  if (!string) return "";
  return string.replace(/(^\s+)|(\s+$)/g, '')
}
