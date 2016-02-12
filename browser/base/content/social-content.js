/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This content script should work in any browser or iframe and should not
 * depend on the frame being contained in tabbrowser. */

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// social frames are always treated as app tabs
docShell.isAppTab = true;

// Error handling class used to listen for network errors in the social frames
// and replace them with a social-specific error page
SocialErrorListener = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsISupports]),

  defaultTemplate: "about:socialerror?mode=tryAgainOnly&url=%{url}&origin=%{origin}",
  urlTemplate: null,

  init() {
    addMessageListener("Social:SetErrorURL", this);
    let webProgress = docShell.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                              .getInterface(Components.interfaces.nsIWebProgress);
    webProgress.addProgressListener(this, Ci.nsIWebProgress.NOTIFY_STATE_REQUEST |
                                          Ci.nsIWebProgress.NOTIFY_LOCATION);
  },
  receiveMessage(message) {
    switch(message.name) {
      case "Social:SetErrorURL": {
        // either a url or null to reset to default template
        this.urlTemplate = message.objects.template;
      }
    }
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
