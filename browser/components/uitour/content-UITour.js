/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

const PREF_TEST_WHITELIST = "browser.uitour.testingOrigins";
const UITOUR_PERMISSION   = "uitour";

var UITourListener = {
  handleEvent(event) {
    if (!Services.prefs.getBoolPref("browser.uitour.enabled")) {
      return;
    }
    if (!this.ensureTrustedOrigin()) {
      return;
    }
    addMessageListener("UITour:SendPageCallback", this);
    addMessageListener("UITour:SendPageNotification", this);
    sendAsyncMessage("UITour:onPageEvent", {
      detail: event.detail,
      type: event.type,
      pageVisibilityState: content.document.visibilityState,
    });
  },

  isTestingOrigin(aURI) {
    if (Services.prefs.getPrefType(PREF_TEST_WHITELIST) != Services.prefs.PREF_STRING) {
      return false;
    }

    // Add any testing origins (comma-seperated) to the whitelist for the session.
    for (let origin of Services.prefs.getCharPref(PREF_TEST_WHITELIST).split(",")) {
      try {
        let testingURI = Services.io.newURI(origin);
        if (aURI.prePath == testingURI.prePath) {
          return true;
        }
      } catch (ex) {
        Cu.reportError(ex);
      }
    }
    return false;
  },

  // This function is copied from UITour.jsm.
  isSafeScheme(aURI) {
    let allowedSchemes = new Set(["https", "about"]);
    if (!Services.prefs.getBoolPref("browser.uitour.requireSecure"))
      allowedSchemes.add("http");

    if (!allowedSchemes.has(aURI.scheme))
      return false;

    return true;
  },

  ensureTrustedOrigin() {
    if (content.top != content)
      return false;

    let uri = content.document.documentURIObject;

    if (uri.schemeIs("chrome"))
      return true;

    if (!this.isSafeScheme(uri))
      return false;

    let permission = Services.perms.testPermission(uri, UITOUR_PERMISSION);
    if (permission == Services.perms.ALLOW_ACTION)
      return true;

    return this.isTestingOrigin(uri);
  },

  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "UITour:SendPageCallback":
        this.sendPageEvent("Response", aMessage.data);
        break;
      case "UITour:SendPageNotification":
        this.sendPageEvent("Notification", aMessage.data);
        break;
      }
  },

  sendPageEvent(type, detail) {
    if (!this.ensureTrustedOrigin()) {
      return;
    }

    let doc = content.document;
    let eventName = "mozUITour" + type;
    let event = new doc.defaultView.CustomEvent(eventName, {
      bubbles: true,
      detail: Cu.cloneInto(detail, doc.defaultView)
    });
    doc.dispatchEvent(event);
  }
};

addEventListener("mozUITour", UITourListener, false, true);
