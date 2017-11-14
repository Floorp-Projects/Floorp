const Cu = Components.utils;

function run_test() {
  // Existing module.
  do_check_true(!Cu.isModuleLoaded("resource://gre/modules/NetUtil.jsm"),
                "isModuleLoaded returned correct value for non-loaded module");
  Cu.import("resource://gre/modules/NetUtil.jsm");
  do_check_true(Cu.isModuleLoaded("resource://gre/modules/NetUtil.jsm"),
                "isModuleLoaded returned true after loading that module");
  Cu.unload("resource://gre/modules/NetUtil.jsm");
  do_check_true(!Cu.isModuleLoaded("resource://gre/modules/NetUtil.jsm"),
                "isModuleLoaded returned false after unloading that module");

  // Non-existing module
  do_check_true(!Cu.isModuleLoaded("resource://gre/modules/non-existing-module.jsm"),
                "isModuleLoaded returned correct value for non-loaded module");
  try {
    Cu.import("resource://gre/modules/non-existing-module.jsm");
    do_check_true(false,
                  "Should have thrown while trying to load a non existing file");
  } catch (ex) {}
  do_check_true(!Cu.isModuleLoaded("resource://gre/modules/non-existing-module.jsm"),
                "isModuleLoaded returned correct value for non-loaded module");
}
