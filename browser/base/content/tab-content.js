/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This content script contains code that requires a tab browser. */

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/ExtensionContent.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "E10SUtils",
  "resource:///modules/E10SUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "BrowserUtils",
  "resource://gre/modules/BrowserUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AboutReader",
  "resource://gre/modules/AboutReader.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ReaderMode",
  "resource://gre/modules/ReaderMode.jsm");
XPCOMUtils.defineLazyGetter(this, "SimpleServiceDiscovery", function() {
  let ssdp = Cu.import("resource://gre/modules/SimpleServiceDiscovery.jsm", {}).SimpleServiceDiscovery;
  // Register targets
  ssdp.registerDevice({
    id: "roku:ecp",
    target: "roku:ecp",
    factory: function(aService) {
      Cu.import("resource://gre/modules/RokuApp.jsm");
      return new RokuApp(aService);
    },
    types: ["video/mp4"],
    extensions: ["mp4"]
  });
  return ssdp;
});

// TabChildGlobal
var global = this;


addMessageListener("Browser:HideSessionRestoreButton", function (message) {
  // Hide session restore button on about:home
  let doc = content.document;
  let container;
  if (doc.documentURI.toLowerCase() == "about:home" &&
      (container = doc.getElementById("sessionRestoreContainer"))) {
    container.hidden = true;
  }
});


addMessageListener("Browser:Reload", function(message) {
  /* First, we'll try to use the session history object to reload so
   * that framesets are handled properly. If we're in a special
   * window (such as view-source) that has no session history, fall
   * back on using the web navigation's reload method.
   */

  let webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
  try {
    let sh = webNav.sessionHistory;
    if (sh)
      webNav = sh.QueryInterface(Ci.nsIWebNavigation);
  } catch (e) {
  }

  let reloadFlags = message.data.flags;
  try {
    E10SUtils.wrapHandlingUserInput(content, message.data.handlingUserInput,
                                    () => webNav.reload(reloadFlags));
  } catch (e) {
  }
});

addMessageListener("MixedContent:ReenableProtection", function() {
  docShell.mixedContentChannel = null;
});

addMessageListener("SecondScreen:tab-mirror", function(message) {
  if (!Services.prefs.getBoolPref("browser.casting.enabled")) {
    return;
  }
  let app = SimpleServiceDiscovery.findAppForService(message.data.service);
  if (app) {
    let width = content.innerWidth;
    let height = content.innerHeight;
    let viewport = {cssWidth: width, cssHeight: height, width: width, height: height};
    app.mirror(function() {}, content, viewport, function() {}, content);
  }
});

var AboutHomeListener = {
  init: function(chromeGlobal) {
    chromeGlobal.addEventListener('AboutHomeLoad', this, false, true);
  },

  get isAboutHome() {
    return content.document.documentURI.toLowerCase() == "about:home";
  },

  handleEvent: function(aEvent) {
    if (!this.isAboutHome) {
      return;
    }
    switch (aEvent.type) {
      case "AboutHomeLoad":
        this.onPageLoad();
        break;
      case "click":
        this.onClick(aEvent);
        break;
      case "pagehide":
        this.onPageHide(aEvent);
        break;
    }
  },

  receiveMessage: function(aMessage) {
    if (!this.isAboutHome) {
      return;
    }
    switch (aMessage.name) {
      case "AboutHome:Update":
        this.onUpdate(aMessage.data);
        break;
    }
  },

  onUpdate: function(aData) {
    let doc = content.document;
    if (aData.showRestoreLastSession && !PrivateBrowsingUtils.isContentWindowPrivate(content))
      doc.getElementById("launcher").setAttribute("session", "true");

    // Inject search engine and snippets URL.
    let docElt = doc.documentElement;
    // Set snippetsVersion last, which triggers to show the snippets when it's set.
    docElt.setAttribute("snippetsURL", aData.snippetsURL);
    if (aData.showKnowYourRights)
      docElt.setAttribute("showKnowYourRights", "true");
    docElt.setAttribute("snippetsVersion", aData.snippetsVersion);
  },

  onPageLoad: function() {
    addMessageListener("AboutHome:Update", this);
    addEventListener("click", this, true);
    addEventListener("pagehide", this, true);

    sendAsyncMessage("AboutHome:RequestUpdate");
  },

  onClick: function(aEvent) {
    if (!aEvent.isTrusted || // Don't trust synthetic events
        aEvent.button == 2 || aEvent.target.localName != "button") {
      return;
    }

    let originalTarget = aEvent.originalTarget;
    let ownerDoc = originalTarget.ownerDocument;
    if (ownerDoc.documentURI != "about:home") {
      // This shouldn't happen, but we're being defensive.
      return;
    }

    let elmId = originalTarget.getAttribute("id");

    switch (elmId) {
      case "restorePreviousSession":
        sendAsyncMessage("AboutHome:RestorePreviousSession");
        ownerDoc.getElementById("launcher").removeAttribute("session");
        break;

      case "downloads":
        sendAsyncMessage("AboutHome:Downloads");
        break;

      case "bookmarks":
        sendAsyncMessage("AboutHome:Bookmarks");
        break;

      case "history":
        sendAsyncMessage("AboutHome:History");
        break;

      case "addons":
        sendAsyncMessage("AboutHome:Addons");
        break;

      case "sync":
        sendAsyncMessage("AboutHome:Sync");
        break;

      case "settings":
        sendAsyncMessage("AboutHome:Settings");
        break;
    }
  },

  onPageHide: function(aEvent) {
    if (aEvent.target.defaultView.frameElement) {
      return;
    }
    removeMessageListener("AboutHome:Update", this);
    removeEventListener("click", this, true);
    removeEventListener("pagehide", this, true);
  },
};
AboutHomeListener.init(this);

var AboutPrivateBrowsingListener = {
  init(chromeGlobal) {
    chromeGlobal.addEventListener("AboutPrivateBrowsingOpenWindow", this,
                                  false, true);
    chromeGlobal.addEventListener("AboutPrivateBrowsingToggleTrackingProtection", this,
                                  false, true);
    chromeGlobal.addEventListener("AboutPrivateBrowsingDontShowIntroPanelAgain", this,
                                  false, true);
  },

  get isAboutPrivateBrowsing() {
    return content.document.documentURI.toLowerCase() == "about:privatebrowsing";
  },

  handleEvent(aEvent) {
    if (!this.isAboutPrivateBrowsing) {
      return;
    }
    switch (aEvent.type) {
      case "AboutPrivateBrowsingOpenWindow":
        sendAsyncMessage("AboutPrivateBrowsing:OpenPrivateWindow");
        break;
      case "AboutPrivateBrowsingToggleTrackingProtection":
        sendAsyncMessage("AboutPrivateBrowsing:ToggleTrackingProtection");
        break;
      case "AboutPrivateBrowsingDontShowIntroPanelAgain":
        sendAsyncMessage("AboutPrivateBrowsing:DontShowIntroPanelAgain");
        break;
    }
  },
};
AboutPrivateBrowsingListener.init(this);

var AboutReaderListener = {

  _articlePromise: null,

  init: function() {
    addEventListener("AboutReaderContentLoaded", this, false, true);
    addEventListener("DOMContentLoaded", this, false);
    addEventListener("pageshow", this, false);
    addEventListener("pagehide", this, false);
    addMessageListener("Reader:ParseDocument", this);
    addMessageListener("Reader:PushState", this);
  },

  receiveMessage: function(message) {
    switch (message.name) {
      case "Reader:ParseDocument":
        this._articlePromise = ReaderMode.parseDocument(content.document).catch(Cu.reportError);
        content.document.location = "about:reader?url=" + encodeURIComponent(message.data.url);
        break;

      case "Reader:PushState":
        this.updateReaderButton(!!(message.data && message.data.isArticle));
        break;
    }
  },

  get isAboutReader() {
    if (!content) {
      return false;
    }
    return content.document.documentURI.startsWith("about:reader");
  },

  handleEvent: function(aEvent) {
    if (aEvent.originalTarget.defaultView != content) {
      return;
    }

    switch (aEvent.type) {
      case "AboutReaderContentLoaded":
        if (!this.isAboutReader) {
          return;
        }

        if (content.document.body) {
          // Update the toolbar icon to show the "reader active" icon.
          sendAsyncMessage("Reader:UpdateReaderButton");
          new AboutReader(global, content, this._articlePromise);
          this._articlePromise = null;
        }
        break;

      case "pagehide":
        this.cancelPotentialPendingReadabilityCheck();
        sendAsyncMessage("Reader:UpdateReaderButton", { isArticle: false });
        break;

      case "pageshow":
        // If a page is loaded from the bfcache, we won't get a "DOMContentLoaded"
        // event, so we need to rely on "pageshow" in this case.
        if (aEvent.persisted) {
          this.updateReaderButton();
        }
        break;
      case "DOMContentLoaded":
        this.updateReaderButton();
        break;

    }
  },

  /**
   * NB: this function will update the state of the reader button asynchronously
   * after the next mozAfterPaint call (assuming reader mode is enabled and
   * this is a suitable document). Calling it on things which won't be
   * painted is not going to work.
   */
  updateReaderButton: function(forceNonArticle) {
    if (!ReaderMode.isEnabledForParseOnLoad || this.isAboutReader ||
        !content || !(content.document instanceof content.HTMLDocument) ||
        content.document.mozSyntheticDocument) {
      return;
    }

    this.scheduleReadabilityCheckPostPaint(forceNonArticle);
  },

  cancelPotentialPendingReadabilityCheck: function() {
    if (this._pendingReadabilityCheck) {
      removeEventListener("MozAfterPaint", this._pendingReadabilityCheck);
      delete this._pendingReadabilityCheck;
    }
  },

  scheduleReadabilityCheckPostPaint: function(forceNonArticle) {
    if (this._pendingReadabilityCheck) {
      // We need to stop this check before we re-add one because we don't know
      // if forceNonArticle was true or false last time.
      this.cancelPotentialPendingReadabilityCheck();
    }
    this._pendingReadabilityCheck = this.onPaintWhenWaitedFor.bind(this, forceNonArticle);
    addEventListener("MozAfterPaint", this._pendingReadabilityCheck);
  },

  onPaintWhenWaitedFor: function(forceNonArticle) {
    this.cancelPotentialPendingReadabilityCheck();
    // Only send updates when there are articles; there's no point updating with
    // |false| all the time.
    if (ReaderMode.isProbablyReaderable(content.document)) {
      sendAsyncMessage("Reader:UpdateReaderButton", { isArticle: true });
    } else if (forceNonArticle) {
      sendAsyncMessage("Reader:UpdateReaderButton", { isArticle: false });
    }
  },
};
AboutReaderListener.init();


var ContentSearchMediator = {

  whitelist: new Set([
    "about:home",
    "about:newtab",
  ]),

  init: function (chromeGlobal) {
    chromeGlobal.addEventListener("ContentSearchClient", this, true, true);
    addMessageListener("ContentSearch", this);
  },

  handleEvent: function (event) {
    if (this._contentWhitelisted) {
      this._sendMsg(event.detail.type, event.detail.data);
    }
  },

  receiveMessage: function (msg) {
    if (msg.data.type == "AddToWhitelist") {
      for (let uri of msg.data.data) {
        this.whitelist.add(uri);
      }
      this._sendMsg("AddToWhitelistAck");
      return;
    }
    if (this._contentWhitelisted) {
      this._fireEvent(msg.data.type, msg.data.data);
    }
  },

  get _contentWhitelisted() {
    return this.whitelist.has(content.document.documentURI);
  },

  _sendMsg: function (type, data=null) {
    sendAsyncMessage("ContentSearch", {
      type: type,
      data: data,
    });
  },

  _fireEvent: function (type, data=null) {
    let event = Cu.cloneInto({
      detail: {
        type: type,
        data: data,
      },
    }, content);
    content.dispatchEvent(new content.CustomEvent("ContentSearchService",
                                                  event));
  },
};
ContentSearchMediator.init(this);

var PageStyleHandler = {
  init: function() {
    addMessageListener("PageStyle:Switch", this);
    addMessageListener("PageStyle:Disable", this);
    addEventListener("pageshow", () => this.sendStyleSheetInfo());
  },

  get markupDocumentViewer() {
    return docShell.contentViewer;
  },

  sendStyleSheetInfo: function() {
    let filteredStyleSheets = this._filterStyleSheets(this.getAllStyleSheets());

    sendAsyncMessage("PageStyle:StyleSheets", {
      filteredStyleSheets: filteredStyleSheets,
      authorStyleDisabled: this.markupDocumentViewer.authorStyleDisabled,
      preferredStyleSheetSet: content.document.preferredStyleSheetSet
    });
  },

  getAllStyleSheets: function(frameset = content) {
    let selfSheets = Array.slice(frameset.document.styleSheets);
    let subSheets = Array.map(frameset.frames, frame => this.getAllStyleSheets(frame));
    return selfSheets.concat(...subSheets);
  },

  receiveMessage: function(msg) {
    switch (msg.name) {
      case "PageStyle:Switch":
        this.markupDocumentViewer.authorStyleDisabled = false;
        this._stylesheetSwitchAll(content, msg.data.title);
        break;

      case "PageStyle:Disable":
        this.markupDocumentViewer.authorStyleDisabled = true;
        break;
    }

    this.sendStyleSheetInfo();
  },

  _stylesheetSwitchAll: function (frameset, title) {
    if (!title || this._stylesheetInFrame(frameset, title)) {
      this._stylesheetSwitchFrame(frameset, title);
    }

    for (let i = 0; i < frameset.frames.length; i++) {
      // Recurse into sub-frames.
      this._stylesheetSwitchAll(frameset.frames[i], title);
    }
  },

  _stylesheetSwitchFrame: function (frame, title) {
    var docStyleSheets = frame.document.styleSheets;

    for (let i = 0; i < docStyleSheets.length; ++i) {
      let docStyleSheet = docStyleSheets[i];
      if (docStyleSheet.title) {
        docStyleSheet.disabled = (docStyleSheet.title != title);
      } else if (docStyleSheet.disabled) {
        docStyleSheet.disabled = false;
      }
    }
  },

  _stylesheetInFrame: function (frame, title) {
    return Array.some(frame.document.styleSheets, (styleSheet) => styleSheet.title == title);
  },

  _filterStyleSheets: function(styleSheets) {
    let result = [];

    for (let currentStyleSheet of styleSheets) {
      if (!currentStyleSheet.title)
        continue;

      // Skip any stylesheets that don't match the screen media type.
      if (currentStyleSheet.media.length > 0) {
        let mediaQueryList = currentStyleSheet.media.mediaText;
        if (!content.matchMedia(mediaQueryList).matches) {
          continue;
        }
      }

      let URI;
      try {
        URI = Services.io.newURI(currentStyleSheet.href, null, null);
      } catch(e) {
        if (e.result != Cr.NS_ERROR_MALFORMED_URI) {
          throw e;
        }
      }

      if (URI) {
        // We won't send data URIs all of the way up to the parent, as these
        // can be arbitrarily large.
        let sentURI = URI.scheme == "data" ? null : URI.spec;

        result.push({
          title: currentStyleSheet.title,
          disabled: currentStyleSheet.disabled,
          href: sentURI,
        });
      }
    }

    return result;
  },
};
PageStyleHandler.init();

// Keep a reference to the translation content handler to avoid it it being GC'ed.
var trHandler = null;
if (Services.prefs.getBoolPref("browser.translation.detectLanguage")) {
  Cu.import("resource:///modules/translation/TranslationContentHandler.jsm");
  trHandler = new TranslationContentHandler(global, docShell);
}

function gKeywordURIFixup(fixupInfo) {
  fixupInfo.QueryInterface(Ci.nsIURIFixupInfo);
  if (!fixupInfo.consumer) {
    return;
  }

  // Ignore info from other docshells
  let parent = fixupInfo.consumer.QueryInterface(Ci.nsIDocShellTreeItem).sameTypeRootTreeItem;
  if (parent != docShell)
    return;

  let data = {};
  for (let f of Object.keys(fixupInfo)) {
    if (f == "consumer" || typeof fixupInfo[f] == "function")
      continue;

    if (fixupInfo[f] && fixupInfo[f] instanceof Ci.nsIURI) {
      data[f] = fixupInfo[f].spec;
    } else {
      data[f] = fixupInfo[f];
    }
  }

  sendAsyncMessage("Browser:URIFixup", data);
}
Services.obs.addObserver(gKeywordURIFixup, "keyword-uri-fixup", false);
addEventListener("unload", () => {
  Services.obs.removeObserver(gKeywordURIFixup, "keyword-uri-fixup");
}, false);

addMessageListener("Browser:AppTab", function(message) {
  if (docShell) {
    docShell.isAppTab = message.data.isAppTab;
  }
});

var WebBrowserChrome = {
  onBeforeLinkTraversal: function(originalTarget, linkURI, linkNode, isAppTab) {
    return BrowserUtils.onBeforeLinkTraversal(originalTarget, linkURI, linkNode, isAppTab);
  },

  // Check whether this URI should load in the current process
  shouldLoadURI: function(aDocShell, aURI, aReferrer) {
    if (!E10SUtils.shouldLoadURI(aDocShell, aURI, aReferrer)) {
      E10SUtils.redirectLoad(aDocShell, aURI, aReferrer);
      return false;
    }

    return true;
  },
};

if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT) {
  let tabchild = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsITabChild);
  tabchild.webBrowserChrome = WebBrowserChrome;
}


var DOMFullscreenHandler = {
  _fullscreenDoc: null,

  init: function() {
    addMessageListener("DOMFullscreen:Entered", this);
    addMessageListener("DOMFullscreen:CleanUp", this);
    addEventListener("MozDOMFullscreen:Request", this);
    addEventListener("MozDOMFullscreen:Entered", this);
    addEventListener("MozDOMFullscreen:NewOrigin", this);
    addEventListener("MozDOMFullscreen:Exit", this);
    addEventListener("MozDOMFullscreen:Exited", this);
  },

  get _windowUtils() {
    if (!content) {
      return null;
    }
    return content.QueryInterface(Ci.nsIInterfaceRequestor)
                  .getInterface(Ci.nsIDOMWindowUtils);
  },

  receiveMessage: function(aMessage) {
    switch(aMessage.name) {
      case "DOMFullscreen:Entered": {
        if (!this._windowUtils.handleFullscreenRequests() &&
            !content.document.fullscreenElement) {
          // If we don't actually have any pending fullscreen request
          // to handle, neither we have been in fullscreen, tell the
          // parent to just exit.
          sendAsyncMessage("DOMFullscreen:Exit");
        }
        break;
      }
      case "DOMFullscreen:CleanUp": {
        if (this._windowUtils) {
          this._windowUtils.exitFullscreen();
        }
        this._fullscreenDoc = null;
        break;
      }
    }
  },

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "MozDOMFullscreen:Request": {
        sendAsyncMessage("DOMFullscreen:Request");
        break;
      }
      case "MozDOMFullscreen:NewOrigin": {
        this._fullscreenDoc = aEvent.target;
        sendAsyncMessage("DOMFullscreen:NewOrigin", {
          originNoSuffix: this._fullscreenDoc.nodePrincipal.originNoSuffix,
        });
        break;
      }
      case "MozDOMFullscreen:Exit": {
        sendAsyncMessage("DOMFullscreen:Exit");
        break;
      }
      case "MozDOMFullscreen:Entered":
      case "MozDOMFullscreen:Exited": {
        addEventListener("MozAfterPaint", this);
        if (!content || !content.document.fullscreenElement) {
          // If we receive any fullscreen change event, and find we are
          // actually not in fullscreen, also ask the parent to exit to
          // ensure that the parent always exits fullscreen when we do.
          sendAsyncMessage("DOMFullscreen:Exit");
        }
        break;
      }
      case "MozAfterPaint": {
        removeEventListener("MozAfterPaint", this);
        sendAsyncMessage("DOMFullscreen:Painted");
        break;
      }
    }
  }
};
DOMFullscreenHandler.init();

var RefreshBlocker = {
  PREF: "accessibility.blockautorefresh",

  // Bug 1247100 - When a refresh is caused by an HTTP header,
  // onRefreshAttempted will be fired before onLocationChange.
  // When a refresh is caused by a <meta> tag in the document,
  // onRefreshAttempted will be fired after onLocationChange.
  //
  // We only ever want to send a message to the parent after
  // onLocationChange has fired, since the parent uses the
  // onLocationChange update to clear transient notifications.
  // Sending the message before onLocationChange will result in
  // us creating the notification, and then clearing it very
  // soon after.
  //
  // To account for both cases (onRefreshAttempted before
  // onLocationChange, and onRefreshAttempted after onLocationChange),
  // we'll hold a mapping of DOM Windows that we see get
  // sent through both onLocationChange and onRefreshAttempted.
  // When either run, they'll check the WeakMap for the existence
  // of the DOM Window. If it doesn't exist, it'll add it. If
  // it finds it, it'll know that it's safe to send the message
  // to the parent, since we know that both have fired.
  //
  // The DOM Window is removed from blockedWindows when we notice
  // the nsIWebProgress change state to STATE_STOP for the
  // STATE_IS_WINDOW case.
  //
  // DOM Windows are mapped to a JS object that contains the data
  // to be sent to the parent to show the notification. Since that
  // data is only known when onRefreshAttempted is fired, it's only
  // ever stashed in the map if onRefreshAttempted fires first -
  // otherwise, null is set as the value of the mapping.
  blockedWindows: new WeakMap(),

  init() {
    if (Services.prefs.getBoolPref(this.PREF)) {
      this.enable();
    }

    Services.prefs.addObserver(this.PREF, this, false);
  },

  uninit() {
    if (Services.prefs.getBoolPref(this.PREF)) {
      this.disable();
    }

    Services.prefs.removeObserver(this.PREF, this);
  },

  observe(subject, topic, data) {
    if (topic == "nsPref:changed" && data == this.PREF) {
      if (Services.prefs.getBoolPref(this.PREF)) {
        this.enable();
      } else {
        this.disable();
      }
    }
  },

  enable() {
    this._filter = Cc["@mozilla.org/appshell/component/browser-status-filter;1"]
                     .createInstance(Ci.nsIWebProgress);
    this._filter.addProgressListener(this, Ci.nsIWebProgress.NOTIFY_ALL);

    let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(this._filter, Ci.nsIWebProgress.NOTIFY_ALL);

    addMessageListener("RefreshBlocker:Refresh", this);
  },

  disable() {
    let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebProgress);
    webProgress.removeProgressListener(this._filter);

    this._filter.removeProgressListener(this);
    this._filter = null;

    removeMessageListener("RefreshBlocker:Refresh", this);
  },

  send(data) {
    sendAsyncMessage("RefreshBlocker:Blocked", data);
  },

  /**
   * Notices when the nsIWebProgress transitions to STATE_STOP for
   * the STATE_IS_WINDOW case, which will clear any mappings from
   * blockedWindows.
   */
  onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    if (aStateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW &&
        aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
      this.blockedWindows.delete(aWebProgress.DOMWindow);
    }
  },

  /**
   * Notices when the location has changed. If, when running,
   * onRefreshAttempted has already fired for this DOM Window, will
   * send the appropriate refresh blocked data to the parent.
   */
  onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {
    let win = aWebProgress.DOMWindow;
    if (this.blockedWindows.has(win)) {
      let data = this.blockedWindows.get(win);
      if (data) {
        // We saw onRefreshAttempted before onLocationChange, so
        // send the message to the parent to show the notification.
        this.send(data);
      }
    } else {
      this.blockedWindows.set(win, null);
    }
  },

  /**
   * Notices when a refresh / reload was attempted. If, when running,
   * onLocationChange has not yet run, will stash the appropriate data
   * into the blockedWindows map to be sent when onLocationChange fires.
   */
  onRefreshAttempted(aWebProgress, aURI, aDelay, aSameURI) {
    let win = aWebProgress.DOMWindow;
    let outerWindowID = win.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils)
                           .outerWindowID;

    let data = {
      URI: aURI.spec,
      originCharset: aURI.originCharset,
      delay: aDelay,
      sameURI: aSameURI,
      outerWindowID,
    };

    if (this.blockedWindows.has(win)) {
      // onLocationChange must have fired before, so we can tell the
      // parent to show the notification.
      this.send(data);
    } else {
      // onLocationChange hasn't fired yet, so stash the data in the
      // map so that onLocationChange can send it when it fires.
      this.blockedWindows.set(win, data);
    }

    return false;
  },

  receiveMessage(message) {
    let data = message.data;

    if (message.name == "RefreshBlocker:Refresh") {
      let win = Services.wm.getOuterWindowWithId(data.outerWindowID);
      let refreshURI = win.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIDocShell)
                          .QueryInterface(Ci.nsIRefreshURI);

      let URI = BrowserUtils.makeURI(data.URI, data.originCharset, null);

      refreshURI.forceRefreshURI(URI, data.delay, true);
    }
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener2,
                                         Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsISupports]),
};

RefreshBlocker.init();

ExtensionContent.init(this);
addEventListener("unload", () => {
  ExtensionContent.uninit(this);
  RefreshBlocker.uninit();
});
