/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["UITourChild"];

const PREF_TEST_WHITELIST = "browser.uitour.testingOrigins";
const UITOUR_PERMISSION = "uitour";

class UITourChild extends JSWindowActorChild {
  handleEvent(event) {
    if (!Services.prefs.getBoolPref("browser.uitour.enabled")) {
      return;
    }
    if (!this.ensureTrustedOrigin()) {
      return;
    }

    this.sendAsyncMessage("UITour:onPageEvent", {
      detail: event.detail,
      type: event.type,
      pageVisibilityState: this.document.visibilityState,
    });
  }

  isTestingOrigin(aURI) {
    if (
      Services.prefs.getPrefType(PREF_TEST_WHITELIST) !=
      Services.prefs.PREF_STRING
    ) {
      return false;
    }

    // Add any testing origins (comma-seperated) to the whitelist for the session.
    for (let origin of Services.prefs
      .getCharPref(PREF_TEST_WHITELIST)
      .split(",")) {
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
  }

  // This function is copied from UITour.jsm.
  isSafeScheme(aURI) {
    let allowedSchemes = new Set(["https", "about"]);
    if (!Services.prefs.getBoolPref("browser.uitour.requireSecure")) {
      allowedSchemes.add("http");
    }

    if (!allowedSchemes.has(aURI.scheme)) {
      return false;
    }

    return true;
  }

  ensureTrustedOrigin() {
    if (this.browsingContext.top != this.browsingContext) {
      return false;
    }

    let uri = this.document.documentURIObject;

    if (uri.schemeIs("chrome")) {
      return true;
    }

    if (!this.isSafeScheme(uri)) {
      return false;
    }

    let principal = Services.scriptSecurityManager.principalWithOA(
      this.document.nodePrincipal,
      {}
    );
    let permission = Services.perms.testPermissionFromPrincipal(
      principal,
      UITOUR_PERMISSION
    );
    if (permission == Services.perms.ALLOW_ACTION) {
      return true;
    }

    // Bug 1557153: To allow Skyline messaging, workaround for UNKNOWN_ACTION
    // overriding browser/app/permissions default
    return uri.host == "www.mozilla.org" || this.isTestingOrigin(uri);
  }

  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "UITour:SendPageCallback":
        this.sendPageEvent("Response", aMessage.data);
        break;
      case "UITour:SendPageNotification":
        this.sendPageEvent("Notification", aMessage.data);
        break;
    }
  }

  sendPageEvent(type, detail) {
    if (!this.ensureTrustedOrigin()) {
      return;
    }

    let win = this.contentWindow;
    let eventName = "mozUITour" + type;
    let event = new win.CustomEvent(eventName, {
      bubbles: true,
      detail: Cu.cloneInto(detail, win),
    });
    win.document.dispatchEvent(event);
  }
}
