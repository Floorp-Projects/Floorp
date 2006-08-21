# -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
# The Original Code is the Feed Writer.
#
# The Initial Developer of the Original Code is Google Inc.
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ben Goodger <beng@google.com>
#   Jeff Walden <jwalden+code@mit.edu>
#   Asaf Romano <mozilla.mano@sent.com>
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
# ***** END LICENSE BLOCK ***** */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

function LOG(str) {
  var prefB = 
    Cc["@mozilla.org/preferences-service;1"].
    getService(Ci.nsIPrefBranch);

  var shouldLog = false;
  try {
    shouldLog = prefB.getBoolPref("feeds.log");
  } 
  catch (ex) {
  }

  if (shouldLog)
    dump("*** Feeds: " + str + "\n");
}

const XML_NS = "http://www.w3.org/XML/1998/namespace"
const HTML_NS = "http://www.w3.org/1999/xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const TYPE_MAYBE_FEED = "application/vnd.mozilla.maybe.feed";
const URI_BUNDLE = "chrome://browser/locale/feeds/subscribe.properties";

const PREF_SELECTED_APP = "browser.feeds.handlers.application";
const PREF_SELECTED_WEB = "browser.feeds.handlers.webservice";
const PREF_SELECTED_ACTION = "browser.feeds.handler";
const PREF_SELECTED_READER = "browser.feeds.handler.default";
const PREF_SKIP_PREVIEW_PAGE = "browser.feeds.skip_preview_page";
const PREF_SHOW_FIRST_RUN_UI = "browser.feeds.showFirstRunUI";

const FW_CLASSID = Components.ID("{49bb6593-3aff-4eb3-a068-2712c28bd58e}");
const FW_CLASSNAME = "Feed Writer";
const FW_CONTRACTID = "@mozilla.org/browser/feeds/result-writer;1";

function FeedWriter() {
}
FeedWriter.prototype = {
  _getPropertyAsBag: function FW__getPropertyAsBag(container, property) {
    return container.fields.getProperty(property).
                     QueryInterface(Ci.nsIPropertyBag2);
  },
  
  _getPropertyAsString: function FW__getPropertyAsString(container, property) {
    try {
      return container.fields.getPropertyAsAString(property);
    }
    catch (e) {
    }
    return "";
  },
  
  _setContentText: function FW__setContentText(id, text) {
    var element = this._document.getElementById(id);
    while (element.hasChildNodes())
      element.removeChild(element.firstChild);
    element.appendChild(this._document.createTextNode(text));
  },
  
  /**
   * Safely sets the href attribute on an anchor tag, providing the URI 
   * specified can be loaded according to rules. 
   * @param   element
   *          The element to set a URI attribute on
   * @param   attribute
   *          The attribute of the element to set the URI to, e.g. href or src
   * @param   uri
   *          The URI spec to set as the href
   */
  _safeSetURIAttribute: 
  function FW__safeSetURIAttribute(element, attribute, uri) {
    var secman = 
        Cc["@mozilla.org/scriptsecuritymanager;1"].
        getService(Ci.nsIScriptSecurityManager);    
    const flags = Ci.nsIScriptSecurityManager.DISALLOW_SCRIPT_OR_DATA;
    try {
      secman.checkLoadURIStr(this._window.location.href, uri, flags);
      // checkLoadURIStr will throw if the link URI should not be loaded per 
      // the rules specified in |flags|, so we'll never "linkify" the link...
      element.setAttribute(attribute, uri);
    }
    catch (e) {
      // Not allowed to load this link because secman.checkLoadURIStr threw
    }
  },
  
  get _bundle() {
    var sbs = 
        Cc["@mozilla.org/intl/stringbundle;1"].
        getService(Ci.nsIStringBundleService);
    return sbs.createBundle(URI_BUNDLE);
  },
  
  _getFormattedString: function FW__getFormattedString(key, params) {
    return this._bundle.formatStringFromName(key, params, params.length);
  },
  
  _getString: function FW__getString(key) {
    return this._bundle.GetStringFromName(key);
  },
  
  /**
   * Writes the feed title into the preview document.
   * @param   container
   *          The feed container
   */
  _setTitleText: function FW__setTitleText(container) {
    if(container.title) {
      this._setContentText("feedTitleText", container.title.plainText());
      this._document.title = container.title.plainText();
    }
    
    var feed = container.QueryInterface(Ci.nsIFeed);
    if(feed && feed.subtitle)
      this._setContentText("feedSubtitleText", container.subtitle.plainText());
  },
  
  /**
   * Writes the title image into the preview document if one is present.
   * @param   container
   *          The feed container
   */
  _setTitleImage: function FW__setTitleImage(container) {
    try {
      var parts = this._getPropertyAsBag(container, "image");
      
      // Set up the title image (supplied by the feed)
      var feedTitleImage = this._document.getElementById("feedTitleImage");
      this._safeSetURIAttribute(feedTitleImage, "src", 
                                parts.getPropertyAsAString("url"));
      
      // Set up the title image link
      var feedTitleLink = this._document.getElementById("feedTitleLink");
      
      var titleText = 
        this._getFormattedString("linkTitleTextFormat", 
                                 [parts.getPropertyAsAString("title")]);
      feedTitleLink.setAttribute("title", titleText);
      this._safeSetURIAttribute(feedTitleLink, "href", 
                                parts.getPropertyAsAString("link"));

      // Fix the margin on the main title, so that the image doesn't run over
      // the underline
      var feedTitleText = this._document.getElementById("feedTitleText");
      var titleImageWidth = parseInt(parts.getPropertyAsAString("width")) + 15;
      feedTitleText.style.marginRight = titleImageWidth + "px";
    }
    catch (e) {
      LOG("Failed to set Title Image (this is benign): " + e);
    }
  },
  
  /**
   * Writes all entries contained in the feed.
   * @param   container
   *          The container of entries in the feed
   */
  _writeFeedContent: function FW__writeFeedContent(container) {
    // Build the actual feed content
    var feedContent = this._document.getElementById("feedContent");
    var feed = container.QueryInterface(Ci.nsIFeed);
    
    for (var i = 0; i < feed.items.length; ++i) {
      var entry = feed.items.queryElementAt(i, Ci.nsIFeedEntry);
      entry.QueryInterface(Ci.nsIFeedContainer);
      
      var entryContainer = this._document.createElementNS(HTML_NS, "div");
      entryContainer.className = "entry";

      // If the entry has a title, make it a like
      if (entry.title) {
        var a = this._document.createElementNS(HTML_NS, "a");
        a.appendChild(this._document.createTextNode(entry.title.plainText()));
      
        // Entries are not required to have links, so entry.link can be null.
        if (entry.link)
          this._safeSetURIAttribute(a, "href", entry.link.spec);

        var title = this._document.createElementNS(HTML_NS, "h3");
        title.appendChild(a);
        entryContainer.appendChild(title);
      }

      var body = this._document.createElementNS(HTML_NS, "p");
      var summary = entry.summary || entry.content;
      var docFragment = null;
      if (summary) {

        if (summary.base)
          body.setAttributeNS(XML_NS, "base", summary.base.spec);
        else
          LOG("no base?");
        docFragment = summary.createDocumentFragment(body);
        body.appendChild(docFragment);

        // If the entry doesn't have a title, append a # permalink
        // See http://scripting.com/rss.xml for an example
        if (!entry.title && entry.link) {
          var a = this._document.createElementNS(HTML_NS, "a");
          a.appendChild(this._document.createTextNode("#"));
          this._safeSetURIAttribute(a, "href", entry.link.spec);
          body.appendChild(this._document.createTextNode(" "));
          body.appendChild(a);
        }

      }
      body.className = "feedEntryContent";
      entryContainer.appendChild(body);
      feedContent.appendChild(entryContainer);
    }
  },
  
  /**
   * Gets a valid nsIFeedContainer object from the parsed nsIFeedResult.
   * Displays error information if there was one.
   * @param   result
   *          The parsed feed result
   * @returns A valid nsIFeedContainer object containing the contents of
   *          the feed.
   */
  _getContainer: function FW__getContainer(result) {
    var feedService = 
        Cc["@mozilla.org/browser/feeds/result-service;1"].
        getService(Ci.nsIFeedResultService);
        
    var ios = 
        Cc["@mozilla.org/network/io-service;1"].
        getService(Ci.nsIIOService);
 
    try {
      var result = 
        feedService.getFeedResult(this._getOriginalURI(this._window));
    }
    catch (e) {
      LOG("Subscribe Preview: feed not available?!");
    }
    
    if (result.bozo) {
      LOG("Subscribe Preview: feed result is bozo?!");
    }

    try {
      var container = result.doc;
      container.title;
    }
    catch (e) {
      LOG("Subscribe Preview: An error occurred in parsing! Fortunately, you can still subscribe...");
      var feedError = this._document.getElementById("feedError");
      feedError.removeAttribute("style");
      var feedBody = this._document.getElementById("feedBody");
      feedBody.setAttribute("style", "display:none;");
      this._setContentText("errorCode", e);
      return null;
    }
    return container;
  },
  
  /**
   * Get the human-readable display name of a file. This could be the 
   * application name.
   * @param   file
   *          A nsIFile to look up the name of
   * @returns The display name of the application represented by the file.
   */
  _getFileDisplayName: function FW__getFileDisplayName(file) {
#ifdef XP_WIN
    if (file instanceof Ci.nsILocalFileWin) {
      try {
        return file.getVersionInfoField("FileDescription");
      }
      catch (e) {
      }
    }
#endif
#ifdef XP_MACOSX
    var lfm = file.QueryInterface(Components.interfaces.nsILocalFileMac_MOZILLA_1_8_BRANCH);
    try {
      return lfm.bundleDisplayName;
    }
    catch (e) {
      // fall through to the file name
    }
#endif
    var ios = 
        Cc["@mozilla.org/network/io-service;1"].
        getService(Ci.nsIIOService);
    var url = ios.newFileURI(file).QueryInterface(Ci.nsIURL);
    return url.fileName;
  },

  /**
   * Get moz-icon url for a file
   * @param   file
   *          A nsIFile to look up the name of
   * @returns moz-icon url of the given file as a string
   */
  _getFileIconURL: function FW__getFileIconURL(file) {
    var ios = Cc["@mozilla.org/network/io-service;1"].
              getService(Components.interfaces.nsIIOService);
    var fph = ios.getProtocolHandler("file")
                 .QueryInterface(Ci.nsIFileProtocolHandler);
    var urlSpec = fph.getURLSpecFromFile(file);
    return "moz-icon://" + urlSpec + "?size=16";
  },

  _chooseClientApp: function FW__chooseClientApp() {
    try {
      var fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
      fp.init(this._window,
              this._getString("chooseApplicationDialogTitle"),
              Ci.nsIFilePicker.modeOpen);
      fp.appendFilters(Ci.nsIFilePicker.filterApps);

      if (fp.show() == Ci.nsIFilePicker.returnOK) {
        var selectedApp = fp.file;
        if (selectedApp) {
          // XXXben - we need to compare this with the running instance executable
          //          just don't know how to do that via script...
          // XXXmano TBD: can probably add this to nsIShellService
#ifdef XP_WIN
          if (fp.file.leafName != "firefox.exe") {
#else
#ifdef XP_MACOSX
          if (fp.file.leafName != "Firefox.app") {
#else
          if (fp.file.leafName != "firefox-bin") {
#endif
#endif
            var selectedAppMenuItem =
              this._document.getElementById("selectedAppMenuItem");

            selectedAppMenuItem.wrappedJSObject.file = selectedApp;
            selectedAppMenuItem.setAttribute("label",
                                             this._getFileDisplayName(selectedApp));
            selectedAppMenuItem.setAttribute("src",
                                             this._getFileIconURL(selectedApp));

            // Show and select the selected application menuitem
            selectedAppMenuItem.wrappedJSObject.hidden = false;
            selectedAppMenuItem.doCommand();
            return;
          }
        }
      }
    }
    catch(ex) { }

    // Select the (per-prefs) selected handler if no application was selected
    this._setSelectedHandler();
  },

  _setAlwaysUseCheckedState: function FW__setAlwaysUseCheckedState() {
    var checkbox = this._document.getElementById("alwaysUse");
    if (checkbox) {
      var alwaysUse = false;
      try {
        var prefs = Cc["@mozilla.org/preferences-service;1"].
                    getService(Ci.nsIPrefBranch);
        if (prefs.getCharPref(PREF_SELECTED_ACTION) != "ask")
          alwaysUse = true;
      }
      catch(ex) { }
      checkbox.wrappedJSObject.checked = alwaysUse;
    }
  },

  _setAlwaysUseLabel: function FW__setAlwaysUseLabel() {
    var checkbox = this._document.getElementById("alwaysUse");
    if (checkbox) {
      var handlersMenuList = this._document.getElementById("handlersMenuList");
      if (handlersMenuList) {
        var handlerName = handlersMenuList.wrappedJSObject.selectedItem.getAttribute("label");
        checkbox.wrappedJSObject.label = this._getFormattedString("alwaysUse", [handlerName]);
      }
    }
  },

  /**
   * See nsIDOMEventListener
   */
  handleEvent: function(event) {
    if (event.target.ownerDocument != this._document) {
      LOG("FeedWriter.handleEvent: Someone passed the feed writer as a listener to the events of another document!");
      return;
    }

    if (event.type == "command") {
      switch(event.target.id) {
        case "subscribeButton":
          this.subscribe();
          break;
        case "chooseApplicationMenuItem":
          this._chooseClientApp();
          break;
        default:
          this._setAlwaysUseLabel();
      }
    }
  },

  _setSelectedHandler: function FW__setSelectedHandler() {
    var prefs =   
        Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefBranch);

    var handler = "bookmarks";
    try {
      handler = prefs.getCharPref(PREF_SELECTED_READER);
    }
    catch (ex) { }
    
    switch (handler) {
      case "web": {
        var handlersMenuList = this._document.getElementById("handlersMenuList");
        if (handlersMenuList) {
          var url = prefs.getCharPref(PREF_SELECTED_WEB);
          var handlers =
            handlersMenuList.getElementsByAttribute("webhandlerurl", url);
          if (handlers.length == 0) {
            LOG("FeedWriter._setSelectedHandler: selected web handler isn't in the menulist")
            return;
          }

          handlers[0].doCommand();
        }
        break;
      }
      case "client": {
        var selectedAppMenuItem =
          this._document.getElementById("selectedAppMenuItem");
        if (selectedAppMenuItem) {
          try {
            var selectedApp = prefs.getComplexValue(PREF_SELECTED_APP,
                                                    Ci.nsILocalFile);
          } catch(ex) { }

          if (selectedApp) {
            selectedAppMenuItem.file = selectedApp;
            selectedAppMenuItem.setAttribute("label",
                                             this._getFileDisplayName(selectedApp));
            selectedAppMenuItem.setAttribute("src",
                                             this._getFileIconURL(selectedApp));

            selectedAppMenuItem.wrappedJSObject.hidden = false;
            selectedAppMenuItem.doCommand();

#ifdef ENABLE_SYSTEM_FEED_READER_CODE
            // Only show the default reader menuitem if the default reader
            // isn't the selected application
            var defaultHandlerMenuItem =
            this._document.getElementById("defaultHandlerMenuItem");
            if (defaultHandlerMenuItem) {
              defaultHandlerMenuItem.wrappedJSObject.hidden =
                defaultHandlerMenuItem.file.path == selectedApp.path;
            }
#endif
            break;
          }
        }
      }
      case "bookmarks":
      default: {
        var liveBookmarksMenuItem =
          this._document.getElementById("liveBookmarksMenuItem");
        if (liveBookmarksMenuItem)
          liveBookmarksMenuItem.doCommand();
      } 
    }
  },

  _initSubscriptionUI: function FW__initSubscriptionUI() {
    var handlersMenuPopup =
      this._document.getElementById("handlersMenuPopup");
    if (!handlersMenuPopup)
      return;

    // Last-selected application
    var selectedApp;
    menuItem = this._document.createElementNS(XUL_NS, "menuitem");
    menuItem.id = "selectedAppMenuItem";
    menuItem.className = "menuitem-iconic";
    try {
      var prefs = Cc["@mozilla.org/preferences-service;1"].
                  getService(Ci.nsIPrefBranch);
      selectedApp = prefs.getComplexValue(PREF_SELECTED_APP,
                                          Ci.nsILocalFile);
      if (selectedApp.exists()) {
        menuItem.setAttribute("label",
                              this._getFileDisplayName(selectedApp));
        menuItem.setAttribute("src",
                              this._getFileIconURL(selectedApp));
        menuItem.setAttribute("handlerType", "client");
        menuItem.wrappedJSObject.file = selectedApp;
      }
      else {
        // Hide the menuitem if the last selected application doesn't exist
        menuItem.setAttribute("hidden", "true");
      }
    }
    catch(ex) {
      // Hide the menuitem until an application is selected
      menuItem.setAttribute("hidden", "true");
    }
    handlersMenuPopup.appendChild(menuItem);

#ifdef ENABLE_SYSTEM_FEED_READER_CODE
    // On Windows, also list the default feed reader
    var defaultReader;
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

      defaultReader = Cc["@mozilla.org/file/local;1"].
                      createInstance(Ci.nsILocalFile);
      defaultReader.initWithPath(path);

      if (defaultReader.exists()) {
        menuItem = this._document.createElementNS(XUL_NS, "menuitem");
        menuItem.id = "defaultHandlerMenuItem";
        menuItem.className = "menuitem-iconic";
        menuItem.setAttribute("label",
                              this._getFileDisplayName(defaultReader));
        menuItem.setAttribute("src",
                              this._getFileIconURL(defaultReader));
        menuItem.setAttribute("handlerType", "client");
        menuItem.wrappedJSObject.file = defaultReader;

        // Hide the default reader item if it points to the same application
        // as the last-selected application
        if (defaultReader.path == selectedApp.path)
          menuItem.setAttribute("hidden", "true");
  
        handlersMenuPopup.appendChild(menuItem);
      }
    }
    catch (e) {
      LOG("No feed handler registered on system");
    }
#endif

    // "Choose Application..." menuitem
    menuItem = this._document.createElementNS(XUL_NS, "menuitem");
    menuItem.id = "chooseApplicationMenuItem";
    menuItem.setAttribute("label", this._getString("chooseApplicationMenuItem"));
    handlersMenuPopup.appendChild(menuItem);

    // separator
    handlersMenuPopup.appendChild(this._document.createElementNS(XUL_NS,
                                  "menuseparator"));

    // List of web handlers
    var wccr = 
      Cc["@mozilla.org/embeddor.implemented/web-content-handler-registrar;1"].
      getService(Ci.nsIWebContentConverterService);
    var handlers = wccr.getContentHandlers(TYPE_MAYBE_FEED, {});
    if (handlers.length != 0) {
      var ios = Cc["@mozilla.org/network/io-service;1"].
                getService(Ci.nsIIOService);
      for (var i = 0; i < handlers.length; ++i) {
        menuItem = this._document.createElementNS(XUL_NS, "menuitem");
        menuItem.className = "menuitem-iconic";
        menuItem.setAttribute("label", handlers[i].name);
        menuItem.setAttribute("handlerType", "web");
        menuItem.setAttribute("webhandlerurl", handlers[i].uri);

        var uri = ios.newURI(handlers[i].uri, null, null);
        menuItem.setAttribute("src", uri.prePath + "/favicon.ico");

        handlersMenuPopup.appendChild(menuItem);
      }
    }

    this._setSelectedHandler();

    // "Always use..." checkbox initial state
    this._setAlwaysUseCheckedState();
    this._setAlwaysUseLabel();

    // We update the "Always use.." checkbox label whenever the selected item
    // in the list is changed
    handlersMenuPopup.addEventListener("command", this, false);

    // Set up the "Subscribe Now" button
    this._document
        .getElementById("subscribeButton")
        .addEventListener("command", this, false);
    
    // first-run ui
    var showFirstRunUI = true;
    try {
      showFirstRunUI = prefs.getBoolPref(PREF_SHOW_FIRST_RUN_UI);
    }
    catch (ex) { }
    if (showFirstRunUI) {
      var feedHeader = this._document.getElementById("feedHeader");
      if (feedHeader)
        feedHeader.setAttribute("firstrun", "true");

      prefs.setBoolPref(PREF_SHOW_FIRST_RUN_UI, false);
    }
  },
  
  /**
   * Returns the original URI object of the feed and ensures that this
   * component is only ever invoked from the preview document.  
   * @param window 
   *        The window of the document invoking the BrowserFeedWriter
   */
  _getOriginalURI: function FW__getOriginalURI(window) {
    var chan = 
        window.QueryInterface(Ci.nsIInterfaceRequestor).
        getInterface(Ci.nsIWebNavigation).
        QueryInterface(Ci.nsIDocShell).currentDocumentChannel;
    const kPrefix = "jar:file:";
    
    if (chan.URI.spec.substring(0, kPrefix.length) == kPrefix)
      return chan.originalURI;
    else
      return null;
  },
  
  _window: null,
  _document: null,
  
  /**
   * See nsIFeedWriter
   */
  write: function FW_write(window) {
    var originalURI = this._getOriginalURI(window);
    if (!originalURI)
      return;
    try {
      this._window = window;
      this._document = window.document;
      
      LOG("Subscribe Preview: feed uri = " + this._window.location.href);
      
      // Set up the subscription UI
      this._initSubscriptionUI();
      var prefs =   
        Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefBranch2);
      prefs.addObserver(PREF_SELECTED_ACTION, this, false);
      prefs.addObserver(PREF_SELECTED_READER, this, false);
      prefs.addObserver(PREF_SELECTED_WEB, this, false);
      prefs.addObserver(PREF_SELECTED_APP, this, false);
      
      // Set up the feed content
      var container = this._getContainer();
      if (!container)
        return;
      
      this._setTitleText(container);
      
      this._setTitleImage(container);
      
      this._writeFeedContent(container);
    }
    finally {
      var feedService = 
          Cc["@mozilla.org/browser/feeds/result-service;1"].
          getService(Ci.nsIFeedResultService);
      feedService.removeFeedResult(originalURI);
    }
  },

  /**
   * See nsIFeedWriter
   */
  close: function FW_close() {
    this._document = null;
    this._window = null;
    var prefs =   
        Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefBranch2);
    prefs.removeObserver(PREF_SELECTED_ACTION, this);
    prefs.removeObserver(PREF_SELECTED_READER, this);
    prefs.removeObserver(PREF_SELECTED_WEB, this);
    prefs.removeObserver(PREF_SELECTED_APP, this);
  },

  /**
   * See nsIFeedWriter
   */    
  subscribe: function FW_subscribe() {
    // Subscribe to the feed using the selected handler and save prefs
    var prefs =   
        Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefBranch);
    var defaultHandler = "reader";
    var useAsDefault = this._document.getElementById("alwaysUse")
                                     .getAttribute("checked");

    var selectedHandler = this._document.getElementById("handlersMenuList")
                              .wrappedJSObject.selectedItem;
    if (selectedHandler.hasAttribute("webhandlerurl")) {
      var webURI = selectedHandler.getAttribute("webhandlerurl");
      prefs.setCharPref(PREF_SELECTED_READER, "web");
      prefs.setCharPref(PREF_SELECTED_WEB, webURI);

      var wccr = 
        Cc["@mozilla.org/embeddor.implemented/web-content-handler-registrar;1"].
        getService(Ci.nsIWebContentConverterService);
      var handler = wccr.getWebContentHandlerByURI(TYPE_MAYBE_FEED, webURI);
      if (handler) {
        if (useAsDefault)
          wccr.setAutoHandler(TYPE_MAYBE_FEED, handler);

        this._window.location.href =
          handler.getHandlerURI(this._window.location.href);
      }
    }
    else {
      switch (selectedHandler.id) {
        case "selectedAppMenuItem":
#ifdef ENABLE_SYSTEM_FEED_READER_CODE
        case "defaultHandlerMenuItem":
#endif
          prefs.setCharPref(PREF_SELECTED_READER, "client");
          prefs.setComplexValue(PREF_SELECTED_APP, Ci.nsILocalFile, 
                                selectedHandler.file);
          break;
        case "liveBookmarksMenuItem":
          defaultHandler = "bookmarks";
          prefs.setCharPref(PREF_SELECTED_READER, "bookmarks");
          break;
      }

      var request = this._window
                        .QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIWebNavigation)
                        .QueryInterface(Ci.nsIDocShell)
                        .currentDocumentChannel;
      var feedService = Cc["@mozilla.org/browser/feeds/result-service;1"].
                        getService(Ci.nsIFeedResultService);
      feedService.addToClientReader(request, this._window.location.href);
    }

    // If "Always use..." is checked, we should set PREF_SELECTED_ACTION
    // to either "reader" (If a web reader or if an application is selected), or
    // to "bookmarks" (if the live bookmarks option is selected).
    // Otherwise, we should set it to "ask"
    if (useAsDefault)
      prefs.setCharPref(PREF_SELECTED_ACTION, defaultHandler);
    else
      prefs.setCharPref(PREF_SELECTED_ACTION, "ask");
  },
  
  /**
   * See nsIObserver
   */
  observe: function FW_observe(subject, topic, data) {
    if (topic == "nsPref:changed") {
      switch (data) {
        case PREF_SELECTED_READER:
        case PREF_SELECTED_WEB:
        case PREF_SELECTED_APP:
          this._setSelectedHandler();
          break;
        case PREF_SELECTED_ACTION:
          this._setAlwaysUseCheckedState();
      }
    } 
  },
  
  /**
   * See nsIClassInfo
   */
  getInterfaces: function WCCR_getInterfaces(countRef) {
    var interfaces = 
        [Ci.nsIFeedWriter, Ci.nsIClassInfo, Ci.nsISupports];
    countRef.value = interfaces.length;
    return interfaces;
  },
  getHelperForLanguage: function WCCR_getHelperForLanguage(language) {
    return null;
  },
  contractID: FW_CONTRACTID,
  classDescription: FW_CLASSNAME,
  classID: FW_CLASSID,
  implementationLanguage: Ci.nsIProgrammingLanguage.JAVASCRIPT,
  flags: Ci.nsIClassInfo.DOM_OBJECT,

  QueryInterface: function FW_QueryInterface(iid) {
    if (iid.equals(Ci.nsIFeedWriter) ||
        iid.equals(Ci.nsIClassInfo) ||
        iid.equals(Ci.nsIDOMEventListener) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

var Module = {
  QueryInterface: function M_QueryInterface(iid) {
    if (iid.equals(Ci.nsIModule) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },
  
  getClassObject: function M_getClassObject(cm, cid, iid) {
    if (!iid.equals(Ci.nsIFactory))
      throw Cr.NS_ERROR_NOT_IMPLEMENTED;
    
    if (cid.equals(FW_CLASSID))
      return new GenericComponentFactory(FeedWriter);
      
    throw Cr.NS_ERROR_NO_INTERFACE;
  },
  
  registerSelf: function M_registerSelf(cm, file, location, type) {
    var cr = cm.QueryInterface(Ci.nsIComponentRegistrar);
    
    cr.registerFactoryLocation(FW_CLASSID, FW_CLASSNAME, FW_CONTRACTID,
                               file, location, type);
    
    var catman = 
        Cc["@mozilla.org/categorymanager;1"].
        getService(Ci.nsICategoryManager);
    catman.addCategoryEntry("JavaScript global constructor",
                            "BrowserFeedWriter", FW_CONTRACTID, true, true);
  },
  
  unregisterSelf: function M_unregisterSelf(cm, location, type) {
    var cr = cm.QueryInterface(Ci.nsIComponentRegistrar);
    cr.unregisterFactoryLocation(FW_CLASSID, location);
  },
  
  canUnload: function M_canUnload(cm) {
    return true;
  }
};

function NSGetModule(cm, file) {
  return Module;
}

#include ../../../../toolkit/content/debug.js
#include GenericFactory.js
