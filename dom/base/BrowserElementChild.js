/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Cu = Components.utils;
let Ci = Components.interfaces;
let Cc = Components.classes;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

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

function BrowserElementChild() {
  this._init();
};

BrowserElementChild.prototype = {
  _init: function() {
    debug("Starting up.");
    sendAsyncMsg("hello");

    docShell.QueryInterface(Ci.nsIWebProgress)
            .addProgressListener(this._progressListener,
                                 Ci.nsIWebProgress.NOTIFY_LOCATION |
                                 Ci.nsIWebProgress.NOTIFY_STATE_WINDOW);

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
                             .getInterface(Components.interfaces.nsIDOMWindowUtils);

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

    addMessageListener("browser-element-api:get-screenshot",
                       this._recvGetScreenshot.bind(this));
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

  // The docShell keeps a weak reference to the progress listener, so we need
  // to keep a strong ref to it ourselves.
  _progressListener: {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                           Ci.nsISupportsWeakReference,
                                           Ci.nsISupports]),
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

    onStatusChange: function(webProgress, request, status, message) {},
    onProgressChange: function(webProgress, request, curSelfProgress,
                               maxSelfProgress, curTotalProgress, maxTotalProgress) {},
    onSecurityChange: function(webProgress, request, aState) {}
  },
};

var api = new BrowserElementChild();
