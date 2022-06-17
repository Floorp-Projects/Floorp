add_task(function test_JSModule() {
  const URL1 = "resource://test/jsm_loaded-1.jsm";
  const URL2 = "resource://test/jsm_loaded-2.jsm";
  const URL3 = "resource://test/jsm_loaded-3.jsm";

  Assert.ok(!Cu.loadedJSModules.includes(URL1));
  Assert.ok(!Cu.isJSModuleLoaded(URL1));
  Assert.ok(!Cu.loadedJSModules.includes(URL2));
  Assert.ok(!Cu.isJSModuleLoaded(URL2));
  Assert.ok(!Cu.loadedJSModules.includes(URL3));
  Assert.ok(!Cu.isJSModuleLoaded(URL3));
  Assert.ok(!Cu.loadedESModules.includes(URL1));
  Assert.ok(!Cu.isESModuleLoaded(URL1));
  Assert.ok(!Cu.loadedESModules.includes(URL2));
  Assert.ok(!Cu.isESModuleLoaded(URL2));
  Assert.ok(!Cu.loadedESModules.includes(URL3));
  Assert.ok(!Cu.isESModuleLoaded(URL3));

  ChromeUtils.import(URL1);

  Assert.ok(Cu.loadedJSModules.includes(URL1));
  Assert.ok(Cu.isJSModuleLoaded(URL1));
  Assert.ok(!Cu.loadedJSModules.includes(URL2));
  Assert.ok(!Cu.isJSModuleLoaded(URL2));
  Assert.ok(!Cu.loadedJSModules.includes(URL3));
  Assert.ok(!Cu.isJSModuleLoaded(URL3));
  Assert.ok(!Cu.loadedESModules.includes(URL1));
  Assert.ok(!Cu.isESModuleLoaded(URL1));
  Assert.ok(!Cu.loadedESModules.includes(URL2));
  Assert.ok(!Cu.isESModuleLoaded(URL2));
  Assert.ok(!Cu.loadedESModules.includes(URL3));
  Assert.ok(!Cu.isESModuleLoaded(URL3));

  ChromeUtils.import(URL2);

  Assert.ok(Cu.loadedJSModules.includes(URL1));
  Assert.ok(Cu.isJSModuleLoaded(URL1));
  Assert.ok(Cu.loadedJSModules.includes(URL2));
  Assert.ok(Cu.isJSModuleLoaded(URL2));
  Assert.ok(!Cu.loadedJSModules.includes(URL3));
  Assert.ok(!Cu.isJSModuleLoaded(URL3));
  Assert.ok(!Cu.loadedESModules.includes(URL1));
  Assert.ok(!Cu.isESModuleLoaded(URL1));
  Assert.ok(!Cu.loadedESModules.includes(URL2));
  Assert.ok(!Cu.isESModuleLoaded(URL2));
  Assert.ok(!Cu.loadedESModules.includes(URL3));
  Assert.ok(!Cu.isESModuleLoaded(URL3));

  ChromeUtils.import(URL3);

  Assert.ok(Cu.loadedJSModules.includes(URL1));
  Assert.ok(Cu.isJSModuleLoaded(URL1));
  Assert.ok(Cu.loadedJSModules.includes(URL2));
  Assert.ok(Cu.isJSModuleLoaded(URL2));
  Assert.ok(Cu.loadedJSModules.includes(URL3));
  Assert.ok(Cu.isJSModuleLoaded(URL3));
  Assert.ok(!Cu.loadedESModules.includes(URL1));
  Assert.ok(!Cu.isESModuleLoaded(URL1));
  Assert.ok(!Cu.loadedESModules.includes(URL2));
  Assert.ok(!Cu.isESModuleLoaded(URL2));
  Assert.ok(!Cu.loadedESModules.includes(URL3));
  Assert.ok(!Cu.isESModuleLoaded(URL3));
});

add_task(function test_ESModule() {
  const URL1 = "resource://test/es6module_loaded-1.sys.mjs";
  const URL2 = "resource://test/es6module_loaded-2.sys.mjs";
  const URL3 = "resource://test/es6module_loaded-3.sys.mjs";

  Assert.ok(!Cu.loadedJSModules.includes(URL1));
  Assert.ok(!Cu.isJSModuleLoaded(URL1));
  Assert.ok(!Cu.loadedJSModules.includes(URL2));
  Assert.ok(!Cu.isJSModuleLoaded(URL2));
  Assert.ok(!Cu.loadedJSModules.includes(URL3));
  Assert.ok(!Cu.isJSModuleLoaded(URL3));
  Assert.ok(!Cu.loadedESModules.includes(URL1));
  Assert.ok(!Cu.isESModuleLoaded(URL1));
  Assert.ok(!Cu.loadedESModules.includes(URL2));
  Assert.ok(!Cu.isESModuleLoaded(URL2));
  Assert.ok(!Cu.loadedESModules.includes(URL3));
  Assert.ok(!Cu.isESModuleLoaded(URL3));

  ChromeUtils.importESModule(URL1);

  Assert.ok(!Cu.loadedJSModules.includes(URL1));
  Assert.ok(!Cu.isJSModuleLoaded(URL1));
  Assert.ok(!Cu.loadedJSModules.includes(URL2));
  Assert.ok(!Cu.isJSModuleLoaded(URL2));
  Assert.ok(!Cu.loadedJSModules.includes(URL3));
  Assert.ok(!Cu.isJSModuleLoaded(URL3));
  Assert.ok(Cu.loadedESModules.includes(URL1));
  Assert.ok(Cu.isESModuleLoaded(URL1));
  Assert.ok(!Cu.loadedESModules.includes(URL2));
  Assert.ok(!Cu.isESModuleLoaded(URL2));
  Assert.ok(!Cu.loadedESModules.includes(URL3));
  Assert.ok(!Cu.isESModuleLoaded(URL3));

  ChromeUtils.importESModule(URL2);

  Assert.ok(!Cu.loadedJSModules.includes(URL1));
  Assert.ok(!Cu.isJSModuleLoaded(URL1));
  Assert.ok(!Cu.loadedJSModules.includes(URL2));
  Assert.ok(!Cu.isJSModuleLoaded(URL2));
  Assert.ok(!Cu.loadedJSModules.includes(URL3));
  Assert.ok(!Cu.isJSModuleLoaded(URL3));
  Assert.ok(Cu.loadedESModules.includes(URL1));
  Assert.ok(Cu.isESModuleLoaded(URL1));
  Assert.ok(Cu.loadedESModules.includes(URL2));
  Assert.ok(Cu.isESModuleLoaded(URL2));
  Assert.ok(!Cu.loadedESModules.includes(URL3));
  Assert.ok(!Cu.isESModuleLoaded(URL3));

  ChromeUtils.importESModule(URL3);

  Assert.ok(!Cu.loadedJSModules.includes(URL1));
  Assert.ok(!Cu.isJSModuleLoaded(URL1));
  Assert.ok(!Cu.loadedJSModules.includes(URL2));
  Assert.ok(!Cu.isJSModuleLoaded(URL2));
  Assert.ok(!Cu.loadedJSModules.includes(URL3));
  Assert.ok(!Cu.isJSModuleLoaded(URL3));
  Assert.ok(Cu.loadedESModules.includes(URL1));
  Assert.ok(Cu.isESModuleLoaded(URL1));
  Assert.ok(Cu.loadedESModules.includes(URL2));
  Assert.ok(Cu.isESModuleLoaded(URL2));
  Assert.ok(Cu.loadedESModules.includes(URL3));
  Assert.ok(Cu.isESModuleLoaded(URL3));
});
