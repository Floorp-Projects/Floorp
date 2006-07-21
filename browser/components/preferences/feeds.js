# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# ***** BEGIN LICENSE BLOCK *****
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
# The Original Code is the Firefox Preferences System.
#
# The Initial Developer of the Original Code is
# Jeff Walden <jwalden+code@mit.edu>.
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
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


var gFeedsPane = {
  /**
   * Ensures feed reader list is initialized before Feed Reader UI is
   * filled from preferences.
   */
  _feedReadersInited: false,

  _pane: null,

  /**
   * Initializes this.
   */
  init: function ()
  {
    this._pane = document.getElementById("paneFeeds");

    this._initFeedReaders();
    this._attachPreferenceBindings();
    this.onReaderPrefsChange();
  },

  _attachPreferenceBindings: function ()
  {
    var feedClick = document.getElementById("feedClick");
    feedClick.setAttribute("preference", "browser.feeds.handler");

    var reader = document.getElementById("chooseFeedReader");
    reader.setAttribute("preference", "browser.feeds.handler.default");
  },

  /*
   * Preferences:
   *
   * browser.feeds.handler
   * - "bookmarks", "reader" (clarified further using the .default preference),
   *   or "ask" -- indicates the default handler being used to process feeds
   * browser.feeds.handler.default
   * - "bookmarks", "client" or "web" -- indicates the chosen feed reader used
   *   to display feeds, either transiently (i.e., when the "use as default"
   *   checkbox is unchecked, corresponds to when browser.feeds.handler=="ask")
   *   or more permanently (i.e., the item displayed in the dropdown in Feeds
   *   preferences)
   * browser.feeds.handler.webservice
   * - the URL of the currently selected web service used to read feeds
   * browser.feeds.handlers.application
   * - nsILocalFile, stores the current client-side feed reading app if one has
   *   been chosen
   */

  /**
   * Converts the selected radio button into a preference value, sets any
   * ancillary preferences (such as web/client readers), and returns the new
   * value of browser.feeds.handler.
   */
  writeFeedAction: function ()
  {
    var app = document.getElementById("browser.feeds.handlers.application");
    var web = document.getElementById("browser.feeds.handlers.webservice");
    var selReader = document.getElementById("browser.feeds.handler.default");

    var feedAction = document.getElementById("feedClick").value;
    switch (feedAction) {
      case "reader":
        var menu = document.getElementById("chooseFeedReader");
        var reader = menu.selectedItem;

        var type = reader.getAttribute("type");
        switch (type) {
          case "client":
            app.value = reader.getAttribute("value");
            selReader.value = type;
            break;

          case "web":
            web.value = reader.getAttribute("value");
            selReader.value = type;
            break;

          default:
            throw "Unhandled reader type: " + feedAction;
        }
        break;

      default:
        throw "Unhandled feed action: " + feedAction;

      case "ask":
      case "bookmarks":
        break;

    }
    return feedAction;
  },

  /**
   * Converts the value of browser.feeds.handler.default into the appropriate
   * menu item in the menulist of readers, returning the value of that item.
   */
  readFeedReader: function ()
  {
    var reader = document.getElementById("browser.feeds.handler");
    var selReader = document.getElementById("browser.feeds.handler.default");

    var web = document.getElementById("browser.feeds.handlers.webservice");
    var app = document.getElementById("browser.feeds.handlers.application");

    var menu = document.getElementById("chooseFeedReader");

    // we have the type of reader being used -- get its corresponding value
    // and return it
    var defaultHandler = selReader.value;
    switch (defaultHandler) {
      case "web":
        return web.value;
        break;

      case "client":
        return app.value.path || app.value;
        break;

      case "bookmarks":
        // we could handle this case with a preference specifically for the
        // chosen reader, but honestly -- is it really worth it?
      default:
        menu.selectedIndex = 0;
        return menu.value;
    }
  },

  /**
   * Determines the reader displayed in the feed reader menulist and stores that
   * reader to preferences.
   */
  writeFeedReader: function ()
  {
    var menu = document.getElementById("chooseFeedReader");

    var reader = document.getElementById("browser.feeds.handler");
    var selReader = document.getElementById("browser.feeds.handler.default");
    var web = document.getElementById("browser.feeds.handlers.webservice");
    var app = document.getElementById("browser.feeds.handlers.application");

    var selected = menu.selectedItem;
    var type = selected.getAttribute("type");
    switch (type) {
      case "web":
        web.value = selected.value;
        break;

      case "client":
        app.value = selected.value;
        break;

      case "add":
        // we're choosing a new client app
        var newApp = this._addNewReader();
        if (newApp) {
          this._initFeedReaders();
          app.value = newApp;
          type = "client";
        } else {
          type = selReader.value; // return to existing value
        }
        break;

      default:
        throw "Unhandled type: " + type;
    }
       return type;
  },

  /**
   * Syncs current UI with the values stored in preferences.  This is necessary
   * because the UI items are represented using multiple preferences, so the
   * sync can't happen automatically without extra code.
   */
  onReaderPrefsChange: function ()
  {
    var handler = document.getElementById("browser.feeds.handler");
    var selReader = document.getElementById("browser.feeds.handler.default");

    handler.updateElements();
    selReader.updateElements();
  },

  /**
   * Populates the UI list of available feed readers.  The current feed reader
   * must be manually selected in the list.
   */
  _initFeedReaders: function ()
  {
    // XXX make UI a listbox with icons, etc!

    const Cc = Components.classes, Ci = Components.interfaces;

    var readers = [];

    // CLIENT-SIDE

    // first, get the client reader in preferences
    try {
      var clientApp = document.getElementById("clientApp");
      var app = document.getElementById("browser.feeds.handlers.application");
      clientApp.file = app.value;

      var client = { type: "client",
                     path: clientApp.file.path,
                     name: clientApp.label,
                     icon: clientApp.image };
      readers.push(client);
    }
    catch (e) { /* no client feed reader set */ }

    // get any readers stored in system preferences
#ifdef XP_WIN
    try {
      // look for current feed handler in the Windows registry
      const WRK = Ci.nsIWindowsRegKey;
      var regKey = Cc["@mozilla.org/windows-registry-key;1"]
                     .createInstance(WRK);
      regKey.open(WRK.ROOT_KEY_CLASSES_ROOT,
                  "feed\\shell\\open\\command",
                  WRK.ACCESS_READ);
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
      var file = Cc["@mozilla.org/file/local;1"]
                   .createInstance(Ci.nsILocalFile);
      file.initWithPath(path);
      clientApp.file = file;

      client = { type: "client",
                 path: clientApp.file.path,
                 name: clientApp.label,
                 icon: clientApp.image };

      readers.push(client);
    }
    catch (e) { /* no feed: handler on system */ }
#endif

    // WEB SERVICES
    const TYPE_MAYBE_FEED = "application/vnd.mozilla.maybe.feed";
    var wccr = Cc["@mozilla.org/embeddor.implemented/web-content-handler-registrar;1"]
                 .getService(Ci.nsIWebContentConverterService);
    var ios = Cc["@mozilla.org/network/io-service;1"]
                .getService(Ci.nsIIOService);

    var handlers = wccr.getContentHandlers(TYPE_MAYBE_FEED, {});
    for (var i = 0; i < handlers.length; i++) {
      var uri = ios.newURI(handlers[i].uri, null, null);
      var webReader = { type: "web",
                        path: uri.spec,
                        name: handlers[i].name,
                        icon: uri.prePath + "/favicon.ico" };
      readers.push(webReader);
    }

    // Now that we have all the readers, sort them and add them to the UI
    var menulist = document.getElementById("chooseFeedReader");
    menulist.textContent = ""; // clear out any previous elements
    var popup = document.createElement("menupopup");

    // insert a blank item to indicate no selected preference, and an
    // "add" item to allow addition of new readers

    var bundle = document.getElementById("bundlePreferences");

    readers.sort(function(a, b) { return (a.name.toLowerCase() < b.name.toLowerCase()) ? -1 : 1; });
    for (var i = 0; i < readers.length; i++) {
      var reader = readers[i];

      item = document.createElement("menuitem");
      item.setAttribute("label", reader.name);
      item.setAttribute("value", reader.path);
      item.setAttribute("type", reader.type);
      item.setAttribute("path", reader.path);

      popup.appendChild(item);
    }

    // put "add new reader" at end, since it won't be used much
    item = document.createElement("menuitem");
    item.setAttribute("label", bundle.getString("addReader"));
    item.setAttribute("value", "add");
    item.setAttribute("type", "add");
    popup.appendChild(item);

    menulist.appendChild(popup);

  },

  /**
   * Displays a prompt from which the user may add a new (client) feed reader.
   *
   * @returns null if no new reader was added, or the path to the application if
   *          one was added
   */
  _addNewReader: function ()
  {
    const Cc = Components.classes, Ci = Components.interfaces;
    var fp = Cc["@mozilla.org/filepicker;1"]
               .createInstance(Ci.nsIFilePicker);
    fp.init(window, document.title, Ci.nsIFilePicker.modeOpen);
    fp.appendFilters(Ci.nsIFilePicker.filterApps);
    if (fp.show() == Ci.nsIFilePicker.returnOK && fp.file) {
      // XXXben - we need to compare this with the running instance executable
      //          just don't know how to do that via script...
      if (fp.file.leafName == "firefox.exe")
        return null;

      var pref = document.getElementById("browser.feeds.handlers.application");
      pref.value = fp.file;

      return fp.file.path;
    }
    return null;

  }
};
