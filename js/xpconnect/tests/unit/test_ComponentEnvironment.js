let tgt = {};

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

const a = ChromeUtils.import("resource://test/environment_script.js", tgt);
const b = ChromeUtils.import("resource://test/environment_checkscript.jsm", tgt);

const isShared = Cu.getGlobalForObject(a) === Cu.getGlobalForObject(b);


// Components should not share namespace
if (isShared) {
  todo_check_eq(tgt.bound, "");
  Assert.equal(tgt.bound, "ei,fo,", "Modules should have no shared non-eval bindings");
} else {
  Assert.equal(tgt.bound, "", "Modules should have no shared bindings");
}
