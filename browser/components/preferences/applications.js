/*
# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
# The Original Code is the Download Actions Manager.
#
# The Initial Developer of the Original Code is
# Ben Goodger.
# Portions created by the Initial Developer are Copyright (C) 2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ben Goodger <ben@mozilla.org>
#   Jeff Walden <jwalden+code@mit.edu>
#   Asaf Romano <mozilla.mano@sent.com>
#   Myk Melez <myk@mozilla.org>
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
 */

//****************************************************************************//
// Constants & Enumeration Values

/*
#ifndef XP_MACOSX
*/
var Cc = Components.classes;
var Ci = Components.interfaces;
var Cr = Components.results;
var TYPE_MAYBE_FEED = "application/vnd.mozilla.maybe.feed";
const kXULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
/*
#endif
*/

const PREF_DISABLED_PLUGIN_TYPES = "plugin.disable_full_page_plugin_for_types";

// Preferences that affect which entries to show in the list.
const PREF_SHOW_PLUGINS_IN_LIST = "browser.download.show_plugins_in_list";
const PREF_HIDE_PLUGINS_WITHOUT_EXTENSIONS =
  "browser.download.hide_plugins_without_extensions";

/*
 * Preferences where we store handling information about the feed type.
 *
 * browser.feeds.handler
 * - "bookmarks", "reader" (clarified further using the .default preference),
 *   or "ask" -- indicates the default handler being used to process feeds;
 *   "bookmarks" is obsolete; to specify that the handler is bookmarks,
 *   set browser.feeds.handler.default to "bookmarks";
 *
 * browser.feeds.handler.default
 * - "bookmarks", "client" or "web" -- indicates the chosen feed reader used
 *   to display feeds, either transiently (i.e., when the "use as default"
 *   checkbox is unchecked, corresponds to when browser.feeds.handler=="ask")
 *   or more permanently (i.e., the item displayed in the dropdown in Feeds
 *   preferences)
 *
 * browser.feeds.handler.webservice
 * - the URL of the currently selected web service used to read feeds
 *
 * browser.feeds.handlers.application
 * - nsILocalFile, stores the current client-side feed reading app if one has
 *   been chosen
 */
const PREF_FEED_SELECTED_APP    = "browser.feeds.handlers.application";
const PREF_FEED_SELECTED_WEB    = "browser.feeds.handlers.webservice";
const PREF_FEED_SELECTED_ACTION = "browser.feeds.handler";
const PREF_FEED_SELECTED_READER = "browser.feeds.handler.default";

// The nsHandlerInfoAction enumeration values in nsIHandlerInfo identify
// the actions the application can take with content of various types.
// But since nsIHandlerInfo doesn't support plugins, there's no value
// identifying the "use plugin" action, so we use this constant instead.
const kActionUsePlugin = 5;


//****************************************************************************//
// Utilities

function getDisplayNameForFile(aFile) {
/*
#ifdef XP_WIN
*/
  if (aFile instanceof Ci.nsILocalFileWin) {
    try {
      return aFile.getVersionInfoField("FileDescription"); 
    }
    catch(ex) {
      // fall through to the file name
    }
  }
/*
#endif
#ifdef XP_MACOSX
*/
  if (aFile instanceof Ci.nsILocalFileMac) {
    try {
      return aFile.bundleDisplayName;
    }
    catch(ex) {
      // fall through to the file name
    }
  }
/*
#endif
*/

  return Cc["@mozilla.org/network/io-service;1"].
         getService(Ci.nsIIOService).
         newFileURI(aFile).
         QueryInterface(Ci.nsIURL).
         fileName;
}

function getLocalHandlerApp(aFile) {
  var localHandlerApp = Cc["@mozilla.org/uriloader/local-handler-app;1"].
                        createInstance(Ci.nsILocalHandlerApp);
  localHandlerApp.name = getDisplayNameForFile(aFile);
  localHandlerApp.executable = aFile;

  return localHandlerApp;
}


//****************************************************************************//
// HandlerInfoWrapper

/**
 * This object wraps nsIHandlerInfo with some additional functionality
 * the Applications prefpane needs to display and allow modification of
 * the list of handled types.
 * 
 * We create an instance of this wrapper for each entry we might display
 * in the prefpane, and we compose the instances from various sources,
 * including navigator.plugins and the handler service.
 *
 * We don't implement all the original nsIHandlerInfo functionality,
 * just the stuff that the prefpane needs.
 * 
 * In theory, all of the custom functionality in this wrapper should get
 * pushed down into nsIHandlerInfo eventually.
 */
function HandlerInfoWrapper(aType, aHandlerInfo) {
  this._type = aType;
  this.wrappedHandlerInfo = aHandlerInfo;
}

HandlerInfoWrapper.prototype = {
  // The wrapped nsIHandlerInfo object.  In general, this object is private,
  // but there are a couple cases where callers access it directly for things
  // we haven't (yet?) implemented, so we make it a public property.
  wrappedHandlerInfo: null,

  //**************************************************************************//
  // Convenience Utils

  _handlerSvc: Cc["@mozilla.org/uriloader/handler-service;1"].
               getService(Ci.nsIHandlerService),

  // Retrieve this as nsIPrefBranch and then immediately QI to nsIPrefBranch2
  // so both interfaces are available to callers.
  _prefSvc: Cc["@mozilla.org/preferences-service;1"].
            getService(Ci.nsIPrefBranch).
            QueryInterface(Ci.nsIPrefBranch2),

  _categoryMgr: Cc["@mozilla.org/categorymanager;1"].
                getService(Ci.nsICategoryManager),

  element: function(aID) {
    return document.getElementById(aID);
  },


  //**************************************************************************//
  // nsISupports

  QueryInterface: function(aIID) {
    if (aIID.equals(Ci.nsIHandlerInfo) ||
        aIID.equals(Ci.nsISupports) ||
        (aIID.equals(Ci.nsIMIMEInfo) &&
         this.wrappedHandlerInfo instanceof Ci.nsIMIMEInfo))
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  },


  //**************************************************************************//
  // nsIHandlerInfo

  // The MIME type or protocol scheme.
  _type: null,
  get type() {
    return this._type;
  },

  get description() {
    if (this.wrappedHandlerInfo.description)
      return this.wrappedHandlerInfo.description;

    if (this.primaryExtension) {
      let bundle = this.element("bundlePreferences");
      var extension = this.primaryExtension.toUpperCase();
      return bundle.getFormattedString("fileEnding", [extension]);
    }

    return this.type;
  },

  get preferredApplicationHandler() {
    return this.wrappedHandlerInfo.preferredApplicationHandler;
  },

  set preferredApplicationHandler(aNewValue) {
    this.wrappedHandlerInfo.preferredApplicationHandler = aNewValue;

    // Make sure the preferred handler is in the set of possible handlers.
    if (aNewValue) {
      var found = false;
      var possibleApps = this.possibleApplicationHandlers.
                         QueryInterface(Ci.nsIArray).enumerate();
      while (possibleApps.hasMoreElements() && !found)
        found = possibleApps.getNext().equals(aNewValue);
      if (!found)
        this.possibleApplicationHandlers.appendElement(aNewValue, false);
    }
  },

  get possibleApplicationHandlers() {
    return this.wrappedHandlerInfo.possibleApplicationHandlers;
  },

  get hasDefaultHandler() {
    return this.wrappedHandlerInfo.hasDefaultHandler;
  },

  get defaultDescription() {
    return this.wrappedHandlerInfo.defaultDescription;
  },

  // What to do with content of this type.
  get preferredAction() {
    // If we have an enabled plugin, then the action is to use that plugin.
    if (this.plugin && !this.isDisabledPluginType)
      return kActionUsePlugin;

    // XXX nsIMIMEService::getFromTypeAndExtension returns handler infos
    // whose default action is saveToDisk; should we do that here too?
    // And will there ever be handler info objects with no preferred action?
    if (!this.wrappedHandlerInfo.preferredAction) {
      if (gApplicationsPane.isValidHandlerApp(this.preferredApplicationHandler))
        return Ci.nsIHandlerInfo.useHelperApp;
      else
        return Ci.nsIHandlerInfo.useSystemDefault;
    }

    // If the action is to use a helper app, but we don't have a preferred
    // helper app, switch to using the system default.
    if (this.wrappedHandlerInfo.preferredAction == Ci.nsIHandlerInfo.useHelperApp &&
        !gApplicationsPane.isValidHandlerApp(this.preferredApplicationHandler))
      return Ci.nsIHandlerInfo.useSystemDefault;

    return this.wrappedHandlerInfo.preferredAction;
  },

  set preferredAction(aNewValue) {
    // We don't modify the preferred action if the new action is to use a plugin
    // because handler info objects don't understand our custom "use plugin"
    // value.  Also, leaving it untouched means that we can automatically revert
    // to the old setting if the user ever removes the plugin.

    if (aNewValue != kActionUsePlugin)
      this.wrappedHandlerInfo.preferredAction = aNewValue;
  },

  get alwaysAskBeforeHandling() {
    // If this type is handled only by a plugin, we can't trust the value
    // in the handler info object, since it'll be a default based on the absence
    // of any user configuration, and the default in that case is to always ask,
    // even though we never ask for content handled by a plugin, so special case
    // plugin-handled types by returning false here.
    if (this.plugin && this.handledOnlyByPlugin)
      return false;

    return this.wrappedHandlerInfo.alwaysAskBeforeHandling;
  },

  set alwaysAskBeforeHandling(aNewValue) {
    this.wrappedHandlerInfo.alwaysAskBeforeHandling = aNewValue;
  },


  //**************************************************************************//
  // nsIMIMEInfo

  // The primary file extension associated with this type, if any.
  //
  // XXX Plugin objects contain an array of MimeType objects with "suffixes"
  // properties; if this object has an associated plugin, shouldn't we check
  // those properties for an extension?
  get primaryExtension() {
    try {
      if (this.wrappedHandlerInfo instanceof Ci.nsIMIMEInfo &&
          this.wrappedHandlerInfo.primaryExtension)
        return this.wrappedHandlerInfo.primaryExtension
    } catch(ex) {}

    return null;
  },


  //**************************************************************************//
  // Plugin Handling

  // A plugin that can handle this type, if any.
  //
  // Note: just because we have one doesn't mean it *will* handle the type.
  // That depends on whether or not the type is in the list of types for which
  // plugin handling is disabled.
  plugin: null,

  // Whether or not this type is only handled by a plugin or is also handled
  // by some user-configured action as specified in the handler info object.
  //
  // Note: we can't just check if there's a handler info object for this type,
  // because OS and user configuration is mixed up in the handler info object,
  // so we always need to retrieve it for the OS info and can't tell whether
  // it represents only OS-default information or user-configured information.
  //
  // FIXME: once handler info records are broken up into OS-provided records
  // and user-configured records, stop using this boolean flag and simply
  // check for the presence of a user-configured record to determine whether
  // or not this type is only handled by a plugin.  Filed as bug 395142.
  handledOnlyByPlugin: undefined,

  get isDisabledPluginType() {
    return this._getDisabledPluginTypes().indexOf(this.type) != -1;
  },

  _getDisabledPluginTypes: function() {
    var types = "";

    if (this._prefSvc.prefHasUserValue(PREF_DISABLED_PLUGIN_TYPES))
      types = this._prefSvc.getCharPref(PREF_DISABLED_PLUGIN_TYPES);

    // Only split if the string isn't empty so we don't end up with an array
    // containing a single empty string.
    if (types != "")
      return types.split(",");

    return [];
  },

  disablePluginType: function() {
    var disabledPluginTypes = this._getDisabledPluginTypes();

    if (disabledPluginTypes.indexOf(this.type) == -1)
      disabledPluginTypes.push(this.type);

    this._prefSvc.setCharPref(PREF_DISABLED_PLUGIN_TYPES,
                              disabledPluginTypes.join(","));

    // Update the category manager so existing browser windows update.
    this._categoryMgr.deleteCategoryEntry("Gecko-Content-Viewers",
                                          this.type,
                                          false);
  },

  enablePluginType: function() {
    var disabledPluginTypes = this._getDisabledPluginTypes();

    var type = this.type;
    disabledPluginTypes = disabledPluginTypes.filter(function(v) v != type);

    this._prefSvc.setCharPref(PREF_DISABLED_PLUGIN_TYPES,
                              disabledPluginTypes.join(","));

    // Update the category manager so existing browser windows update.
    this._categoryMgr.
      addCategoryEntry("Gecko-Content-Viewers",
                       this.type,
                       "@mozilla.org/content/plugin/document-loader-factory;1",
                       false,
                       true);
  },


  //**************************************************************************//
  // Storage

  store: function() {
    this._handlerSvc.store(this.wrappedHandlerInfo);
  },

  remove: function() {
    this._handlerSvc.remove(this.wrappedHandlerInfo);
  },


  //**************************************************************************//
  // Icons

  get smallIcon() {
    return this._getIcon(16);
  },

  get largeIcon() {
    return this._getIcon(32);
  },

  _getIcon: function(aSize) {
    if (this.primaryExtension)
      return "moz-icon://goat." + this.primaryExtension + "?size=" + aSize;

    if (this.wrappedHandlerInfo instanceof Ci.nsIMIMEInfo)
      return "moz-icon://goat?size=" + aSize + "&contentType=" + this.type;

    // FIXME: consider returning some generic icon when we can't get a URL for
    // one (for example in the case of protocol schemes).  Filed as bug 395141.
    return null;
  }

};


//****************************************************************************//
// Feed Handler Info

/**
 * This object implements nsIHandlerInfo for the feed type.  It's a separate
 * object because we currently store handling information for the feed type
 * in a set of preferences rather than the nsIHandlerService-managed datastore.
 * 
 * This object inherits from HandlerInfoWrapper in order to get functionality
 * that isn't special to the feed type.
 * 
 * XXX Should we inherit from HandlerInfoWrapper?  After all, we override
 * most of that wrapper's properties and methods, and we have to dance around
 * the fact that the wrapper expects to have a wrappedHandlerInfo, which we
 * don't provide.
 */
var feedHandlerInfo = {

  __proto__: new HandlerInfoWrapper(TYPE_MAYBE_FEED, null),


  //**************************************************************************//
  // Convenience Utils

  _converterSvc:
    Cc["@mozilla.org/embeddor.implemented/web-content-handler-registrar;1"].
    getService(Ci.nsIWebContentConverterService),

  _shellSvc: Cc["@mozilla.org/browser/shell-service;1"].
             getService(Ci.nsIShellService),


  //**************************************************************************//
  // nsIHandlerInfo

  get description() {
    return gApplicationsPane._bundle.getString("webFeed");
  },

  get preferredApplicationHandler() {
    switch (this.element(PREF_FEED_SELECTED_READER).value) {
      case "client":
        var file = this.element(PREF_FEED_SELECTED_APP).value;
        if (file)
          return getLocalHandlerApp(file);

        return null;

      case "web":
        var uri = this.element(PREF_FEED_SELECTED_WEB).value;
        if (!uri)
          return null;
        return this._converterSvc.getWebContentHandlerByURI(TYPE_MAYBE_FEED,
                                                            uri);

      case "bookmarks":
      default:
        // When the pref is set to bookmarks, we handle feeds internally,
        // we don't forward them to a local or web handler app, so there is
        // no preferred handler.
        return null;
    }
  },

  set preferredApplicationHandler(aNewValue) {
    if (aNewValue instanceof Ci.nsILocalHandlerApp) {
      this.element(PREF_FEED_SELECTED_APP).value = aNewValue.executable;
      this.element(PREF_FEED_SELECTED_READER).value = "client";
    }
    else if (aNewValue instanceof Ci.nsIWebContentHandlerInfo) {
      this.element(PREF_FEED_SELECTED_WEB).value = aNewValue.uri;
      this.element(PREF_FEED_SELECTED_READER).value = "web";
      // Make the web handler be the new "auto handler" for feeds.
      // Note: we don't have to unregister the auto handler when the user picks
      // a non-web handler (local app, Live Bookmarks, etc.) because the service
      // only uses the "auto handler" when the selected reader is a web handler.
      // We also don't have to unregister it when the user turns on "always ask"
      // (i.e. preview in browser), since that also overrides the auto handler.
      this._converterSvc.setAutoHandler(this.type, aNewValue);
    }
  },

  get possibleApplicationHandlers() {
    var handlerApps = Cc["@mozilla.org/array;1"].
                      createInstance(Ci.nsIMutableArray);

    // Add the "selected" local application, if there is one and it's different
    // from the default handler for the OS.  Unlike for other types, there can
    // be only one of these at a time for the feed type, since feed preferences
    // only store a single local app.
    var preferredAppFile = this.element(PREF_FEED_SELECTED_APP).value;
    if (preferredAppFile && preferredAppFile.exists()) {
      let preferredApp = getLocalHandlerApp(preferredAppFile);
      let defaultApp = this._defaultApplicationHandler;
      if (!defaultApp || !defaultApp.equals(preferredApp))
        handlerApps.appendElement(preferredApp, false);
    }

    // Add the registered web handlers.  There can be any number of these.
    var webHandlers = this._converterSvc.getContentHandlers(this.type, {});
    for each (let webHandler in webHandlers)
      handlerApps.appendElement(webHandler, false);

    return handlerApps;
  },

  __defaultApplicationHandler: undefined,
  get _defaultApplicationHandler() {
    if (typeof this.__defaultApplicationHandler != "undefined")
      return this.__defaultApplicationHandler;

    var defaultFeedReader;
    try {
      defaultFeedReader = this._shellSvc.defaultFeedReader;
    }
    catch(ex) {
      // no default reader
    }

    if (defaultFeedReader) {
      let handlerApp = Cc["@mozilla.org/uriloader/local-handler-app;1"].
                       createInstance(Ci.nsIHandlerApp);
      handlerApp.name = getDisplayNameForFile(defaultFeedReader);
      handlerApp.QueryInterface(Ci.nsILocalHandlerApp);
      handlerApp.executable = defaultFeedReader;

      this.__defaultApplicationHandler = handlerApp;
    }
    else {
      this.__defaultApplicationHandler = null;
    }

    return this.__defaultApplicationHandler;
  },

  get hasDefaultHandler() {
    try {
      if (this._shellSvc.defaultFeedReader)
        return true;
    }
    catch(ex) {
      // no default reader
    }

    return false;
  },

  get defaultDescription() {
    if (this.hasDefaultHandler)
      return this._defaultApplicationHandler.name;

    // Should we instead return null?
    return "";
  },

  // What to do with content of this type.
  get preferredAction() {
    switch (this.element(PREF_FEED_SELECTED_ACTION).value) {

      case "bookmarks":
        return Ci.nsIHandlerInfo.handleInternally;

      case "reader":
        let preferredApp = this.preferredApplicationHandler;
        let defaultApp = this._defaultApplicationHandler;

        // If we have a valid preferred app, return useSystemDefault if it's
        // the default app; otherwise return useHelperApp.
        if (gApplicationsPane.isValidHandlerApp(preferredApp)) {
          if (defaultApp && defaultApp.equals(preferredApp))
            return Ci.nsIHandlerInfo.useSystemDefault;

          return Ci.nsIHandlerInfo.useHelperApp;
        }

        // The pref is set to "reader", but we don't have a valid preferred app.
        // What do we do now?  Not sure this is the best option (perhaps we
        // should direct the user to the default app, if any), but for now let's
        // direct the user to live bookmarks.
        return Ci.nsIHandlerInfo.handleInternally;

      // If the action is "ask", then alwaysAskBeforeHandling will override
      // the action, so it doesn't matter what we say it is, it just has to be
      // something that doesn't cause the controller to hide the type.
      case "ask":
      default:
        return Ci.nsIHandlerInfo.handleInternally;
    }
  },

  set preferredAction(aNewValue) {
    switch (aNewValue) {

      case Ci.nsIHandlerInfo.handleInternally:
        this.element(PREF_FEED_SELECTED_READER).value = "bookmarks";
        break;

      case Ci.nsIHandlerInfo.useHelperApp:
        this.element(PREF_FEED_SELECTED_ACTION).value = "reader";
        // The controller has already set preferredApplicationHandler
        // to the new helper app.
        break;

      case Ci.nsIHandlerInfo.useSystemDefault:
        this.element(PREF_FEED_SELECTED_ACTION).value = "reader";
        this.preferredApplicationHandler = this._defaultApplicationHandler;
        break;
    }
  },

  get alwaysAskBeforeHandling() {
    return this.element(PREF_FEED_SELECTED_ACTION).value == "ask";
  },

  set alwaysAskBeforeHandling(aNewValue) {
    if (aNewValue == true)
      this.element(PREF_FEED_SELECTED_ACTION).value = "ask";
    else
      this.element(PREF_FEED_SELECTED_ACTION).value = "reader";
  },


  //**************************************************************************//
  // nsIMIMEInfo

  get primaryExtension() {
    return "xml";
  },


  //**************************************************************************//
  // Plugin Handling

  handledOnlyByPlugin: false,


  //**************************************************************************//
  // Storage

  // Changes to the preferred action and handler take effect immediately
  // (we write them out to the preferences right as they happen), so we don't
  // need to do anything when the controller calls store() after modifying
  // the handler.
  // XXX Should we hold off on making the changes until this method gets called?
  store: function() {},

  // The feed type cannot be removed.
  remove: function() {},


  //**************************************************************************//
  // Icons

  get smallIcon() {
    return "chrome://browser/skin/feeds/feedIcon16.png";
  },

  get largeIcon() {
    return "chrome://browser/skin/feeds/feedIcon.png";
  }

};


//****************************************************************************//
// Prefpane Controller

var gApplicationsPane = {
  // The set of types the app knows how to handle.  A hash of HandlerInfoWrapper
  // objects, indexed by type.
  _handledTypes: {},

  _bundle       : null,
  _list         : null,
  _filter       : null,

  // Retrieve this as nsIPrefBranch and then immediately QI to nsIPrefBranch2
  // so both interfaces are available to callers.
  _prefSvc      : Cc["@mozilla.org/preferences-service;1"].
                  getService(Ci.nsIPrefBranch).
                  QueryInterface(Ci.nsIPrefBranch2),

  _mimeSvc      : Cc["@mozilla.org/uriloader/external-helper-app-service;1"].
                  getService(Ci.nsIMIMEService),

  _helperAppSvc : Cc["@mozilla.org/uriloader/external-helper-app-service;1"].
                  getService(Ci.nsIExternalHelperAppService),

  _handlerSvc   : Cc["@mozilla.org/uriloader/handler-service;1"].
                  getService(Ci.nsIHandlerService),

  _ioSvc        : Cc["@mozilla.org/network/io-service;1"].
                  getService(Ci.nsIIOService),


  //**************************************************************************//
  // Initialization & Destruction

  init: function() {
    // Initialize shortcuts to some commonly accessed elements.
    this._bundle = document.getElementById("bundlePreferences");
    this._list = document.getElementById("handlersView");
    this._filter = document.getElementById("filter");

    // Observe preferences that influence what we display so we can rebuild
    // the view when they change.
    this._prefSvc.addObserver(PREF_SHOW_PLUGINS_IN_LIST, this, false);
    this._prefSvc.addObserver(PREF_HIDE_PLUGINS_WITHOUT_EXTENSIONS, this, false);
    this._prefSvc.addObserver(PREF_FEED_SELECTED_APP, this, false);
    this._prefSvc.addObserver(PREF_FEED_SELECTED_WEB, this, false);
    this._prefSvc.addObserver(PREF_FEED_SELECTED_ACTION, this, false);
    this._prefSvc.addObserver(PREF_FEED_SELECTED_READER, this, false);

    // Listen for window unload so we can remove our preference observers.
    window.addEventListener("unload", this, false);

    // Figure out how we should be sorting the list.  We persist sort settings
    // across sessions, so we can't assume the default sort column and direction.
    // XXX should we be using the XUL sort service instead?
    if (document.getElementById("typeColumn").hasAttribute("sortDirection"))
      this._sortColumn = document.getElementById("typeColumn");
    else if (document.getElementById("actionColumn").hasAttribute("sortDirection"))
      this._sortColumn = document.getElementById("actionColumn");

    // Load the data and build the list of handlers.
    // By doing this in a timeout, we let the preferences dialog resize itself
    // to an appropriate size before we add a bunch of items to the list.
    // Otherwise, if there are many items, and the Applications prefpane
    // is the one that gets displayed when the user first opens the dialog,
    // the dialog might stretch too much in an attempt to fit them all in.
    // XXX Shouldn't we perhaps just set a max-height on the richlistbox?
    var _delayedPaneLoad = function(self) {
      self._loadData();
      self.rebuildView();
      self._list.focus();
    }
    setTimeout(_delayedPaneLoad, 0, this);
  },

  destroy: function() {
    window.removeEventListener("unload", this, false);
    this._prefSvc.removeObserver(PREF_SHOW_PLUGINS_IN_LIST, this);
    this._prefSvc.removeObserver(PREF_HIDE_PLUGINS_WITHOUT_EXTENSIONS, this);
    this._prefSvc.removeObserver(PREF_FEED_SELECTED_APP, this);
    this._prefSvc.removeObserver(PREF_FEED_SELECTED_WEB, this);
    this._prefSvc.removeObserver(PREF_FEED_SELECTED_ACTION, this);
    this._prefSvc.removeObserver(PREF_FEED_SELECTED_READER, this);
  },


  //**************************************************************************//
  // nsISupports

  QueryInterface: function(aIID) {
    if (aIID.equals(Ci.nsIObserver) ||
        aIID.equals(Ci.nsIDOMEventListener ||
        aIID.equals(Ci.nsISupports)))
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  },


  //**************************************************************************//
  // nsIObserver

  observe: function (aSubject, aTopic, aData) {
    // Rebuild the list when there are changes to preferences that influence
    // whether or not to show certain entries in the list.
    if (aTopic == "nsPref:changed")
      this.rebuildView();
  },


  //**************************************************************************//
  // nsIDOMEventListener

  handleEvent: function(aEvent) {
    if (aEvent.type == "unload") {
      this.destroy();
    }
  },


  //**************************************************************************//
  // Composed Model Construction

  _loadData: function() {
    this._loadFeedHandler();
    this._loadPluginHandlers();
    this._loadApplicationHandlers();
  },

  _loadFeedHandler: function() {
    this._handledTypes[TYPE_MAYBE_FEED] = feedHandlerInfo;
  },

  /**
   * Load the set of handlers defined by plugins.
   *
   * Note: if there's more than one plugin for a given MIME type, we assume
   * the last one is the one that the application will use.  That may not be
   * correct, but it's how we've been doing it for years.
   *
   * Perhaps we should instead query navigator.mimeTypes for the set of types
   * supported by the application and then get the plugin from each MIME type's
   * enabledPlugin property.  But if there's a plugin for a type, we need
   * to know about it even if it isn't enabled, since we're going to give
   * the user an option to enable it.
   * 
   * I'll also note that my reading of nsPluginTag::RegisterWithCategoryManager
   * suggests that enabledPlugin is only determined during registration
   * and does not get updated when plugin.disable_full_page_plugin_for_types
   * changes (unless modification of that preference spawns reregistration).
   * So even if we could use enabledPlugin to get the plugin that would be used,
   * we'd still need to check the pref ourselves to find out if it's enabled.
   */
  _loadPluginHandlers: function() {
    for (let i = 0; i < navigator.plugins.length; ++i) {
      let plugin = navigator.plugins[i];
      for (let j = 0; j < plugin.length; ++j) {
        let type = plugin[j].type;
        let handlerInfoWrapper;

        if (typeof this._handledTypes[type] == "undefined") {
          let wrappedHandlerInfo =
            this._mimeSvc.getFromTypeAndExtension(type, null);
          handlerInfoWrapper = new HandlerInfoWrapper(type, wrappedHandlerInfo);
          this._handledTypes[type] = handlerInfoWrapper;
        }
        else
          handlerInfoWrapper = this._handledTypes[type];

        handlerInfoWrapper.plugin = plugin;
        handlerInfoWrapper.handledOnlyByPlugin = true;
      }
    }
  },

  /**
   * Load the set of handlers defined by the application datastore.
   */
  _loadApplicationHandlers: function() {
    var wrappedHandlerInfos = this._handlerSvc.enumerate();
    while (wrappedHandlerInfos.hasMoreElements()) {
      let wrappedHandlerInfo = wrappedHandlerInfos.getNext().
                               QueryInterface(Ci.nsIHandlerInfo);
      let type = wrappedHandlerInfo.type;
      let handlerInfoWrapper;

      if (typeof this._handledTypes[type] == "undefined") {
        handlerInfoWrapper = new HandlerInfoWrapper(type, wrappedHandlerInfo);
        this._handledTypes[type] = handlerInfoWrapper;
      }
      else
        handlerInfoWrapper = this._handledTypes[type];

      handlerInfoWrapper.handledOnlyByPlugin = false;
    }
  },


  //**************************************************************************//
  // View Construction

  rebuildView: function() {
    // Clear the list of entries.
    while (this._list.childNodes.length > 1)
      this._list.removeChild(this._list.lastChild);

    var visibleTypes = this._getVisibleTypes();

    if (this._sortColumn)
      this._sortTypes(visibleTypes);

    for each (let visibleType in visibleTypes) {
      let item = document.createElement("richlistitem");
      item.setAttribute("type", visibleType.type);
      item.setAttribute("typeDescription", visibleType.description);
      item.setAttribute("typeIcon", visibleType.smallIcon);
      item.setAttribute("actionDescription",
                        this._describePreferredAction(visibleType));
      item.setAttribute("actionIcon",
                        this._getIconURLForPreferredAction(visibleType));
      this._list.appendChild(item);
      if (visibleType.type == this._list.getAttribute("lastSelectedType"))
        this._list.selectedItem = item;
    }
  },

  _getVisibleTypes: function() {
    var visibleTypes = [];

    var showPlugins = this._prefSvc.getBoolPref(PREF_SHOW_PLUGINS_IN_LIST);
    var hideTypesWithoutExtensions =
      this._prefSvc.getBoolPref(PREF_HIDE_PLUGINS_WITHOUT_EXTENSIONS);

    for (let type in this._handledTypes) {
      let handlerInfo = this._handledTypes[type];

      // Hide types without extensions if so prefed so we don't show a whole
      // bunch of obscure types handled by plugins on Mac.
      // Note: though protocol types don't have extensions, we still show them;
      // the pref is only meant to be applied to MIME types.
      // FIXME: if the type has a plugin, should we also check the "suffixes"
      // property of the plugin?  Filed as bug 395135.
      if (hideTypesWithoutExtensions &&
          handlerInfo.wrappedHandlerInfo instanceof Ci.nsIMIMEInfo &&
          !handlerInfo.primaryExtension)
        continue;

      // Hide types handled only by plugins if so prefed.
      if (handlerInfo.handledOnlyByPlugin && !showPlugins)
        continue;

      // Hide types handled only by disabled plugins.
      // FIXME: we should show these types to give the user a chance to reenable
      // the plugins.  Filed as bug 395136.
      if (handlerInfo.handledOnlyByPlugin && handlerInfo.isDisabledPluginType)
        continue;

      // Don't display entries for types we always ask about before handling.
      // FIXME: that's what the old code did, but we should be showing these
      // types and letting users choose to do something different.  Filed as
      // bug 395138.
      if (handlerInfo.alwaysAskBeforeHandling &&
          handlerInfo.type != TYPE_MAYBE_FEED)
        continue;

      // If the user is filtering the list, then only show matching types.
      if (this._filter.value && !this._matchesFilter(handlerInfo))
        continue;

      // We couldn't find any reason to exclude the type, so include it.
      visibleTypes.push(handlerInfo);
    }

    return visibleTypes;
  },

  _matchesFilter: function(aType) {
    var filterValue = this._filter.value.toLowerCase();
    return aType.description.toLowerCase().indexOf(filterValue) != -1 ||
           this._describePreferredAction(aType).toLowerCase().indexOf(filterValue) != -1;
  },

  /**
   * Describe, in a human-readable fashion, the preferred action to take on
   * the type represented by the given handler info object.
   *
   * XXX Should this be part of the HandlerInfoWrapper interface?  It would
   * violate the separation of model and view, but it might make more sense
   * nonetheless (f.e. it would make sortTypes easier).
   *
   * @param aHandlerInfo {nsIHandlerInfo} the type whose preferred action
   *                                      is being described
   */
  _describePreferredAction: function(aHandlerInfo) {
    // alwaysAskBeforeHandling overrides the preferred action, so if that flag
    // is set, then describe that behavior instead.  Currently we hide all types
    // with alwaysAskBeforeHandling except for the feed type, so here we use
    // a feed-specific message to describe the behavior.
    if (aHandlerInfo.alwaysAskBeforeHandling)
      return this._bundle.getString("alwaysAskAboutFeed");

    switch (aHandlerInfo.preferredAction) {
      case Ci.nsIHandlerInfo.saveToDisk:
        return this._bundle.getString("saveToDisk");

      case Ci.nsIHandlerInfo.useHelperApp:
        return aHandlerInfo.preferredApplicationHandler.name;

      case Ci.nsIHandlerInfo.handleInternally:
        // For the feed type, handleInternally means live bookmarks.
        if (aHandlerInfo.type == TYPE_MAYBE_FEED)
          return this._bundle.getString("liveBookmarks");

        // For other types, handleInternally looks like either useHelperApp
        // or useSystemDefault depending on whether or not there's a preferred
        // handler app.
        if (this.isValidHandlerApp(aHandlerInfo.preferredApplicationHandler))
          return aHandlerInfo.preferredApplicationHandler.name;

        return aHandlerInfo.defaultDescription;

        // XXX Why don't we say the app will handle the type internally?
        // Is it because the app can't actually do that?  But if that's true,
        // then why would a preferredAction ever get set to this value
        // in the first place?

      case Ci.nsIHandlerInfo.useSystemDefault:
        return aHandlerInfo.defaultDescription;

      case kActionUsePlugin:
        return aHandlerInfo.plugin.name;
    }
  },

  /**
   * Whether or not the given handler app is valid.
   *
   * @param aHandlerApp {nsIHandlerApp} the handler app in question
   *
   * @returns {boolean} whether or not it's valid
   */
  isValidHandlerApp: function(aHandlerApp) {
    if (!aHandlerApp)
      return false;

    if (aHandlerApp instanceof Ci.nsILocalHandlerApp)
      return aHandlerApp.executable &&
             aHandlerApp.executable.exists() &&
             aHandlerApp.executable.isExecutable();

    if (aHandlerApp instanceof Ci.nsIWebHandlerApp)
      return aHandlerApp.uriTemplate;

    if (aHandlerApp instanceof Ci.nsIWebContentHandlerInfo)
      return aHandlerApp.uri;

    return false;
  },

  /**
   * Rebuild the actions menu for the selected entry.  Gets called by
   * the richlistitem constructor when an entry in the list gets selected.
   */
  rebuildActionsMenu: function() {
    var typeItem = this._list.selectedItem;
    var handlerInfo = this._handledTypes[typeItem.type];
    var menu =
      document.getAnonymousElementByAttribute(typeItem, "class", "actionsMenu");
    var menuPopup = menu.firstChild;

    // Clear out existing items.
    while (menuPopup.hasChildNodes())
      menuPopup.removeChild(menuPopup.lastChild);

    // If this is the feed type, add "always ask" and "live bookmarks" items.
    if (handlerInfo.type == TYPE_MAYBE_FEED) {
      let menuItem = document.createElementNS(kXULNS, "menuitem");
      menuItem.setAttribute("alwaysAsk", "true");
      menuItem.setAttribute("label", this._bundle.getString("alwaysAskAboutFeed"));
      menuPopup.appendChild(menuItem);
      if (handlerInfo.alwaysAskBeforeHandling)
        menu.selectedItem = menuItem;

      menuItem = document.createElementNS(kXULNS, "menuitem");
      menuItem.setAttribute("action", Ci.nsIHandlerInfo.handleInternally);
      menuItem.setAttribute("label", this._bundle.getString("liveBookmarks"));
      menuItem.setAttribute("image", "chrome://browser/skin/page-livemarks.png");
      menuPopup.appendChild(menuItem);
      if (handlerInfo.preferredAction == Ci.nsIHandlerInfo.handleInternally)
        menu.selectedItem = menuItem;
    }

    // Create a menu item for the OS default application, if any.
    if (handlerInfo.hasDefaultHandler) {
      let menuItem = document.createElementNS(kXULNS, "menuitem");
      menuItem.setAttribute("action", Ci.nsIHandlerInfo.useSystemDefault);
      menuItem.setAttribute("label", handlerInfo.defaultDescription);

      if (handlerInfo.wrappedHandlerInfo) {
        let iconURL =
          this._getIconURLForSystemDefault(handlerInfo.wrappedHandlerInfo);
        menuItem.setAttribute("image", iconURL);
      }

      menuPopup.appendChild(menuItem);
      if (handlerInfo.preferredAction == Ci.nsIHandlerInfo.useSystemDefault)
        menu.selectedItem = menuItem;
    }

    // Create menu items for possible handlers.
    let preferredApp = handlerInfo.preferredApplicationHandler;
    let possibleApps = handlerInfo.possibleApplicationHandlers.
                       QueryInterface(Ci.nsIArray).enumerate();
    while (possibleApps.hasMoreElements()) {
      let possibleApp = possibleApps.getNext();
      if (!this.isValidHandlerApp(possibleApp))
        continue;

      let menuItem = document.createElementNS(kXULNS, "menuitem");
      menuItem.setAttribute("action", Ci.nsIHandlerInfo.useHelperApp);
      menuItem.setAttribute("label", possibleApp.name);
      menuItem.setAttribute("image", this._getIconURLForHandlerApp(possibleApp));

      // Attach the handler app object to the menu item so we can use it
      // to make changes to the datastore when the user selects the item.
      menuItem.handlerApp = possibleApp;

      menuPopup.appendChild(menuItem);

      // Select this app if the preferred action is to use a helper app
      // and this is the preferred app.
      if (handlerInfo.preferredAction == Ci.nsIHandlerInfo.useHelperApp &&
          preferredApp.equals(possibleApp))
        menu.selectedItem = menuItem;
    }

    // Create a menu item for the plugin.
    if (handlerInfo.plugin) {
      let menuItem = document.createElementNS(kXULNS, "menuitem");
      menuItem.setAttribute("action", kActionUsePlugin);
      menuItem.setAttribute("label", handlerInfo.plugin.name);
      menuPopup.appendChild(menuItem);
      if (handlerInfo.preferredAction == kActionUsePlugin)
        menu.selectedItem = menuItem;
    }

    // Create a menu item for saving to disk.
    // Note: this option isn't available to protocol types, since we don't know
    // what it means to save a URL having a certain scheme to disk, nor is it
    // available to feeds, since the feed code doesn't implement the capability.
    // And it's not available to types handled only by plugins either, although
    // I would think we'd want to give users the ability to redirect that stuff
    // to disk (so maybe we should revisit that decision).
    if ((handlerInfo instanceof Ci.nsIMIMEInfo) &&
        handlerInfo.type != TYPE_MAYBE_FEED &&
        !handlerInfo.handledOnlyByPlugin) {
      let menuItem = document.createElementNS(kXULNS, "menuitem");
      menuItem.setAttribute("action", Ci.nsIHandlerInfo.saveToDisk);
      menuItem.setAttribute("label", this._bundle.getString("saveToDisk"));
      menuPopup.appendChild(menuItem);
      if (handlerInfo.preferredAction == Ci.nsIHandlerInfo.saveToDisk)
        menu.selectedItem = menuItem;
    }

    // Create a menu item for selecting a local application.
    {
      let menuItem = document.createElementNS(kXULNS, "menuitem");
      menuItem.setAttribute("oncommand", "gApplicationsPane.chooseApp(event)");
      menuItem.setAttribute("label", this._bundle.getString("chooseApp"));
      menuPopup.appendChild(menuItem);
    }

    // Create a menu item for removing this entry unless it's a plugin
    // or the feed type, which cannot be removed.
    // XXX Should this perhaps be a button on the entry, or should we perhaps
    // provide no UI for removing entries at all?
    if (!handlerInfo.plugin && handlerInfo.type != TYPE_MAYBE_FEED) {
      let menuItem = document.createElementNS(kXULNS, "menuitem");
      menuItem.setAttribute("oncommand", "gApplicationsPane.removeType(event)");
      menuItem.setAttribute("label", this._bundle.getString("removeType"));
      menuPopup.appendChild(menuItem);
    }
  },


  //**************************************************************************//
  // Sorting & Filtering

  _sortColumn: null,

  /**
   * Sort the list when the user clicks on a column header.
   */
  sort: function (event) {
    var column = event.target;

    // If the user clicked on a new sort column, remove the direction indicator
    // from the old column.
    if (this._sortColumn && this._sortColumn != column)
      this._sortColumn.removeAttribute("sortDirection");

    this._sortColumn = column;

    // Set (or switch) the sort direction indicator.
    if (column.getAttribute("sortDirection") == "ascending")
      column.setAttribute("sortDirection", "descending");
    else
      column.setAttribute("sortDirection", "ascending");

    this.rebuildView();
  },

  /**
   * Given an array of HandlerInfoWrapper objects, sort them according to
   * the current sort order.  Used by rebuildView to sort the set of visible
   * types before building the list from them.
   */
  _sortTypes: function(aTypes) {
    if (!this._sortColumn)
      return;

    function sortByType(a, b) {
      return a.description.toLowerCase().localeCompare(b.description.toLowerCase());
    }

    var t = this;
    function sortByAction(a, b) {
      return t._describePreferredAction(a).toLowerCase().
             localeCompare(t._describePreferredAction(b).toLowerCase());
    }

    switch (this._sortColumn.getAttribute("value")) {
      case "type":
        aTypes.sort(sortByType);
        break;
      case "action":
        aTypes.sort(sortByAction);
        break;
    }

    if (this._sortColumn.getAttribute("sortDirection") == "descending")
      aTypes.reverse();
  },

  /**
   * Filter the list when the user enters a filter term into the filter field.
   */
  filter: function() {
    if (this._filter.value == "") {
      this.clearFilter();
      return;
    }

    this.rebuildView();

    document.getElementById("filterActiveLabel").hidden = false;
    document.getElementById("clearFilter").disabled = false;
  },

  _filterTimeout: null,

  onFilterInput: function() {
    if (this._filterTimeout)
      clearTimeout(this._filterTimeout);
   
    this._filterTimeout = setTimeout("gApplicationsPane.filter()", 500);
  },

  onFilterKeyPress: function(aEvent) {
    if (aEvent.keyCode == KeyEvent.DOM_VK_ESCAPE)
      this.clearFilter();
  },
  
  clearFilter: function() {
    this._filter.value = "";
    this.rebuildView();

    this._filter.focus();
    document.getElementById("filterActiveLabel").hidden = true;
    document.getElementById("clearFilter").disabled = true;
  },

  focusFilterBox: function() {
    this._filter.focus();
    this._filter.select();
  },


  //**************************************************************************//
  // Changes

  onSelectAction: function(event) {
    var actionItem = event.originalTarget;
    var typeItem = this._list.selectedItem;
    var handlerInfo = this._handledTypes[typeItem.type];

    if (actionItem.hasAttribute("alwaysAsk")) {
      handlerInfo.alwaysAskBeforeHandling = true;
    }
    else if (actionItem.hasAttribute("action")) {
      let action = parseInt(actionItem.getAttribute("action"));

      // Set the plugin state if we're enabling or disabling a plugin.
      if (action == kActionUsePlugin)
        handlerInfo.enablePluginType();
      else if (handlerInfo.plugin && !handlerInfo.isDisabledPluginType)
        handlerInfo.disablePluginType();

      // Set the preferred application handler.
      // FIXME: consider leaving the existing preferred app in the list
      // when we set the preferred action to something other than useHelperApp
      // so that legacy datastores that don't have the preferred app in the list
      // of possible apps still include the preferred app in the list of apps
      // the user can use to handle the type.  Filed as bug 395140.
      if (action == Ci.nsIHandlerInfo.useHelperApp)
        handlerInfo.preferredApplicationHandler = actionItem.handlerApp;
      else
        handlerInfo.preferredApplicationHandler = null;

      // Set the "always ask" flag.
      handlerInfo.alwaysAskBeforeHandling = false;

      // Set the preferred action.
      handlerInfo.preferredAction = action;
    }

    handlerInfo.store();

    // Update the action label so it says the right thing once this type item
    // is no longer selected.
    typeItem.setAttribute("actionDescription",
                          this._describePreferredAction(handlerInfo));
  },

  chooseApp: function(aEvent) {
    // Don't let the normal "on select action" handler get this event,
    // as we handle it specially ourselves.
    aEvent.stopPropagation();

    var fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    var winTitle = this._bundle.getString("fpTitleChooseApp");
    fp.init(window, winTitle, Ci.nsIFilePicker.modeOpen);
    fp.appendFilters(Ci.nsIFilePicker.filterApps);

    if (fp.show() == Ci.nsIFilePicker.returnOK && fp.file) {
      // XXXben - we need to compare this with the running instance executable
      //          just don't know how to do that via script...
      // XXXmano TBD: can probably add this to nsIShellService
#ifdef XP_WIN
#expand      if (fp.file.leafName == "__MOZ_APP_NAME__.exe")
#else
#ifdef XP_MACOSX
#expand      if (fp.file.leafName == "__MOZ_APP_DISPLAYNAME__.app")
#else
#expand      if (fp.file.leafName == "__MOZ_APP_NAME__-bin")
#endif
#endif
        { this.rebuildActionsMenu(); return; }

      let handlerApp = Cc["@mozilla.org/uriloader/local-handler-app;1"].
                       createInstance(Ci.nsIHandlerApp);
      handlerApp.name = getDisplayNameForFile(fp.file);
      handlerApp.QueryInterface(Ci.nsILocalHandlerApp);
      handlerApp.executable = fp.file;

      var handlerInfo = this._handledTypes[this._list.selectedItem.type];

      handlerInfo.preferredApplicationHandler = handlerApp;
      handlerInfo.preferredAction = Ci.nsIHandlerInfo.useHelperApp;

      handlerInfo.store();
    }

    // We rebuild the actions menu whether the user picked an app or canceled.
    // If they picked an app, we want to add the app to the menu and select it.
    // If they canceled, we want to go back to their previous selection.
    this.rebuildActionsMenu();
  },

  removeType: function() {
    var promptService = Cc["@mozilla.org/embedcomp/prompt-service;1"].
                        getService(Ci.nsIPromptService);
    var flags = Ci.nsIPromptService.BUTTON_TITLE_IS_STRING * Ci.nsIPromptService.BUTTON_POS_0;
    flags += Ci.nsIPromptService.BUTTON_TITLE_CANCEL * Ci.nsIPromptService.BUTTON_POS_1;

    var title = this._bundle.getString("removeTitle");
    var message = this._bundle.getString("removeMessage");
    var button = this._bundle.getString("removeButton");
    var rv = promptService.confirmEx(window, title, message, flags, button, 
                                     null, null, null, { value: 0 });

    if (rv == 0) {
      // Remove information about the type from the handlers datastore.
      let listItem = this._list.selectedItem;
      let handlerInfo = this._handledTypes[listItem.type];
      handlerInfo.remove();

      // Select the next item in the list (or the previous item if the item
      // being removed is the last item).
      if (this._list.selectedIndex == this._list.getRowCount() - 1)
        this._list.selectedIndex = this._list.selectedIndex - 1;
      else
        this._list.selectedIndex = this._list.selectedIndex + 1;

      // Remove the item from the list.
      this._list.removeChild(listItem);
    }
    else {
      // Rebuild the actions menu so we go back to their previous selection.
      this.rebuildActionsMenu();
    }
  },

  // Mark which item in the list was last selected so we can reselect it
  // when we rebuild the list or when the user returns to the prefpane.
  onSelectionChanged: function() {
    this._list.setAttribute("lastSelectedType",
                            this._list.selectedItem.getAttribute("type"));
  },

  _getIconURLForPreferredAction: function(aHandlerInfo) {
    var preferredApp = aHandlerInfo.preferredApplicationHandler;

    if (aHandlerInfo.preferredAction == Ci.nsIHandlerInfo.useHelperApp &&
        this.isValidHandlerApp(preferredApp))
      return this._getIconURLForHandlerApp(preferredApp);

    if (aHandlerInfo.preferredAction == Ci.nsIHandlerInfo.useSystemDefault &&
        aHandlerInfo.wrappedHandlerInfo)
      return this._getIconURLForSystemDefault(aHandlerInfo.wrappedHandlerInfo);

    // We don't know how to get an icon URL for any other actions.
    return "";
  },

  _getIconURLForHandlerApp: function(aHandlerApp) {
    if (aHandlerApp instanceof Ci.nsILocalHandlerApp)
      return this._getIconURLForFile(aHandlerApp.executable);

    if (aHandlerApp instanceof Ci.nsIWebHandlerApp)
      return this._getIconURLForWebApp(aHandlerApp.uriTemplate);

    if (aHandlerApp instanceof Ci.nsIWebContentHandlerInfo)
      return this._getIconURLForWebApp(aHandlerApp.uri)

    // We know nothing about other kinds of handler apps.
    return "";
  },

  _getIconURLForFile: function(aFile) {
    var fph = this._ioSvc.getProtocolHandler("file").
              QueryInterface(Ci.nsIFileProtocolHandler);
    var urlSpec = fph.getURLSpecFromFile(aFile);

    return "moz-icon://" + urlSpec + "?size=16";
  },

  _getIconURLForWebApp: function(aWebAppURITemplate) {
    var uri = this._ioSvc.newURI(aWebAppURITemplate, null, null);

    // Unfortunately we can't use the favicon service to get the favicon,
    // because the service looks in the annotations table for a record with
    // the exact URL we give it, and users won't have such records for URLs
    // they don't visit, and users won't visit the web app's URL template,
    // they'll only visit URLs derived from that template (i.e. with %s
    // in the template replaced by the URL of the content being handled).

    if (/^https?/.test(uri.scheme))
      return uri.prePath + "/favicon.ico";

    return "";
  },

  _getIconURLForSystemDefault: function(aHandlerInfo) {
    // Handler info objects for MIME types on Windows implement a property
    // bag interface from which we can get an icon for the default app, so if
    // we're dealing with a MIME type on Windows, then try to get the icon.
    if (aHandlerInfo instanceof Ci.nsIMIMEInfo &&
        aHandlerInfo instanceof Ci.nsIPropertyBag) {
      try {
        let url = aHandlerInfo.getProperty("defaultApplicationIconURL");
        if (url)
          return url + "?size=16";
      }
      catch(ex) {}
    }

    // We don't know how to get an icon URL on any other OSes or for any other
    // classes of content type.
    return "";
  }

};
