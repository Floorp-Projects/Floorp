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
var passwordmanager     = null;
var signons             = [];
var rejects             = [];
var nopreviewList       = [];
var nocaptureList       = [];
var goneSS              = ""; // signon
var goneIS              = ""; // ignored site
var goneNP              = ""; // nopreview
var goneNC              = ""; // nocapture
var deleted_signons_count = 0;
var deleted_rejects_count = 0;
var deleted_nopreviews_count = 0;
var deleted_nocaptures_count = 0;
var nopreviews_count = 0;
var nocaptures_count = 0;
var pref;
var encrypted = "";

// function : <SignonViewer.js>::Startup();
// purpose  : initialises interface, calls init functions for each page
function Startup()
{
  signonviewer = Components.classes["@mozilla.org/signonviewer/signonviewer-world;1"].createInstance();
  signonviewer = signonviewer.QueryInterface(Components.interfaces.nsISignonViewer);

  passwordmanager = Components.classes["@mozilla.org/passwordmanager;1"].getService();
  passwordmanager = passwordmanager.QueryInterface(Components.interfaces.nsIPasswordManager);

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
        element.setAttribute("hidden", "true");
        element = document.getElementById("nocapture");
        element.setAttribute("hidden", "true");
      }
    } catch(e) {
      dump("wallet.enabled pref is missing from all.js");
    }

    try {
      encrypted = pref.GetBoolPref("wallet.crypto");
    } catch(e) {
      dump("wallet.crypto pref is missing from all.js");
    }

  } catch (ex) {
    dump("failed to get prefs service!\n");
    pref = null;
  }

  var tab = window.arguments[0];
  if (tab == "S") {
    element = document.getElementById("signonTabbox");
    element.selectedIndex = 0;

    // hide non-used tabs
    element = document.getElementById("nopreview");
    element.setAttribute("hidden", "true");
    element = document.getElementById("nocapture");
    element.setAttribute("hidden", "true");
    if (!LoadSignons()) {
      return; /* user failed to unlock the database */
    }
    LoadReject();
  } else if (tab == "W") {
    element = document.getElementById("signonviewer");
    element.setAttribute("title", element.getAttribute("alttitle"));
    
    element = document.getElementById("signonTabbox");
    element.selectedIndex = 2;
    // hide non-used tabs
    element = document.getElementById("signonTab");
    element.setAttribute("hidden", "true");
    element = document.getElementById("signonSitesTab");
    element.setAttribute("hidden", "true");

    LoadNopreview();
    LoadNocapture();
  } else {
    /* invalid value for argument */
  }
}

/*** =================== SAVED SIGNONS CODE =================== ***/

// function : <SignonViewer.js>::AddSignonToList();
// purpose  : creates an array of signon objects
function AddSignonToList(count, host, user, rawuser) {
  signons[count] = new Signon(count, host, user, rawuser);
}

// function : <SignonViewer.js>::Signon();
// purpose  : an home-brewed object that represents a individual signon
function Signon(number, host, user, rawuser)
{
  this.number = number;
  this.host = host;
  this.user = user;
  this.rawuser = rawuser;
}

// function : <SignonViewer.js>::LoadSignons();
// purpose  : reads signons from interface and loads into tree
function LoadSignons()
{
  var enumerator = passwordmanager.enumerator;
  var count = 0;
  var bundle = srGetStrBundle("chrome://communicator/locale/wallet/SignonViewer.properties");
  while (enumerator.hasMoreElements()) {
    try {
      var nextPassword = enumerator.getNext();
    } catch(e) {
      /* user supplied invalid database key */
      window.close();
      return false;
    }
    nextPassword = nextPassword.QueryInterface(Components.interfaces.nsIPassword);

    var host = nextPassword.host;
    var user = nextPassword.user;
    var rawuser = user;

    if (user == "") {
      /* no username passed in, parse it out of url */
      var unused = { };
      var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                    .getService(Components.interfaces.nsIIOService);
      var username;
      try {
        username = ioService.extractUrlPart(host, ioService.url_Username, unused, unused);
      } catch(e) {
        username = "";
      }
      if (username != "") {
        user = username;
      } else {
        user = "<>";
      }
    }

    if (encrypted) {
      user = bundle.formatStringFromName ("encrypted", [user], 1);
    }

    AddSignonToList(count, host, user, rawuser);
    AddItem("savesignonlist", [host,user], "signon_", count++);
  }
  if (count == 0) {
    document.getElementById("removeAllSignons").setAttribute("disabled","true");
  }
  return true;
}

// function : <SignonViewer.js>::DeleteSignon();
// purpose  : deletes a particular signon
function DeleteSignon()
{
  var signonstree = document.getElementById("signonstree");
  deleted_signons_count += signonstree.selectedItems.length;
  var newIndex = signonstree.selectedIndex;
  goneSS += DeleteItemSelected('signonstree','signon_','savesignonlist');
  var netSignonsCount = signons.length - deleted_signons_count;
  if (netSignonsCount) {
    signonstree.selectedIndex = (newIndex < netSignonsCount) ? newIndex : netSignonsCount-1;
  }
  DoButtonEnabling("signonstree");
  if (netSignonsCount <= 0) {
    document.getElementById("removeAllSignons").setAttribute("disabled","true");
  }
}

// function : <SignonViewer.js>::DeleteAllSignons();
// purpose  : deletes all the signons
function DeleteAllSignons() {
  // delete selected item
  goneSS += DeleteAllItems(signons.length, "signon_", "savesignonlist");
  if( !document.getElementById("removeSignon").disabled ) {
    document.getElementById("removeSignon").setAttribute("disabled", "true")
  }
  document.getElementById("removeAllSignons").setAttribute("disabled","true");
}

/*** =================== IGNORED SIGNONS CODE =================== ***/

// function : <SignonViewer.js>::AddRejectToList();
// purpose  : creates an array of reject objects
function AddRejectToList(count, host) {
  rejects[count] = new Reject(count, host);
}

// function : <SignonViewer.js>::Reject();
// purpose  : an home-brewed object that represents a individual reject
function Reject(number, host)
{
  this.number = number;
  this.host = host;
}

// function : <SignonViewer.js>::LoadReject();
// purpose  : reads rejected sites from interface and loads into tree
function LoadReject()
{
  var enumerator = passwordmanager.rejectEnumerator;
  var count = 0;
  while (enumerator.hasMoreElements()) {
    var nextReject = enumerator.getNext();
    nextReject = nextReject.QueryInterface(Components.interfaces.nsIPassword);
    var host = nextReject.host;
    AddRejectToList(count, host);
    AddItem("ignoredlist", [host], "reject_", count++);
  }
  if (count == 0) {
    document.getElementById("removeAllSites").setAttribute("disabled","true");
  }
  return true;
}

// function : <SignonViewer.js>::DeleteIgnoredSite();
// purpose  : deletes ignored site(s)
function DeleteIgnoredSite()
{
  var ignoretree = document.getElementById("ignoretree");
  deleted_rejects_count += ignoretree.selectedItems.length;
  var newIndex = ignoretree.selectedIndex;
  goneIS += DeleteItemSelected('ignoretree','reject_','ignoredlist');
  var netRejectsCount = rejects.length - deleted_rejects_count;
  if (netRejectsCount) {
    ignoretree.selectedIndex = (newIndex < netRejectsCount) ? newIndex : netRejectsCount-1;
  }
  DoButtonEnabling("ignoretree");
  if (netRejectsCount <= 0) {
    document.getElementById("removeAllSites").setAttribute("disabled","true");
  }
}

// function : <SignonViewer.js>::DeleteAllSites();
// purpose  : deletes all the ignored sites
function DeleteAllSites() {
  // delete selected item
  goneIS += DeleteAllItems(rejects.length, "reject_", "ignoredlist");
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
  nopreviewList = signonviewer.getNopreviewValue();
  if (nopreviewList.length > 0) {
    var delim = nopreviewList[0];
    nopreviewList = nopreviewList.split(delim);
  }
  for(var i = 1; i < nopreviewList.length; i++)
  {
    var currSignon = TrimString(nopreviewList[i]);
    // TEMP HACK until morse fixes signon viewer functions
    currSignon = RemoveHTMLFormatting(currSignon);
    AddItem("nopreviewlist",[currSignon],"nopreview_",i-1);
  }
  if (nopreviewList) {
    nopreviews_count = nopreviewList.length-1; // -1 because first item is always blank
  }
  if (nopreviews_count == 0) {
    document.getElementById("removeAllNopreviews").setAttribute("disabled","true");
  }
}

// function : <SignonViewer>::DeleteNoPreviewForm()
// purpose  : deletes no-preview entry(s)
function DeleteNoPreviewForm()
{
  var nopreviewtree = document.getElementById("nopreviewtree");
  deleted_nopreviews_count += nopreviewtree.selectedItems.length;
  var newIndex = nopreviewtree.selectedIndex;
  goneNP += DeleteItemSelected('nopreviewtree','nopreview_','nopreviewlist');
  var netNopreviewsCount = nopreviews_count - deleted_nopreviews_count;
  if (netNopreviewsCount) {
    nopreviewtree.selectedIndex = (newIndex < netNopreviewsCount) ? newIndex : netNopreviewsCount-1;
  }
  DoButtonEnabling("nopreviewtree");
  if (netNopreviewsCount <= 0) {
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
  nocaptureList = signonviewer.getNocaptureValue();
  if (nocaptureList.length > 0) {
    var delim = nocaptureList[0];
    nocaptureList = nocaptureList.split(delim);
  }
  for(var i = 1; i < nocaptureList.length; i++)
  {
    var currSignon = TrimString(nocaptureList[i]);
    // TEMP HACK until morse fixes signon viewer functions
    currSignon = RemoveHTMLFormatting(currSignon);
    AddItem("nocapturelist",[currSignon],"nocapture_",i-1);
  }
  if (nocaptureList) {
    nocaptures_count = nocaptureList.length-1; // -1 because first item is always blank
  }
  if (nocaptures_count == 0) {
    document.getElementById("removeAllNocaptures").setAttribute("disabled","true");
  }
}

// function : <SignonViewer>::DeleteNoCaptureForm()
// purpose  : deletes no-capture entry(s)
function DeleteNoCaptureForm()
{
  var nocapturetree = document.getElementById("nocapturetree");
  deleted_nocaptures_count += nocapturetree.selectedItems.length;
  var newIndex = nocapturetree.selectedIndex;
  goneNC += DeleteItemSelected('nocapturetree','nocapture_','nocapturelist');
  var netNocapturesCount = nocaptures_count - deleted_nocaptures_count;
  if (netNocapturesCount) {
    nocapturetree.selectedIndex = (newIndex < netNocapturesCount) ? newIndex : netNocapturesCount-1;
  }
  DoButtonEnabling("nocapturetree");
  if (netNocapturesCount <= 0) {
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
  var deletedSignons = [];
  deletedSignons = goneSS.split(",");
  var signonCount;
  for (signonCount=0; signonCount<deletedSignons.length-1; signonCount++) {
    passwordmanager.removeUser(signons[deletedSignons[signonCount]].host,
                               signons[deletedSignons[signonCount]].rawuser);
  }

  var deletedRejects = [];
  deletedRejects = goneIS.split(",");
  var rejectCount;
  for (rejectCount=0; rejectCount<deletedRejects.length-1; rejectCount++) {
    passwordmanager.removeReject(rejects[deletedRejects[rejectCount]].host);
  }

  var result = "|goneC|"+goneNC+"|goneP|"+goneNP+"|";
  signonviewer.setValue(result, window);
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
    cell.setAttribute("label", cells[i])
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
  var i;
  var selitems = cookietree.selectedItems;
  for(i = 0; i < selitems.length; i++) 
  { 
    delnarray[i] = document.getElementById(selitems[i].getAttribute("id"));
    var itemid = parseInt(selitems[i].getAttribute("id").substring(prefix.length,selitems[i].getAttribute("id").length));
    rv += (itemid + ",");
  }
  for(i = 0; i < delnarray.length; i++) 
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
  var button;
  switch( treeid ) {
  case "signonstree":
    button = document.getElementById("removeSignon");
    break;
  case "ignoretree":
    button = document.getElementById("removeIgnoredSite");
    break;
  case "nopreviewtree":
    button = document.getElementById("removeNoPreview");
    break;
  case "nocapturetree":
    button = document.getElementById("removeNoCapture");
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

