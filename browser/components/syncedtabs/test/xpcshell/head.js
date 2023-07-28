var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
var { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

XPCOMUtils.defineLazyGetter(this, "FxAccountsCommon", function () {
  return ChromeUtils.import("resource://gre/modules/FxAccountsCommon.js");
});

do_get_profile(); // fxa needs a profile directory for storage.
