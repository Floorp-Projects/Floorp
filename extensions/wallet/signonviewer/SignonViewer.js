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
dump("entering startup\n");

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
  var element;
  if (isPasswordManager) {

    signonsOutliner = document.getElementById("signonsOutliner");
    rejectsOutliner = document.getElementById("rejectsOutliner");

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

    nopreviewsOutliner = document.getElementById("nopreviewsOutliner");
    nocapturesOutliner = document.getElementById("nocapturesOutliner");

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
}

/*** =================== SAVED SIGNONS CODE =================== ***/

var signonsOutlinerView = {
  rowCount : 0,
  setOutliner : function(outliner){},
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
var signonsOutliner;

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
  var signonBundle = document.getElementById("signonBundle");

  while (enumerator.hasMoreElements()) {
    var nextPassword;
    try {
      nextPassword = enumerator.getNext();
    } catch(e) {
      /* user supplied invalid database key */
      window.close();
      return false;
    }
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
      user = signonBundle.getFormattedString("encrypted", [user], 1);
    }

    signons[count] = new Signon(count++, host, user, rawuser);
  }
  signonsOutlinerView.rowCount = signons.length;

  // sort and display the table
  signonsOutliner.outlinerBoxObject.view = signonsOutlinerView;
  SignonColumnSort('host');

  // disable "remove all signons" button if there are no signons
  if (signons.length == 0) {
    document.getElementById("removeAllSignons").setAttribute("disabled","true");
  }
  return true;
}

function SignonSelected() {
  var selections = GetOutlinerSelections(signonsOutliner);
  if (selections.length) {
    document.getElementById("removeSignon").removeAttribute("disabled");
  }
}

function DeleteSignon() {
  DeleteSelectedItemFromOutliner(signonsOutliner, signonsOutlinerView,
                                 signons, deletedSignons,
                                 "removeSignon", "removeAllSignons");
}

function DeleteAllSignons() {
  DeleteAllFromOutliner(signonsOutliner, signonsOutlinerView,
                        signons, deletedSignons,
                        "removeSignon", "removeAllSignons");
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
    SortOutliner(signonsOutliner, signonsOutlinerView, signons,
                 column, lastSignonSortColumn, lastSignonSortAscending);
  lastSignonSortColumn = column;
}

/*** =================== REJECTED SIGNONS CODE =================== ***/

var rejectsOutlinerView = {
  rowCount : 0,
  setOutliner : function(outliner){},
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
var rejectsOutliner;

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
  rejectsOutlinerView.rowCount = rejects.length;

  // sort and display the table
  rejectsOutliner.outlinerBoxObject.view = rejectsOutlinerView;
  RejectColumnSort('host');

  if (rejects.length == 0) {
    document.getElementById("removeAllRejects").setAttribute("disabled","true");
  }
}

function RejectSelected() {
  var selections = GetOutlinerSelections(rejectsOutliner);
  if (selections.length) {
    document.getElementById("removeReject").removeAttribute("disabled");
  }
}

function DeleteReject() {
  DeleteSelectedItemFromOutliner(rejectsOutliner, rejectsOutlinerView,
                                 rejects, deletedRejects,
                                 "removeReject", "removeAllRejects");
}

function DeleteAllRejects() {
  DeleteAllFromOutliner(rejectsOutliner, rejectsOutlinerView,
                        rejects, deletedRejects,
                        "removeReject", "removeAllRejects");
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
    SortOutliner(rejectsOutliner, rejectsOutlinerView, rejects,
                 column, lastRejectSortColumn, lastRejectSortAscending);
  lastRejectSortColumn = column;
}

/*** =================== NO PREVIEW FORMS CODE =================== ***/

var nopreviewsOutlinerView = {
  rowCount : 0,
  setOutliner : function(outliner){},
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
var nopreviewsOutliner;

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
  nopreviewsOutlinerView.rowCount = nopreviews.length;

  // sort and display the table
  nopreviewsOutliner.outlinerBoxObject.view = nopreviewsOutlinerView;
  NopreviewColumnSort('host');

  if (nopreviews.length == 0) {
    document.getElementById("removeAllNopreviews").setAttribute("disabled","true");
  }
}

function NopreviewSelected() {
  var selections = GetOutlinerSelections(nopreviewsOutliner);
  if (selections.length) {
    document.getElementById("removeNopreview").removeAttribute("disabled");
  }
}

function DeleteNopreview() {
  DeleteSelectedItemFromOutliner(nopreviewsOutliner, nopreviewsOutlinerView,
                                 nopreviews, deletedNopreviews,
                                 "removeNopreview", "removeAllNopreviews");
}

function DeleteAllNopreviews() {
  DeleteAllFromOutliner(nopreviewsOutliner, nopreviewsOutlinerView,
                        nopreviews, deletedNopreviews,
                        "removeNopreview", "removeAllNopreviews");
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
    SortOutliner(nopreviewsOutliner, nopreviewsOutlinerView, nopreviews,
                 column, lastNopreviewSortColumn, lastNopreviewSortAscending);
  lastNopreviewSortColumn = column;
}

/*** =================== NO CAPTURE FORMS CODE =================== ***/


var nocapturesOutlinerView = {
  rowCount : 0,
  setOutliner : function(outliner){},
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
var nocapturesOutliner;

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
  nocapturesOutlinerView.rowCount = nocaptures.length;

  // sort and display the table
  nocapturesOutliner.outlinerBoxObject.view = nocapturesOutlinerView;
  NocaptureColumnSort('host');

  if (nocaptures.length == 0) {
    document.getElementById("removeAllNocaptures").setAttribute("disabled","true");
  }
}

function NocaptureSelected() {
  var selections = GetOutlinerSelections(nocapturesOutliner);
  if (selections.length) {
    document.getElementById("removeNocapture").removeAttribute("disabled");
  }
}

function DeleteNocapture() {
  DeleteSelectedItemFromOutliner(nocapturesOutliner, nocapturesOutlinerView,
                                 nocaptures, deletedNocaptures,
                                 "removeNocapture", "removeAllNocaptures");
}

function DeleteAllNocaptures() {
  DeleteAllFromOutliner(nocapturesOutliner, nocapturesOutlinerView,
                        nocaptures, deletedNocaptures,
                        "removeNocapture", "removeAllNocaptures");
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
    SortOutliner(nocapturesOutliner, nocapturesOutlinerView, nocaptures,
                 column, lastNocaptureSortColumn, lastNocaptureSortAscending);
  lastNocaptureSortColumn = column;
}

/*** =================== GENERAL CODE =================== ***/

function onAccept() {

  // 
  for (var s=0; s<deletedSignons.length; s++) {
    passwordmanager.removeUser(deletedSignons[s].host, deletedSignons[s].rawuser);
  }

  for (var r=0; r<deletedRejects.length; r++) {
    passwordmanager.removeReject(deletedRejects[r].host);
  }

  var i;
  var result = "|goneC|";
  for (i=0; i<deletedNocaptures.length; i++) {
    result += deletedNocaptures[i].number;
    result += ",";
  }
  result += "|goneP|";
  for (i=0; i<deletedNopreviews.length; i++) {
    result += deletedNopreviews[i].number;
    result += ",";
  }
  result += "|";
  signonviewer.setValue(result, window);
  return true;
}

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
