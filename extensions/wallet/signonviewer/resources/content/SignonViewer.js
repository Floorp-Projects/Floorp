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
 *   Mike Calmus
 */

/*** =================== INITIALISATION CODE =================== ***/

var kObserverService;
var kSignonBundle;
var gSelectUserInUse = false;

// interface variables
var signonviewer        = null;
var passwordmanager     = null;

// password-manager lists
var signons             = [];
var rejects             = [];
var deletedSignons      = [];
var deletedRejects      = [];

// form-manager lists
var nopreviews          = [];
var nocaptures          = [];
var deletedNopreviews   = [];
var deletedNocaptures   = [];

// delete the following lines !!!!!
var goneNP              = ""; // nopreview
var goneNC              = ""; // nocapture

// differentiate between password manager and form manager
var isPasswordManager = (window.arguments[0] == "S");

// variables for encryption
var encrypted = "";

function Startup() {

  // xpconnect to password manager interfaces
  signonviewer = Components.classes["@mozilla.org/signonviewer/signonviewer-world;1"].createInstance();
  signonviewer = signonviewer.QueryInterface(Components.interfaces.nsISignonViewer);
  passwordmanager = Components.classes["@mozilla.org/passwordmanager;1"].getService();
  passwordmanager = passwordmanager.QueryInterface(Components.interfaces.nsIPasswordManager);

  // determine if database is encrypted
  try {
    var pref = Components.classes["@mozilla.org/preferences-service;1"]
                     .getService(Components.interfaces.nsIPrefBranch);
    try {
      encrypted = pref.getBoolPref("wallet.crypto");
    } catch(e) {
      dump("wallet.crypto pref is missing from all.js");
    }

  } catch (ex) {
    dump("failed to get prefs service!\n");
    pref = null;
  }

  // determine whether to run in password-manager mode or form-manager mode
  var tabBox = document.getElementById("tabbox");
  kSignonBundle = document.getElementById("signonBundle");
  var element;
  if (isPasswordManager) {

    // be prepared to reload the display if anything changes
    kObserverService = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
    kObserverService.addObserver(signonReloadDisplay, "signonChanged", false);

    // be prepared to disable the buttons when selectuser dialog is in use
    kObserverService.addObserver(signonReloadDisplay, "signonSelectUser", false);

    signonsTree = document.getElementById("signonsTree");
    rejectsTree = document.getElementById("rejectsTree");

    // set initial password-manager tab
    element = document.getElementById("signonsTab");
    tabBox.selectedTab = element;

    // hide form-manager tabs
    element = document.getElementById("nopreviewsTab");
    element.hidden = "true";
    element = document.getElementById("nocapturesTab");
    element.hidden = "true"

    // load password manager items
    if (!LoadSignons()) {
      return; /* user failed to unlock the database */
    }
    LoadRejects();

  } else {

    nopreviewsTree = document.getElementById("nopreviewsTree");
    nocapturesTree = document.getElementById("nocapturesTree");

    // change title on window
    var wind = document.getElementById("signonviewer");
    wind.setAttribute("title", wind.getAttribute("alttitle"));

    // set initial form-manager tab
    element = document.getElementById("nopreviewsTab");
    tabBox.selectedTab = element;

    // hide password-manager tabs
    element = document.getElementById("signonsTab");
    element.hidden = "true";
    element = document.getElementById("signonsitesTab");
    element.hidden = "true";

    // load form manager items
    LoadNopreview();
    LoadNocapture();
  }

  // label the close button
  document.documentElement.getButton("accept").label = kSignonBundle.getString("close");
}

function Shutdown() {
  if (isPasswordManager) {
    kObserverService.removeObserver(signonReloadDisplay, "signonChanged");
    kObserverService.removeObserver(signonReloadDisplay, "signonSelectUser");
  }
}

var signonReloadDisplay = {
  observe: function(subject, topic, state) {
    if (topic == "signonChanged") {
      if (state == "signons") {
        signons.length = 0;
        if (lastSignonSortColumn == "host") {
          lastSignonSortAscending = !lastSignonSortAscending; // prevents sort from being reversed
        }
        LoadSignons();
      } else if (state == "rejects") {
        rejects.length = 0;
        if (lastRejectSortColumn == "host") {
          lastRejectSortAscending = !lastRejectSortAscending; // prevents sort from being reversed
        }
        LoadRejects();
      } else if (state == "nocaptures") {
        nocaptures.length = 0;
        if (lastNocaptureSortColumn == "host") {
          lastNocaptureSortAscending = !lastNocaptureSortAscending; // prevents sort from being reversed
        }
        LoadNocapture();
      } else if (state == "nopreviews") {
        nopreviews.length = 0;
        if (lastNopreviewSortColumn == "host") {
          lastNopreviewSortAscending = !lastNopreviewSortAscending; // prevents sort from being reversed
        }
        LoadNopreview();
      }
    } else if (topic == "signonSelectUser") {
      if (state == "suspend") {
        gSelectUserInUse = true;
        document.getElementById("removeSignon").disabled = true;
        document.getElementById("removeAllSignons").disabled = true;
      } else if (state == "resume") {
        gSelectUserInUse = false;
        var selections = GetTreeSelections(signonsTree);
        if (selections.length > 0) {
          document.getElementById("removeSignon").disabled = false;
        }
        if (signons.length > 0) {
          document.getElementById("removeAllSignons").disabled = false;
        }
      } else if (state == "inUse") {
        gSelectUserInUse = true;
      }
    }
  }
}

/*** =================== SAVED SIGNONS CODE =================== ***/

var signonsTreeView = {
  rowCount : 0,
  setTree : function(tree){},
  getImageSrc : function(row,column) {},
  getProgressMode : function(row,column) {},
  getCellValue : function(row,column) {},
  getCellText : function(row,column){
    var rv="";
    if (column=="siteCol") {
      rv = signons[row].host;
    } else if (column=="userCol") {
      rv = signons[row].user;
    }
    return rv;
  },
  isSeparator : function(index) {return false;},
  isSorted: function() { return false; },
  isContainer : function(index) {return false;},
  cycleHeader : function(aColId, aElt) {},
  getRowProperties : function(row,column,prop){},
  getColumnProperties : function(column,columnElement,prop){},
  getCellProperties : function(row,prop){}
 };
var signonsTree;

function Signon(number, host, user, rawuser) {
  this.number = number;
  this.host = host;
  this.user = user;
  this.rawuser = rawuser;
}

function LoadSignons() {
  // loads signons into table
  var enumerator = passwordmanager.enumerator;
  var count = 0;

  while (enumerator.hasMoreElements()) {
    var nextPassword;
    try {
      nextPassword = enumerator.getNext();

      nextPassword = nextPassword.QueryInterface(Components.interfaces.nsIPassword);
      var host = nextPassword.host;
      var user = nextPassword.user;
      var rawuser = user;

      // if no username supplied, try to parse it out of the url
      if (user == "") {
        var unused = { };
        var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService);
        var username;
        try {
          username = ioService.newURI(host, null, null).username;
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
        user = kSignonBundle.getFormattedString("encrypted", [user], 1);
      }

      signons[count] = new Signon(count++, host, user, rawuser);
    } catch(e) {
      /* An entry is corrupt. Go to next element. */
    }
  }
  signonsTreeView.rowCount = signons.length;

  // sort and display the table
  signonsTree.treeBoxObject.view = signonsTreeView;
  SignonColumnSort('host');

  // disable "remove all signons" button if there are no signons
  var element = document.getElementById("removeAllSignons");
  if (signons.length == 0 || gSelectUserInUse) {
    element.setAttribute("disabled","true");
  } else {
    element.removeAttribute("disabled");
  }
 
  return true;
}

function SignonSelected() {
  var selections = GetTreeSelections(signonsTree);
  if (selections.length && !gSelectUserInUse) {
    document.getElementById("removeSignon").removeAttribute("disabled");
  }
}

function DeleteSignon() {
  DeleteSelectedItemFromTree(signonsTree, signonsTreeView,
                                 signons, deletedSignons,
                                 "removeSignon", "removeAllSignons");
  FinalizeSignonDeletions();
}

function DeleteAllSignons() {
  DeleteAllFromTree(signonsTree, signonsTreeView,
                        signons, deletedSignons,
                        "removeSignon", "removeAllSignons");
  FinalizeSignonDeletions();
}

function FinalizeSignonDeletions() {
  for (var s=0; s<deletedSignons.length; s++) {
    passwordmanager.removeUser(deletedSignons[s].host, deletedSignons[s].rawuser);
  }
  deletedSignons.length = 0;
}

function HandleSignonKeyPress(e) {
  if (e.keyCode == 46) {
    DeleteSignonSelected();
  }
}

var lastSignonSortColumn = "";
var lastSignonSortAscending = false;

function SignonColumnSort(column) {
  lastSignonSortAscending =
    SortTree(signonsTree, signonsTreeView, signons,
                 column, lastSignonSortColumn, lastSignonSortAscending);
  lastSignonSortColumn = column;
}

/*** =================== REJECTED SIGNONS CODE =================== ***/

var rejectsTreeView = {
  rowCount : 0,
  setTree : function(tree){},
  getImageSrc : function(row,column) {},
  getProgressMode : function(row,column) {},
  getCellValue : function(row,column) {},
  getCellText : function(row,column){
    var rv="";
    if (column=="rejectCol") {
      rv = rejects[row].host;
    }
    return rv;
  },
  isSeparator : function(index) {return false;},
  isSorted: function() { return false; },
  isContainer : function(index) {return false;},
  cycleHeader : function(aColId, aElt) {},
  getRowProperties : function(row,column,prop){},
  getColumnProperties : function(column,columnElement,prop){},
  getCellProperties : function(row,prop){}
 };
var rejectsTree;

function Reject(number, host) {
  this.number = number;
  this.host = host;
}

function LoadRejects() {
  var enumerator = passwordmanager.rejectEnumerator;
  var count = 0;
  while (enumerator.hasMoreElements()) {
    var nextReject = enumerator.getNext();
    nextReject = nextReject.QueryInterface(Components.interfaces.nsIPassword);
    var host = nextReject.host;
    rejects[count] = new Reject(count++, host);
  }
  rejectsTreeView.rowCount = rejects.length;

  // sort and display the table
  rejectsTree.treeBoxObject.view = rejectsTreeView;
  RejectColumnSort('host');

  var element = document.getElementById("removeAllRejects");
  if (rejects.length == 0) {
    element.setAttribute("disabled","true");
  } else {
    element.removeAttribute("disabled");
  }
}

function RejectSelected() {
  var selections = GetTreeSelections(rejectsTree);
  if (selections.length) {
    document.getElementById("removeReject").removeAttribute("disabled");
  }
}

function DeleteReject() {
  DeleteSelectedItemFromTree(rejectsTree, rejectsTreeView,
                                 rejects, deletedRejects,
                                 "removeReject", "removeAllRejects");
  FinalizeRejectDeletions();
}

function DeleteAllRejects() {
  DeleteAllFromTree(rejectsTree, rejectsTreeView,
                        rejects, deletedRejects,
                        "removeReject", "removeAllRejects");
  FinalizeRejectDeletions();
}

function FinalizeRejectDeletions() {
  for (var r=0; r<deletedRejects.length; r++) {
    passwordmanager.removeReject(deletedRejects[r].host);
  }
  deletedRejects.length = 0;
}

function HandleRejectKeyPress(e) {
  if (e.keyCode == 46) {
    DeleteRejectSelected();
  }
}

var lastRejectSortColumn = "";
var lastRejectSortAscending = false;

function RejectColumnSort(column) {
  lastRejectSortAscending =
    SortTree(rejectsTree, rejectsTreeView, rejects,
                 column, lastRejectSortColumn, lastRejectSortAscending);
  lastRejectSortColumn = column;
}

/*** =================== NO PREVIEW FORMS CODE =================== ***/

var nopreviewsTreeView = {
  rowCount : 0,
  setTree : function(tree){},
  getImageSrc : function(row,column) {},
  getProgressMode : function(row,column) {},
  getCellValue : function(row,column) {},
  getCellText : function(row,column){
    var rv="";
    if (column=="nopreviewCol") {
      rv = nopreviews[row].host;
    }
    return rv;
  },
  isSeparator : function(index) {return false;},
  isSorted: function() { return false; },
  isContainer : function(index) {return false;},
  cycleHeader : function(aColId, aElt) {},
  getRowProperties : function(row,column,prop){},
  getColumnProperties : function(column,columnElement,prop){},
  getCellProperties : function(row,prop){}
 };
var nopreviewsTree;

function Nopreview(number, host) {
  this.number = number;
  this.host = host;
}

function LoadNopreview() {
  var list = signonviewer.getNopreviewValue();
  var count = 0;
  if (list.length > 0) {
    var delim = list[0];
    list = list.split(delim);
  }
  for (var i = 1; i < list.length; i++) {
    var host = TrimString(list[i]);
    nopreviews[count] = new Nopreview(count++, host);
  }
  nopreviewsTreeView.rowCount = nopreviews.length;

  // sort and display the table
  nopreviewsTree.treeBoxObject.view = nopreviewsTreeView;
  NopreviewColumnSort('host');

  var element = document.getElementById("removeAllNopreviews");
  if (nopreviews.length == 0) {
    element.setAttribute("disabled","true");
  } else {
    element.removeAttribute("disabled");
  }
}

function NopreviewSelected() {
  var selections = GetTreeSelections(nopreviewsTree);
  if (selections.length) {
    document.getElementById("removeNopreview").removeAttribute("disabled");
  }
}

function DeleteNopreview() {
  DeleteSelectedItemFromTree(nopreviewsTree, nopreviewsTreeView,
                                 nopreviews, deletedNopreviews,
                                 "removeNopreview", "removeAllNopreviews");
  FinalizeNopreviewDeletions();
}

function DeleteAllNopreviews() {
  DeleteAllFromTree(nopreviewsTree, nopreviewsTreeView,
                        nopreviews, deletedNopreviews,
                        "removeNopreview", "removeAllNopreviews");
  FinalizeNopreviewDeletions();
}

function FinalizeNopreviewDeletions() {
  var i;
  var result = "|goneP|";
  for (i=0; i<deletedNopreviews.length; i++) {
    result += deletedNopreviews[i].number;
    result += ",";
  }
  result += "|";
  signonviewer.setValue(result, window);
  deletedNoPreviews.length = 0;
}

function HandleNopreviewKeyPress(e) {
  if (e.keyCode == 46) {
    DeleteNopreviewSelected();
  }
}

var lastNopreviewSortColumn = "";
var lastNopreviewSortAscending = false;

function NopreviewColumnSort(column) {
  lastNopreviewSortAscending =
    SortTree(nopreviewsTree, nopreviewsTreeView, nopreviews,
                 column, lastNopreviewSortColumn, lastNopreviewSortAscending);
  lastNopreviewSortColumn = column;
}

/*** =================== NO CAPTURE FORMS CODE =================== ***/


var nocapturesTreeView = {
  rowCount : 0,
  setTree : function(tree){},
  getImageSrc : function(row,column) {},
  getProgressMode : function(row,column) {},
  getCellValue : function(row,column) {},
  getCellText : function(row,column){
    var rv="";
    if (column=="nocaptureCol") {
      rv = nocaptures[row].host;
    }
    return rv;
  },
  isSeparator : function(index) {return false;},
  isSorted: function() { return false; },
  isContainer : function(index) {return false;},
  cycleHeader : function(aColId, aElt) {},
  getRowProperties : function(row,column,prop){},
  getColumnProperties : function(column,columnElement,prop){},
  getCellProperties : function(row,prop){}
 };
var nocapturesTree;

function Nocapture(number, host) {
  this.number = number;
  this.host = host;
}

function LoadNocapture() {
  var list = signonviewer.getNocaptureValue();
  var count = 0;
  if (list.length > 0) {
    var delim = list[0];
    list = list.split(delim);
  }
  for (var i = 1; i < list.length; i++) {
    var host = TrimString(list[i]);
    nocaptures[count] = new Nocapture(count++, host);
  }
  nocapturesTreeView.rowCount = nocaptures.length;

  // sort and display the table
  nocapturesTree.treeBoxObject.view = nocapturesTreeView;
  NocaptureColumnSort('host');

  var element = document.getElementById("removeAllNocaptures");
  if (nocaptures.length == 0) {
    element.setAttribute("disabled","true");
  } else {
    element.removeAttribute("disabled");
  }
}

function NocaptureSelected() {
  var selections = GetTreeSelections(nocapturesTree);
  if (selections.length) {
    document.getElementById("removeNocapture").removeAttribute("disabled");
  }
}

function DeleteNocapture() {
  DeleteSelectedItemFromTree(nocapturesTree, nocapturesTreeView,
                                 nocaptures, deletedNocaptures,
                                 "removeNocapture", "removeAllNocaptures");
  FinalizeNocaptureDeletions();
}

function DeleteAllNocaptures() {
  DeleteAllFromTree(nocapturesTree, nocapturesTreeView,
                        nocaptures, deletedNocaptures,
                        "removeNocapture", "removeAllNocaptures");
  FinalizeNocaptureDeletions();
}

function FinalizeNocaptureDeletions() {
  var i;
  var result = "|goneC|";
  for (i=0; i<deletedNocaptures.length; i++) {
    result += deletedNocaptures[i].number;
    result += ",";
  }
  result += "|";
  signonviewer.setValue(result, window);
  deletedNoCaptures.length = 0;
}

function HandleNocaptureKeyPress(e) {
  if (e.keyCode == 46) {
    DeleteNocaptureSelected();
  }
}

var lastNocaptureSortColumn = "";
var lastNocaptureSortAscending = false;

function NocaptureColumnSort(column) {
  lastNocaptureSortAscending =
    SortTree(nocapturesTree, nocapturesTreeView, nocaptures,
                 column, lastNocaptureSortColumn, lastNocaptureSortAscending);
  lastNocaptureSortColumn = column;
}

/*** =================== GENERAL CODE =================== ***/

// Remove whitespace from both ends of a string
function TrimString(string)
{
  if (!string) {
    return "";
  }
  return string.replace(/(^\s+)|(\s+$)/g, '')
}

function doHelpButton() {
  if (isPasswordManager) {
     openHelp("password_mgr");
  } else {
     openHelp("forms_sites");
  }
}
