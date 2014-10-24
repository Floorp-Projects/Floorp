let {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

const PREF_TEST_WHITELIST = "browser.uitour.testingOrigins";
const UITOUR_PERMISSION   = "uitour";

let UITourListener = {
  handleEvent: function (event) {
    if (!Services.prefs.getBoolPref("browser.uitour.enabled")) {
      return;
    }
    if (!this.ensureTrustedOrigin()) {
      return;
    }
    addMessageListener("UITour:SendPageCallback", this);
    sendAsyncMessage("UITour:onPageEvent", {detail: event.detail, type: event.type});
  },

  isTestingOrigin: function(aURI) {
    if (Services.prefs.getPrefType(PREF_TEST_WHITELIST) != Services.prefs.PREF_STRING) {
      return false;
    }

    // Add any testing origins (comma-seperated) to the whitelist for the session.
    for (let origin of Services.prefs.getCharPref(PREF_TEST_WHITELIST).split(",")) {
      try {
        let testingURI = Services.io.newURI(origin, null, null);
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
  isSafeScheme: function(aURI) {
    let allowedSchemes = new Set(["https", "about"]);
    if (!Services.prefs.getBoolPref("browser.uitour.requireSecure"))
      allowedSchemes.add("http");

    if (!allowedSchemes.has(aURI.scheme))
      return false;

    return true;
  },

  ensureTrustedOrigin: function() {
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

  receiveMessage: function(aMessage) {
    switch (aMessage.name) {
      case "UITour:SendPageCallback":
        this.sendPageCallback(aMessage.data);
        break;
    }
  },

  sendPageCallback: function (detail) {
    let doc = content.document;
    let event = new doc.defaultView.CustomEvent("mozUITourResponse", {
      bubbles: true,
      detail: Cu.cloneInto(detail, doc.defaultView)
    });
    doc.dispatchEvent(event);
  }
};

addEventListener("mozUITour", UITourListener, false, true);
