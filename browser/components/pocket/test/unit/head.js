ChromeUtils.defineModuleGetter(
  this,
  "pktApi",
  "chrome://pocket/content/pktApi.jsm"
);
XPCOMUtils.defineLazyScriptGetter(
  this,
  "pktUI",
  "chrome://pocket/content/pktUI.js"
);

const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");
