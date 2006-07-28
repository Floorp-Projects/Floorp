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
 *   Jeff Walden <jwalden+code@mit.edu>
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
const PREF_SELECTED_ACTION = "browser.feeds.handler";
const PREF_SELECTED_READER = "browser.feeds.handler.default";

function LOG(str) {
  dump("*** " + str + "\n");
}

var SubscriptionOptions = {

  /**
   * Initializes the UI for the subscription options dialog, including the
   * currently-selected default feed reader, the client-side feed reader, and
   * the list of web service feed readers.
   */
  init: function SO_init() {
    var prefs =   
        Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefBranch);
  
    this._initClientApp();
    this.populateWebHandlers();
    
    if ("arguments" in window && window.arguments[0] == "subscribe") {
      var strings = document.getElementById("bundle");
      var okButton = document.documentElement.getButton("accept");
      okButton.label = strings.getString("subscribeNow");
      document.title = strings.getString("subscribeTitle");
      
      okButton.className += " feedSubscribeButton";
    }
    
    var reader = document.getElementById("reader");
    var readers = document.getElementById("readers");
    readers.selectedIndex = 0;

    var handler = "bookmarks";
    try {
      handler = prefs.getCharPref(PREF_SELECTED_READER);
    }
    catch (e) {
    }

    switch (handler) {
      case "bookmarks":
        reader.value = "bookmarks";
        break;

      case "client":
        reader.value = "reader";
        try {
          readers.value = prefs.getCharPref(PREF_SELECTED_APP);
        }
        catch (e) {
          readers.selectedIndex = 0;
        }
        readers.focus();
        break;

      case "web":
        reader.value = "reader";
        try {
          readers.value = prefs.getCharPref(PREF_SELECTED_WEB);
        }
        catch (e) {
          readers.selectedIndex = 0;
        }
        readers.focus();
        break;

      default:
        LOG("unexpected default reader: " + handler);
        reader.value = "bookmarks";
        break;
    }

    try {
      var defaultHandler = prefs.getCharPref(PREF_SELECTED_ACTION);
    }
    catch (e) {
      defaultHandler = "ask";
    }

    // user must explicitly choose to make the selection the default reader
    if (defaultHandler != "ask")
      document.getElementById("useAsDefault").checked = true;
  },

  /**
   * Initializes the client-side feed reader list item, if possible.
   */
  _initClientApp: function SO__initClientApp() {
    var clientApp = document.getElementById("clientApp");
    try {
      var prefs =   
          Cc["@mozilla.org/preferences-service;1"].
          getService(Ci.nsIPrefBranch);
      clientApp.file = 
        prefs.getComplexValue(PREF_SELECTED_APP, Ci.nsILocalFile);
      var application = document.getElementById("applicationName");
      application.setAttribute("label", clientApp.label);
      application.setAttribute("image", clientApp.image);
    }
    catch (e) {
      // No specified file, look on the system for one
#ifdef XP_WIN
      try {
        const WRK = Ci.nsIWindowsRegKey;
        var regKey =
            Cc["@mozilla.org/windows-registry-key;1"].createInstance(WRK);
        regKey.open(WRK.ROOT_KEY_CLASSES_ROOT, 
                    "feed\\shell\\open\\command", WRK.ACCESS_READ);
        var path = regKey.readStringValue("");
        if (path.charAt(0) == "\"") {
          // Everything inside the quotes
          path = path.substr(1);
          path = path.substr(0, path.indexOf("\""));
        }
        else {
          // Everything up to the first space
          path = path.substr(0, path.indexOf(" "));
        }
        var file =
            Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
        file.initWithPath(path);
        clientApp.file = file;
        var application = document.getElementById("applicationName");
        application.setAttribute("label", clientApp.label);
        application.setAttribute("image", clientApp.image);
      }
      catch (e) {
        LOG("SubscriptionOptions.init: No feed: handler registered on system");
      }
#endif
    }
  },

  /**
   * Enables/disables the client/web reader lists in response to a change in
   * selection in the reader type radiogroup.
   */
  readerTypeChanged: function SO_readerTypeChanged() {
    var reader = document.getElementById("reader");
    var chooseClientApp = document.getElementById("chooseClientApp");
    var readers = document.getElementById("readers");
    readers.disabled = chooseClientApp.disabled = 
      reader.value == "bookmarks";
  },

  /**
   * Gets the set of web services which are registered as feed readers with the
   * browser and adds them to the UI.
   */
  populateWebHandlers: function SO_populateWebHandlers() {
    var wccr = 
        Cc["@mozilla.org/embeddor.implemented/web-content-handler-registrar;1"].
        getService(Ci.nsIWebContentConverterService);
    var handlers = wccr.getContentHandlers(TYPE_MAYBE_FEED, {});
    if (handlers.length == 0)
      return;
      
    var appRow = document.getElementById("application");
    while (appRow.nextSibling)
      appRow.parentNode.removeChild(appRow.parentNode.lastChild);
    
    var ios = 
        Cc["@mozilla.org/network/io-service;1"].
        getService(Ci.nsIIOService);
    for (var i = 0; i < handlers.length; ++i) {
      var row = document.createElementNS(XUL_NS, "listitem");
      row.className = "listitem-iconic";
      row.setAttribute("label", handlers[i].name);
      row.setAttribute("value", handlers[i].uri);

      var uri = ios.newURI(handlers[i].uri, null, null);
      row.setAttribute("image", uri.prePath + "/favicon.ico");
      
      appRow.parentNode.appendChild(row);
    }
  },

  /**
   * Displays a file picker in which the user may choose a client-side feed
   * reader, and stores the chosen file, if any, in UI.
   *
   * @returns bool
   *          true if a new client reader was selected, false otherwise
   */
  chooseClientApp: function SO_chooseClientApp() {
    var fp = 
        Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    fp.init(window, title, Ci.nsIFilePicker.modeOpen);
    fp.appendFilters(Ci.nsIFilePicker.filterApps);
    if (fp.show() == Ci.nsIFilePicker.returnOK && fp.file) {
      // XXXben - we need to compare this with the running instance executable
      //          just don't know how to do that via script...
      if (fp.file.leafName == "firefox.exe")
        return false;
    
      var clientApp = document.getElementById("clientApp");
      clientApp.file = fp.file;
      var application = document.getElementById("applicationName");
      application.setAttribute("label", clientApp.label);
      application.setAttribute("image", clientApp.image);
      
      var okButton = document.documentElement.getButton("accept");
      okButton.disabled = !clientApp.file.exists();
      
      return true;
    }
    return false;
  },

  /**
   * Stores the currently-displayed feed reader choices to preferences prior to
   * acceptance of the options dialog.
   */
  accept: function SO_accept() {
    var prefs =   
        Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefBranch);

    var reader = document.getElementById("reader");
    var readers = document.getElementById("readers");

    var selectedHandler = reader.value;

    // if a client app is chosen, always save it -- it's only used if the user
    // actually selected it for use
    var clientApp = document.getElementById("clientApp");
    if (clientApp.file)
      prefs.setComplexValue(PREF_SELECTED_APP, Ci.nsILocalFile, 
                            clientApp.file);
    
    // only save selected value to preferences if the user checked the
    // "use as default" checkbox
    var useAsDefault = document.getElementById("useAsDefault").checked;
    prefs.setCharPref(PREF_SELECTED_ACTION,
                      useAsDefault ? selectedHandler : "ask");

    // Save whatever choice the user made as the "default" feed reader
    if (selectedHandler == "reader") {
      if (readers.selectedIndex == 0)
        selectedHandler = "client";
      else
        selectedHandler = "web";
    }
    prefs.setCharPref(PREF_SELECTED_READER, selectedHandler);

    if (selectedHandler == "web") {
      prefs.setCharPref(PREF_SELECTED_WEB, readers.selectedItem.value);

      var wccr = 
          Cc["@mozilla.org/embeddor.implemented/web-content-handler-registrar;1"].
          getService(Ci.nsIWebContentConverterService);
      if (useAsDefault) {
        var handler = 
            wccr.getWebContentHandlerByURI(TYPE_MAYBE_FEED, readers.selectedItem.value);
        if (handler)
          wccr.setAutoHandler(TYPE_MAYBE_FEED, handler);
      }
      else
        wccr.setAutoHandler(TYPE_MAYBE_FEED, null);
    }
    
    try {
      var params = window.arguments[1].QueryInterface(Ci.nsIDialogParamBlock);
      // Used to tell the preview page that the user chose to subscribe with
      // a particular reader, and so it should subscribe now.
      const PARAM_USER_SUBSCRIBED = 0;
      params.SetInt(PARAM_USER_SUBSCRIBED, 1);
    }
    catch (e) {
    }
    
    prefs.QueryInterface(Ci.nsIPrefService);
    prefs.savePrefFile(null);
  },

  /**
   * Displays a webpage describing what Live Bookmarks are.
   */
  whatAreLiveBookmarks: function SO_whatAreLiveBookmarks(button) {
    var url = button.getAttribute("url");
    openDialog("chrome://browser/content/browser.xul", "_blank", 
               "chrome,all,dialog=no", url, null, null);
  }
};

