var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
var { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

XPCOMUtils.defineLazyGetter(this, "FxAccountsCommon", function() {
  return ChromeUtils.import("resource://gre/modules/FxAccountsCommon.js");
});

do_get_profile(); // fxa needs a profile directory for storage.
