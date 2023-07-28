var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
var { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  FxAccountsCommon: "resource://gre/modules/FxAccountsCommon.sys.mjs",
});

do_get_profile(); // fxa needs a profile directory for storage.
