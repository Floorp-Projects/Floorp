/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

dump("######################## BrowserElementChildPreload.js loaded\n");

var BrowserElementIsReady = false;

let { classes: Cc, interfaces: Ci, results: Cr, utils: Cu }  = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/BrowserElementPromptService.jsm");

// Event whitelisted for bubbling.
let whitelistedEvents = [
  Ci.nsIDOMKeyEvent.DOM_VK_ESCAPE,   // Back button.
  Ci.nsIDOMKeyEvent.DOM_VK_SLEEP,    // Power button.
  Ci.nsIDOMKeyEvent.DOM_VK_CONTEXT_MENU,
  Ci.nsIDOMKeyEvent.DOM_VK_F5,       // Search button.
  Ci.nsIDOMKeyEvent.DOM_VK_PAGE_UP,  // Volume up.
  Ci.nsIDOMKeyEvent.DOM_VK_PAGE_DOWN // Volume down.
];

function debug(msg) {
  //dump("BrowserElementChildPreload - " + msg + "\n");
}

function sendAsyncMsg(msg, data) {
  // Ensure that we don't send any messages before BrowserElementChild.js
  // finishes loading.
  if (!BrowserElementIsReady)
    return;

  if (!data) {
    data = { };
  }

  data.msg_name = msg;
  sendAsyncMessage('browser-element-api:call', data);
}

function sendSyncMsg(msg, data) {
  // Ensure that we don't send any messages before BrowserElementChild.js
  // finishes loading.
  if (!BrowserElementIsReady)
    return;

  if (!data) {
    data = { };
  }

  data.msg_name = msg;
  return sendSyncMessage('browser-element-api:call', data);
}

let CERTIFICATE_ERROR_PAGE_PREF = 'security.alternate_certificate_error_page';

let NS_ERROR_MODULE_BASE_OFFSET = 0x45;
let NS_ERROR_MODULE_SECURITY= 21;
function NS_ERROR_GET_MODULE(err) {
  return ((((err) >> 16) - NS_ERROR_MODULE_BASE_OFFSET) & 0x1fff) 
}

function NS_ERROR_GET_CODE(err) {
  return ((err) & 0xffff);
}

let SEC_ERROR_BASE = Ci.nsINSSErrorsService.NSS_SEC_ERROR_BASE;
let SEC_ERROR_UNKNOWN_ISSUER = (SEC_ERROR_BASE + 13);
let SEC_ERROR_CA_CERT_INVALID =   (SEC_ERROR_BASE + 36);
let SEC_ERROR_UNTRUSTED_ISSUER = (SEC_ERROR_BASE + 20);
let SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE = (SEC_ERROR_BASE + 30);
let SEC_ERROR_UNTRUSTED_CERT = (SEC_ERROR_BASE + 21);
let SEC_ERROR_INADEQUATE_KEY_USAGE = (SEC_ERROR_BASE + 90);
let SEC_ERROR_EXPIRED_CERTIFICATE = (SEC_ERROR_BASE + 11);
let SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED = (SEC_ERROR_BASE + 176);

let SSL_ERROR_BASE = Ci.nsINSSErrorsService.NSS_SSL_ERROR_BASE;
let SSL_ERROR_BAD_CERT_DOMAIN = (SSL_ERROR_BASE + 12);

function getErrorClass(errorCode) {
  let NSPRCode = -1 * NS_ERROR_GET_CODE(errorCode);
 
  switch (NSPRCode) {
    case SEC_ERROR_UNKNOWN_ISSUER:
    case SEC_ERROR_CA_CERT_INVALID:
    case SEC_ERROR_UNTRUSTED_ISSUER:
    case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE:
    case SEC_ERROR_UNTRUSTED_CERT:
    case SEC_ERROR_INADEQUATE_KEY_USAGE:
    case SSL_ERROR_BAD_CERT_DOMAIN:
    case SEC_ERROR_EXPIRED_CERTIFICATE:
    case SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED:
      return Ci.nsINSSErrorsService.ERROR_CLASS_BAD_CERT;
    default:
      return Ci.nsINSSErrorsService.ERROR_CLASS_SSL_PROTOCOL;
  }

  return null;
}

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

  // _forcedVisible corresponds to the visibility state our owner has set on us
  // (via iframe.setVisible).  ownerVisible corresponds to whether the docShell
  // whose window owns this element is visible.
  //
  // Our docShell is visible iff _forcedVisible and _ownerVisible are both
  // true.
  this._forcedVisible = true;
  this._ownerVisible = true;

  this._nextPaintHandler = null;

  this._init();
};

BrowserElementChild.prototype = {

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  _init: function() {
    debug("Starting up.");

    BrowserElementPromptService.mapWindowToBrowserElementChild(content, this);

    docShell.QueryInterface(Ci.nsIWebProgress)
            .addProgressListener(this._progressListener,
                                 Ci.nsIWebProgress.NOTIFY_LOCATION |
                                 Ci.nsIWebProgress.NOTIFY_SECURITY |
                                 Ci.nsIWebProgress.NOTIFY_STATE_WINDOW);

    docShell.QueryInterface(Ci.nsIWebNavigation)
            .sessionHistory = Cc["@mozilla.org/browser/shistory;1"]
                                .createInstance(Ci.nsISHistory);

    // This is necessary to get security web progress notifications.
    var securityUI = Cc['@mozilla.org/secure_browser_ui;1']
                       .createInstance(Ci.nsISecureBrowserUI);
    securityUI.init(content);

    // A cache of the menuitem dom objects keyed by the id we generate
    // and pass to the embedder
    this._ctxHandlers = {};
    // Counter of contextmenu events fired
    this._ctxCounter = 0;

    this._shuttingDown = false;

    addEventListener('DOMTitleChanged',
                     this._titleChangedHandler.bind(this),
                     /* useCapture = */ true,
                     /* wantsUntrusted = */ false);

    addEventListener('DOMLinkAdded',
                     this._iconChangedHandler.bind(this),
                     /* useCapture = */ true,
                     /* wantsUntrusted = */ false);

    // This listens to unload events from our message manager, but /not/ from
    // the |content| window.  That's because the window's unload event doesn't
    // bubble, and we're not using a capturing listener.  If we'd used
    // useCapture == true, we /would/ hear unload events from the window, which
    // is not what we want!
    addEventListener('unload',
                     this._unloadHandler.bind(this),
                     /* useCapture = */ false,
                     /* wantsUntrusted = */ false);

    // Registers a MozAfterPaint handler for the very first paint.
    this._addMozAfterPaintHandler(function () {
      let bgColor = content.getComputedStyle(content.document.body)
                           .getPropertyValue('background-color');
      sendAsyncMsg('firstpaint', {backgroundColor: bgColor});
    });

    let self = this;

    let mmCalls = {
      "purge-history": this._recvPurgeHistory,
      "get-screenshot": this._recvGetScreenshot,
      "set-visible": this._recvSetVisible,
      "get-visible": this._recvVisible,
      "send-mouse-event": this._recvSendMouseEvent,
      "send-touch-event": this._recvSendTouchEvent,
      "get-can-go-back": this._recvCanGoBack,
      "get-can-go-forward": this._recvCanGoForward,
      "go-back": this._recvGoBack,
      "go-forward": this._recvGoForward,
      "reload": this._recvReload,
      "stop": this._recvStop,
      "unblock-modal-prompt": this._recvStopWaiting,
      "fire-ctx-callback": this._recvFireCtxCallback,
      "owner-visibility-change": this._recvOwnerVisibilityChange,
      "exit-fullscreen": this._recvExitFullscreen.bind(this),
      "activate-next-paint-listener": this._activateNextPaintListener.bind(this),
      "deactivate-next-paint-listener": this._deactivateNextPaintListener.bind(this)
    }

    addMessageListener("browser-element-api:call", function(aMessage) {
      if (aMessage.data.msg_name in mmCalls) {
        return mmCalls[aMessage.data.msg_name].apply(self, arguments);
      }
    });

    let els = Cc["@mozilla.org/eventlistenerservice;1"]
                .getService(Ci.nsIEventListenerService);

    // We are using the system group for those events so if something in the
    // content called .stopPropagation() this will still be called.
    els.addSystemEventListener(global, 'keydown',
                               this._keyEventHandler.bind(this),
                               /* useCapture = */ true);
    els.addSystemEventListener(global, 'keypress',
                               this._keyEventHandler.bind(this),
                               /* useCapture = */ true);
    els.addSystemEventListener(global, 'keyup',
                               this._keyEventHandler.bind(this),
                               /* useCapture = */ true);
    els.addSystemEventListener(global, 'DOMWindowClose',
                               this._windowCloseHandler.bind(this),
                               /* useCapture = */ false);
    els.addSystemEventListener(global, 'DOMWindowCreated',
                               this._windowCreatedHandler.bind(this),
                               /* useCapture = */ true);
    els.addSystemEventListener(global, 'contextmenu',
                               this._contextmenuHandler.bind(this),
                               /* useCapture = */ false);
    els.addSystemEventListener(global, 'scroll',
                               this._scrollEventHandler.bind(this),
                               /* useCapture = */ false);

    Services.obs.addObserver(this,
                             "fullscreen-origin-change",
                             /* ownsWeak = */ true);

    Services.obs.addObserver(this,
                             'ask-parent-to-exit-fullscreen',
                             /* ownsWeak = */ true);

    Services.obs.addObserver(this,
                             'ask-parent-to-rollback-fullscreen',
                             /* ownsWeak = */ true);

    Services.obs.addObserver(this,
                             'xpcom-shutdown',
                             /* ownsWeak = */ true);
  },

  observe: function(subject, topic, data) {
    // Ignore notifications not about our document.
    if (subject != content.document)
      return;
    switch (topic) {
      case 'fullscreen-origin-change':
        sendAsyncMsg('fullscreen-origin-change', { _payload_: data });
        break;
      case 'ask-parent-to-exit-fullscreen':
        sendAsyncMsg('exit-fullscreen');
        break;
      case 'ask-parent-to-rollback-fullscreen':
        sendAsyncMsg('rollback-fullscreen');
        break;
      case 'xpcom-shutdown':
        this._shuttingDown = true;
        break;
    }
  },

  /**
   * Called when our TabChildGlobal starts to die.  This is not called when the
   * page inside |content| unloads.
   */
  _unloadHandler: function() {
    this._shuttingDown = true;
  },

  _tryGetInnerWindowID: function(win) {
    let utils = win.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIDOMWindowUtils);
    try {
      return utils.currentInnerWindowID;
    }
    catch(e) {
      return null;
    }
  },

  /**
   * Show a modal prompt.  Called by BrowserElementPromptService.
   */
  showModalPrompt: function(win, args) {
    let utils = win.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIDOMWindowUtils);

    args.windowID = { outer: utils.outerWindowID,
                      inner: this._tryGetInnerWindowID(win) };
    sendAsyncMsg('showmodalprompt', args);

    let returnValue = this._waitForResult(win);

    if (args.promptType == 'prompt' ||
        args.promptType == 'confirm' ||
        args.promptType == 'custom-prompt') {
      return returnValue;
    }
  },

  /**
   * Spin in a nested event loop until we receive a unblock-modal-prompt message for
   * this window.
   */
  _waitForResult: function(win) {
    debug("_waitForResult(" + win + ")");
    let utils = win.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIDOMWindowUtils);

    let outerWindowID = utils.outerWindowID;
    let innerWindowID = this._tryGetInnerWindowID(win);
    if (innerWindowID === null) {
      // I have no idea what waiting for a result means when there's no inner
      // window, so let's just bail.
      debug("_waitForResult: No inner window. Bailing.");
      return;
    }

    this._windowIDDict[outerWindowID] = Cu.getWeakReference(win);

    debug("Entering modal state (outerWindowID=" + outerWindowID + ", " +
                                "innerWindowID=" + innerWindowID + ")");

    // In theory, we're supposed to pass |modalStateWin| back to
    // leaveModalStateWithWindow.  But in practice, the window is always null,
    // because it's the window associated with this script context, which
    // doesn't have a window.  But we'll play along anyway in case this
    // changes.
    var modalStateWin = utils.enterModalStateWithWindow();

    // We'll decrement win.modalDepth when we receive a unblock-modal-prompt message
    // for the window.
    if (!win.modalDepth) {
      win.modalDepth = 0;
    }
    win.modalDepth++;
    let origModalDepth = win.modalDepth;

    let thread = Services.tm.currentThread;
    debug("Nested event loop - begin");
    while (win.modalDepth == origModalDepth && !this._shuttingDown) {
      // Bail out of the loop if the inner window changed; that means the
      // window navigated.  Bail out when we're shutting down because otherwise
      // we'll leak our window.
      if (this._tryGetInnerWindowID(win) !== innerWindowID) {
        debug("_waitForResult: Inner window ID changed " +
              "while in nested event loop.");
        break;
      }

      thread.processNextEvent(/* mayWait = */ true);
    }
    debug("Nested event loop - finish");

    // If we exited the loop because the inner window changed, then bail on the
    // modal prompt.
    if (innerWindowID !== this._tryGetInnerWindowID(win)) {
      throw Components.Exception("Modal state aborted by navigation",
                                 Cr.NS_ERROR_NOT_AVAILABLE);
    }

    let returnValue = win.modalReturnValue;
    delete win.modalReturnValue;

    if (!this._shuttingDown) {
      utils.leaveModalStateWithWindow(modalStateWin);
    }

    debug("Leaving modal state (outerID=" + outerWindowID + ", " +
                               "innerID=" + innerWindowID + ")");
    return returnValue;
  },

  _recvStopWaiting: function(msg) {
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
    delete this._windowIDDict[outerID];

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

  _recvExitFullscreen: function() {
    var utils = content.document.defaultView
                       .QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDOMWindowUtils);
    utils.exitFullscreen();
  },

  _titleChangedHandler: function(e) {
    debug("Got titlechanged: (" + e.target.title + ")");
    var win = e.target.defaultView;

    // Ignore titlechanges which don't come from the top-level
    // <iframe mozbrowser> window.
    if (win == content) {
      sendAsyncMsg('titlechange', { _payload_: e.target.title });
    }
    else {
      debug("Not top level!");
    }
  },

  _iconChangedHandler: function(e) {
    debug("Got iconchanged: (" + e.target.href + ")");
    var hasIcon = e.target.rel.split(' ').some(function(x) {
      return x.toLowerCase() === 'icon';
    });

    if (hasIcon) {
      var win = e.target.ownerDocument.defaultView;
      // Ignore iconchanges which don't come from the top-level
      // <iframe mozbrowser> window.
      if (win == content) {
        sendAsyncMsg('iconchange', { _payload_: e.target.href });
      }
      else {
        debug("Not top level!");
      }
    }
  },

  _addMozAfterPaintHandler: function(callback) {
    function onMozAfterPaint() {
      let uri = docShell.QueryInterface(Ci.nsIWebNavigation).currentURI;
      if (uri.spec != "about:blank") {
        debug("Got afterpaint event: " + uri.spec);
        removeEventListener('MozAfterPaint', onMozAfterPaint,
                            /* useCapture = */ true);
        callback();
      }
    }

    addEventListener('MozAfterPaint', onMozAfterPaint, /* useCapture = */ true);
    return onMozAfterPaint;
  },

  _removeMozAfterPaintHandler: function(listener) {
    removeEventListener('MozAfterPaint', listener,
                        /* useCapture = */ true);
  },

  _activateNextPaintListener: function(e) {
    if (!this._nextPaintHandler) {
      this._nextPaintHandler = this._addMozAfterPaintHandler(function () {
        this._nextPaintHandler = null;
        sendAsyncMsg('nextpaint');
      }.bind(this));
    }
  },

  _deactivateNextPaintListener: function(e) {
    if (this._nextPaintHandler) {
      this._removeMozAfterPaintHandler(this._nextPaintHandler);
      this._nextPaintHandler = null;
    }
  },

  _windowCloseHandler: function(e) {
    let win = e.target;
    if (win != content || e.defaultPrevented) {
      return;
    }

    debug("Closing window " + win);
    sendAsyncMsg('close');

    // Inform the window implementation that we handled this close ourselves.
    e.preventDefault();
  },

  _windowCreatedHandler: function(e) {
    let targetDocShell = e.target.defaultView
          .QueryInterface(Ci.nsIInterfaceRequestor)
          .getInterface(Ci.nsIWebNavigation);
    if (targetDocShell != docShell) {
      return;
    }

    let uri = docShell.QueryInterface(Ci.nsIWebNavigation).currentURI;
    debug("Window created: " + uri.spec);
    if (uri.spec != "about:blank") {
      this._addMozAfterPaintHandler(function () {
        sendAsyncMsg('documentfirstpaint');
      });
    }
  },

  _contextmenuHandler: function(e) {
    debug("Got contextmenu");

    if (e.defaultPrevented) {
      return;
    }

    this._ctxCounter++;
    this._ctxHandlers = {};

    var elem = e.target;
    var menuData = {systemTargets: [], contextmenu: null};
    var ctxMenuId = null;

    while (elem && elem.parentNode) {
      var ctxData = this._getSystemCtxMenuData(elem);
      if (ctxData) {
        menuData.systemTargets.push({
          nodeName: elem.nodeName,
          data: ctxData
        });
      }

      if (!ctxMenuId && 'hasAttribute' in elem && elem.hasAttribute('contextmenu')) {
        ctxMenuId = elem.getAttribute('contextmenu');
      }
      elem = elem.parentNode;
    }

    if (ctxMenuId) {
      var menu = e.target.ownerDocument.getElementById(ctxMenuId);
      if (menu) {
        menuData.contextmenu = this._buildMenuObj(menu, '');
      }
    }

    // The value returned by the contextmenu sync call is true iff the embedder
    // called preventDefault() on its contextmenu event.
    //
    // We call preventDefault() on our contextmenu event iff the embedder called
    // preventDefault() on /its/ contextmenu event.  This way, if the embedder
    // ignored the contextmenu event, TabChild will fire a click.
    if (sendSyncMsg('contextmenu', menuData)[0]) {
      e.preventDefault();
    } else {
      this._ctxHandlers = {};
    }
  },

  _getSystemCtxMenuData: function(elem) {
    if ((elem instanceof Ci.nsIDOMHTMLAnchorElement && elem.href) ||
        (elem instanceof Ci.nsIDOMHTMLAreaElement && elem.href)) {
      return elem.href;
    }
    if (elem instanceof Ci.nsIImageLoadingContent && elem.currentURI) {
      return elem.currentURI.spec;
    }
    if ((elem instanceof Ci.nsIDOMHTMLMediaElement) ||
        (elem instanceof Ci.nsIDOMHTMLImageElement)) {
      return elem.currentSrc || elem.src;
    }
    return false;
  },

  _scrollEventHandler: function(e) {
    let win = e.target.defaultView;
    if (win != content) {
      return;
    }

    debug("scroll event " + win);
    sendAsyncMsg("scroll", { top: win.scrollY, left: win.scrollX });
  },

  _recvPurgeHistory: function(data) {
    debug("Received purgeHistory message: (" + data.json.id + ")");

    let history = docShell.QueryInterface(Ci.nsIWebNavigation).sessionHistory;

    try {
      if (history && history.count) {
        history.PurgeHistory(history.count);
      }
    } catch(e) {}

    sendAsyncMsg('got-purge-history', { id: data.json.id, successRv: true });
  },

  _recvGetScreenshot: function(data) {
    debug("Received getScreenshot message: (" + data.json.id + ")");

    let self = this;
    let maxWidth = data.json.args.width;
    let maxHeight = data.json.args.height;
    let domRequestID = data.json.id;

    let takeScreenshotClosure = function() {
      self._takeScreenshot(maxWidth, maxHeight, domRequestID);
    };

    let maxDelayMS = 2000;
    try {
      maxDelayMS = Services.prefs.getIntPref('dom.browserElement.maxScreenshotDelayMS');
    }
    catch(e) {}

    // Try to wait for the event loop to go idle before we take the screenshot,
    // but once we've waited maxDelayMS milliseconds, go ahead and take it
    // anyway.
    Cc['@mozilla.org/message-loop;1'].getService(Ci.nsIMessageLoop).postIdleTask(
      takeScreenshotClosure, maxDelayMS);
  },

  /**
   * Actually take a screenshot and foward the result up to our parent, given
   * the desired maxWidth and maxHeight, and given the DOMRequest ID associated
   * with the request from the parent.
   */
  _takeScreenshot: function(maxWidth, maxHeight, domRequestID) {
    // You can think of the screenshotting algorithm as carrying out the
    // following steps:
    //
    // - Let scaleWidth be the factor by which we'd need to downscale the
    //   viewport so it would fit within maxWidth.  (If the viewport's width
    //   is less than maxWidth, let scaleWidth be 1.) Compute scaleHeight
    //   the same way.
    //
    // - Scale the viewport by max(scaleWidth, scaleHeight).  Now either the
    //   viewport's width is no larger than maxWidth, the viewport's height is
    //   no larger than maxHeight, or both.
    //
    // - Crop the viewport so its width is no larger than maxWidth and its
    //   height is no larger than maxHeight.
    //
    // - Return a screenshot of the page's viewport scaled and cropped per
    //   above.
    debug("Taking a screenshot: maxWidth=" + maxWidth +
          ", maxHeight=" + maxHeight +
          ", domRequestID=" + domRequestID + ".");

    let scaleWidth = Math.min(1, maxWidth / content.innerWidth);
    let scaleHeight = Math.min(1, maxHeight / content.innerHeight);

    let scale = Math.max(scaleWidth, scaleHeight);

    let canvasWidth = Math.min(maxWidth, Math.round(content.innerWidth * scale));
    let canvasHeight = Math.min(maxHeight, Math.round(content.innerHeight * scale));

    var canvas = content.document
      .createElementNS("http://www.w3.org/1999/xhtml", "canvas");
    canvas.mozOpaque = true;
    canvas.width = canvasWidth;
    canvas.height = canvasHeight;

    var ctx = canvas.getContext("2d");
    ctx.scale(scale, scale);
    ctx.drawWindow(content, 0, 0, content.innerWidth, content.innerHeight,
                   "rgb(255,255,255)");

    // Take a JPEG screenshot to hack around the fact that we can't specify
    // opaque PNG.  This requires us to unpremultiply the alpha channel, which
    // is expensive on ARM processors because they lack a hardware integer
    // division instruction.
    canvas.toBlob(function(blob) {
      sendAsyncMsg('got-screenshot', {
        id: domRequestID,
        successRv: blob
      });
    }, 'image/jpeg');
  },

  _recvFireCtxCallback: function(data) {
    debug("Received fireCtxCallback message: (" + data.json.menuitem + ")");
    // We silently ignore if the embedder uses an incorrect id in the callback
    if (data.json.menuitem in this._ctxHandlers) {
      this._ctxHandlers[data.json.menuitem].click();
      this._ctxHandlers = {};
    } else {
      debug("Ignored invalid contextmenu invocation");
    }
  },

  _buildMenuObj: function(menu, idPrefix) {
    function maybeCopyAttribute(src, target, attribute) {
      if (src.getAttribute(attribute)) {
        target[attribute] = src.getAttribute(attribute);
      }
    }

    var menuObj = {type: 'menu', items: []};
    maybeCopyAttribute(menu, menuObj, 'label');

    for (var i = 0, child; child = menu.children[i++];) {
      if (child.nodeName === 'MENU') {
        menuObj.items.push(this._buildMenuObj(child, idPrefix + i + '_'));
      } else if (child.nodeName === 'MENUITEM') {
        var id = this._ctxCounter + '_' + idPrefix + i;
        var menuitem = {id: id, type: 'menuitem'};
        maybeCopyAttribute(child, menuitem, 'label');
        maybeCopyAttribute(child, menuitem, 'icon');
        this._ctxHandlers[id] = child;
        menuObj.items.push(menuitem);
      }
    }
    return menuObj;
  },

  _recvSetVisible: function(data) {
    debug("Received setVisible message: (" + data.json.visible + ")");
    if (this._forcedVisible == data.json.visible) {
      return;
    }

    this._forcedVisible = data.json.visible;
    this._updateVisibility();
  },

  _recvVisible: function(data) {
    sendAsyncMsg('got-visible', {
      id: data.json.id,
      successRv: docShell.isActive
    });
  },

  /**
   * Called when the window which contains this iframe becomes hidden or
   * visible.
   */
  _recvOwnerVisibilityChange: function(data) {
    debug("Received ownerVisibilityChange: (" + data.json.visible + ")");
    this._ownerVisible = data.json.visible;
    this._updateVisibility();
  },

  _updateVisibility: function() {
    var visible = this._forcedVisible && this._ownerVisible;
    if (docShell.isActive !== visible) {
      docShell.isActive = visible;
      sendAsyncMsg('visibilitychange', {visible: visible});
    }
  },

  _recvSendMouseEvent: function(data) {
    let json = data.json;
    let utils = content.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDOMWindowUtils);
    utils.sendMouseEvent(json.type, json.x, json.y, json.button,
                         json.clickCount, json.modifiers);
  },

  _recvSendTouchEvent: function(data) {
    let json = data.json;
    let utils = content.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDOMWindowUtils);
    utils.sendTouchEvent(json.type, json.identifiers, json.touchesX,
                         json.touchesY, json.radiisX, json.radiisY,
                         json.rotationAngles, json.forces, json.count,
                         json.modifiers);
  },

  _recvCanGoBack: function(data) {
    var webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
    sendAsyncMsg('got-can-go-back', {
      id: data.json.id,
      successRv: webNav.canGoBack
    });
  },

  _recvCanGoForward: function(data) {
    var webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
    sendAsyncMsg('got-can-go-forward', {
      id: data.json.id,
      successRv: webNav.canGoForward
    });
  },

  _recvGoBack: function(data) {
    try {
      docShell.QueryInterface(Ci.nsIWebNavigation).goBack();
    } catch(e) {
      // Silently swallow errors; these happen when we can't go back.
    }
  },

  _recvGoForward: function(data) {
    try {
      docShell.QueryInterface(Ci.nsIWebNavigation).goForward();
    } catch(e) {
      // Silently swallow errors; these happen when we can't go forward.
    }
  },

  _recvReload: function(data) {
    let webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
    let reloadFlags = data.json.hardReload ?
      webNav.LOAD_FLAGS_BYPASS_PROXY | webNav.LOAD_FLAGS_BYPASS_CACHE :
      webNav.LOAD_FLAGS_NONE;
    try {
      webNav.reload(reloadFlags);
    } catch(e) {
      // Silently swallow errors; these can happen if a used cancels reload
    }
  },

  _recvStop: function(data) {
    let webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
    webNav.stop(webNav.STOP_NETWORK);
  },

  _keyEventHandler: function(e) {
    if (whitelistedEvents.indexOf(e.keyCode) != -1 && !e.defaultPrevented) {
      sendAsyncMsg('keyevent', {
        type: e.type,
        keyCode: e.keyCode,
        charCode: e.charCode,
      });
    }
  },

  // The docShell keeps a weak reference to the progress listener, so we need
  // to keep a strong ref to it ourselves.
  _progressListener: {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                           Ci.nsISupportsWeakReference]),
    _seenLoadStart: false,

    onLocationChange: function(webProgress, request, location, flags) {
      // We get progress events from subshells here, which is kind of weird.
      if (webProgress != docShell) {
        return;
      }

      // Ignore locationchange events which occur before the first loadstart.
      // These are usually about:blank loads we don't care about.
      if (!this._seenLoadStart) {
        return;
      }

      // Remove password and wyciwyg from uri.
      location = Cc["@mozilla.org/docshell/urifixup;1"]
        .getService(Ci.nsIURIFixup).createExposableURI(location);

      sendAsyncMsg('locationchange', { _payload_: location.spec });
    },

    onStateChange: function(webProgress, request, stateFlags, status) {
      if (webProgress != docShell) {
        return;
      }

      if (stateFlags & Ci.nsIWebProgressListener.STATE_START) {
        this._seenLoadStart = true;
        sendAsyncMsg('loadstart');
      }

      if (stateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
        sendAsyncMsg('loadend');

        // Ignoring NS_BINDING_ABORTED, which is set when loading page is
        // stopped.
        if (status == Cr.NS_OK ||
            status == Cr.NS_BINDING_ABORTED) {
          return;
        }

        if (NS_ERROR_GET_MODULE(status) == NS_ERROR_MODULE_SECURITY && 
            getErrorClass(status) == Ci.nsINSSErrorsService.ERROR_CLASS_BAD_CERT) {

          // XXX Is there a point firing the event if the error page is not
          // certerror? If yes, maybe we should add a property to the
          // event to to indicate whether there is a custom page. That would
          // let the embedder have more control over the desired behavior.
          var errorPage = null;
          try {
            errorPage = Services.prefs.getCharPref(CERTIFICATE_ERROR_PAGE_PREF);
          } catch(e) {}

          if (errorPage == 'certerror') {
            sendAsyncMsg('error', { type: 'certerror' });
            return;
          }
        }

        // TODO See nsDocShell::DisplayLoadError for a list of all the error
        // codes (the status param) we should eventually handle here.
        sendAsyncMsg('error', { type: 'other' });
      }
    },

    onSecurityChange: function(webProgress, request, state) {
      if (webProgress != docShell) {
        return;
      }

      var stateDesc;
      if (state & Ci.nsIWebProgressListener.STATE_IS_SECURE) {
        stateDesc = 'secure';
      }
      else if (state & Ci.nsIWebProgressListener.STATE_IS_BROKEN) {
        stateDesc = 'broken';
      }
      else if (state & Ci.nsIWebProgressListener.STATE_IS_INSECURE) {
        stateDesc = 'insecure';
      }
      else {
        debug("Unexpected securitychange state!");
        stateDesc = '???';
      }

      // XXX Until bug 764496 is fixed, this will always return false.
      var isEV = !!(state & Ci.nsIWebProgressListener.STATE_IDENTITY_EV_TOPLEVEL);

      sendAsyncMsg('securitychange', { state: stateDesc, extendedValidation: isEV });
    },

    onStatusChange: function(webProgress, request, status, message) {},
    onProgressChange: function(webProgress, request, curSelfProgress,
                               maxSelfProgress, curTotalProgress, maxTotalProgress) {},
  },

  // Expose the message manager for WebApps and others.
  _messageManagerPublic: {
    sendAsyncMessage: global.sendAsyncMessage.bind(global),
    sendSyncMessage: global.sendSyncMessage.bind(global),
    addMessageListener: global.addMessageListener.bind(global),
    removeMessageListener: global.removeMessageListener.bind(global)
  },

  get messageManager() {
    return this._messageManagerPublic;
  }
};

var api = new BrowserElementChild();

