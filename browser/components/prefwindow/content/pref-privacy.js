# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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


var _elementIDs = ["histDay", "browserCacheDiskCache", "enableCookies",
                    "enableCookiesForOriginatingSiteOnly", "enableCookiesForCurrentSessionOnly",
                    "enableCookiesButAskFirst", "enableFormFill", "enablePasswords"];

function Startup() {
  var cookiesEnabled = document.getElementById("enableCookies").checked;
  updateBroadcaster(!cookiesEnabled);

  var globalHistory = Components.classes["@mozilla.org/browser/global-history;1"].getService(Components.interfaces.nsIBrowserHistory);
  document.getElementById("history").setAttribute("cleardisabled", globalHistory.count == 0);
  
  // Initially disable the cookies clear button if there are no saved cookies
  var cookieMgr = Components.classes["@mozilla.org/cookiemanager;1"].getService();
  cookieMgr = cookieMgr.QueryInterface(Components.interfaces.nsICookieManager);

  var e = cookieMgr.enumerator;
  document.getElementById("cookies").setAttribute("cleardisabled", !e.hasMoreElements());

  // Initially disable the password clear button if there are no saved passwords
  var passwdMgr = Components.classes["@mozilla.org/passwordmanager;1"].getService();
  passwdMgr = passwdMgr.QueryInterface(Components.interfaces.nsIPasswordManager);

  e = passwdMgr.enumerator;
  document.getElementById("passwords").setAttribute("cleardisabled", !e.hasMoreElements());

  // Initially disable the downloads clear button if there the downloads list is empty
  try {
    e = PrivacyPanel.getDownloads();
    var hasDownloads = e.hasMoreElements();
  }
  catch (e) {
    hasDownloads = false;
  }
  document.getElementById("downloads").setAttribute("cleardisabled", !hasDownloads);
  
  // Initially disable the form history clear button if the history is empty
  var formHistory = Components.classes["@mozilla.org/satchel/form-history;1"]
                              .getService(Components.interfaces.nsIFormHistory);
  document.getElementById("formfill").setAttribute("cleardisabled", formHistory.rowCount == 0);
  
  var categories = document.getElementById("privacyCategories");
  categories.addEventListener("clear", PrivacyPanel.clear, false);
}

function unload()
{
  var categories = document.getElementById("privacyCategories");
  for (var i = 0; i < categories.childNodes.length; ++i) {
    var expander = categories.childNodes[i];
    document.persist(expander.id, "open");
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
      var globalHistory = Components.classes["@mozilla.org/browser/global-history;1"]
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
      
      dlMgr.startBatchUpdate();  
      while (downloads.hasMoreElements()) {
        var download = downloads.getNext().QueryInterface(Components.interfaces.nsIRDFResource);
        try {
          dlMgr.removeDownload(download.Value);
        }
        catch (e) {
        }
      }
      dlMgr.endBatchUpdate();  
      
      var rds = ds.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
      if (rds)
        rds.Flush();
      
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
  
};

function viewCookies() 
{
  window.openDialog("chrome://communicator/content/wallet/CookieViewer.xul","_blank",
                    "chrome,resizable=yes", "cookieManager");
}

function viewSignons() 
{
  window.openDialog("chrome://communicator/content/wallet/SignonViewer.xul","_blank",
                    "chrome,resizable=yes", "S");
}

function updateBroadcaster(aDisable)
{
  var broadcaster = document.getElementById("cookieBroadcaster");
  var checkbox1 = document.getElementById("enableCookiesForOriginatingSiteOnly");
  var checkbox2 = document.getElementById("enableCookiesForCurrentSessionOnly");
  var checkbox3 = document.getElementById("enableCookiesButAskFirst");
  if (aDisable) {
    broadcaster.setAttribute("disabled", "true");
    checkbox1.checked = false;
    checkbox2.checked = false;
    checkbox3.checked = false;
  }
  else
    broadcaster.removeAttribute("disabled");
}
