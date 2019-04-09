/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint-env mozilla/frame-script */

function debug(msg) {
  // dump("BrowserElementChildPreload - " + msg + "\n");
}

debug("loaded");

var BrowserElementIsReady;

var {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
var {BrowserElementPromptService} = ChromeUtils.import("resource://gre/modules/BrowserElementPromptService.jsm");

var kLongestReturnedString = 128;

var Timer = Components.Constructor("@mozilla.org/timer;1",
                                   "nsITimer",
                                   "initWithCallback");

function sendAsyncMsg(msg, data) {
  // Ensure that we don't send any messages before BrowserElementChild.js
  // finishes loading.
  if (!BrowserElementIsReady) {
    return;
  }

  if (!data) {
    data = { };
  }

  data.msg_name = msg;
  sendAsyncMessage("browser-element-api:call", data);
}

function sendSyncMsg(msg, data) {
  // Ensure that we don't send any messages before BrowserElementChild.js
  // finishes loading.
  if (!BrowserElementIsReady) {
    return undefined;
  }

  if (!data) {
    data = { };
  }

  data.msg_name = msg;
  return sendSyncMessage("browser-element-api:call", data);
}

var CERTIFICATE_ERROR_PAGE_PREF = "security.alternate_certificate_error_page";

var OBSERVED_EVENTS = [
  "xpcom-shutdown",
  "audio-playback",
  "activity-done",
  "will-launch-app",
];

var LISTENED_EVENTS = [
  { type: "DOMTitleChanged", useCapture: true, wantsUntrusted: false },
  { type: "DOMLinkAdded", useCapture: true, wantsUntrusted: false },
  { type: "MozScrolledAreaChanged", useCapture: true, wantsUntrusted: false },
  { type: "MozDOMFullscreen:Request", useCapture: true, wantsUntrusted: false },
  { type: "MozDOMFullscreen:NewOrigin", useCapture: true, wantsUntrusted: false },
  { type: "MozDOMFullscreen:Exit", useCapture: true, wantsUntrusted: false },
  { type: "DOMMetaAdded", useCapture: true, wantsUntrusted: false },
  { type: "DOMMetaChanged", useCapture: true, wantsUntrusted: false },
  { type: "DOMMetaRemoved", useCapture: true, wantsUntrusted: false },
  { type: "scrollviewchange", useCapture: true, wantsUntrusted: false },
  { type: "click", useCapture: false, wantsUntrusted: false },
  // This listens to unload events from our message manager, but /not/ from
  // the |content| window.  That's because the window's unload event doesn't
  // bubble, and we're not using a capturing listener.  If we'd used
  // useCapture == true, we /would/ hear unload events from the window, which
  // is not what we want!
  { type: "unload", useCapture: false, wantsUntrusted: false },
];

// We are using the system group for those events so if something in the
// content called .stopPropagation() this will still be called.
var LISTENED_SYSTEM_EVENTS = [
  { type: "DOMWindowClose", useCapture: false },
  { type: "DOMWindowCreated", useCapture: false },
  { type: "DOMWindowResize", useCapture: false },
  { type: "contextmenu", useCapture: false },
  { type: "scroll", useCapture: false },
];

/**
 * The BrowserElementChild implements one half of <iframe mozbrowser>.
 * (The other half is, unsurprisingly, BrowserElementParent.)
 *
 * This script is injected into an <iframe mozbrowser> via
 * nsIMessageManager::LoadFrameScript().
 *
 * Our job here is to listen for events within this frame and bubble them up to
 * the parent process.
 */

var global = this;

function BrowserElementChild() {
  // Maps outer window id --> weak ref to window.  Used by modal dialog code.
  this._windowIDDict = {};

  this._isContentWindowCreated = false;

  this._init();
}

BrowserElementChild.prototype = {

  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver,
                                          Ci.nsISupportsWeakReference]),

  _init() {
    debug("Starting up.");

    BrowserElementPromptService.mapWindowToBrowserElementChild(content, this);

    docShell.QueryInterface(Ci.nsIWebProgress)
            .addProgressListener(this._progressListener,
                                 Ci.nsIWebProgress.NOTIFY_LOCATION |
                                 Ci.nsIWebProgress.NOTIFY_SECURITY |
                                 Ci.nsIWebProgress.NOTIFY_STATE_WINDOW);

    let webNavigation = docShell.QueryInterface(Ci.nsIWebNavigation);
    if (!webNavigation.sessionHistory) {
      // XXX(nika): I don't think this code should ever be hit? We should run
      // BrowserChild::Init before we run this code which will perform this setup
      // for us.
      docShell.initSessionHistory();
    }

    // This is necessary to get security web progress notifications.
    var securityUI = Cc["@mozilla.org/secure_browser_ui;1"]
                       .createInstance(Ci.nsISecureBrowserUI);
    securityUI.init(docShell);

    // A cache of the menuitem dom objects keyed by the id we generate
    // and pass to the embedder
    this._ctxHandlers = {};
    // Counter of contextmenu events fired
    this._ctxCounter = 0;

    this._shuttingDown = false;

    LISTENED_EVENTS.forEach(event => {
      addEventListener(event.type, this, event.useCapture, event.wantsUntrusted);
    });

    // Registers a MozAfterPaint handler for the very first paint.
    this._addMozAfterPaintHandler(function() {
      sendAsyncMsg("firstpaint");
    });

    addMessageListener("browser-element-api:call", this);

    LISTENED_SYSTEM_EVENTS.forEach(event => {
      Services.els.addSystemEventListener(global, event.type, this, event.useCapture);
    });

    OBSERVED_EVENTS.forEach((aTopic) => {
      Services.obs.addObserver(this, aTopic);
    });
  },

  /**
   * Shut down the frame's side of the browser API.  This is called when:
   *   - our BrowserChildGlobal starts to die
   *   - the content is moved to frame without the browser API
   * This is not called when the page inside |content| unloads.
   */
  destroy() {
    debug("Destroying");
    this._shuttingDown = true;

    BrowserElementPromptService.unmapWindowToBrowserElementChild(content);

    docShell.QueryInterface(Ci.nsIWebProgress)
            .removeProgressListener(this._progressListener);

    LISTENED_EVENTS.forEach(event => {
      removeEventListener(event.type, this, event.useCapture, event.wantsUntrusted);
    });

    removeMessageListener("browser-element-api:call", this);

    LISTENED_SYSTEM_EVENTS.forEach(event => {
      Services.els.removeSystemEventListener(global, event.type, this, event.useCapture);
    });

    OBSERVED_EVENTS.forEach((aTopic) => {
      Services.obs.removeObserver(this, aTopic);
    });
  },

  handleEvent(event) {
    switch (event.type) {
      case "DOMTitleChanged":
        this._titleChangedHandler(event);
        break;
      case "DOMLinkAdded":
        this._linkAddedHandler(event);
        break;
      case "MozScrolledAreaChanged":
        this._mozScrollAreaChanged(event);
        break;
      case "MozDOMFullscreen:Request":
        this._mozRequestedDOMFullscreen(event);
        break;
      case "MozDOMFullscreen:NewOrigin":
        this._mozFullscreenOriginChange(event);
        break;
      case "MozDOMFullscreen:Exit":
        this._mozExitDomFullscreen(event);
        break;
      case "DOMMetaAdded":
        this._metaChangedHandler(event);
        break;
      case "DOMMetaChanged":
        this._metaChangedHandler(event);
        break;
      case "DOMMetaRemoved":
        this._metaChangedHandler(event);
        break;
      case "scrollviewchange":
        this._ScrollViewChangeHandler(event);
        break;
      case "click":
        this._ClickHandler(event);
        break;
      case "unload":
        this.destroy(event);
        break;
      case "DOMWindowClose":
        this._windowCloseHandler(event);
        break;
      case "DOMWindowCreated":
        this._windowCreatedHandler(event);
        break;
      case "DOMWindowResize":
        this._windowResizeHandler(event);
        break;
      case "contextmenu":
        this._contextmenuHandler(event);
        break;
      case "scroll":
        this._scrollEventHandler(event);
        break;
    }
  },

  receiveMessage(message) {
    let self = this;

    let mmCalls = {
      "send-mouse-event": this._recvSendMouseEvent,
      "get-can-go-back": this._recvCanGoBack,
      "get-can-go-forward": this._recvCanGoForward,
      "go-back": this._recvGoBack,
      "go-forward": this._recvGoForward,
      "reload": this._recvReload,
      "stop": this._recvStop,
      "zoom": this._recvZoom,
      "unblock-modal-prompt": this._recvStopWaiting,
      "fire-ctx-callback": this._recvFireCtxCallback,
      "owner-visibility-change": this._recvOwnerVisibilityChange,
      "entered-fullscreen": this._recvEnteredFullscreen,
      "exit-fullscreen": this._recvExitFullscreen,
    };

    if (message.data.msg_name in mmCalls) {
      return mmCalls[message.data.msg_name].apply(self, arguments);
    }
    return undefined;
  },

  _paintFrozenTimer: null,
  observe(subject, topic, data) {
    // Ignore notifications not about our document.  (Note that |content| /can/
    // be null; see bug 874900.)

    if (topic !== "activity-done" &&
        topic !== "audio-playback" &&
        topic !== "will-launch-app" &&
        (!content || subject !== content.document)) {
      return;
    }
    if (topic == "activity-done" && docShell !== subject)
      return;
    switch (topic) {
      case "activity-done":
        sendAsyncMsg("activitydone", { success: (data == "activity-success") });
        break;
      case "audio-playback":
        if (subject === content) {
          sendAsyncMsg("audioplaybackchange", { _payload_: data });
        }
        break;
      case "xpcom-shutdown":
        this._shuttingDown = true;
        break;
      case "will-launch-app":
        // If the launcher is not visible, let's ignore the message.
        if (!docShell.isActive) {
          return;
        }

        // If this is not a content process, let's not freeze painting.
        if (Services.appinfo.processType != Services.appinfo.PROCESS_TYPE_CONTENT) {
          return;
        }

        docShell.contentViewer.pausePainting();

        this._paintFrozenTimer && this._paintFrozenTimer.cancel();
        this._paintFrozenTimer = new Timer(this, 3000, Ci.nsITimer.TYPE_ONE_SHOT);
        break;
    }
  },

  notify(timer) {
    docShell.contentViewer.resumePainting();
    this._paintFrozenTimer.cancel();
    this._paintFrozenTimer = null;
  },

  get _windowUtils() {
    return content.document.defaultView.windowUtils;
  },

  _tryGetInnerWindowID(win) {
    let utils = win.windowUtils;
    try {
      return utils.currentInnerWindowID;
    } catch (e) {
      return null;
    }
  },

  /**
   * Show a modal prompt.  Called by BrowserElementPromptService.
   */
  showModalPrompt(win, args) {
    let utils = win.windowUtils;

    args.windowID = { outer: utils.outerWindowID,
                      inner: this._tryGetInnerWindowID(win) };
    sendAsyncMsg("showmodalprompt", args);

    let returnValue = this._waitForResult(win);

    if (args.promptType == "prompt" ||
        args.promptType == "confirm" ||
        args.promptType == "custom-prompt") {
      return returnValue;
    }
    return undefined;
  },

  /**
   * Spin in a nested event loop until we receive a unblock-modal-prompt message for
   * this window.
   */
  _waitForResult(win) {
    debug("_waitForResult(" + win + ")");
    let utils = win.windowUtils;

    let outerWindowID = utils.outerWindowID;
    let innerWindowID = this._tryGetInnerWindowID(win);
    if (innerWindowID === null) {
      // I have no idea what waiting for a result means when there's no inner
      // window, so let's just bail.
      debug("_waitForResult: No inner window. Bailing.");
      return undefined;
    }

    this._windowIDDict[outerWindowID] = Cu.getWeakReference(win);

    debug("Entering modal state (outerWindowID=" + outerWindowID + ", " +
                                "innerWindowID=" + innerWindowID + ")");

    utils.enterModalState();

    // We'll decrement win.modalDepth when we receive a unblock-modal-prompt message
    // for the window.
    if (!win.modalDepth) {
      win.modalDepth = 0;
    }
    win.modalDepth++;
    let origModalDepth = win.modalDepth;

    debug("Nested event loop - begin");
    Services.tm.spinEventLoopUntil(() => {
      // Bail out of the loop if the inner window changed; that means the
      // window navigated.  Bail out when we're shutting down because otherwise
      // we'll leak our window.
      if (this._tryGetInnerWindowID(win) !== innerWindowID) {
        debug("_waitForResult: Inner window ID changed " +
              "while in nested event loop.");
        return true;
      }

      return win.modalDepth !== origModalDepth || this._shuttingDown;
    });
    debug("Nested event loop - finish");

    if (win.modalDepth == 0) {
      delete this._windowIDDict[outerWindowID];
    }

    // If we exited the loop because the inner window changed, then bail on the
    // modal prompt.
    if (innerWindowID !== this._tryGetInnerWindowID(win)) {
      throw Components.Exception("Modal state aborted by navigation",
                                 Cr.NS_ERROR_NOT_AVAILABLE);
    }

    let returnValue = win.modalReturnValue;
    delete win.modalReturnValue;

    if (!this._shuttingDown) {
      utils.leaveModalState();
    }

    debug("Leaving modal state (outerID=" + outerWindowID + ", " +
                               "innerID=" + innerWindowID + ")");
    return returnValue;
  },

  _recvStopWaiting(msg) {
    let outerID = msg.json.windowID.outer;
    let innerID = msg.json.windowID.inner;
    let returnValue = msg.json.returnValue;
    debug("recvStopWaiting(outer=" + outerID + ", inner=" + innerID +
          ", returnValue=" + returnValue + ")");

    if (!this._windowIDDict[outerID]) {
      debug("recvStopWaiting: No record of outer window ID " + outerID);
      return;
    }

    let win = this._windowIDDict[outerID].get();

    if (!win) {
      debug("recvStopWaiting, but window is gone\n");
      return;
    }

    if (innerID !== this._tryGetInnerWindowID(win)) {
      debug("recvStopWaiting, but inner ID has changed\n");
      return;
    }

    debug("recvStopWaiting " + win);
    win.modalReturnValue = returnValue;
    win.modalDepth--;
  },

  _recvEnteredFullscreen() {
    if (!this._windowUtils.handleFullscreenRequests() &&
        !content.document.fullscreenElement) {
      // If we don't actually have any pending fullscreen request
      // to handle, neither we have been in fullscreen, tell the
      // parent to just exit.
      sendAsyncMsg("exit-dom-fullscreen");
    }
  },

  _recvExitFullscreen() {
    this._windowUtils.exitFullscreen();
  },

  _titleChangedHandler(e) {
    debug("Got titlechanged: (" + e.target.title + ")");
    var win = e.target.defaultView;

    // Ignore titlechanges which don't come from the top-level
    // <iframe mozbrowser> window.
    if (win == content) {
      sendAsyncMsg("titlechange", { _payload_: e.target.title });
    } else {
      debug("Not top level!");
    }
  },

  _maybeCopyAttribute(src, target, attribute) {
    if (src.getAttribute(attribute)) {
      target[attribute] = src.getAttribute(attribute);
    }
  },

  _iconChangedHandler(e) {
    debug("Got iconchanged: (" + e.target.href + ")");
    let icon = { href: e.target.href };
    this._maybeCopyAttribute(e.target, icon, "sizes");
    this._maybeCopyAttribute(e.target, icon, "rel");
    sendAsyncMsg("iconchange", icon);
  },

  _openSearchHandler(e) {
    debug("Got opensearch: (" + e.target.href + ")");

    if (e.target.type !== "application/opensearchdescription+xml") {
      return;
    }

    sendAsyncMsg("opensearch", { title: e.target.title,
                                 href: e.target.href });
  },

  // Processes the "rel" field in <link> tags and forward to specific handlers.
  _linkAddedHandler(e) {
    let win = e.target.ownerGlobal;
    // Ignore links which don't come from the top-level
    // <iframe mozbrowser> window.
    if (win != content) {
      debug("Not top level!");
      return;
    }

    let handlers = {
      "icon": this._iconChangedHandler.bind(this),
      "apple-touch-icon": this._iconChangedHandler.bind(this),
      "apple-touch-icon-precomposed": this._iconChangedHandler.bind(this),
      "search": this._openSearchHandler,
    };

    debug("Got linkAdded: (" + e.target.href + ") " + e.target.rel);
    e.target.rel.split(" ").forEach(function(x) {
      let token = x.toLowerCase();
      if (handlers[token]) {
        handlers[token](e);
      }
    }, this);
  },

  _metaChangedHandler(e) {
    let win = e.target.ownerGlobal;
    // Ignore metas which don't come from the top-level
    // <iframe mozbrowser> window.
    if (win != content) {
      debug("Not top level!");
      return;
    }

    var name = e.target.name;
    var property = e.target.getAttributeNS(null, "property");

    if (!name && !property) {
      return;
    }

    debug("Got metaChanged: (" + (name || property) + ") " +
          e.target.content);

    let handlers = {
      "viewmode": this._genericMetaHandler,
      "theme-color": this._genericMetaHandler,
      "theme-group": this._genericMetaHandler,
      "application-name": this._applicationNameChangedHandler,
    };
    let handler = handlers[name];

    if ((property || name).match(/^og:/)) {
      name = property || name;
      handler = this._genericMetaHandler;
    }

    if (handler) {
      handler(name, e.type, e.target);
    }
  },

  _applicationNameChangedHandler(name, eventType, target) {
    if (eventType !== "DOMMetaAdded") {
      // Bug 1037448 - Decide what to do when <meta name="application-name">
      // changes
      return;
    }

    let meta = { name,
                 content: target.content };

    let lang;
    let elm;

    for (elm = target;
         !lang && elm && elm.nodeType == target.ELEMENT_NODE;
         elm = elm.parentNode) {
      if (elm.hasAttribute("lang")) {
        lang = elm.getAttribute("lang");
        continue;
      }

      if (elm.hasAttributeNS("http://www.w3.org/XML/1998/namespace", "lang")) {
        lang = elm.getAttributeNS("http://www.w3.org/XML/1998/namespace", "lang");
        continue;
      }
    }

    // No lang has been detected.
    if (!lang && elm.nodeType == target.DOCUMENT_NODE) {
      lang = elm.contentLanguage;
    }

    if (lang) {
      meta.lang = lang;
    }

    sendAsyncMsg("metachange", meta);
  },

  _ScrollViewChangeHandler(e) {
    e.stopPropagation();
    let detail = {
      state: e.state,
    };
    sendAsyncMsg("scrollviewchange", detail);
  },

  _ClickHandler(e) {
    let isHTMLLink = node =>
        ((ChromeUtils.getClassName(node) === "HTMLAnchorElement" && node.href) ||
         (ChromeUtils.getClassName(node) === "HTMLAreaElement" && node.href) ||
         ChromeUtils.getClassName(node) === "HTMLLinkElement");

    // Open in a new tab if middle click or ctrl/cmd-click,
    // and e.target is a link or inside a link.
    if ((Services.appinfo.OS == "Darwin" && e.metaKey) ||
        (Services.appinfo.OS != "Darwin" && e.ctrlKey) ||
         e.button == 1) {
      let node = e.target;
      while (node && !isHTMLLink(node)) {
        node = node.parentNode;
      }

      if (node) {
        sendAsyncMsg("opentab", {url: node.href});
      }
    }
  },

  _genericMetaHandler(name, eventType, target) {
    let meta = {
      name,
      content: target.content,
      type: eventType.replace("DOMMeta", "").toLowerCase(),
    };
    sendAsyncMsg("metachange", meta);
  },

  _addMozAfterPaintHandler(callback) {
    function onMozAfterPaint() {
      let uri = docShell.QueryInterface(Ci.nsIWebNavigation).currentURI;
      if (uri.spec != "about:blank") {
        debug("Got afterpaint event: " + uri.spec);
        removeEventListener("MozAfterPaint", onMozAfterPaint,
                            /* useCapture = */ true);
        callback();
      }
    }

    addEventListener("MozAfterPaint", onMozAfterPaint, /* useCapture = */ true);
    return onMozAfterPaint;
  },

  _windowCloseHandler(e) {
    let win = e.target;
    if (win != content || e.defaultPrevented) {
      return;
    }

    debug("Closing window " + win);
    sendAsyncMsg("close");

    // Inform the window implementation that we handled this close ourselves.
    e.preventDefault();
  },

  _windowCreatedHandler(e) {
    let targetDocShell = e.target.defaultView.docShell;
    if (targetDocShell != docShell) {
      return;
    }

    let uri = docShell.QueryInterface(Ci.nsIWebNavigation).currentURI;
    debug("Window created: " + uri.spec);
    if (uri.spec != "about:blank") {
      this._addMozAfterPaintHandler(function() {
        sendAsyncMsg("documentfirstpaint");
      });
      this._isContentWindowCreated = true;
    }
  },

  _windowResizeHandler(e) {
    let win = e.target;
    if (win != content || e.defaultPrevented) {
      return;
    }

    debug("resizing window " + win);
    sendAsyncMsg("resize", { width: e.detail.width, height: e.detail.height });

    // Inform the window implementation that we handled this resize ourselves.
    e.preventDefault();
  },

  _contextmenuHandler(e) {
    debug("Got contextmenu");

    if (e.defaultPrevented) {
      return;
    }

    this._ctxCounter++;
    this._ctxHandlers = {};

    var elem = e.target;
    var menuData = {systemTargets: [], contextmenu: null};
    var ctxMenuId = null;
    var clipboardPlainTextOnly = Services.prefs.getBoolPref("clipboard.plainTextOnly");
    var copyableElements = {
      image: false,
      link: false,
      hasElements() {
        return this.image || this.link;
      },
    };

    // Set the event target as the copy image command needs it to
    // determine what was context-clicked on.
    docShell.contentViewer.QueryInterface(Ci.nsIContentViewerEdit).setCommandNode(elem);

    while (elem && elem.parentNode) {
      var ctxData = this._getSystemCtxMenuData(elem);
      if (ctxData) {
        menuData.systemTargets.push({
          nodeName: elem.nodeName,
          data: ctxData,
        });
      }

      if (!ctxMenuId && "hasAttribute" in elem && elem.hasAttribute("contextmenu")) {
        ctxMenuId = elem.getAttribute("contextmenu");
      }

      // Enable copy image/link option
      if (elem.nodeName == "IMG") {
        copyableElements.image = !clipboardPlainTextOnly;
      } else if (elem.nodeName == "A") {
        copyableElements.link = true;
      }

      elem = elem.parentNode;
    }

    if (ctxMenuId || copyableElements.hasElements()) {
      var menu = null;
      if (ctxMenuId) {
        menu = e.target.ownerDocument.getElementById(ctxMenuId);
      }
      menuData.contextmenu = this._buildMenuObj(menu, "", copyableElements);
    }

    // Pass along the position where the context menu should be located
    menuData.clientX = e.clientX;
    menuData.clientY = e.clientY;
    menuData.screenX = e.screenX;
    menuData.screenY = e.screenY;

    // The value returned by the contextmenu sync call is true if the embedder
    // called preventDefault() on its contextmenu event.
    //
    // We call preventDefault() on our contextmenu event if the embedder called
    // preventDefault() on /its/ contextmenu event.  This way, if the embedder
    // ignored the contextmenu event, BrowserChild will fire a click.
    if (sendSyncMsg("contextmenu", menuData)[0]) {
      e.preventDefault();
    } else {
      this._ctxHandlers = {};
    }
  },

  _getSystemCtxMenuData(elem) {
    let documentURI =
      docShell.QueryInterface(Ci.nsIWebNavigation).currentURI.spec;

    if ((ChromeUtils.getClassName(elem) === "HTMLAnchorElement" && elem.href) ||
        (ChromeUtils.getClassName(elem) === "HTMLAreaElement" && elem.href)) {
      return {uri: elem.href,
              documentURI,
              text: elem.textContent.substring(0, kLongestReturnedString)};
    }
    if (elem instanceof Ci.nsIImageLoadingContent &&
        (elem.currentRequestFinalURI || elem.currentURI)) {
      let uri = elem.currentRequestFinalURI || elem.currentURI;
      return {uri: uri.spec, documentURI};
    }
    if (ChromeUtils.getClassName(elem) === "HTMLImageElement") {
      return {uri: elem.src, documentURI};
    }
    if (ChromeUtils.getClassName(elem) === "HTMLVideoElement" ||
        ChromeUtils.getClassName(elem) === "HTMLAudioElement") {
      let hasVideo = !(elem.readyState >= elem.HAVE_METADATA &&
                       (elem.videoWidth == 0 || elem.videoHeight == 0));
      return {uri: elem.currentSrc || elem.src,
              hasVideo,
              documentURI};
    }
    if (ChromeUtils.getClassName(elem) === "HTMLInputElement" &&
        elem.hasAttribute("name")) {
      // For input elements, we look for a parent <form> and if there is
      // one we return the form's method and action uri.
      let parent = elem.parentNode;
      while (parent) {
        if (ChromeUtils.getClassName(parent) === "HTMLFormElement" &&
            parent.hasAttribute("action")) {
          let actionHref = docShell.QueryInterface(Ci.nsIWebNavigation)
                                   .currentURI
                                   .resolve(parent.getAttribute("action"));
          let method = parent.hasAttribute("method")
            ? parent.getAttribute("method").toLowerCase()
            : "get";
          return {
            documentURI,
            action: actionHref,
            method,
            name: elem.getAttribute("name"),
          };
        }
        parent = parent.parentNode;
      }
    }
    return false;
  },

  _scrollEventHandler(e) {
    let win = e.target.defaultView;
    if (win != content) {
      return;
    }

    debug("scroll event " + win);
    sendAsyncMsg("scroll", { top: win.scrollY, left: win.scrollX });
  },

  _mozScrollAreaChanged(e) {
    sendAsyncMsg("scrollareachanged", {
      width: e.width,
      height: e.height,
    });
  },

  _mozRequestedDOMFullscreen(e) {
    sendAsyncMsg("requested-dom-fullscreen");
  },

  _mozFullscreenOriginChange(e) {
    sendAsyncMsg("fullscreen-origin-change", {
      originNoSuffix: e.target.nodePrincipal.originNoSuffix,
    });
  },

  _mozExitDomFullscreen(e) {
    sendAsyncMsg("exit-dom-fullscreen");
  },

  _recvFireCtxCallback(data) {
    debug("Received fireCtxCallback message: (" + data.json.menuitem + ")");

    let doCommandIfEnabled = (command) => {
      if (docShell.isCommandEnabled(command)) {
        docShell.doCommand(command);
      }
    };

    if (data.json.menuitem == "copy-image") {
      doCommandIfEnabled("cmd_copyImage");
    } else if (data.json.menuitem == "copy-link") {
      doCommandIfEnabled("cmd_copyLink");
    } else if (data.json.menuitem in this._ctxHandlers) {
      this._ctxHandlers[data.json.menuitem].click();
      this._ctxHandlers = {};
    } else {
      // We silently ignore if the embedder uses an incorrect id in the callback
      debug("Ignored invalid contextmenu invocation");
    }
  },

  _buildMenuObj(menu, idPrefix, copyableElements) {
    var menuObj = {type: "menu", customized: false, items: []};
    // Customized context menu
    if (menu) {
      this._maybeCopyAttribute(menu, menuObj, "label");

      for (var i = 0, child; (child = menu.children[i++]);) {
        if (child.nodeName === "MENU") {
          menuObj.items.push(this._buildMenuObj(child, idPrefix + i + "_", false));
        } else if (child.nodeName === "MENUITEM") {
          var id = this._ctxCounter + "_" + idPrefix + i;
          var menuitem = {id, type: "menuitem"};
          this._maybeCopyAttribute(child, menuitem, "label");
          this._maybeCopyAttribute(child, menuitem, "icon");
          this._ctxHandlers[id] = child;
          menuObj.items.push(menuitem);
        }
      }

      if (menuObj.items.length > 0) {
        menuObj.customized = true;
      }
    }
    // Note: Display "Copy Link" first in order to make sure "Copy Image" is
    //       put together with other image options if elem is an image link.
    // "Copy Link" menu item
    if (copyableElements.link) {
      menuObj.items.push({id: "copy-link"});
    }
    // "Copy Image" menu item
    if (copyableElements.image) {
      menuObj.items.push({id: "copy-image"});
    }

    return menuObj;
  },

  /**
   * Called when the window which contains this iframe becomes hidden or
   * visible.
   */
  _recvOwnerVisibilityChange(data) {
    debug("Received ownerVisibilityChange: (" + data.json.visible + ")");
    var visible = data.json.visible;
    if (docShell && docShell.isActive !== visible) {
      docShell.isActive = visible;

      // Ensure painting is not frozen if the app goes visible.
      if (visible && this._paintFrozenTimer) {
        this.notify();
      }
    }
  },

  _recvSendMouseEvent(data) {
    let json = data.json;
    let utils = content.windowUtils;
    utils.sendMouseEventToWindow(json.type, json.x, json.y, json.button,
                                 json.clickCount, json.modifiers);
  },

  _recvCanGoBack(data) {
    var webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
    sendAsyncMsg("got-can-go-back", {
      id: data.json.id,
      successRv: webNav.canGoBack,
    });
  },

  _recvCanGoForward(data) {
    var webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
    sendAsyncMsg("got-can-go-forward", {
      id: data.json.id,
      successRv: webNav.canGoForward,
    });
  },

  _recvGoBack(data) {
    try {
      docShell.QueryInterface(Ci.nsIWebNavigation).goBack();
    } catch (e) {
      // Silently swallow errors; these happen when we can't go back.
    }
  },

  _recvGoForward(data) {
    try {
      docShell.QueryInterface(Ci.nsIWebNavigation).goForward();
    } catch (e) {
      // Silently swallow errors; these happen when we can't go forward.
    }
  },

  _recvReload(data) {
    let webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
    let reloadFlags = data.json.hardReload ?
      webNav.LOAD_FLAGS_BYPASS_PROXY | webNav.LOAD_FLAGS_BYPASS_CACHE :
      webNav.LOAD_FLAGS_NONE;
    try {
      webNav.reload(reloadFlags);
    } catch (e) {
      // Silently swallow errors; these can happen if a used cancels reload
    }
  },

  _recvStop(data) {
    let webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
    webNav.stop(webNav.STOP_NETWORK);
  },

  // The docShell keeps a weak reference to the progress listener, so we need
  // to keep a strong ref to it ourselves.
  _progressListener: {
    QueryInterface: ChromeUtils.generateQI([Ci.nsIWebProgressListener,
                                            Ci.nsISupportsWeakReference]),
    _seenLoadStart: false,

    onLocationChange(webProgress, request, location, flags) {
      // We get progress events from subshells here, which is kind of weird.
      if (webProgress != docShell) {
        return;
      }

      // Ignore locationchange events which occur before the first loadstart.
      // These are usually about:blank loads we don't care about.
      if (!this._seenLoadStart) {
        return;
      }

      // Remove password from uri.
      location = Services.uriFixup.createExposableURI(location);

      var webNav = docShell.QueryInterface(Ci.nsIWebNavigation);

      sendAsyncMsg("locationchange", { url: location.spec,
                                       canGoBack: webNav.canGoBack,
                                       canGoForward: webNav.canGoForward });
    },

    // eslint-disable-next-line complexity
    onStateChange(webProgress, request, stateFlags, status) {
      if (webProgress != docShell) {
        return;
      }

      if (stateFlags & Ci.nsIWebProgressListener.STATE_START) {
        this._seenLoadStart = true;
        sendAsyncMsg("loadstart");
      }

      if (stateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
        let bgColor = "transparent";
        try {
          bgColor = content.getComputedStyle(content.document.body)
                           .getPropertyValue("background-color");
        } catch (e) {}
        sendAsyncMsg("loadend", {backgroundColor: bgColor});

        switch (status) {
          case Cr.NS_OK :
          case Cr.NS_BINDING_ABORTED :
            // Ignoring NS_BINDING_ABORTED, which is set when loading page is
            // stopped.
          case Cr.NS_ERROR_PARSED_DATA_CACHED:
            return;

          // TODO See nsDocShell::DisplayLoadError to see what extra
          // information we should be annotating this first block of errors
          // with. Bug 1107091.
          case Cr.NS_ERROR_UNKNOWN_PROTOCOL :
            sendAsyncMsg("error", { type: "unknownProtocolFound" });
            return;
          case Cr.NS_ERROR_FILE_NOT_FOUND :
            sendAsyncMsg("error", { type: "fileNotFound" });
            return;
          case Cr.NS_ERROR_UNKNOWN_HOST :
            sendAsyncMsg("error", { type: "dnsNotFound" });
            return;
          case Cr.NS_ERROR_CONNECTION_REFUSED :
            sendAsyncMsg("error", { type: "connectionFailure" });
            return;
          case Cr.NS_ERROR_NET_INTERRUPT :
            sendAsyncMsg("error", { type: "netInterrupt" });
            return;
          case Cr.NS_ERROR_NET_TIMEOUT :
            sendAsyncMsg("error", { type: "netTimeout" });
            return;
          case Cr.NS_ERROR_CSP_FRAME_ANCESTOR_VIOLATION :
            sendAsyncMsg("error", { type: "cspBlocked" });
            return;
          case Cr.NS_ERROR_PHISHING_URI :
            sendAsyncMsg("error", { type: "deceptiveBlocked" });
            return;
          case Cr.NS_ERROR_MALWARE_URI :
            sendAsyncMsg("error", { type: "malwareBlocked" });
            return;
          case Cr.NS_ERROR_HARMFUL_URI :
            sendAsyncMsg("error", { type: "harmfulBlocked" });
            return;
          case Cr.NS_ERROR_UNWANTED_URI :
            sendAsyncMsg("error", { type: "unwantedBlocked" });
            return;
          case Cr.NS_ERROR_FORBIDDEN_URI :
            sendAsyncMsg("error", { type: "forbiddenBlocked" });
            return;

          case Cr.NS_ERROR_OFFLINE :
            sendAsyncMsg("error", { type: "offline" });
            return;
          case Cr.NS_ERROR_MALFORMED_URI :
            sendAsyncMsg("error", { type: "malformedURI" });
            return;
          case Cr.NS_ERROR_REDIRECT_LOOP :
            sendAsyncMsg("error", { type: "redirectLoop" });
            return;
          case Cr.NS_ERROR_UNKNOWN_SOCKET_TYPE :
            sendAsyncMsg("error", { type: "unknownSocketType" });
            return;
          case Cr.NS_ERROR_NET_RESET :
            sendAsyncMsg("error", { type: "netReset" });
            return;
          case Cr.NS_ERROR_DOCUMENT_NOT_CACHED :
            sendAsyncMsg("error", { type: "notCached" });
            return;
          case Cr.NS_ERROR_DOCUMENT_IS_PRINTMODE :
            sendAsyncMsg("error", { type: "isprinting" });
            return;
          case Cr.NS_ERROR_PORT_ACCESS_NOT_ALLOWED :
            sendAsyncMsg("error", { type: "deniedPortAccess" });
            return;
          case Cr.NS_ERROR_UNKNOWN_PROXY_HOST :
            sendAsyncMsg("error", { type: "proxyResolveFailure" });
            return;
          case Cr.NS_ERROR_PROXY_CONNECTION_REFUSED :
            sendAsyncMsg("error", { type: "proxyConnectFailure" });
            return;
          case Cr.NS_ERROR_INVALID_CONTENT_ENCODING :
            sendAsyncMsg("error", { type: "contentEncodingFailure" });
            return;
          case Cr.NS_ERROR_REMOTE_XUL :
            sendAsyncMsg("error", { type: "remoteXUL" });
            return;
          case Cr.NS_ERROR_UNSAFE_CONTENT_TYPE :
            sendAsyncMsg("error", { type: "unsafeContentType" });
            return;
          case Cr.NS_ERROR_CORRUPTED_CONTENT :
            sendAsyncMsg("error", { type: "corruptedContentErrorv2" });
            return;
          case Cr.NS_ERROR_BLOCKED_BY_POLICY :
            sendAsyncMsg("error", { type: "blockedByPolicy" });
            return;

          default:
            // getErrorClass() will throw if the error code passed in is not a NSS
            // error code.
            try {
              let nssErrorsService = Cc["@mozilla.org/nss_errors_service;1"]
                                       .getService(Ci.nsINSSErrorsService);
              if (nssErrorsService.getErrorClass(status)
                    == Ci.nsINSSErrorsService.ERROR_CLASS_BAD_CERT) {
                // XXX Is there a point firing the event if the error page is not
                // certerror? If yes, maybe we should add a property to the
                // event to to indicate whether there is a custom page. That would
                // let the embedder have more control over the desired behavior.
                let errorPage = Services.prefs.getCharPref(CERTIFICATE_ERROR_PAGE_PREF, "");

                if (errorPage == "certerror") {
                  sendAsyncMsg("error", { type: "certerror" });
                  return;
                }
              }
            } catch (e) {}

            sendAsyncMsg("error", { type: "other" });
        }
      }
    },

    onSecurityChange(webProgress, request, state) {
      if (webProgress != docShell) {
        return;
      }

      var securityStateDesc;
      if (state & Ci.nsIWebProgressListener.STATE_IS_SECURE) {
        securityStateDesc = "secure";
      } else if (state & Ci.nsIWebProgressListener.STATE_IS_BROKEN) {
        securityStateDesc = "broken";
      } else if (state & Ci.nsIWebProgressListener.STATE_IS_INSECURE) {
        securityStateDesc = "insecure";
      } else {
        debug("Unexpected securitychange state!");
        securityStateDesc = "???";
      }

      var mixedStateDesc;
      if (state & Ci.nsIWebProgressListener.STATE_BLOCKED_MIXED_ACTIVE_CONTENT) {
        mixedStateDesc = "blocked_mixed_active_content";
      } else if (state & Ci.nsIWebProgressListener.STATE_LOADED_MIXED_ACTIVE_CONTENT) {
        // Note that STATE_LOADED_MIXED_ACTIVE_CONTENT implies STATE_IS_BROKEN
        mixedStateDesc = "loaded_mixed_active_content";
      }

      var isEV = !!(state & Ci.nsIWebProgressListener.STATE_IDENTITY_EV_TOPLEVEL);
      var isMixedContent = !!(state &
        (Ci.nsIWebProgressListener.STATE_BLOCKED_MIXED_ACTIVE_CONTENT |
        Ci.nsIWebProgressListener.STATE_LOADED_MIXED_ACTIVE_CONTENT));

      sendAsyncMsg("securitychange", {
        state: securityStateDesc,
        mixedState: mixedStateDesc,
        extendedValidation: isEV,
        mixedContent: isMixedContent,
      });
    },
  },

  // Expose the message manager for WebApps and others.
  _messageManagerPublic: {
    sendAsyncMessage: global.sendAsyncMessage.bind(global),
    sendSyncMessage: global.sendSyncMessage.bind(global),
    addMessageListener: global.addMessageListener.bind(global),
    removeMessageListener: global.removeMessageListener.bind(global),
  },

  get messageManager() {
    return this._messageManagerPublic;
  },
};

var api = null;
if ("DoPreloadPostfork" in this && typeof this.DoPreloadPostfork === "function") {
  // If we are preloaded, instantiate BrowserElementChild after a content
  // process is forked.
  this.DoPreloadPostfork(function() {
    api = new BrowserElementChild();
  });
} else {
  api = new BrowserElementChild();
}
