function run_test() {
  // Existing module.
  Assert.ok(!Cu.isModuleLoaded("resource://test/jsm_loaded-1.jsm"),
            "isModuleLoaded returned correct value for non-loaded module");
  ChromeUtils.import("resource://test/jsm_loaded-1.jsm");
  Assert.ok(Cu.isModuleLoaded("resource://test/jsm_loaded-1.jsm"),
            "isModuleLoaded returned true after loading that module");
  Cu.unload("resource://test/jsm_loaded-1.jsm");
  Assert.ok(!Cu.isModuleLoaded("resource://test/jsm_loaded-1.jsm"),
            "isModuleLoaded returned false after unloading that module");

  // Non-existing module
  Assert.ok(!Cu.isModuleLoaded("resource://gre/modules/non-existing-module.jsm"),
            "isModuleLoaded returned correct value for non-loaded module");
  Assert.throws(
    () => ChromeUtils.import("resource://gre/modules/non-existing-module.jsm"),
    /NS_ERROR_FILE_NOT_FOUND/,
    "Should have thrown while trying to load a non existing file"
  );
}
