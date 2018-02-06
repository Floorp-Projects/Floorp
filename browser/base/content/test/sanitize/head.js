ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  Downloads: "resource://gre/modules/Downloads.jsm",
  FormHistory: "resource://gre/modules/FormHistory.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  Sanitizer: "resource:///modules/Sanitizer.jsm",
});

