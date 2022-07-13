Services.prefs.setBoolPref("browser.startup.record", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("browser.startup.record");
});

add_task(function test_JSModule() {
  const URL = "resource://test/import_stack.jsm";
  ChromeUtils.import(URL);
  Assert.ok(Cu.getModuleImportStack(URL).includes("test_JSModule"));
});

add_task(function test_ESModule() {
  const URL = "resource://test/import_stack.sys.mjs";
  ChromeUtils.importESModule(URL);
  Assert.ok(Cu.getModuleImportStack(URL).includes("test_ESModule"));
});
