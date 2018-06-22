const { XPCOMUtils } = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", {});
const { DOMLocalization } = ChromeUtils.import("resource://gre/modules/DOMLocalization.jsm", {});

DOMLocalization.prototype.classID =
  Components.ID("{29cc3895-8835-4c5b-b53a-0c0d1a458dee}");
DOMLocalization.prototype.QueryInterface =
  ChromeUtils.generateQI([Ci.mozIDOMLocalization, Ci.nsISupportsWeakReference]);

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([DOMLocalization]);
