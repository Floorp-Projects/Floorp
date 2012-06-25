/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Cu = Components.utils;
let Ci = Components.interfaces;
let Cc = Components.classes;
let Cr = Components.results;

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
  //dump("BrowserElementChild - " + msg + "\n");
}

function sendAsyncMsg(msg, data) {
  sendAsyncMessage('browser-element-api:' + msg, data);
}

function sendSyncMsg(msg, data) {
  return sendSyncMessage('browser-element-api:' + msg, data);
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

  this._init();
};

BrowserElementChild.prototype = {
  _init: function() {
    debug("Starting up.");
    sendAsyncMsg("hello");

    BrowserElementPromptService.mapWindowToBrowserElementChild(content, this);

    docShell.isBrowserFrame = true;
    docShell.QueryInterface(Ci.nsIWebProgress)
            .addProgressListener(this._progressListener,
                                 Ci.nsIWebProgress.NOTIFY_LOCATION |
                                 Ci.nsIWebProgress.NOTIFY_SECURITY |
                                 Ci.nsIWebProgress.NOTIFY_STATE_WINDOW);

    // This is necessary to get security web progress notifications.
    var securityUI = Cc['@mozilla.org/secure_browser_ui;1']
                       .createInstance(Ci.nsISecureBrowserUI);
    securityUI.init(content);

    // A mozbrowser iframe contained inside a mozapp iframe should return false
    // for nsWindowUtils::IsPartOfApp (unless the mozbrowser iframe is itself
    // also mozapp).  That is, mozapp is transitive down to its children, but
    // mozbrowser serves as a barrier.
    //
    // This is because mozapp iframes have some privileges which we don't want
    // to extend to untrusted mozbrowser content.
    //
    // Get the app manifest from the parent, if our frame has one.
    let appManifestURL = sendSyncMsg('get-mozapp-manifest-url')[0];
    let windowUtils = content.QueryInterface(Ci.nsIInterfaceRequestor)
                             .getInterface(Ci.nsIDOMWindowUtils);

    if (!!appManifestURL) {
      windowUtils.setIsApp(true);
      windowUtils.setApp(appManifestURL);
    } else {
      windowUtils.setIsApp(false);
    }

    addEventListener('DOMTitleChanged',
                     this._titleChangedHandler.bind(this),
                     /* useCapture = */ true,
                     /* wantsUntrusted = */ false);

    addEventListener('DOMLinkAdded',
                     this._iconChangedHandler.bind(this),
                     /* useCapture = */ true,
                     /* wantsUntrusted = */ false);

    var self = this;
    function addMsgListener(msg, handler) {
      addMessageListener('browser-element-api:' + msg, handler.bind(self));
    }

    addMsgListener("get-screenshot", this._recvGetScreenshot);
    addMsgListener("set-visible", this._recvSetVisible);
    addMsgListener("unblock-modal-prompt", this._recvStopWaiting);

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
                               this._closeHandler.bind(this),
                               /* useCapture = */ false);
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
        args.promptType == 'confirm') {
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
    while (win.modalDepth == origModalDepth) {
      // Bail out of the loop if the inner window changed; that means the
      // window navigated.
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

    utils.leaveModalStateWithWindow(modalStateWin);

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

  _titleChangedHandler: function(e) {
    debug("Got titlechanged: (" + e.target.title + ")");
    var win = e.target.defaultView;

    // Ignore titlechanges which don't come from the top-level
    // <iframe mozbrowser> window.
    if (win == content) {
      sendAsyncMsg('titlechange', e.target.title);
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
        sendAsyncMsg('iconchange', e.target.href);
      }
      else {
        debug("Not top level!");
      }
    }
  },

  _closeHandler: function(e) {
    let win = e.target;
    if (win != content || e.defaultPrevented) {
      return;
    }

    debug("Closing window " + win);
    sendAsyncMsg('close');

    // Inform the window implementation that we handled this close ourselves.
    e.preventDefault();
  },

  _recvGetScreenshot: function(data) {
    debug("Received getScreenshot message: (" + data.json.id + ")");
    var canvas = content.document
      .createElementNS("http://www.w3.org/1999/xhtml", "canvas");
    var ctx = canvas.getContext("2d");
    canvas.mozOpaque = true;
    canvas.height = content.innerHeight;
    canvas.width = content.innerWidth;
    ctx.drawWindow(content, 0, 0, content.innerWidth,
                   content.innerHeight, "rgb(255,255,255)");
    sendAsyncMsg('got-screenshot', {
      id: data.json.id,
      screenshot: canvas.toDataURL("image/png")
    });
  },

  _recvSetVisible: function(data) {
    debug("Received setVisible message: (" + data.json.visible + ")");
    if (docShell.isActive !== data.json.visible) {
      docShell.isActive = data.json.visible;
    }
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

      sendAsyncMsg('locationchange', location.spec);
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

      sendAsyncMsg('securitychange', {state: stateDesc, extendedValidation: isEV});
    },

    onStatusChange: function(webProgress, request, status, message) {},
    onProgressChange: function(webProgress, request, curSelfProgress,
                               maxSelfProgress, curTotalProgress, maxTotalProgress) {},
  },
};

var api = new BrowserElementChild();
