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
  dump("*** " + str + "\n");
}

const HTML_NS = "http://www.w3.org/1999/xhtml";
const TYPE_MAYBE_FEED = "application/vnd.mozilla.maybe.feed";
const URI_BUNDLE = "chrome://browser/locale/feeds/subscribe.properties";

const PREF_SELECTED_APP = "browser.feeds.handlers.application";
const PREF_SELECTED_WEB = "browser.feeds.handlers.webservice";
const PREF_SELECTED_HANDLER = "browser.feeds.handler";
const PREF_SKIP_PREVIEW_PAGE = "browser.feeds.skip_preview_page";

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
    this._setContentText("feedTitleText", container.title);
    this._setContentText("feedSubtitleText", 
                         this._getPropertyAsString(container, "description"));
    this._document.title = container.title;
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
    // XXXben - do something with this. parameterize?
    const MAX_CHARS = 600;
    
    // Build the actual feed content
    var feedContent = this._document.getElementById("feedContent");
    var feed = container.QueryInterface(Ci.nsIFeed);
    
    for (var i = 0; i < feed.items.length; ++i) {
      var entry = feed.items.queryElementAt(i, Ci.nsIFeedEntry);
      entry.QueryInterface(Ci.nsIFeedContainer);
      
      var entryContainer = this._document.createElementNS(HTML_NS, "div");
      entryContainer.className = "entry";
      
      var a = this._document.createElementNS(HTML_NS, "a");
      a.appendChild(this._document.createTextNode(entry.title));
      
      // Entries are not required to have links, so entry.link can be null.
      if (entry.link)
        this._safeSetURIAttribute(a, "href", entry.link.spec);

      var title = this._document.createElementNS(HTML_NS, "h3");
      title.appendChild(a);
      entryContainer.appendChild(title);
      
      var body = this._document.createElementNS(HTML_NS, "p");
      var summary = entry.summary(true)
      if (summary && summary.length > MAX_CHARS)
        summary = summary.substring(0, MAX_CHARS) + "...";
      
      // XXXben - Change to use innerHTML
      body.appendChild(this._document.createTextNode(summary));
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
    var feedURI = ios.newURI(this._window.location.href, null, null);
    try {
      var result = feedService.getFeedResult(feedURI);
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
    var lfm = file.QueryInterface(Components.interfaces.nsILocalFileMac);
    try {
      if (lfm.isPackage())
        return lfm.bundleName;
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
  
  _initSelectedHandler: function FW__initSelectedHandler() {
    var prefs =   
        Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefBranch);
    var chosen = 
        this._document.getElementById("feedSubscribeLineHandlerChosen");
    var unchosen = 
        this._document.getElementById("feedSubscribeLineHandlerUnchosen");
    var ios = 
        Cc["@mozilla.org/network/io-service;1"].
        getService(Ci.nsIIOService);
    try {
      var iconURI = "chrome://browser/skin/places/livemarkItem.png";
      var handler = prefs.getCharPref(PREF_SELECTED_HANDLER);
      switch (handler) {
      case "client":
        var selectedApp = 
            prefs.getComplexValue(PREF_SELECTED_APP, Ci.nsILocalFile);      
        var displayName = this._getFileDisplayName(selectedApp);
        this._setContentText("feedSubscribeHandleText", displayName);
        
        var url = ios.newFileURI(selectedApp).QueryInterface(Ci.nsIURL);
        iconURI = "moz-icon://" + url.spec;
        break;
      case "web":
        var webURI = prefs.getCharPref(PREF_SELECTED_WEB);
        var wccr = 
            Cc["@mozilla.org/web-content-handler-registrar;1"].
            getService(Ci.nsIWebContentConverterService);
        var title ="Unknown";
        var handler = 
            wccr.getWebContentHandlerByURI(TYPE_MAYBE_FEED, webURI);
        if (handler)
          title = handler.name;
        var uri = ios.newURI(webURI, null, null);
        iconURI = uri.prePath + "/favicon.ico";
        
        this._setContentText("feedSubscribeHandleText", title);
        break;
      case "bookmarks":
        this._setContentText("feedSubscribeHandleText", 
                             this._getString("liveBookmarks"));
        break;
      }
      unchosen.setAttribute("hidden", "true");
      chosen.removeAttribute("hidden");
      var button = this._document.getElementById("feedSubscribeLink");
      button.focus();
      
      
      var displayArea = 
          this._document.getElementById("feedSubscribeHandleText");
      displayArea.style.setProperty("background-image", 
                                    "url(\"" + iconURI + "\")", "");
    }
    catch (e) {
      LOG("Failed to set Handler: " + e);
      // No selected handlers yet! Make the user choose...
      chosen.setAttribute("hidden", "true");
      unchosen.removeAttribute("hidden");
      this._document.getElementById("feedHeader").setAttribute("firstrun", "true");
      
      var button = this._document.getElementById("feedChooseInitialReader");
      button.focus();
    }
  },
  
  /**
   * Ensures that this component is only ever invoked from the preview 
   * document.
   * @param   window
   *          The window of the document invoking the BrowserFeedWriter
   */
  _isValidWindow: function FW__isValidWindow(window) {
    var chan = 
        window.QueryInterface(Ci.nsIInterfaceRequestor).
        getInterface(Ci.nsIWebNavigation).
        QueryInterface(Ci.nsIDocShell).currentDocumentChannel;
    const kPrefix = "jar:file:";
    return chan.URI.spec.substring(0, kPrefix.length) == kPrefix;
  },
  
  _window: null,
  _document: null,
  
  /**
   * See nsIFeedWriter
   */
  write: function FW_write(window) {
    if (!this._isValidWindow(window))
      return;
    
    this._window = window;
    this._document = window.document;
      
    LOG("Subscribe Preview: feed uri = " + this._window.location.href);

    // Set up the displayed handler
    this._initSelectedHandler();
    var prefs =   
        Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefBranch2);
    prefs.addObserver(PREF_SELECTED_HANDLER, this, false);
    prefs.addObserver(PREF_SELECTED_APP, this, false);
    
    // Set up the feed content
    var container = this._getContainer();
    if (!container)
      return;
    
    this._setTitleText(container);
    
    this._setTitleImage(container);
    
    this._writeFeedContent(container);
  },
  
  /**
   * See nsIFeedWriter
   */
  changeOptions: function FW_changeOptions() {
    var paramBlock = 
        Cc["@mozilla.org/embedcomp/dialogparam;1"].
        createInstance(Ci.nsIDialogParamBlock);
    // Used to tell the preview page that the user chose to subscribe with
    // a particular reader, and so it should subscribe now.
    const PARAM_USER_SUBSCRIBED = 0;
    paramBlock.SetInt(PARAM_USER_SUBSCRIBED, 0);
    this._window.openDialog("chrome://browser/content/feeds/options.xul", "", 
                            "modal,centerscreen", "subscribe", paramBlock);
    if (paramBlock.GetInt(PARAM_USER_SUBSCRIBED) == 1)
      this.subscribe();
  },
  
  /**
   * See nsIFeedWriter
   */
  close: function FW_close() {
    var prefs =   
        Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefBranch2);
    prefs.removeObserver(PREF_SELECTED_HANDLER, this);
    prefs.removeObserver(PREF_SELECTED_APP, this);
  },
    
  /**
   * See nsIFeedWriter
   */
  subscribe: function FW_subscribe() {
    var prefs =   
        Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefBranch);
    try {
      var handler = prefs.getCharPref(PREF_SELECTED_HANDLER);
    }
    catch (e) {
      // Something is bogus in our state. Prompt the user to fix it.
      this.changeOptions();
      return; 
    }
    if (handler == "web") {
      var webURI = prefs.getCharPref(PREF_SELECTED_WEB);
      var wccr = 
          Cc["@mozilla.org/web-content-handler-registrar;1"].
          getService(Ci.nsIWebContentConverterService);
      var handler = 
          wccr.getWebContentHandlerByURI(TYPE_MAYBE_FEED, webURI);
      this._window.location.href = 
        handler.getHandlerURI(this._window.location.href);
    }
    else {
      var feedService = 
          Cc["@mozilla.org/browser/feeds/result-service;1"].
          getService(Ci.nsIFeedResultService);
      feedService.addToClientReader(this._window.location.href);
    }
  },
  
  /**
   * See nsIObserver
   */
  observe: function FW_observe(subject, topic, data) {
    if (topic == "nsPref:changed")
      this._initSelectedHandler();
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
