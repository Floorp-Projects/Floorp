let tgt_load = {};
let tgt_check = {};

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});
const a = ChromeUtils.import("resource://test/environment_loadscript.jsm", tgt_load);
const b = ChromeUtils.import("resource://test/environment_checkscript.jsm", tgt_check);

const isShared = Cu.getGlobalForObject(a) === Cu.getGlobalForObject(b);

// Check target bindings
var tgt_subscript_bound = "";
for (var name of ["vu", "vq", "vl", "gt", "ed", "ei", "fo", "fi", "fd"])
    if (tgt_load.target.hasOwnProperty(name))
        tgt_subscript_bound += name + ",";

// Expected subscript loader behavior is as follows:
//  - Qualified vars and |this| access occur on target object
//  - Lexical vars occur on ExtensibleLexicalEnvironment of target object
//  - Bareword assignments and global |this| access occur on caller's global
Assert.equal(tgt_load.bound, "vu,ei,fo,fi,", "Should have expected module binding set");
Assert.equal(tgt_subscript_bound, "vq,gt,ed,fd,", "Should have expected subscript binding set");

// Components should not share namespace
if (isShared) {
  todo_check_eq(tgt_check.bound, "");
  Assert.equal(tgt_check.bound, "ei,fo,", "Modules should have no shared non-eval bindings");
} else {
  Assert.equal(tgt_check.bound, "", "Modules should have no shared bindings");
}
