/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This content script contains code that requires a tab browser. */

/* eslint-env mozilla/frame-script */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "E10SUtils",
  "resource://gre/modules/E10SUtils.jsm");
ChromeUtils.defineModuleGetter(this, "BrowserUtils",
  "resource://gre/modules/BrowserUtils.jsm");
ChromeUtils.defineModuleGetter(this, "Utils",
  "resource://gre/modules/sessionstore/Utils.jsm");
ChromeUtils.defineModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");
ChromeUtils.defineModuleGetter(this, "AboutReader",
  "resource://gre/modules/AboutReader.jsm");
ChromeUtils.defineModuleGetter(this, "ReaderMode",
  "resource://gre/modules/ReaderMode.jsm");
ChromeUtils.defineModuleGetter(this, "PageStyleHandler",
  "resource:///modules/PageStyleHandler.jsm");
ChromeUtils.defineModuleGetter(this, "LightweightThemeChildListener",
  "resource:///modules/LightweightThemeChildListener.jsm");

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


// XXX(nika): Should we try to call this in the parent process instead?
addMessageListener("Browser:Reload", function(message) {
  /* First, we'll try to use the session history object to reload so
   * that framesets are handled properly. If we're in a special
   * window (such as view-source) that has no session history, fall
   * back on using the web navigation's reload method.
   */

  let webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
  try {
    if (webNav.sessionHistory) {
      webNav = webNav.sessionHistory;
    }
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

var LightweightThemeChildListenerStub = {
  _childListener: null,
  get childListener() {
    if (!this._childListener) {
      this._childListener = new LightweightThemeChildListener();
    }
    return this._childListener;
  },

  init() {
    addEventListener("LightweightTheme:Support", this, false, true);
    addMessageListener("LightweightTheme:Update", this);
    sendAsyncMessage("LightweightTheme:Request");
  },

  handleEvent(event) {
    return this.childListener.handleEvent(event);
  },

  receiveMessage(msg) {
    return this.childListener.receiveMessage(msg);
  },
};

LightweightThemeChildListenerStub.init();


var AboutReaderListener = {

  _articlePromise: null,

  _isLeavingReaderableReaderMode: false,

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
          this._isLeavingReaderableReaderMode = this.isReaderableAboutReader;
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

  get isReaderableAboutReader() {
    return this.isAboutReader &&
      !content.document.documentElement.dataset.isError;
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
        // this._isLeavingReaderableReaderMode is used here to keep the Reader Mode icon
        // visible in the location bar when transitioning from reader-mode page
        // back to the readable source page.
        sendAsyncMessage("Reader:UpdateReaderButton", { isArticle: this._isLeavingReaderableReaderMode });
        if (this._isLeavingReaderableReaderMode) {
          this._isLeavingReaderableReaderMode = false;
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
    "about:welcome",
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

addMessageListener("PageStyle:Switch", PageStyleHandler);
addMessageListener("PageStyle:Disable", PageStyleHandler);
addEventListener("pageshow", PageStyleHandler);

// Keep a reference to the translation content handler to avoid it it being GC'ed.
var trHandler = null;
if (Services.prefs.getBoolPref("browser.translation.detectLanguage")) {
  ChromeUtils.import("resource:///modules/translation/TranslationContentHandler.jsm");
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
Services.obs.addObserver(gKeywordURIFixup, "keyword-uri-fixup");
addEventListener("unload", () => {
  Services.obs.removeObserver(gKeywordURIFixup, "keyword-uri-fixup");
}, false);

addMessageListener("Browser:AppTab", function(message) {
  if (docShell) {
    docShell.isAppTab = message.data.isAppTab;
  }
});

var WebBrowserChrome = {
  onBeforeLinkTraversal(originalTarget, linkURI, linkNode, isAppTab) {
    return BrowserUtils.onBeforeLinkTraversal(originalTarget, linkURI, linkNode, isAppTab);
  },

  // Check whether this URI should load in the current process
  shouldLoadURI(aDocShell, aURI, aReferrer, aHasPostData, aTriggeringPrincipal) {
    if (!E10SUtils.shouldLoadURI(aDocShell, aURI, aReferrer, aHasPostData)) {
      E10SUtils.redirectLoad(aDocShell, aURI, aReferrer, aTriggeringPrincipal, false);
      return false;
    }

    return true;
  },

  shouldLoadURIInThisProcess(aURI) {
    return E10SUtils.shouldLoadURIInThisProcess(aURI);
  },

  // Try to reload the currently active or currently loading page in a new process.
  reloadInFreshProcess(aDocShell, aURI, aReferrer, aTriggeringPrincipal, aLoadFlags) {
    E10SUtils.redirectLoad(aDocShell, aURI, aReferrer, aTriggeringPrincipal, true, aLoadFlags);
    return true;
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

Services.obs.notifyObservers(this, "tab-content-frameloader-created");

addMessageListener("AllowScriptsToClose", () => {
  content.QueryInterface(Ci.nsIInterfaceRequestor)
         .getInterface(Ci.nsIDOMWindowUtils)
         .allowScriptsToClose();
});

addEventListener("MozAfterPaint", function onFirstPaint() {
  removeEventListener("MozAfterPaint", onFirstPaint);
  sendAsyncMessage("Browser:FirstPaint");
});

// Remove this once bug 1397365 is fixed.
addEventListener("MozAfterPaint", function onFirstNonBlankPaint() {
  if (content.document.documentURI == "about:blank" && !content.opener)
    return;
  removeEventListener("MozAfterPaint", onFirstNonBlankPaint);
  sendAsyncMessage("Browser:FirstNonBlankPaint");
});
