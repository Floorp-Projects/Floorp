# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
# 
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
# 
# The Original Code is Mozilla.org Code.
# 
# The Initial Developer of the Original Code is
# Doron Rosenberg.
# Portions created by the Initial Developer are Copyright (C) 2001
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
#   Ben Goodger <ben@netscape.com> (Original Author)
#   Dan Dwitte <dwitte@stanford.edu>
#   Pierre Chanial <p_ch@verizon.net>
# 
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
# 
# ***** END LICENSE BLOCK *****

#define DL_RETAIN_WINDOW 0

var _elementIDs = ["histDay", "browserCacheDiskCache", "cookieBehavior", "enableCookies",
                   "enableCookiesForOriginatingSiteOnly", "enableCookiesForCurrentSessionOnly",
                   "enableCookiesButAskFirst", "enableFormFill", "enablePasswords", 
                   "downloadsRetentionPolicy"];

function Startup() {

  // Initially disable the clear buttons when needed
  var globalHistory = Components.classes["@mozilla.org/browser/global-history;2"].getService(Components.interfaces.nsIBrowserHistory);
  document.getElementById("history").setAttribute("cleardisabled", globalHistory.count == 0);
  
  var cookieMgr = Components.classes["@mozilla.org/cookiemanager;1"].getService();
  cookieMgr = cookieMgr.QueryInterface(Components.interfaces.nsICookieManager);
  var e = cookieMgr.enumerator;
  document.getElementById("cookies").setAttribute("cleardisabled", !e.hasMoreElements());

  var passwdMgr = Components.classes["@mozilla.org/passwordmanager;1"].getService();
  passwdMgr = passwdMgr.QueryInterface(Components.interfaces.nsIPasswordManager);
  e = passwdMgr.enumerator;
  document.getElementById("passwords").setAttribute("cleardisabled", !e.hasMoreElements());

  try {
    e = PrivacyPanel.getDownloads();
    var hasDownloads = e.hasMoreElements();
  }
  catch (e) {
    hasDownloads = false;
  }
  document.getElementById("downloads").setAttribute("cleardisabled", !hasDownloads);
  
  var formHistory = Components.classes["@mozilla.org/satchel/form-history;1"]
                              .getService(Components.interfaces.nsIFormHistory);
  document.getElementById("formfill").setAttribute("cleardisabled", formHistory.rowCount == 0);
  
  // set up the pref checkboxes according to the network.cookie.cookieBehavior pref
  // 0: enabled
  // 1: enabled for originating website only
  // 2: disabled
  var cookieBehavior = document.getElementById("cookieBehavior").getAttribute("value");
  document.getElementById("enableCookies").checked = cookieBehavior != 2;
  document.getElementById("enableCookiesForOriginatingSiteOnly").checked = cookieBehavior == 1;
  updateCookieBroadcaster();

  var categories = document.getElementById("privacyCategories");
  categories.addEventListener("clear", PrivacyPanel.clear, false);

  // XXXben - we do this because of a bug with the download retention window menulist. 
  // The bug is that when the Options dialog opens, or you switch from another panel to
  // this panel, style is incompletely resolved on the menulist's display area anonymous
  // content - it is resolved on the all a/c subcomponents *except* menulist-label (the
  // text nodes)... and (as a result, I think) when style is resolved later as the menulist
  // goes from visbility: collapse to being visible, the menulist-label has the wrong parent
  // style context which causes the style context parent checking to complain heartily. The
  // symptom is that the menulist is not initialized with the currently selected value from
  // preferences. I suspect this is related to the fact that the menulist is inserted into
  // an XBL insertion point, as this problem does not occur when the menulist is placed outside
  // the bound element. dbaron is helping me with this with a reduced test case, but in 
  // the meantime, I'm working around this bug by placing the menulist outside the bound element
  // until it is completely initialized and then scooting it in, which is what this code does. 
  var drb = document.getElementById("downloadsRetentionBox");
  var drp = document.getElementById("downloadsRetentionPolicy");
  drp.removeAttribute("hidden");
  document.documentElement.removeChild(drp);
  drb.appendChild(drp);
}

function unload()
{
  var categories = document.getElementById("privacyCategories");
  for (var i = 0; i < categories.childNodes.length; ++i) {
    var expander = categories.childNodes[i];
    document.persist(expander.id, "open");
  }  
}

function cookieViewerOnPrefsOK()
{
  var dataObject = parent.hPrefWindow.wsm.dataManager.pageData["chrome://browser/content/cookieviewer/CookieViewer.xul"].userData;
  if ('deletedCookies' in dataObject) {
    var cookiemanager = Components.classes["@mozilla.org/cookiemanager;1"].getService();
    cookiemanager = cookiemanager.QueryInterface(Components.interfaces.nsICookieManager);

    for (var p = 0; p < dataObject.deletedCookies.length; ++p) {
      cookiemanager.remove(dataObject.deletedCookies[p].host,
                           dataObject.deletedCookies[p].name,
                           dataObject.deletedCookies[p].path,
                           dataObject.cookieBool);
    }
  }
}

var PrivacyPanel = {
  confirm: function (aTitle, aMessage, aActionButtonLabel)
  {
    var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);

    var flags = promptService.BUTTON_TITLE_IS_STRING * promptService.BUTTON_POS_0;
    flags += promptService.BUTTON_TITLE_CANCEL * promptService.BUTTON_POS_1;

    rv = promptService.confirmEx(window, aTitle, aMessage, flags, aActionButtonLabel, null, null, null, { value: 0 });
    return rv == 0;
  },

  clear: function (aEvent) {
    if (aEvent.target.localName != "expander") 
      return;
      
    var rv = PrivacyPanel.clearData[aEvent.target.id](true);
    if (rv)
      aEvent.target.setAttribute("cleardisabled", "true");
  },
  
  clearAll: function () {
    var privacyBundle = document.getElementById("privacyBundle");
    var title = privacyBundle.getString("prefRemoveAllTitle");
    var msg = privacyBundle.getString("prefRemoveAllMsg");
    var button = privacyBundle.getString("prefRemoveAllRemoveButton");
    
    if (PrivacyPanel.confirm(title, msg, button)) {
      for (var fn in PrivacyPanel.clearData) {
        PrivacyPanel.clearData[fn](false);
        document.getElementById(fn).setAttribute("cleardisabled", "true");
      }
    }
  },
  
  getDownloads: function() {
    var dlMgr = Components.classes["@mozilla.org/download-manager;1"].getService(Components.interfaces.nsIDownloadManager);
    var ds = dlMgr.datasource;
    
    var rdfs = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);
    var root = rdfs.GetResource("NC:DownloadsRoot");
    
    var rdfc = Components.classes["@mozilla.org/rdf/container;1"].createInstance(Components.interfaces.nsIRDFContainer);
    rdfc.Init(ds, root);
  
    return rdfc.GetElements();
  },
  
  clearData: { 
    // The names of these functions match the id of the expander in the XUL file that correspond
    // to them. 
    history: function ()
    {
      var globalHistory = Components.classes["@mozilla.org/browser/global-history;2"]
                                    .getService(Components.interfaces.nsIBrowserHistory);
      globalHistory.removeAllPages();
      
      return true;
    },
    
    cache: function ()
    {
      function clearCacheOfType(aType)
      {
        var classID = Components.classes["@mozilla.org/network/cache-service;1"];
        var cacheService = classID.getService(Components.interfaces.nsICacheService);
        cacheService.evictEntries(aType);
      }
    
      clearCacheOfType(Components.interfaces.nsICache.STORE_ON_DISK);
      clearCacheOfType(Components.interfaces.nsICache.STORE_IN_MEMORY);
      
      return true;
    },
    
    cookies: function ()
    { 
      var cookieMgr = Components.classes["@mozilla.org/cookiemanager;1"].getService();
      cookieMgr = cookieMgr.QueryInterface(Components.interfaces.nsICookieManager);

      var e = cookieMgr.enumerator;
      var cookies = [];
      while (e.hasMoreElements()) {
        var cookie = e.getNext().QueryInterface(Components.interfaces.nsICookie);
        cookies.push(cookie);
      }

      for (var i = 0; i < cookies.length; ++i)
        cookieMgr.remove(cookies[i].host, cookies[i].name, cookies[i].path, false);
      
      return true;
    },
    
    formfill: function ()
    {
      var formHistory = Components.classes["@mozilla.org/satchel/form-history;1"]
                                  .getService(Components.interfaces.nsIFormHistory);
      formHistory.removeAllEntries();
      
      return true;
    },
    
    downloads: function ()
    {
      var dlMgr = Components.classes["@mozilla.org/download-manager;1"].getService(Components.interfaces.nsIDownloadManager);
      try {
        var downloads = PrivacyPanel.getDownloads();
      }
      catch (e) {
        return true;
      }

      var rdfs = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);
      var state = rdfs.GetResource("http://home.netscape.com/NC-rdf#DownloadState");
      var ds = dlMgr.datasource;
      var dls = [];
      
      while (downloads.hasMoreElements()) {
        var download = downloads.getNext().QueryInterface(Components.interfaces.nsIRDFResource);
        dls.push(download);
      }
      dlMgr.startBatchUpdate();
      for (var i = 0; i < dls.length; ++i) {
        try {
          dlMgr.removeDownload(dls[i].Value);
        }
        catch (e) {
        }
      }
      dlMgr.endBatchUpdate();  
      
      return true;
    },
    
    passwords: function (aShowPrompt)
    {
      var privacyBundle = document.getElementById("privacyBundle");
      var title = privacyBundle.getString("prefRemovePasswdsTitle");
      var msg = privacyBundle.getString("prefRemovePasswdsMsg");
      var button = privacyBundle.getString("prefRemovePasswdsRemoveButton");
      
      if (!aShowPrompt || PrivacyPanel.confirm(title, msg, button)) {
        var passwdMgr = Components.classes["@mozilla.org/passwordmanager;1"].getService();
        passwdMgr = passwdMgr.QueryInterface(Components.interfaces.nsIPasswordManager);

        var e = passwdMgr.enumerator;
        var passwds = [];
        while (e.hasMoreElements()) {
          var passwd = e.getNext().QueryInterface(Components.interfaces.nsIPassword);
          passwds.push(passwd);
        }
        
        for (var i = 0; i < passwds.length; ++i)
          passwdMgr.removeUser(passwds[i].host, passwds[i].user);

        return true;
      }
      return false;
    }
  }  
}

function viewCookies() 
{
  window.openDialog("chrome://browser/content/cookieviewer/CookieViewer.xul","_blank",
                    "chrome,resizable=yes", "cookieManager");
}

function cookieExceptions()
{
  window.openDialog("chrome://browser/content/cookieviewer/CookieExceptions.xul","_blank",
                    "chrome,resizable=yes", "cookieExceptions");
}

function viewSignons() 
{
    window.openDialog("chrome://passwordmgr/content/passwordManager.xul","_blank",
                      "chrome,resizable=yes", "8");
}

function updateCookieBehavior()
{
  var cookiesEnabled = document.getElementById("enableCookies").checked;
  var cookiesOriginating = document.getElementById("enableCookiesForOriginatingSiteOnly").checked;
  document.getElementById("cookieBehavior").setAttribute("value", cookiesEnabled ? (cookiesOriginating ? 1 : 0) : 2);
}

function updateCookieBroadcaster()
{
  var broadcaster = document.getElementById("cookieBroadcaster");
  var checkbox    = document.getElementById("enableCookies");
  if (!checkbox.checked) {
    broadcaster.setAttribute("disabled", "true");
    document.getElementById("enableCookiesForOriginatingSiteOnly").checked = false;
    document.getElementById("enableCookiesForCurrentSessionOnly").checked = false;
    document.getElementById("enableCookiesButAskFirst").checked = false;
  }
  else
    broadcaster.removeAttribute("disabled");
}

function onPrefsOK()
{
  var permissionmanager = Components.classes["@mozilla.org/permissionmanager;1"].getService();
  permissionmanager = permissionmanager.QueryInterface(Components.interfaces.nsIPermissionManager);

  var dataObject = parent.hPrefWindow.wsm.dataManager.pageData["chrome://browser/content/cookieviewer/CookieExceptions.xul"].userData;
  if ('deletedPermissions' in dataObject) {
    for (var p = 0; p < dataObject.deletedPermissions.length; ++p) {
      permissionmanager.remove(dataObject.deletedPermissions[p].host, dataObject.deletedPermissions[p].type);
    }
  }
  
  if ('permissions' in dataObject) {
    var uri = Components.classes["@mozilla.org/network/standard-url;1"]
                        .createInstance(Components.interfaces.nsIURI);    

    for (p = 0; p < dataObject.permissions.length; ++p) {
      uri.spec = dataObject.permissions[p].host;
      if (permissionmanager.testPermission(uri, "cookie") != dataObject.permissions[p].perm)
        permissionmanager.add(uri, "cookie", dataObject.permissions[p].perm);
    }
  }
}

function unexpandOld(event)
{
  var box = document.getElementById("privacyCategories");
  var newExpander = event.originalTarget.parentNode.parentNode;
  for (var i = 0; i < box.childNodes.length; ++i) {
    if (box.childNodes[i] != newExpander && box.childNodes[i].getAttribute("open"))
      box.childNodes[i].open = false;
  }
}
