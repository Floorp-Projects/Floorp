/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This content script should work in any browser or iframe and should not
 * depend on the frame being contained in tabbrowser. */

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// Tie `content` to this frame scripts' global scope explicitly. If we don't, then
// `content` might be out of eval's scope and GC'ed before this script is done.
// See bug 1229195 for empirical proof.
var gContent = content;

// social frames are always treated as app tabs
docShell.isAppTab = true;
var gHookedWindowCloseForPanelClose = false;

var gDOMContentLoaded = false;
addEventListener("DOMContentLoaded", function(event) {
  if (event.target == content.document) {
    gDOMContentLoaded = true;
    sendAsyncMessage("DOMContentLoaded");
  }
});
addEventListener("unload", function(event) {
  if (event.target == content.document) {
    gDOMContentLoaded = false;
    gHookedWindowCloseForPanelClose = false;
  }
}, true);

var gDOMTitleChangedByUs = false;
addEventListener("DOMTitleChanged", function(e) {
  if (!gDOMTitleChangedByUs) {
    sendAsyncMessage("Social:DOMTitleChanged", {
      title: e.target.title
    });
  }
  gDOMTitleChangedByUs = false;
});

addEventListener("Social:Notification", function(event) {
  let frame = docShell.chromeEventHandler;
  let origin = frame.getAttribute("origin");
  sendAsyncMessage("Social:Notification", {
    "origin": origin,
    "detail": JSON.parse(event.detail)
  });
});

addMessageListener("Social:OpenGraphData", (message) => {
  let ev = new content.CustomEvent("OpenGraphData", { detail: JSON.stringify(message.data) });
  content.dispatchEvent(ev);
});

addMessageListener("Social:ClearFrame", (message) => {
  docShell.createAboutBlankContentViewer(null);
});

// Error handling class used to listen for network errors in the social frames
// and replace them with a social-specific error page
const SocialErrorListener = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMEventListener,
                                         Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsISupports]),

  defaultTemplate: "about:socialerror?mode=tryAgainOnly&url=%{url}&origin=%{origin}",
  urlTemplate: null,

  init() {
    addMessageListener("Loop:MonitorPeerConnectionLifecycle", this);
    addMessageListener("Loop:GetAllWebrtcStats", this);
    addMessageListener("Social:CustomEvent", this);
    addMessageListener("Social:EnsureFocus", this);
    addMessageListener("Social:EnsureFocusElement", this);
    addMessageListener("Social:HookWindowCloseForPanelClose", this);
    addMessageListener("Social:ListenForEvents", this);
    addMessageListener("Social:SetDocumentTitle", this);
    addMessageListener("Social:SetErrorURL", this);
    addMessageListener("Social:DisableDialogs", this);
    addMessageListener("Social:WaitForDocumentVisible", this);
    addMessageListener("WaitForDOMContentLoaded", this);
    let webProgress = docShell.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                              .getInterface(Components.interfaces.nsIWebProgress);
    webProgress.addProgressListener(this, Ci.nsIWebProgress.NOTIFY_STATE_REQUEST |
                                          Ci.nsIWebProgress.NOTIFY_LOCATION);
  },

  receiveMessage(message) {
    let document = content.document;

    switch (message.name) {
      case "Loop:GetAllWebrtcStats":
        content.WebrtcGlobalInformation.getAllStats(allStats => {
          content.WebrtcGlobalInformation.getLogging("", logs => {
            sendAsyncMessage("Loop:GetAllWebrtcStats", {
              allStats: allStats,
              logs: logs
            });
          });
        }, message.data.peerConnectionID);
        break;
      case "Loop:MonitorPeerConnectionLifecycle":
        let ourID = content.QueryInterface(Ci.nsIInterfaceRequestor)
          .getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;

        let onPCLifecycleChange = (pc, winID, type) => {
          if (winID != ourID) {
            return;
          }

          sendAsyncMessage("Loop:PeerConnectionLifecycleChange", {
            iceConnectionState: pc.iceConnectionState,
            locationHash: content.location.hash,
            peerConnectionID: pc.id,
            type: type
          });
        };

        let pc_static = new content.RTCPeerConnectionStatic();
        pc_static.registerPeerConnectionLifecycleCallback(onPCLifecycleChange);
        break;
      case "Social:CustomEvent":
        let ev = new content.CustomEvent(message.data.name, message.data.detail ?
          { detail: message.data.detail } : null);
        content.dispatchEvent(ev);
        break;
      case "Social:EnsureFocus":
        Services.focus.focusedWindow = content;
        sendAsyncMessage("Social:FocusEnsured");
        break;
      case "Social:EnsureFocusElement":
        let fm = Services.focus;
        fm.moveFocus(document.defaultView, null, fm.MOVEFOCUS_FIRST, fm.FLAG_NOSCROLL);
        sendAsyncMessage("Social:FocusEnsured");
        break;
      case "Social:HookWindowCloseForPanelClose":
        if (gHookedWindowCloseForPanelClose) {
          break;
        }
        gHookedWindowCloseForPanelClose = true;
        // We allow window.close() to close the panel, so add an event handler for
        // this, then cancel the event (so the window itself doesn't die) and
        // close the panel instead.
        // However, this is typically affected by the dom.allow_scripts_to_close_windows
        // preference, but we can avoid that check by setting a flag on the window.
        let dwu = content.QueryInterface(Ci.nsIInterfaceRequestor)
           .getInterface(Ci.nsIDOMWindowUtils);
        dwu.allowScriptsToClose();

        content.addEventListener("DOMWindowClose", function _mozSocialDOMWindowClose(evt) {
          // preventDefault stops the default window.close() function being called,
          // which doesn't actually close anything but causes things to get into
          // a bad state (an internal 'closed' flag is set and debug builds start
          // asserting as the window is used.).
          // None of the windows we inject this API into are suitable for this
          // default close behaviour, so even if we took no action above, we avoid
          // the default close from doing anything.
          evt.preventDefault();

          sendAsyncMessage("Social:DOMWindowClose");
        }, true);
        break;
      case "Social:ListenForEvents":
        for (let eventName of message.data.eventNames) {
          content.addEventListener(eventName, this);
        }
        break;
      case "Social:SetDocumentTitle":
        let title = message.data.title;
        if (title && (title = title.trim())) {
          gDOMTitleChangedByUs = true;
          document.title = title;
        }
        break;
      case "Social:SetErrorURL":
        // Either a url or null to reset to default template.
        this.urlTemplate = message.data.template;
        break;
      case "Social:WaitForDocumentVisible":
        if (!document.hidden) {
          sendAsyncMessage("Social:DocumentVisible");
          break;
        }

        document.addEventListener("visibilitychange", function onVisibilityChanged() {
          document.removeEventListener("visibilitychange", onVisibilityChanged);
          sendAsyncMessage("Social:DocumentVisible");
        });
        break;
      case "Social:DisableDialogs":
        let windowUtils = content.QueryInterface(Ci.nsIInterfaceRequestor).
                          getInterface(Ci.nsIDOMWindowUtils);
        windowUtils.disableDialogs();
        break;
      case "WaitForDOMContentLoaded":
        if (gDOMContentLoaded) {
          sendAsyncMessage("DOMContentLoaded");
        }
        break;
    }
  },

  handleEvent: function(event) {
    sendAsyncMessage("Social:CustomEvent", {
      name: event.type
    });
  },

  setErrorPage() {
    // if this is about:providerdirectory, use the directory iframe
    let frame = docShell.chromeEventHandler;
    let origin = frame.getAttribute("origin");
    let src = frame.getAttribute("src");
    if (src == "about:providerdirectory") {
      frame = content.document.getElementById("activation-frame");
      src = frame.getAttribute("src");
    }

    let url = this.urlTemplate || this.defaultTemplate;
    url = url.replace("%{url}", encodeURIComponent(src));
    url = url.replace("%{origin}", encodeURIComponent(origin));
    if (frame != docShell.chromeEventHandler) {
      // Unable to access frame.docShell here. This is our own frame and doesn't
      // provide reload, so we'll just set the src.
      frame.setAttribute("src", url);
    } else {
      let webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
      webNav.loadURI(url, null, null, null, null);
    }
    sendAsyncMessage("Social:ErrorPageNotify", {
        origin: origin,
        url: src
    });
  },

  onStateChange(aWebProgress, aRequest, aState, aStatus) {
    let failure = false;
    if ((aState & Ci.nsIWebProgressListener.STATE_IS_REQUEST))
      return;
    if ((aState & Ci.nsIWebProgressListener.STATE_STOP)) {
      if (aRequest instanceof Ci.nsIHttpChannel) {
        try {
          // Change the frame to an error page on 4xx (client errors)
          // and 5xx (server errors).  responseStatus throws if it is not set.
          failure = aRequest.responseStatus >= 400 &&
                    aRequest.responseStatus < 600;
        } catch (e) {
          failure = aStatus != Components.results.NS_OK;
        }
      }
    }

    // Calling cancel() will raise some OnStateChange notifications by itself,
    // so avoid doing that more than once
    if (failure && aStatus != Components.results.NS_BINDING_ABORTED) {
      // if tp is enabled and we get a failure, ignore failures (ie. STATE_STOP)
      // on child resources since they *may* have been blocked. We don't have an
      // easy way to know if a particular url is blocked by TP, only that
      // something was.
      if (docShell.hasTrackingContentBlocked) {
        let frame = docShell.chromeEventHandler;
        let src = frame.getAttribute("src");
        if (aRequest && aRequest.name != src) {
          Cu.reportError("SocialErrorListener ignoring blocked content error for " + aRequest.name);
          return;
        }
      }

      aRequest.cancel(Components.results.NS_BINDING_ABORTED);
      this.setErrorPage();
    }
  },

  onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {
    if (aRequest && aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_ERROR_PAGE) {
      aRequest.cancel(Components.results.NS_BINDING_ABORTED);
      this.setErrorPage();
    }
  },

  onProgressChange() {},
  onStatusChange() {},
  onSecurityChange() {},
};

SocialErrorListener.init();
