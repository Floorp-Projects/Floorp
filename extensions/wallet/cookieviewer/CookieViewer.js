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

// interface variables
var cookiemanager         = null;          // cookiemanager interfa
var permissionmanager     = null;          // permissionmanager interface
var gDateService = null;

// cookies and permissions list
var cookies               = [];
var permissions           = [];
var deletedCookies       = [];
var deletedPermissions   = [];

// differentiate between cookies and images
var isImages = (window.arguments[0] == "imageManager");
const cookieType = 0;
const imageType = 1;

var cookieBundle;

function Startup() {

  // arguments passed to this routine:
  //   cookieManager 
  //   cookieManagerFromIcon
  //   imageManager
 
  // xpconnect to cookiemanager/permissionmanager interfaces
  cookiemanager = Components.classes["@mozilla.org/cookiemanager;1"].getService();
  cookiemanager = cookiemanager.QueryInterface(Components.interfaces.nsICookieManager);
  permissionmanager = Components.classes["@mozilla.org/permissionmanager;1"].getService();
  permissionmanager = permissionmanager.QueryInterface(Components.interfaces.nsIPermissionManager);

  // intialize gDateService
  if (!gDateService) {
    const nsScriptableDateFormat_CONTRACTID = "@mozilla.org/intl/scriptabledateformat;1";
    const nsIScriptableDateFormat = Components.interfaces.nsIScriptableDateFormat;
    gDateService = Components.classes[nsScriptableDateFormat_CONTRACTID]
      .getService(nsIScriptableDateFormat);
  }

  // intialize string bundle
  cookieBundle = document.getElementById("cookieBundle");

  // determine if labelling is for cookies or images
  try {
    var tabBox = document.getElementById("tabbox");
    var element;
    if (!isImages) {
      element = document.getElementById("cookiesTab");
      tabBox.selectedTab = element;
    } else {
      element = document.getElementById("cookieviewer");
      element.setAttribute("title", cookieBundle.getString("imageTitle"));
      element = document.getElementById("permissionsTab");
      element.label = cookieBundle.getString("tabBannedImages");
      tabBox.selectedTab = element;
      element = document.getElementById("permissionsText");
      element.value = cookieBundle.getString("textBannedImages");
      element = document.getElementById("cookiesTab");
      element.hidden = "true";
    }
  } catch(e) {
  }

  // load in the cookies and permissions
  cookiesOutliner = document.getElementById("cookiesOutliner");
  permissionsOutliner = document.getElementById("permissionsOutliner");
  if (!isImages) {
    loadCookies();
  }
  loadPermissions();

  window.sizeToContent();
}

/*** =================== COOKIES CODE =================== ***/

const nsICookie = Components.interfaces.nsICookie;

var cookiesOutlinerView = {
  rowCount : 0,
  setOutliner : function(outliner){},
  getCellText : function(row,column){
    var rv="";
    if (column=="domainCol") {
      rv = cookies[row].rawHost;
    } else if (column=="nameCol") {
      rv = cookies[row].name;
    } else if (column=="statusCol") {
      rv = GetStatusString(cookies[row].status);
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
var cookiesOutliner;

function Cookie(number,name,value,isDomain,host,rawHost,path,isSecure,expires,
                status,policy) {
  this.number = number;
  this.name = name;
  this.value = value;
  this.isDomain = isDomain;
  this.host = host;
  this.rawHost = rawHost;
  this.path = path;
  this.isSecure = isSecure;
  this.expires = expires;
  this.status = status;
  this.policy = policy;
}

function loadCookies() {
  // load cookies into a table
  var enumerator = cookiemanager.enumerator;
  var count = 0;
  var showPolicyField = false;
  while (enumerator.hasMoreElements()) {
    var nextCookie = enumerator.getNext();
    nextCookie = nextCookie.QueryInterface(Components.interfaces.nsICookie);
    var host = nextCookie.host;
    if (nextCookie.policy != nsICookie.POLICY_UNKNOWN) {
      showPolicyField = true;
    }
    cookies[count] =
      new Cookie(count++, nextCookie.name, nextCookie.value, nextCookie.isDomain, host,
                 (host.charAt(0)==".") ? host.substring(1,host.length) : host,
                 nextCookie.path, nextCookie.isSecure, nextCookie.expires,
                 nextCookie.status, nextCookie.policy);
  }
  cookiesOutlinerView.rowCount = cookies.length;

  // sort and display the table
  cookiesOutliner.outlinerBoxObject.view = cookiesOutlinerView;
  if (window.arguments[0] == "cookieManagerFromIcon") { // came here by clicking on cookie icon

    // turn off the icon
    var observerService = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
    observerService.notifyObservers(null, "cookieIcon", "off");

    // unhide the status column and sort by reverse order on that column
    document.getElementById("statusCol").removeAttribute("hidden");
    lastCookieSortAscending = true; // force order to have blanks last instead of vice versa
    CookieColumnSort('status');

  } else {
    // sort by host column
    CookieColumnSort('rawHost');
  }

  // disable "remove all cookies" button if there are no cookies
  if (cookies.length == 0) {
    document.getElementById("removeAllCookies").setAttribute("disabled","true");
  }

  // show policy field if at least one cookie has a policy
  if (showPolicyField) {
    document.getElementById("policyField").removeAttribute("hidden");
  }
}

function GetExpiresString(expires) {
  if (expires) {
    var date = new Date(1000*expires);
    return gDateService.FormatDateTime("", gDateService.dateFormatLong,
                                       gDateService.timeFormatSeconds, date.getFullYear(),
                                       date.getMonth()+1, date.getDate(), date.getHours(),
                                       date.getMinutes(), date.getSeconds());
  }
  return cookieBundle.getString("AtEndOfSession");
}

function GetStatusString(status) {
  switch (status) {
    case nsICookie.STATUS_ACCEPTED:
      return cookieBundle.getString("accepted");
    case nsICookie.STATUS_FLAGGED:
      return cookieBundle.getString("flagged");
    case nsICookie.STATUS_DOWNGRADED:
      return cookieBundle.getString("downgraded");
  }
  return "";
}

function GetPolicyString(policy) {
  switch (policy) {
    case nsICookie.POLICY_NONE:
      return cookieBundle.getString("policyUnstated");
    case nsICookie.POLICY_NO_CONSENT:
      return cookieBundle.getString("policyNoConsent");
    case nsICookie.POLICY_IMPLICIT_CONSENT:
      return cookieBundle.getString("policyImplicitConsent");
    case nsICookie.POLICY_EXPLICIT_CONSENT:
      return cookieBundle.getString("policyExplicitConsent");
    case nsICookie.POLICY_NO_II:
      return cookieBundle.getString("policyNoIICollected");
  }
  return "";
}

function CookieSelected() {
  var selections = GetOutlinerSelections(cookiesOutliner);
  if (selections.length) {
    document.getElementById("removeCookie").removeAttribute("disabled");
  } else {
    ClearCookieProperties();
    return true;
  }
    
  var idx = selections[0];
  if (idx >= cookies.length) {
    // Something got out of synch.  See bug 119812 for details
    dump("Outliner and viewer state are out of sync! " +
         "Help us figure out the problem in bug 119812");
    return;
  }

  var props = [
    {id: "ifl_name", value: cookies[idx].name},
    {id: "ifl_value", value: cookies[idx].value}, 
    {id: "ifl_isDomain",
     value: cookies[idx].isDomain ?
            cookieBundle.getString("domainColon") : cookieBundle.getString("hostColon")},
    {id: "ifl_host", value: cookies[idx].host},
    {id: "ifl_path", value: cookies[idx].path},
    {id: "ifl_isSecure",
     value: cookies[idx].isSecure ?
            cookieBundle.getString("yes") : cookieBundle.getString("no")},
    {id: "ifl_expires", value: GetExpiresString(cookies[idx].expires)},
    {id: "ifl_policy", value: GetPolicyString(cookies[idx].policy)}
  ];

  var value;
  var field;
  for (var i = 0; i < props.length; i++)
  {
    field = document.getElementById(props[i].id);
    if ((selections.length > 1) && (props[i].id != "ifl_isDomain")) {
      value = ""; // clear field if multiple selections
    } else {
      value = props[i].value;
    }
    field.value = value;
  }
  return true;
}

function ClearCookieProperties() {
  var properties = 
    ["ifl_name","ifl_value","ifl_host","ifl_path","ifl_isSecure","ifl_expires","ifl_policy"];
  for (var prop=0; prop<properties.length; prop++) {
    document.getElementById(properties[prop]).value = "";
  }
}

function DeleteCookie() {
  DeleteSelectedItemFromOutliner(cookiesOutliner, cookiesOutlinerView,
                                 cookies, deletedCookies,
                                 "removeCookie", "removeAllCookies");
  if (!cookies.length) {
    ClearCookieProperties();
  }
}

function DeleteAllCookies() {
  ClearCookieProperties();
  DeleteAllFromOutliner(cookiesOutliner, cookiesOutlinerView,
                        cookies, deletedCookies,
                        "removeCookie", "removeAllCookies");
}

function HandleCookieKeyPress(e) {
  if (e.keyCode == 46) {
    DeleteCookie();
  }
}

var lastCookieSortColumn = "";
var lastCookieSortAscending = false;

function CookieColumnSort(column) {
  lastCookieSortAscending =
    SortOutliner(cookiesOutliner, cookiesOutlinerView, cookies,
                 column, lastCookieSortColumn, lastCookieSortAscending);
  lastCookieSortColumn = column;
}

/*** =================== PERMISSIONS CODE =================== ***/

var permissionsOutlinerView = {
  rowCount : 0,
  setOutliner : function(outliner){},
  getCellText : function(row,column){
    var rv="";
    if (column=="siteCol") {
      rv = permissions[row].rawHost;
    } else if (column=="statusCol") {
      rv = permissions[row].capability;
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
var permissionsOutliner;

function Permission(number, host, rawHost, type, capability) {
  this.number = number;
  this.host = host;
  this.rawHost = rawHost;
  this.type = type;
  this.capability = capability;
}

function loadPermissions() {
  // load permissions into a table
  var enumerator = permissionmanager.enumerator;
  var count = 0;
  var contentStr;
  var canStr, cannotStr;
  if (isImages) {
    canStr="canImages";
    cannotStr="cannotImages";
  } else {
    canStr="can";
    cannotStr="cannot";
  }
  while (enumerator.hasMoreElements()) {
    var nextPermission = enumerator.getNext();
    nextPermission = nextPermission.QueryInterface(Components.interfaces.nsIPermission);
    if (nextPermission.type == (isImages ? imageType : cookieType)) {
      var host = nextPermission.host;
      permissions[count] = 
        new Permission(count++, host,
                       (host.charAt(0)==".") ? host.substring(1,host.length) : host,
                       nextPermission.type,
                       cookieBundle.getString(nextPermission.capability?canStr:cannotStr));
    }
  }
  permissionsOutlinerView.rowCount = permissions.length;

  // sort and display the table
  permissionsOutliner.outlinerBoxObject.view = permissionsOutlinerView;
  PermissionColumnSort('rawHost');

  // disable "remove all" button if there are no cookies/images
  if (permissions.length == 0) {
    document.getElementById("removeAllPermissions").setAttribute("disabled","true");
  }

}

function PermissionSelected() {
  var selections = GetOutlinerSelections(permissionsOutliner);
  if (selections.length) {
    document.getElementById("removePermission").removeAttribute("disabled");
  }
}

function DeletePermission() {
  DeleteSelectedItemFromOutliner(permissionsOutliner, permissionsOutlinerView,
                                 permissions, deletedPermissions,
                                 "removePermission", "removeAllPermissions");
}

function DeleteAllPermissions() {
  DeleteAllFromOutliner(permissionsOutliner, permissionsOutlinerView,
                        permissions, deletedPermissions,
                        "removePermission", "removeAllPermissions");
}

function HandlePermissionKeyPress(e) {
  if (e.keyCode == 46) {
    DeletePermission();
  }
}

var lastPermissionSortColumn = "";
var lastPermissionSortAscending = false;

function PermissionColumnSort(column) {
  lastPermissionSortAscending =
    SortOutliner(permissionsOutliner, permissionsOutlinerView, permissions,
                 column, lastPermissionSortColumn, lastPermissionSortAscending);
  lastPermissionSortColumn = column;
}

/*** =================== GENERAL CODE =================== ***/

function onAccept() {

  for (var c=0; c<deletedCookies.length; c++) {
    cookiemanager.remove(deletedCookies[c].host,
                         deletedCookies[c].name,
                         deletedCookies[c].path,
                         document.getElementById("checkbox").checked);
  }

  for (var p=0; p<deletedPermissions.length; p++) {
    permissionmanager.remove(deletedPermissions[p].host, deletedPermissions[p].type);
  }

  return true;
}

/*** ============ CODE FOR HELP BUTTON =================== ***/


function getSelectedTab()
{
  var selTab = document.getElementById('tabbox').selectedTab;
  var selTabID = selTab.getAttribute('id');
  if (selTabID == 'cookiesTab') {
    key = "cookies_stored";
  } else {
    key = "cookie_sites";
  }  
  return key;
}


function doHelpButton() {
   var uri = getSelectedTab();
   openHelp(uri);
}
