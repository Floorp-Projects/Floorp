/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Feed Subscribe Handler.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <beng@google.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const TYPE_MAYBE_FEED = "application/vnd.mozilla.maybe.feed";

const PREF_SELECTED_APP = "browser.feeds.handlers.application";
const PREF_SELECTED_WEB = "browser.feeds.handlers.webservice";
const PREF_SELECTED_HANDLER = "browser.feeds.handler";
const PREF_SKIP_PREVIEW_PAGE = "browser.feeds.skip_preview_page";

function LOG(str) {
  dump("*** " + str + "\n");
}

var SubscriptionOptions = {

  init: function SO_init() {
    var prefs =   
        Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefBranch);
  
    var autoHandle = document.getElementById("autoHandle");
    try {
      autoHandle.checked = prefs.getBoolPref(PREF_SKIP_PREVIEW_PAGE);
    }
    catch (e) {
    }
    
    var clientApp = document.getElementById("clientApp");
    try {
      clientApp.file = 
        prefs.getComplexValue(PREF_SELECTED_APP, Ci.nsILocalFile);
    }
    catch (e) {
      // No specified file, look on the system for one
#ifdef XP_WIN
      const WRK = Ci.nsIWindowsRegKey;
      var regKey =
          Cc["@mozilla.org/windows-registry-key;1"].createInstance(WRK);
      regKey.open(WRK.ROOT_KEY_CLASSES_ROOT, 
                  "feed\\shell\\open\\command", WRK.ACCESS_READ);
      var path = regKey.readStringValue("");
      if (path.charAt(0) == "\"") {
        // Everything inside the quotes
        path = path.substr(1, path.lastIndexOf("\"") - 1);
      }
      else {
        // Everything up to the first space
        path = path.substr(0, path.indexOf(" "));
      }
      var file =
          Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
      file.initWithPath(path);
      clientApp.file = file;
#endif
    }

    var wsp = document.getElementById("webServicePopup");
    this.populateWebHandlers(wsp);
    if (!document.getElementById("noneItem")) {
      // If there are any web handlers installed, this option should
      // be enabled for selection.
      var readerWebOption = document.getElementById("readerWeb");
      readerWebOption.removeAttribute("disabled");
    }
    
    var webService = document.getElementById("webService");
    try {
      webService.value = prefs.getCharPref(PREF_SELECTED_WEB);
    }
    catch (e) {
      webService.selectedIndex = 0;
    }

    var reader = document.getElementById("reader");
    try {
      reader.value =  prefs.getCharPref(PREF_SELECTED_HANDLER);
    }
    catch (e) {
      reader.value = "bookmarks";
    }
  },
  
  populateWebHandlers: function SO_populateWebHandlers(popup) {
    var wccr = 
        Cc["@mozilla.org/web-content-handler-registrar;1"].
        getService(Ci.nsIWebContentConverterRegistrar);
    var handlers = wccr.getContentHandlers(TYPE_MAYBE_FEED, {});
    if (handlers.length == 0)
      return;
     
    while (popup.hasChildNodes())
      popup.removeChild(popup.firstChild);
    var ios = 
        Cc["@mozilla.org/network/io-service;1"].
        getService(Ci.nsIIOService);
    for (var i = 0; i < handlers.length; ++i) {
      var menuitem = document.createElementNS(XUL_NS, "menuitem");
      menuitem.setAttribute("label", handlers[i].name);
      menuitem.setAttribute("value", handlers[i].uri);

      var uri = ios.newURI(handlers[i].uri, null, null);
      menuitem.setAttribute("src", uri.prePath + "/favicon.ico");
      
      popup.appendChild(menuitem);
    }
  },
  
  selectionChanged: function SO_selectionChanged() {
    var reader = document.getElementById("reader");
    var clientApp = document.getElementById("clientApp");
    var chooseClientApp = document.getElementById("chooseClientApp");
    var webService = document.getElementById("webService");
    switch (reader.value) {
    case "client":
      webService.disabled = true;
      clientApp.disabled = chooseClientApp.disabled = false;
      break;
    case "web":
      webService.disabled = false;
      clientApp.disabled = chooseClientApp.disabled = true;
      break;
    case "bookmarks":
      webService.disabled = true;
      clientApp.disabled = chooseClientApp.disabled = true;
      break;
    }
  },
  
  chooseClientApp: function SO_chooseClientApp() {
    var fp = 
        Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    fp.init(window, title, Ci.nsIFilePicker.modeOpen);
    fp.appendFilters(Ci.nsIFilePicker.filterApps);
    if (fp.show() == Ci.nsIFilePicker.returnOK && fp.file) {
      var clientApp = document.getElementById("clientApp");
      clientApp.file = fp.file;
      return true;
    }
    return false;
  },
  
  accept: function SO_accept() {
    var prefs =   
        Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefBranch);

    var reader = document.getElementById("reader");
    prefs.setCharPref(PREF_SELECTED_HANDLER, reader.value);
    
    var clientApp = document.getElementById("clientApp");
    if (clientApp.file)
      prefs.setComplexValue(PREF_SELECTED_APP, Ci.nsILocalFile, 
                            clientApp.file);
    
    var webService = document.getElementById("webService");
    prefs.setCharPref(PREF_SELECTED_WEB, webService.value);
    
    var autoHandle = document.getElementById("autoHandle");
    prefs.setBoolPref(PREF_SKIP_PREVIEW_PAGE, autoHandle.checked);
    
    if (reader.value == "web") {
      var wccr = 
          Cc["@mozilla.org/web-content-handler-registrar;1"].
          getService(Ci.nsIWebContentConverterRegistrar);
      if (autoHandle.checked) {
        var handler = 
            wccr.getWebContentHandlerByURI(TYPE_MAYBE_FEED, webService.value);
        if (handler)
          wccr.setAutoHandler(TYPE_MAYBE_FEED, handler);
      }
      else
        wccr.setAutoHandler(TYPE_MAYBE_FEED, null);
    }
    
    prefs.QueryInterface(Ci.nsIPrefService);
    prefs.savePrefFile(null);
  },
  
  whatAreLiveBookmarks: function SO_whatAreLiveBookmarks(button) {
    var url = button.getAttribute("url");
    openDialog("chrome://browser/content/browser.xul", "_blank", 
               "chrome,all,dialog=no", url, null, null);
  }
};

