ChromeUtils.defineESModuleGetters(this, {
  pktApi: "chrome://pocket/content/pktApi.sys.mjs",
});
XPCOMUtils.defineLazyScriptGetter(
  this,
  "pktUI",
  "chrome://pocket/content/pktUI.js"
);

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
