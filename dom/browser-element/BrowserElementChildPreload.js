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

let kLongestReturnedString = 128;

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

const OBSERVED_EVENTS = [
  'fullscreen-origin-change',
  'ask-parent-to-exit-fullscreen',
  'ask-parent-to-rollback-fullscreen',
  'xpcom-shutdown',
  'activity-done'
];

const COMMAND_MAP = {
  'cut': 'cmd_cut',
  'copy': 'cmd_copyAndCollapseToEnd',
  'paste': 'cmd_paste',
  'selectall': 'cmd_selectAll'
};

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

  this._isContentWindowCreated = false;
  this._pendingSetInputMethodActive = [];
  this._selectionStateChangedTarget = null;

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
                     this._linkAddedHandler.bind(this),
                     /* useCapture = */ true,
                     /* wantsUntrusted = */ false);

    addEventListener('MozScrolledAreaChanged',
                     this._mozScrollAreaChanged.bind(this),
                     /* useCapture = */ true,
                     /* wantsUntrusted = */ false);

    addEventListener('DOMMetaAdded',
                     this._metaChangedHandler.bind(this),
                     /* useCapture = */ true,
                     /* wantsUntrusted = */ false);

    addEventListener('DOMMetaChanged',
                     this._metaChangedHandler.bind(this),
                     /* useCapture = */ true,
                     /* wantsUntrusted = */ false);

    addEventListener('DOMMetaRemoved',
                     this._metaChangedHandler.bind(this),
                     /* useCapture = */ true,
                     /* wantsUntrusted = */ false);

    addEventListener('mozselectionstatechanged',
                     this._selectionStateChangedHandler.bind(this),
                     /* useCapture = */ true,
                     /* wantsUntrusted = */ false);

    addEventListener('scrollviewchange',
                     this._ScrollViewChangeHandler.bind(this),
                     /* useCapture = */ true,
                     /* wantsUntrusted = */ false);

    addEventListener('click',
                     this._ClickHandler.bind(this),
                     /* useCapture = */ false,
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
      sendAsyncMsg('firstpaint');
    });

    let self = this;

    let mmCalls = {
      "purge-history": this._recvPurgeHistory,
      "get-screenshot": this._recvGetScreenshot,
      "get-contentdimensions": this._recvGetContentDimensions,
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
      "zoom": this._recvZoom,
      "unblock-modal-prompt": this._recvStopWaiting,
      "fire-ctx-callback": this._recvFireCtxCallback,
      "owner-visibility-change": this._recvOwnerVisibilityChange,
      "exit-fullscreen": this._recvExitFullscreen.bind(this),
      "activate-next-paint-listener": this._activateNextPaintListener.bind(this),
      "set-input-method-active": this._recvSetInputMethodActive.bind(this),
      "deactivate-next-paint-listener": this._deactivateNextPaintListener.bind(this),
      "do-command": this._recvDoCommand
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
    els.addSystemEventListener(global, 'DOMWindowClose',
                               this._windowCloseHandler.bind(this),
                               /* useCapture = */ false);
    els.addSystemEventListener(global, 'DOMWindowCreated',
                               this._windowCreatedHandler.bind(this),
                               /* useCapture = */ true);
    els.addSystemEventListener(global, 'DOMWindowResize',
                               this._windowResizeHandler.bind(this),
                               /* useCapture = */ false);
    els.addSystemEventListener(global, 'contextmenu',
                               this._contextmenuHandler.bind(this),
                               /* useCapture = */ false);
    els.addSystemEventListener(global, 'scroll',
                               this._scrollEventHandler.bind(this),
                               /* useCapture = */ false);

    OBSERVED_EVENTS.forEach((aTopic) => {
      Services.obs.addObserver(this, aTopic, false);
    });
  },

  observe: function(subject, topic, data) {
    // Ignore notifications not about our document.  (Note that |content| /can/
    // be null; see bug 874900.)
    if (topic !== 'activity-done' && (!content || subject != content.document))
      return;
    if (topic == 'activity-done' && docShell !== subject)
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
      case 'activity-done':
        sendAsyncMsg('activitydone', { success: (data == 'activity-success') });
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
    OBSERVED_EVENTS.forEach((aTopic) => {
      Services.obs.removeObserver(this, aTopic);
    });
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

    Services.obs.notifyObservers(null, 'BEC:ShownModalPrompt', null);

    if (args.promptType == 'prompt' ||
        args.promptType == 'confirm' ||
        args.promptType == 'custom-prompt') {
      return returnValue;
    }
  },

  _isCommandEnabled: function(cmd) {
    let command = COMMAND_MAP[cmd];
    if (!command) {
      return false;
    }

    return docShell.isCommandEnabled(command);
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

    utils.enterModalState();

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

  _maybeCopyAttribute: function(src, target, attribute) {
    if (src.getAttribute(attribute)) {
      target[attribute] = src.getAttribute(attribute);
    }
  },

  _iconChangedHandler: function(e) {
    debug('Got iconchanged: (' + e.target.href + ')');
    let icon = { href: e.target.href };
    this._maybeCopyAttribute(e.target, icon, 'sizes');
    this._maybeCopyAttribute(e.target, icon, 'rel');
    sendAsyncMsg('iconchange', icon);
  },

  _openSearchHandler: function(e) {
    debug('Got opensearch: (' + e.target.href + ')');

    if (e.target.type !== "application/opensearchdescription+xml") {
      return;
    }

    sendAsyncMsg('opensearch', { title: e.target.title,
                                 href: e.target.href });

  },

  _manifestChangedHandler: function(e) {
    debug('Got manifestchanged: (' + e.target.href + ')');
    let manifest = { href: e.target.href };
    sendAsyncMsg('manifestchange', manifest);

  },

  // Processes the "rel" field in <link> tags and forward to specific handlers.
  _linkAddedHandler: function(e) {
    let win = e.target.ownerDocument.defaultView;
    // Ignore links which don't come from the top-level
    // <iframe mozbrowser> window.
    if (win != content) {
      debug('Not top level!');
      return;
    }

    let handlers = {
      'icon': this._iconChangedHandler.bind(this),
      'apple-touch-icon': this._iconChangedHandler.bind(this),
      'search': this._openSearchHandler,
      'manifest': this._manifestChangedHandler
    };

    debug('Got linkAdded: (' + e.target.href + ') ' + e.target.rel);
    e.target.rel.split(' ').forEach(function(x) {
      let token = x.toLowerCase();
      if (handlers[token]) {
        handlers[token](e);
      }
    }, this);
  },

  _metaChangedHandler: function(e) {
    let win = e.target.ownerDocument.defaultView;
    // Ignore metas which don't come from the top-level
    // <iframe mozbrowser> window.
    if (win != content) {
      debug('Not top level!');
      return;
    }

    if (!e.target.name) {
      return;
    }

    debug('Got metaChanged: (' + e.target.name + ') ' + e.target.content);

    let handlers = {
      'theme-color': this._themeColorChangedHandler,
      'application-name': this._applicationNameChangedHandler
    };

    let handler = handlers[e.target.name];
    if (handler) {
      handler(e.type, e.target);
    }
  },

  _applicationNameChangedHandler: function(eventType, target) {
    if (eventType !== 'DOMMetaAdded') {
      // Bug 1037448 - Decide what to do when <meta name="application-name">
      // changes
      return;
    }

    let meta = { name: 'application-name',
                 content: target.content };

    let lang;
    let elm;

    for (elm = target;
         !lang && elm && elm.nodeType == target.ELEMENT_NODE;
         elm = elm.parentNode) {
      if (elm.hasAttribute('lang')) {
        lang = elm.getAttribute('lang');
        continue;
      }

      if (elm.hasAttributeNS('http://www.w3.org/XML/1998/namespace', 'lang')) {
        lang = elm.getAttributeNS('http://www.w3.org/XML/1998/namespace', 'lang');
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

    sendAsyncMsg('metachange', meta);
  },

  _ScrollViewChangeHandler: function(e) {
    e.stopPropagation();
    let detail = {
      state: e.state,
    };
    sendAsyncMsg('scrollviewchange', detail);
  },

  _ClickHandler: function(e) {

    let isHTMLLink = node =>
      ((node instanceof Ci.nsIDOMHTMLAnchorElement && node.href) ||
       (node instanceof Ci.nsIDOMHTMLAreaElement && node.href) ||
        node instanceof Ci.nsIDOMHTMLLinkElement);

    // Open in a new tab if middle click or ctrl/cmd-click,
    // and e.target is a link or inside a link.
    if ((Services.appinfo.OS == 'Darwin' && e.metaKey) ||
        (Services.appinfo.OS != 'Darwin' && e.ctrlKey) ||
         e.button == 1) {

      let node = e.target;
      while (node && !isHTMLLink(node)) {
        node = node.parentNode;
      }

      if (node) {
        sendAsyncMsg('opentab', {url: node.href});
      }
    }
  },

  _selectionStateChangedHandler: function(e) {
    e.stopPropagation();

    if (!this._isContentWindowCreated) {
      return;
    }

    let boundingClientRect = e.boundingClientRect;

    let isCollapsed = (e.selectedText.length == 0);
    let isMouseUp = (e.states.indexOf('mouseup') == 0);
    let canPaste = this._isCommandEnabled("paste");

    if (this._selectionStateChangedTarget != e.target) {
      // SelectionStateChanged events with the following states are not
      // necessary to trigger the text dialog, bypass these events
      // by default.
      //
      if(e.states.length == 0 ||
         e.states.indexOf('drag') == 0 ||
         e.states.indexOf('keypress') == 0 ||
         e.states.indexOf('mousedown') == 0) {
        return;
      }

      // The collapsed SelectionStateChanged event is unnecessary to dispatch,
      // bypass this event by default, but here comes some exceptional cases
      if (isCollapsed) {
        if (isMouseUp && canPaste) {
          // Always dispatch to support shortcut mode which can paste previous
          // copied content easily
        } else if (e.states.indexOf('blur') == 0) {
          // Always dispatch to notify the blur for the focus content
        } else if (e.states.indexOf('taponcaret') == 0) {
          // Always dispatch to notify the caret be touched
        } else {
          return;
        }
      }
    }

    // If we select something and selection range is visible, we cache current
    // event's target to selectionStateChangedTarget.
    // And dispatch the next SelectionStateChagne event if target is matched, so
    // that the parent side can hide the text dialog.
    // We clear selectionStateChangedTarget if selection carets are invisible.
    if (e.visible && !isCollapsed) {
      this._selectionStateChangedTarget = e.target;
    } else if (canPaste && isCollapsed) {
      this._selectionStateChangedTarget = e.target;
    } else {
      this._selectionStateChangedTarget = null;
    }

    let zoomFactor = content.screen.width / content.innerWidth;

    let detail = {
      rect: {
        width: boundingClientRect ? boundingClientRect.width : 0,
        height: boundingClientRect ? boundingClientRect.height : 0,
        top: boundingClientRect ? boundingClientRect.top : 0,
        bottom: boundingClientRect ? boundingClientRect.bottom : 0,
        left: boundingClientRect ? boundingClientRect.left : 0,
        right: boundingClientRect ? boundingClientRect.right : 0,
      },
      commands: {
        canSelectAll: this._isCommandEnabled("selectall"),
        canCut: this._isCommandEnabled("cut"),
        canCopy: this._isCommandEnabled("copy"),
        canPaste: this._isCommandEnabled("paste"),
      },
      zoomFactor: zoomFactor,
      states: e.states,
      isCollapsed: (e.selectedText.length == 0),
      visible: e.visible,
    };

    // Get correct geometry information if we have nested iframe.
    let currentWindow = e.target.defaultView;
    while (currentWindow.realFrameElement) {
      let currentRect = currentWindow.realFrameElement.getBoundingClientRect();
      detail.rect.top += currentRect.top;
      detail.rect.bottom += currentRect.top;
      detail.rect.left += currentRect.left;
      detail.rect.right += currentRect.left;
      currentWindow = currentWindow.realFrameElement.ownerDocument.defaultView;
    }

    sendAsyncMsg('selectionstatechanged', detail);
  },

  _themeColorChangedHandler: function(eventType, target) {
    let meta = {
      name: 'theme-color',
      content: target.content,
      type: eventType.replace('DOMMeta', '').toLowerCase()
    };
    sendAsyncMsg('metachange', meta);
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
      this._isContentWindowCreated = true;
      // Handle pending SetInputMethodActive request.
      while (this._pendingSetInputMethodActive.length > 0) {
        this._recvSetInputMethodActive(this._pendingSetInputMethodActive.shift());
      }
    }
  },

  _windowResizeHandler: function(e) {
    let win = e.target;
    if (win != content || e.defaultPrevented) {
      return;
    }

    debug("resizing window " + win);
    sendAsyncMsg('resize', { width: e.detail.width, height: e.detail.height });

    // Inform the window implementation that we handled this resize ourselves.
    e.preventDefault();
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

    // The value returned by the contextmenu sync call is true if the embedder
    // called preventDefault() on its contextmenu event.
    //
    // We call preventDefault() on our contextmenu event if the embedder called
    // preventDefault() on /its/ contextmenu event.  This way, if the embedder
    // ignored the contextmenu event, TabChild will fire a click.
    if (sendSyncMsg('contextmenu', menuData)[0]) {
      e.preventDefault();
    } else {
      this._ctxHandlers = {};
    }
  },

  _getSystemCtxMenuData: function(elem) {
    let documentURI = 
      docShell.QueryInterface(Ci.nsIWebNavigation).currentURI.spec;
    if ((elem instanceof Ci.nsIDOMHTMLAnchorElement && elem.href) ||
        (elem instanceof Ci.nsIDOMHTMLAreaElement && elem.href)) {
      return {uri: elem.href,
              documentURI: documentURI,
              text: elem.textContent.substring(0, kLongestReturnedString)};
    }
    if (elem instanceof Ci.nsIImageLoadingContent && elem.currentURI) {
      return {uri: elem.currentURI.spec, documentURI: documentURI};
    }
    if (elem instanceof Ci.nsIDOMHTMLImageElement) {
      return {uri: elem.src, documentURI: documentURI};
    }
    if (elem instanceof Ci.nsIDOMHTMLMediaElement) {
      let hasVideo = !(elem.readyState >= elem.HAVE_METADATA &&
                       (elem.videoWidth == 0 || elem.videoHeight == 0));
      return {uri: elem.currentSrc || elem.src,
              hasVideo: hasVideo,
              documentURI: documentURI};
    }
    if (elem instanceof Ci.nsIDOMHTMLInputElement &&
        elem.hasAttribute("name")) {
      // For input elements, we look for a parent <form> and if there is
      // one we return the form's method and action uri.
      let parent = elem.parentNode;
      while (parent) {
        if (parent instanceof Ci.nsIDOMHTMLFormElement &&
            parent.hasAttribute("action")) {
          let actionHref = docShell.QueryInterface(Ci.nsIWebNavigation)
                                   .currentURI
                                   .resolve(parent.getAttribute("action"));
          let method = parent.hasAttribute("method")
            ? parent.getAttribute("method").toLowerCase()
            : "get";
          return {
            documentURI: documentURI,
            action: actionHref,
            method: method,
            name: elem.getAttribute("name"),
          }
        }
        parent = parent.parentNode;
      }
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
    let mimeType = data.json.args.mimeType;
    let domRequestID = data.json.id;

    let takeScreenshotClosure = function() {
      self._takeScreenshot(maxWidth, maxHeight, mimeType, domRequestID);
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

  _recvGetContentDimensions: function(data) {
    debug("Received getContentDimensions message: (" + data.json.id + ")");
    sendAsyncMsg('got-contentdimensions', {
      id: data.json.id,
      successRv: this._getContentDimensions()
    });
  },

  _mozScrollAreaChanged: function(e) {
    let dimensions = this._getContentDimensions();
    sendAsyncMsg('scrollareachanged', {
      width: dimensions.width,
      height: dimensions.height
    });
  },

  _getContentDimensions: function() {
    return {
      width: content.document.body.scrollWidth,
      height: content.document.body.scrollHeight
    }
  },

  /**
   * Actually take a screenshot and foward the result up to our parent, given
   * the desired maxWidth and maxHeight (in CSS pixels), and given the
   * DOMRequest ID associated with the request from the parent.
   */
  _takeScreenshot: function(maxWidth, maxHeight, mimeType, domRequestID) {
    // You can think of the screenshotting algorithm as carrying out the
    // following steps:
    //
    // - Calculate maxWidth, maxHeight, and viewport's width and height in the
    //   dimension of device pixels by multiply the numbers with
    //   window.devicePixelRatio.
    //
    // - Let scaleWidth be the factor by which we'd need to downscale the
    //   viewport pixel width so it would fit within maxPixelWidth.
    //   (If the viewport's pixel width is less than maxPixelWidth, let
    //   scaleWidth be 1.) Compute scaleHeight the same way.
    //
    // - Scale the viewport by max(scaleWidth, scaleHeight).  Now either the
    //   viewport's width is no larger than maxWidth, the viewport's height is
    //   no larger than maxHeight, or both.
    //
    // - Crop the viewport so its width is no larger than maxWidth and its
    //   height is no larger than maxHeight.
    //
    // - Set mozOpaque to true and background color to solid white
    //   if we are taking a JPEG screenshot, keep transparent if otherwise.
    //
    // - Return a screenshot of the page's viewport scaled and cropped per
    //   above.
    debug("Taking a screenshot: maxWidth=" + maxWidth +
          ", maxHeight=" + maxHeight +
          ", mimeType=" + mimeType +
          ", domRequestID=" + domRequestID + ".");

    if (!content) {
      // If content is not loaded yet, bail out since even sendAsyncMessage
      // fails...
      debug("No content yet!");
      return;
    }

    let devicePixelRatio = content.devicePixelRatio;

    let maxPixelWidth = Math.round(maxWidth * devicePixelRatio);
    let maxPixelHeight = Math.round(maxHeight * devicePixelRatio);

    let contentPixelWidth = content.innerWidth * devicePixelRatio;
    let contentPixelHeight = content.innerHeight * devicePixelRatio;

    let scaleWidth = Math.min(1, maxPixelWidth / contentPixelWidth);
    let scaleHeight = Math.min(1, maxPixelHeight / contentPixelHeight);

    let scale = Math.max(scaleWidth, scaleHeight);

    let canvasWidth =
      Math.min(maxPixelWidth, Math.round(contentPixelWidth * scale));
    let canvasHeight =
      Math.min(maxPixelHeight, Math.round(contentPixelHeight * scale));

    let transparent = (mimeType !== 'image/jpeg');

    var canvas = content.document
      .createElementNS("http://www.w3.org/1999/xhtml", "canvas");
    if (!transparent)
      canvas.mozOpaque = true;
    canvas.width = canvasWidth;
    canvas.height = canvasHeight;

    let ctx = canvas.getContext("2d", { willReadFrequently: true });
    ctx.scale(scale * devicePixelRatio, scale * devicePixelRatio);

    let flags = ctx.DRAWWINDOW_DRAW_VIEW |
                ctx.DRAWWINDOW_USE_WIDGET_LAYERS |
                ctx.DRAWWINDOW_DO_NOT_FLUSH |
                ctx.DRAWWINDOW_ASYNC_DECODE_IMAGES;
    ctx.drawWindow(content, 0, 0, content.innerWidth, content.innerHeight,
                   transparent ? "rgba(255,255,255,0)" : "rgb(255,255,255)",
                   flags);

    // Take a JPEG screenshot by default instead of PNG with alpha channel.
    // This requires us to unpremultiply the alpha channel, which
    // is expensive on ARM processors because they lack a hardware integer
    // division instruction.
    canvas.toBlob(function(blob) {
      sendAsyncMsg('got-screenshot', {
        id: domRequestID,
        successRv: blob
      });
    }, mimeType);
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
    var menuObj = {type: 'menu', items: []};
    this._maybeCopyAttribute(menu, menuObj, 'label');

    for (var i = 0, child; child = menu.children[i++];) {
      if (child.nodeName === 'MENU') {
        menuObj.items.push(this._buildMenuObj(child, idPrefix + i + '_'));
      } else if (child.nodeName === 'MENUITEM') {
        var id = this._ctxCounter + '_' + idPrefix + i;
        var menuitem = {id: id, type: 'menuitem'};
        this._maybeCopyAttribute(child, menuitem, 'label');
        this._maybeCopyAttribute(child, menuitem, 'icon');
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
    if (docShell && docShell.isActive !== visible) {
      docShell.isActive = visible;
      sendAsyncMsg('visibilitychange', {visible: visible});
    }
  },

  _recvSendMouseEvent: function(data) {
    let json = data.json;
    let utils = content.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDOMWindowUtils);
    utils.sendMouseEventToWindow(json.type, json.x, json.y, json.button,
                                 json.clickCount, json.modifiers);
  },

  _recvSendTouchEvent: function(data) {
    let json = data.json;
    let utils = content.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDOMWindowUtils);
    utils.sendTouchEventToWindow(json.type, json.identifiers, json.touchesX,
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

  _recvZoom: function(data) {
    docShell.contentViewer.fullZoom = data.json.zoom;
  },

  _recvDoCommand: function(data) {
    if (this._isCommandEnabled(data.json.command)) {
      this._selectionStateChangedTarget = null;
      docShell.doCommand(COMMAND_MAP[data.json.command]);
    }
  },

  _recvSetInputMethodActive: function(data) {
    let msgData = { id: data.json.id };
    if (!this._isContentWindowCreated) {
      if (data.json.args.isActive) {
        // To activate the input method, we should wait before the content
        // window is ready.
        this._pendingSetInputMethodActive.push(data);
        return;
      }
      msgData.successRv = null;
      sendAsyncMsg('got-set-input-method-active', msgData);
      return;
    }
    // Unwrap to access webpage content.
    let nav = XPCNativeWrapper.unwrap(content.document.defaultView.navigator);
    if (nav.mozInputMethod) {
      // Wrap to access the chrome-only attribute setActive.
      new XPCNativeWrapper(nav.mozInputMethod).setActive(data.json.args.isActive);
      msgData.successRv = null;
    } else {
      msgData.errorMsg = 'Cannot access mozInputMethod.';
    }
    sendAsyncMsg('got-set-input-method-active', msgData);
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
        let bgColor = 'transparent';
        try {
          bgColor = content.getComputedStyle(content.document.body)
                           .getPropertyValue('background-color');
        } catch (e) {}
        sendAsyncMsg('loadend', {backgroundColor: bgColor});

        switch (status) {
          case Cr.NS_OK :
          case Cr.NS_BINDING_ABORTED :
            // Ignoring NS_BINDING_ABORTED, which is set when loading page is
            // stopped.
            return;

          // TODO See nsDocShell::DisplayLoadError to see what extra
          // information we should be annotating this first block of errors
          // with. Bug 1107091.
          case Cr.NS_ERROR_UNKNOWN_PROTOCOL :
            sendAsyncMsg('error', { type: 'unknownProtocolFound' });
            return;
          case Cr.NS_ERROR_FILE_NOT_FOUND :
            sendAsyncMsg('error', { type: 'fileNotFound' });
            return;
          case Cr.NS_ERROR_UNKNOWN_HOST :
            sendAsyncMsg('error', { type: 'dnsNotFound' });
            return;
          case Cr.NS_ERROR_CONNECTION_REFUSED :
            sendAsyncMsg('error', { type: 'connectionFailure' });
            return;
          case Cr.NS_ERROR_NET_INTERRUPT :
            sendAsyncMsg('error', { type: 'netInterrupt' });
            return;
          case Cr.NS_ERROR_NET_TIMEOUT :
            sendAsyncMsg('error', { type: 'netTimeout' });
            return;
          case Cr.NS_ERROR_CSP_FRAME_ANCESTOR_VIOLATION :
            sendAsyncMsg('error', { type: 'cspBlocked' });
            return;
          case Cr.NS_ERROR_PHISHING_URI :
            sendAsyncMsg('error', { type: 'phishingBlocked' });
            return;
          case Cr.NS_ERROR_MALWARE_URI :
            sendAsyncMsg('error', { type: 'malwareBlocked' });
            return;
          case Cr.NS_ERROR_UNWANTED_URI :
            sendAsyncMsg('error', { type: 'unwantedBlocked' });
            return;

          case Cr.NS_ERROR_OFFLINE :
            sendAsyncMsg('error', { type: 'offline' });
            return;
          case Cr.NS_ERROR_MALFORMED_URI :
            sendAsyncMsg('error', { type: 'malformedURI' });
            return;
          case Cr.NS_ERROR_REDIRECT_LOOP :
            sendAsyncMsg('error', { type: 'redirectLoop' });
            return;
          case Cr.NS_ERROR_UNKNOWN_SOCKET_TYPE :
            sendAsyncMsg('error', { type: 'unknownSocketType' });
            return;
          case Cr.NS_ERROR_NET_RESET :
            sendAsyncMsg('error', { type: 'netReset' });
            return;
          case Cr.NS_ERROR_DOCUMENT_NOT_CACHED :
            sendAsyncMsg('error', { type: 'notCached' });
            return;
          case Cr.NS_ERROR_DOCUMENT_IS_PRINTMODE :
            sendAsyncMsg('error', { type: 'isprinting' });
            return;
          case Cr.NS_ERROR_PORT_ACCESS_NOT_ALLOWED :
            sendAsyncMsg('error', { type: 'deniedPortAccess' });
            return;
          case Cr.NS_ERROR_UNKNOWN_PROXY_HOST :
            sendAsyncMsg('error', { type: 'proxyResolveFailure' });
            return;
          case Cr.NS_ERROR_PROXY_CONNECTION_REFUSED :
            sendAsyncMsg('error', { type: 'proxyConnectFailure' });
            return;
          case Cr.NS_ERROR_INVALID_CONTENT_ENCODING :
            sendAsyncMsg('error', { type: 'contentEncodingFailure' });
            return;
          case Cr.NS_ERROR_REMOTE_XUL :
            sendAsyncMsg('error', { type: 'remoteXUL' });
            return;
          case Cr.NS_ERROR_UNSAFE_CONTENT_TYPE :
            sendAsyncMsg('error', { type: 'unsafeContentType' });
            return;
          case Cr.NS_ERROR_CORRUPTED_CONTENT :
            sendAsyncMsg('error', { type: 'corruptedContentError' });
            return;

          default:
            // getErrorClass() will throw if the error code passed in is not a NSS
            // error code.
            try {
              let nssErrorsService = Cc['@mozilla.org/nss_errors_service;1']
                                       .getService(Ci.nsINSSErrorsService);
              if (nssErrorsService.getErrorClass(status)
                    == Ci.nsINSSErrorsService.ERROR_CLASS_BAD_CERT) {
                // XXX Is there a point firing the event if the error page is not
                // certerror? If yes, maybe we should add a property to the
                // event to to indicate whether there is a custom page. That would
                // let the embedder have more control over the desired behavior.
                let errorPage = null;
                try {
                  errorPage = Services.prefs.getCharPref(CERTIFICATE_ERROR_PAGE_PREF);
                } catch (e) {}

                if (errorPage == 'certerror') {
                  sendAsyncMsg('error', { type: 'certerror' });
                  return;
                }
              }
            } catch (e) {}

            sendAsyncMsg('error', { type: 'other' });
            return;
        }
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

