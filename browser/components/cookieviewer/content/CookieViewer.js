# -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is Mozilla Communicator client code, released March
# 31, 1998.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
#   Ben Goodger

// interface variables
var cookiemanager = Components.classes["@mozilla.org/cookiemanager;1"].getService();
    cookiemanager = cookiemanager.QueryInterface(Components.interfaces.nsICookieManager);
var gDateService = null;

// cookies list
var cookies              = [];
var deletedCookies       = [];

var cookieBundle;
var cookiesTree;
const nsICookie = Components.interfaces.nsICookie;

function Startup() {

  // intialize gDateService
  if (!gDateService) {
    const nsScriptableDateFormat_CONTRACTID = "@mozilla.org/intl/scriptabledateformat;1";
    const nsIScriptableDateFormat = Components.interfaces.nsIScriptableDateFormat;
    gDateService = Components.classes[nsScriptableDateFormat_CONTRACTID]
      .getService(nsIScriptableDateFormat);
  }

  //XXXBlake
  // I removed the observer stuff, so yes, there are edge cases where
  // if you're loading a page when you open prefs/cookie manager,
  // the cookie manager won't display the new cookie added.
  // I don't think it's a big deal (considering how hard it would be to fix)
}

function onOK() {
  window.opener.top.wsm.savePageData(window.location.href, window);
  window.opener.top.hPrefWindow.registerOKCallbackFunc(window.opener.cookieViewerOnPrefsOK);
}

function GetFields()
{
  var dataObject = {};
  dataObject.deletedCookies = deletedCookies;
  dataObject.cookies = cookies;
  dataObject.cookieBool = document.getElementById("checkbox").checked;
  return dataObject;
}

function SetFields(dataObject)
{
  if ('cookies' in dataObject)
    cookies = dataObject.cookies;

  if ('deletedCookies' in dataObject)
    deletedCookies = dataObject.deletedCookies;
  
  if ('cookieBool' in dataObject)
    document.getElementById("checkbox").checked = dataObject.cookieBool;

  loadCookies();
}


/*** =================== COOKIES CODE =================== ***/

var cookiesTreeView = {
  rowCount : 0,
  setTree : function(tree){},
  getImageSrc : function(row,column) {},
  getProgressMode : function(row,column) {},
  getCellValue : function(row,column) {},
  getCellText : function(row,column){
    var rv="";
    if (column.id=="domainCol") {
      rv = cookies[row].rawHost;
    } else if (column.id=="nameCol") {
      rv = cookies[row].name;
    }
    return rv;
  },
  isSeparator : function(index) {return false;},
  isSorted: function() { return false; },
  isContainer : function(index) {return false;},
  cycleHeader : function(column) {},
  getRowProperties : function(row,prop) {},
  getColumnProperties : function(column,prop) {},
  getCellProperties : function(row,column,prop) {}
 };

function Cookie(number,name,value,isDomain,host,rawHost,path,isSecure,expires) {
  this.number = number;
  this.name = name;
  this.value = value;
  this.isDomain = isDomain;
  this.host = host;
  this.rawHost = rawHost;
  this.path = path;
  this.isSecure = isSecure;
  this.expires = expires;
}

function loadCookies() {
  if (!cookieBundle)
    cookieBundle = document.getElementById("cookieBundle");

  if (!cookiesTree)
    cookiesTree = document.getElementById("cookiesTree");

  var dataObject = window.opener.top.hPrefWindow.wsm.dataManager.pageData[window.location.href].userData;
  if (!('cookies' in dataObject)) {
    // load cookies into a table
    var enumerator = cookiemanager.enumerator;
    var count = 0;
    while (enumerator.hasMoreElements()) {
      var nextCookie = enumerator.getNext();
      if (!nextCookie) break;
      nextCookie = nextCookie.QueryInterface(Components.interfaces.nsICookie);
      var host = nextCookie.host;
      cookies[count] =
        new Cookie(count++, nextCookie.name, nextCookie.value, nextCookie.isDomain, host,
                   (host.charAt(0)==".") ? host.substring(1,host.length) : host,
                   nextCookie.path, nextCookie.isSecure, nextCookie.expires);
    }
  }

  cookiesTreeView.rowCount = cookies.length;
  cookiesTree.treeBoxObject.view = cookiesTreeView;

 // sort by host column
  CookieColumnSort('rawHost');

  // disable "remove all cookies" button if there are no cookies
  document.getElementById("removeAllCookies").disabled = cookies.length == 0;
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

function CookieSelected() {
  var selections = GetTreeSelections(cookiesTree);
  if (selections.length) {
    document.getElementById("removeCookie").removeAttribute("disabled");
  } else {
    ClearCookieProperties();
    return true;
  }
    
  var idx = selections[0];
  if (idx >= cookies.length) {
    // Something got out of synch.  See bug 119812 for details
    dump("Tree and viewer state are out of sync! " +
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
            cookieBundle.getString("forSecureOnly") : 
            cookieBundle.getString("forAnyConnection")},
    {id: "ifl_expires", value: GetExpiresString(cookies[idx].expires)},
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
    ["ifl_name","ifl_value","ifl_host","ifl_path","ifl_isSecure","ifl_expires"];
  for (var prop=0; prop<properties.length; prop++) {
    document.getElementById(properties[prop]).value = "";
  }
}

function DeleteCookie() {
  DeleteSelectedItemFromTree(cookiesTree, cookiesTreeView,
                                 cookies, deletedCookies,
                                 "removeCookie", "removeAllCookies");
  if (!cookies.length) {
    ClearCookieProperties();
  }
}

function DeleteAllCookies() {
  ClearCookieProperties();
  DeleteAllFromTree(cookiesTree, cookiesTreeView,
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
    SortTree(cookiesTree, cookiesTreeView, cookies,
                 column, lastCookieSortColumn, lastCookieSortAscending);
  lastCookieSortColumn = column;
}
