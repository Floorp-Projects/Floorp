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
    factory(aService) {
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


addEventListener("MozDOMPointerLock:Entered", function(aEvent) {
  sendAsyncMessage("PointerLock:Entered", {
    originNoSuffix: aEvent.target.nodePrincipal.originNoSuffix
  });
});

addEventListener("MozDOMPointerLock:Exited", function(aEvent) {
  sendAsyncMessage("PointerLock:Exited");
});


addMessageListener("Browser:HideSessionRestoreButton", function(message) {
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
    let viewport = {cssWidth: width, cssHeight: height, width, height};
    app.mirror(function() {}, content, viewport, function() {}, content);
  }
});

var AboutHomeListener = {
  init(chromeGlobal) {
    chromeGlobal.addEventListener('AboutHomeLoad', this, false, true);
  },

  get isAboutHome() {
    return content.document.documentURI.toLowerCase() == "about:home";
  },

  handleEvent(aEvent) {
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

  receiveMessage(aMessage) {
    if (!this.isAboutHome) {
      return;
    }
    switch (aMessage.name) {
      case "AboutHome:Update":
        this.onUpdate(aMessage.data);
        break;
    }
  },

  onUpdate(aData) {
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

  onPageLoad() {
    addMessageListener("AboutHome:Update", this);
    addEventListener("click", this, true);
    addEventListener("pagehide", this, true);

    sendAsyncMessage("AboutHome:MaybeShowAutoMigrationUndoNotification");
    sendAsyncMessage("AboutHome:RequestUpdate");
  },

  onClick(aEvent) {
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

  onPageHide(aEvent) {
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

  _isLeavingReaderMode: false,

  init() {
    addEventListener("AboutReaderContentLoaded", this, false, true);
    addEventListener("DOMContentLoaded", this, false);
    addEventListener("pageshow", this, false);
    addEventListener("pagehide", this, false);
    addMessageListener("Reader:ToggleReaderMode", this);
    addMessageListener("Reader:PushState", this);
  },

  receiveMessage(message) {
    switch (message.name) {
      case "Reader:ToggleReaderMode":
        if (!this.isAboutReader) {
          this._articlePromise = ReaderMode.parseDocument(content.document).catch(Cu.reportError);
          ReaderMode.enterReaderMode(docShell, content);
        } else {
          this._isLeavingReaderMode = true;
          ReaderMode.leaveReaderMode(docShell, content);
        }
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

  handleEvent(aEvent) {
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
        // this._isLeavingReaderMode is used here to keep the Reader Mode icon
        // visible in the location bar when transitioning from reader-mode page
        // back to the source page.
        sendAsyncMessage("Reader:UpdateReaderButton", { isArticle: this._isLeavingReaderMode });
        if (this._isLeavingReaderMode) {
          this._isLeavingReaderMode = false;
        }
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
  updateReaderButton(forceNonArticle) {
    if (!ReaderMode.isEnabledForParseOnLoad || this.isAboutReader ||
        !content || !(content.document instanceof content.HTMLDocument) ||
        content.document.mozSyntheticDocument) {
      return;
    }

    this.scheduleReadabilityCheckPostPaint(forceNonArticle);
  },

  cancelPotentialPendingReadabilityCheck() {
    if (this._pendingReadabilityCheck) {
      removeEventListener("MozAfterPaint", this._pendingReadabilityCheck);
      delete this._pendingReadabilityCheck;
    }
  },

  scheduleReadabilityCheckPostPaint(forceNonArticle) {
    if (this._pendingReadabilityCheck) {
      // We need to stop this check before we re-add one because we don't know
      // if forceNonArticle was true or false last time.
      this.cancelPotentialPendingReadabilityCheck();
    }
    this._pendingReadabilityCheck = this.onPaintWhenWaitedFor.bind(this, forceNonArticle);
    addEventListener("MozAfterPaint", this._pendingReadabilityCheck);
  },

  onPaintWhenWaitedFor(forceNonArticle, event) {
    // In non-e10s, we'll get called for paints other than ours, and so it's
    // possible that this page hasn't been laid out yet, in which case we
    // should wait until we get an event that does relate to our layout. We
    // determine whether any of our content got painted by checking if there
    // are any painted rects.
    if (!event.clientRects.length) {
      return;
    }

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

  init(chromeGlobal) {
    chromeGlobal.addEventListener("ContentSearchClient", this, true, true);
    addMessageListener("ContentSearch", this);
  },

  handleEvent(event) {
    if (this._contentWhitelisted) {
      this._sendMsg(event.detail.type, event.detail.data);
    }
  },

  receiveMessage(msg) {
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

  _sendMsg(type, data = null) {
    sendAsyncMessage("ContentSearch", {
      type,
      data,
    });
  },

  _fireEvent(type, data = null) {
    let event = Cu.cloneInto({
      detail: {
        type,
        data,
      },
    }, content);
    content.dispatchEvent(new content.CustomEvent("ContentSearchService",
                                                  event));
  },
};
ContentSearchMediator.init(this);

var PageStyleHandler = {
  init() {
    addMessageListener("PageStyle:Switch", this);
    addMessageListener("PageStyle:Disable", this);
    addEventListener("pageshow", () => this.sendStyleSheetInfo());
  },

  get markupDocumentViewer() {
    return docShell.contentViewer;
  },

  sendStyleSheetInfo() {
    let filteredStyleSheets = this._filterStyleSheets(this.getAllStyleSheets());

    sendAsyncMessage("PageStyle:StyleSheets", {
      filteredStyleSheets,
      authorStyleDisabled: this.markupDocumentViewer.authorStyleDisabled,
      preferredStyleSheetSet: content.document.preferredStyleSheetSet
    });
  },

  getAllStyleSheets(frameset = content) {
    let selfSheets = Array.slice(frameset.document.styleSheets);
    let subSheets = Array.map(frameset.frames, frame => this.getAllStyleSheets(frame));
    return selfSheets.concat(...subSheets);
  },

  receiveMessage(msg) {
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

  _stylesheetSwitchAll(frameset, title) {
    if (!title || this._stylesheetInFrame(frameset, title)) {
      this._stylesheetSwitchFrame(frameset, title);
    }

    for (let i = 0; i < frameset.frames.length; i++) {
      // Recurse into sub-frames.
      this._stylesheetSwitchAll(frameset.frames[i], title);
    }
  },

  _stylesheetSwitchFrame(frame, title) {
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

  _stylesheetInFrame(frame, title) {
    return Array.some(frame.document.styleSheets, (styleSheet) => styleSheet.title == title);
  },

  _filterStyleSheets(styleSheets) {
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
        if (!currentStyleSheet.ownerNode ||
            // special-case style nodes, which have no href
            currentStyleSheet.ownerNode.nodeName.toLowerCase() != "style") {
          URI = Services.io.newURI(currentStyleSheet.href);
        }
      } catch (e) {
        if (e.result != Cr.NS_ERROR_MALFORMED_URI) {
          throw e;
        }
        continue;
      }

      // We won't send data URIs all of the way up to the parent, as these
      // can be arbitrarily large.
      let sentURI = (!URI || URI.scheme == "data") ? null : URI.spec;

      result.push({
        title: currentStyleSheet.title,
        disabled: currentStyleSheet.disabled,
        href: sentURI,
      });
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

let PrerenderContentHandler = {
  init() {
    this._pending = [];
    this._idMonotonic = 0;
    this._initialized = true;
    addMessageListener("Prerender:Canceled", this);
    addMessageListener("Prerender:Swapped", this);
  },

  get initialized() {
    return !!this._initialized;
  },

  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "Prerender:Canceled": {
        for (let i = 0; i < this._pending.length; ++i) {
          if (this._pending[i].id === aMessage.data.id) {
            if (this._pending[i].failure) {
              this._pending[i].failure.run();
            }
            // Remove the item from the array
            this._pending.splice(i, 1);
            break;
          }
        }
        break;
      }
      case "Prerender:Swapped": {
        for (let i = 0; i < this._pending.length; ++i) {
          if (this._pending[i].id === aMessage.data.id) {
            if (this._pending[i].success) {
              this._pending[i].success.run();
            }
            // Remove the item from the array
            this._pending.splice(i, 1);
            break;
          }
        }
        break;
      }
    }
  },

  startPrerenderingDocument(aHref, aReferrer) {
    // XXX: Make this constant a pref
    if (this._pending.length >= 2) {
      return;
    }

    let id = ++this._idMonotonic;
    sendAsyncMessage("Prerender:Request", {
      href: aHref.spec,
      referrer: aReferrer ? aReferrer.spec : null,
      id,
    });

    this._pending.push({
      href: aHref,
      referrer: aReferrer,
      id,
      success: null,
      failure: null,
    });
  },

  shouldSwitchToPrerenderedDocument(aHref, aReferrer, aSuccess, aFailure) {
    // Check if we think there is a prerendering document pending for the given
    // href and referrer. If we think there is one, we will send a message to
    // the parent process asking it to do a swap, and hook up the success and
    // failure listeners.
    for (let i = 0; i < this._pending.length; ++i) {
      let p = this._pending[i];
      if (p.href.equals(aHref) && p.referrer.equals(aReferrer)) {
        p.success = aSuccess;
        p.failure = aFailure;
        sendAsyncMessage("Prerender:Swap", {id: p.id});
        return true;
      }
    }

    return false;
  }
};

if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT) {
  // We only want to initialize the PrerenderContentHandler in the content
  // process. Outside of the content process, this should be unused.
  PrerenderContentHandler.init();
}

var WebBrowserChrome = {
  onBeforeLinkTraversal(originalTarget, linkURI, linkNode, isAppTab) {
    return BrowserUtils.onBeforeLinkTraversal(originalTarget, linkURI, linkNode, isAppTab);
  },

  // Check whether this URI should load in the current process
  shouldLoadURI(aDocShell, aURI, aReferrer) {
    if (!E10SUtils.shouldLoadURI(aDocShell, aURI, aReferrer)) {
      E10SUtils.redirectLoad(aDocShell, aURI, aReferrer);
      return false;
    }

    return true;
  },

  shouldLoadURIInThisProcess(aURI) {
    return E10SUtils.shouldLoadURIInThisProcess(aURI);
  },

  // Try to reload the currently active or currently loading page in a new process.
  reloadInFreshProcess(aDocShell, aURI, aReferrer) {
    E10SUtils.redirectLoad(aDocShell, aURI, aReferrer, true);
    return true;
  },

  startPrerenderingDocument(aHref, aReferrer) {
    if (PrerenderContentHandler.initialized) {
      PrerenderContentHandler.startPrerenderingDocument(aHref, aReferrer);
    }
  },

  shouldSwitchToPrerenderedDocument(aHref, aReferrer, aSuccess, aFailure) {
    if (PrerenderContentHandler.initialized) {
      return PrerenderContentHandler.shouldSwitchToPrerenderedDocument(
        aHref, aReferrer, aSuccess, aFailure);
    }
    return false;
  }
};

if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT) {
  let tabchild = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsITabChild);
  tabchild.webBrowserChrome = WebBrowserChrome;
}


var DOMFullscreenHandler = {

  init() {
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

  receiveMessage(aMessage) {
    let windowUtils = this._windowUtils;
    switch (aMessage.name) {
      case "DOMFullscreen:Entered": {
        this._lastTransactionId = windowUtils.lastTransactionId;
        if (!windowUtils.handleFullscreenRequests() &&
            !content.document.fullscreenElement) {
          // If we don't actually have any pending fullscreen request
          // to handle, neither we have been in fullscreen, tell the
          // parent to just exit.
          sendAsyncMessage("DOMFullscreen:Exit");
        }
        break;
      }
      case "DOMFullscreen:CleanUp": {
        // If we've exited fullscreen at this point, no need to record
        // transaction id or call exit fullscreen. This is especially
        // important for non-e10s, since in that case, it is possible
        // that no more paint would be triggered after this point.
        if (content.document.fullscreenElement && windowUtils) {
          this._lastTransactionId = windowUtils.lastTransactionId;
          windowUtils.exitFullscreen();
        }
        break;
      }
    }
  },

  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "MozDOMFullscreen:Request": {
        sendAsyncMessage("DOMFullscreen:Request");
        break;
      }
      case "MozDOMFullscreen:NewOrigin": {
        sendAsyncMessage("DOMFullscreen:NewOrigin", {
          originNoSuffix: aEvent.target.nodePrincipal.originNoSuffix,
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
        // Only send Painted signal after we actually finish painting
        // the transition for the fullscreen change.
        // Note that this._lastTransactionId is not set when in non-e10s
        // mode, so we need to check that explicitly.
        if (!this._lastTransactionId ||
            aEvent.transactionId > this._lastTransactionId) {
          removeEventListener("MozAfterPaint", this);
          sendAsyncMessage("DOMFullscreen:Painted");
        }
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

var UserContextIdNotifier = {
  init() {
    addEventListener("DOMWindowCreated", this);
  },

  uninit() {
    removeEventListener("DOMWindowCreated", this);
  },

  handleEvent(aEvent) {
    // When the window is created, we want to inform the tabbrowser about
    // the userContextId in use in order to update the UI correctly.
    // Just because we cannot change the userContextId from an active docShell,
    // we don't need to check DOMContentLoaded again.
    this.uninit();

    // We use the docShell because content.document can have been loaded before
    // setting the originAttributes.
    let loadContext = docShell.QueryInterface(Ci.nsILoadContext);
    let userContextId = loadContext.originAttributes.userContextId;

    sendAsyncMessage("Browser:WindowCreated", { userContextId });
  }
};

UserContextIdNotifier.init();

ExtensionContent.init(this);
addEventListener("unload", () => {
  ExtensionContent.uninit(this);
  RefreshBlocker.uninit();
});

addMessageListener("AllowScriptsToClose", () => {
  content.QueryInterface(Ci.nsIInterfaceRequestor)
         .getInterface(Ci.nsIDOMWindowUtils)
         .allowScriptsToClose();
});

addEventListener("MozAfterPaint", function onFirstPaint() {
  removeEventListener("MozAfterPaint", onFirstPaint);
  sendAsyncMessage("Browser:FirstPaint");
});
