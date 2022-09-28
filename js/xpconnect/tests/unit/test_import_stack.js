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

add_task(function test_ESModule_static_import() {
  const URL1 = "resource://test/import_stack_static_1.sys.mjs";
  const URL2 = "resource://test/import_stack_static_2.sys.mjs";
  const URL3 = "resource://test/import_stack_static_3.sys.mjs";
  const URL4 = "resource://test/import_stack_static_4.sys.mjs";

  ChromeUtils.importESModule(URL1);

  Assert.ok(Cu.getModuleImportStack(URL1).includes("test_ESModule_static"));

  Assert.ok(Cu.getModuleImportStack(URL2).includes("test_ESModule_static"));
  Assert.ok(Cu.getModuleImportStack(URL2).includes(URL1));

  Assert.ok(Cu.getModuleImportStack(URL3).includes("test_ESModule_static"));
  Assert.ok(Cu.getModuleImportStack(URL3).includes(URL1));
  Assert.ok(Cu.getModuleImportStack(URL3).includes(URL2));

  Assert.ok(Cu.getModuleImportStack(URL4).includes("test_ESModule_static"));
  Assert.ok(Cu.getModuleImportStack(URL4).includes(URL1));
  Assert.ok(Cu.getModuleImportStack(URL4).includes(URL2));
  Assert.ok(Cu.getModuleImportStack(URL4).includes(URL3));
});
