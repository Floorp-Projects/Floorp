add_task(function test_Cu_import_shim_first() {
  // Load and cache with shim.

  const exports = {};
  const global = Components.utils.import(
    "resource://test/esmified-1.jsm",
    exports
  );
  Assert.equal(global.loadCount, 1);
  Assert.equal(global.obj.value, 10);
  Assert.equal(exports.loadCount, 1);
  Assert.equal(exports.obj.value, 10);
  Assert.ok(exports.obj === global.obj);

  const ns = ChromeUtils.importModule("resource://test/esmified-1.sys.mjs");
  Assert.equal(ns.loadCount, 1);
  Assert.equal(ns.obj.value, 10);
  Assert.ok(ns.obj === global.obj);

  const exports2 = {};
  const global2 = Components.utils.import(
    "resource://test/esmified-1.jsm", exports2
  );
  Assert.equal(global2.loadCount, 1);
  Assert.equal(global2.obj.value, 10);
  Assert.equal(exports2.loadCount, 1);
  Assert.equal(exports2.obj.value, 10);
  Assert.ok(exports2.obj === global2.obj);
  Assert.ok(exports2.obj === global.obj);

  // Also test with *.js extension.
  const exports3 = {};
  const global3 = Components.utils.import(
    "resource://test/esmified-1.js", exports3
  );
  Assert.equal(global3.loadCount, 1);
  Assert.equal(global3.obj.value, 10);
  Assert.equal(exports3.loadCount, 1);
  Assert.equal(exports3.obj.value, 10);
  Assert.ok(exports3.obj === global3.obj);
  Assert.ok(exports3.obj === global.obj);

  // Also test with *.jsm.js extension.
  const exports4 = {};
  const global4 = Components.utils.import(
    "resource://test/esmified-1.js", exports4
  );
  Assert.equal(global4.loadCount, 1);
  Assert.equal(global4.obj.value, 10);
  Assert.equal(exports4.loadCount, 1);
  Assert.equal(exports4.obj.value, 10);
  Assert.ok(exports4.obj === global4.obj);
  Assert.ok(exports4.obj === global.obj);
});

add_task(function test_Cu_import_no_shim_first() {
  // Load and cache with importModule.

  const ns = ChromeUtils.importModule("resource://test/esmified-2.sys.mjs");
  Assert.equal(ns.loadCount, 1);
  Assert.equal(ns.obj.value, 10);

  const exports = {};
  const global = Components.utils.import(
    "resource://test/esmified-2.jsm", exports
  );
  Assert.equal(global.loadCount, 1);
  Assert.equal(global.obj.value, 10);
  Assert.equal(exports.loadCount, 1);
  Assert.equal(exports.obj.value, 10);
  Assert.ok(exports.obj === global.obj);
  Assert.ok(ns.obj === global.obj);

  const ns2 = ChromeUtils.importModule("resource://test/esmified-2.sys.mjs");
  Assert.equal(ns2.loadCount, 1);
  Assert.equal(ns2.obj.value, 10);
});

add_task(function test_ChromeUtils_import_shim_first() {
  // Load and cache with shim.

  const exports = {};
  const global = ChromeUtils.import(
    "resource://test/esmified-3.jsm", exports
  );
  Assert.equal(global.loadCount, 1);
  Assert.equal(global.obj.value, 10);
  Assert.equal(exports.loadCount, 1);
  Assert.equal(exports.obj.value, 10);
  Assert.ok(exports.obj === global.obj);

  const ns = ChromeUtils.importModule("resource://test/esmified-3.sys.mjs");
  Assert.equal(ns.loadCount, 1);
  Assert.equal(ns.obj.value, 10);
  Assert.ok(ns.obj === global.obj);

  const exports2 = {};
  const global2 = ChromeUtils.import(
    "resource://test/esmified-3.jsm", exports2
  );
  Assert.equal(global2.loadCount, 1);
  Assert.equal(global2.obj.value, 10);
  Assert.equal(exports2.loadCount, 1);
  Assert.equal(exports2.obj.value, 10);
  Assert.ok(exports2.obj === global2.obj);
  Assert.ok(exports2.obj === global.obj);
});

add_task(function test_ChromeUtils_import_no_shim_first() {
  // Load and cache with importModule.

  const ns = ChromeUtils.importModule("resource://test/esmified-4.sys.mjs");
  Assert.equal(ns.loadCount, 1);
  Assert.equal(ns.obj.value, 10);

  const exports = {};
  const global = ChromeUtils.import(
    "resource://test/esmified-4.jsm", exports
  );
  Assert.equal(global.loadCount, 1);
  Assert.equal(global.obj.value, 10);
  Assert.equal(exports.loadCount, 1);
  Assert.equal(exports.obj.value, 10);
  Assert.ok(exports.obj === global.obj);
  Assert.ok(ns.obj === global.obj);

  const ns2 = ChromeUtils.importModule("resource://test/esmified-4.sys.mjs");
  Assert.equal(ns2.loadCount, 1);
  Assert.equal(ns2.obj.value, 10);
});

add_task(function test_ChromeUtils_import_not_exported_no_shim_JSM() {
  // `exports` properties for not-ESM-ified case.

  const exports = ChromeUtils.import(
    "resource://test/not-esmified-not-exported.jsm"
  );

  Assert.equal(exports.exportedVar, "exported var");
  Assert.equal(exports.exportedFunction(), "exported function");
  Assert.equal(exports.exportedLet, "exported let");
  Assert.equal(exports.exportedConst, "exported const");
  Assert.equal(exports.notExportedVar, undefined);
  Assert.equal(exports.notExportedFunction, undefined);
  Assert.equal(exports.notExportedLet, undefined);
  Assert.equal(exports.notExportedConst, undefined);
});

add_task(function test_ChromeUtils_import_not_exported_shim() {
  // `exports` properties for shim case.

  const exports = ChromeUtils.import(
    "resource://test/esmified-not-exported.jsm"
  );

  Assert.equal(exports.exportedVar, "exported var");
  Assert.equal(exports.exportedFunction(), "exported function");
  Assert.equal(exports.exportedLet, "exported let");
  Assert.equal(exports.exportedConst, "exported const");
  Assert.equal(exports.notExportedVar, undefined);
  Assert.equal(exports.notExportedFunction, undefined);
  Assert.equal(exports.notExportedLet, undefined);
  Assert.equal(exports.notExportedConst, undefined);
});

add_task(function test_ChromeUtils_import_not_exported_no_shim_ESM() {
  // `exports` properties for ESM-ified case.

  const exports = ChromeUtils.importModule(
    "resource://test/esmified-not-exported.sys.mjs"
  );

  Assert.equal(exports.exportedVar, "exported var");
  Assert.equal(exports.exportedFunction(), "exported function");
  Assert.equal(exports.exportedLet, "exported let");
  Assert.equal(exports.exportedConst, "exported const");
  Assert.equal(exports.notExportedVar, undefined);
  Assert.equal(exports.notExportedFunction, undefined);
  Assert.equal(exports.notExportedLet, undefined);
  Assert.equal(exports.notExportedConst, undefined);
});

function testReadProxyOps(global, expectedNames, expectedDesc) {
  expectedNames.sort();

  // enumerate
  const names = Object.keys(global).sort();
  Assert.equal(JSON.stringify(names), JSON.stringify(expectedNames),
              `enumerate`);

  // has
  for (const name of expectedNames) {
    Assert.ok(name in global, `has for ${name}`);
  }

  // getOwnPropertyDescriptor
  for (const name of expectedNames) {
    const desc = Object.getOwnPropertyDescriptor(global, name);
    Assert.equal(desc.value, global[name]);
    Assert.equal(desc.writable, expectedDesc.writable,
                 `writable for ${name}`);
    Assert.equal(desc.enumerable, expectedDesc.enumerable,
                 `enumerable for ${name}`);
    Assert.equal(desc.configurable, expectedDesc.configurable,
                 `configurable for ${name}`);
  }
}

function testWriteProxyOps(global, expectedNames) {
  // set: no-op
  for (const name of expectedNames) {
    const before = global[name];
    global[name] = -1;
    Assert.equal(global[name], before, `value after set for ${name}`);
  }

  // delete: no-op
  for (const name of expectedNames) {
    const before = global[name];
    Assert.ok(!(delete global[name]), `delete result for ${name}`);
    Assert.equal(global[name], before, `value after delete for ${name}`);
  }
}

add_task(function test_Cu_import_not_exported_no_shim_JSM() {
  // `exports` and `global` properties for not-ESM-ified case.
  // Not-exported variables should be visible in `global`.

  const exports = {};
  const global = Components.utils.import(
    "resource://test/not-esmified-not-exported.jsm",
    exports
  );

  Assert.equal(global.exportedVar, "exported var");
  Assert.equal(global.exportedFunction(), "exported function");
  Assert.equal(global.exportedLet, "exported let");
  Assert.equal(global.exportedConst, "exported const");
  Assert.equal(global.notExportedVar, "not exported var");
  Assert.equal(global.notExportedFunction(), "not exported function");
  Assert.equal(global.notExportedLet, "not exported let");
  Assert.equal(global.notExportedConst, "not exported const");

  const expectedNames = [
    "EXPORTED_SYMBOLS",
    "exportedVar",
    "exportedFunction",
    "exportedLet",
    "exportedConst",
    "notExportedVar",
    "notExportedFunction",
    "notExportedLet",
    "notExportedConst",
  ];

  testReadProxyOps(global, expectedNames, {
    writable: false,
    enumerable: true,
    configurable: false,
  });
  testWriteProxyOps(global, expectedNames);

  Assert.equal(exports.exportedVar, "exported var");
  Assert.equal(exports.exportedFunction(), "exported function");
  Assert.equal(exports.exportedLet, "exported let");
  Assert.equal(exports.exportedConst, "exported const");
  Assert.equal(exports.notExportedVar, undefined);
  Assert.equal(exports.notExportedFunction, undefined);
  Assert.equal(exports.notExportedLet, undefined);
  Assert.equal(exports.notExportedConst, undefined);
});

add_task(function test_Cu_import_not_exported_shim() {
  // `exports` and `global` properties for shim case.
  // Not-exported variables should be visible in global.

  const exports = {};
  const global = Components.utils.import(
    "resource://test/esmified-not-exported.jsm",
    exports
  );

  Assert.equal(global.exportedVar, "exported var");
  Assert.equal(global.exportedFunction(), "exported function");
  Assert.equal(global.exportedLet, "exported let");
  Assert.equal(global.exportedConst, "exported const");

  Assert.equal(global.notExportedVar, "not exported var");
  Assert.equal(global.notExportedFunction(), "not exported function");
  Assert.equal(global.notExportedLet, "not exported let");
  Assert.equal(global.notExportedConst, "not exported const");

  const expectedNames = [
    "exportedVar",
    "exportedFunction",
    "exportedLet",
    "exportedConst",
    "notExportedVar",
    "notExportedFunction",
    "notExportedLet",
    "notExportedConst",
  ];

  testReadProxyOps(global, expectedNames, {
    writable: false,
    enumerable: true,
    configurable: false,
  });
  testWriteProxyOps(global, expectedNames);

  Assert.equal(exports.exportedVar, "exported var");
  Assert.equal(exports.exportedFunction(), "exported function");
  Assert.equal(exports.exportedLet, "exported let");
  Assert.equal(exports.exportedConst, "exported const");
  Assert.equal(exports.notExportedVar, undefined);
  Assert.equal(exports.notExportedFunction, undefined);
  Assert.equal(exports.notExportedLet, undefined);
  Assert.equal(exports.notExportedConst, undefined);

  const desc = Object.getOwnPropertyDescriptor(global, "*namespace*");
  Assert.ok(!desc, `*namespace* special binding should not be exposed`);
  Assert.equal("*namespace*" in global, false,
               `*namespace* special binding should not be exposed`);
  Assert.equal(global["*namespace*"], undefined,
               `*namespace* special binding should not be exposed`);
});

add_task(function test_Cu_isModuleLoaded_shim() {
  Assert.equal(Cu.isModuleLoaded("resource://test/esmified-5.jsm"), false);
  Assert.equal(Cu.isModuleLoaded("resource://test/esmified-5.js"), false);
  Assert.equal(Cu.isModuleLoaded("resource://test/esmified-5.jsm.js"), false);
  Assert.equal(Cu.loadedModules.includes("resource://test/esmified-5.jsm"), false);
  Assert.equal(Cu.isModuleLoaded("resource://test/esmified-5.sys.mjs"), false);
  Assert.equal(Cu.loadedModules.includes("resource://test/esmified-5.sys.mjs"), false);

  Cu.import("resource://test/esmified-5.jsm", {});

  Assert.equal(Cu.isModuleLoaded("resource://test/esmified-5.jsm"), true);
  Assert.equal(Cu.isModuleLoaded("resource://test/esmified-5.js"), true);
  Assert.equal(Cu.isModuleLoaded("resource://test/esmified-5.jsm.js"), true);
  Assert.equal(Cu.loadedModules.includes("resource://test/esmified-5.jsm"), true);

  // This is false because Cu.isModuleLoaded does not support ESM directly
  // (bug 1768819)
  Assert.equal(Cu.isModuleLoaded("resource://test/esmified-5.sys.mjs"), false);
  Assert.equal(Cu.loadedModules.includes("resource://test/esmified-5.sys.mjs"), false);
});

add_task(function test_Cu_isModuleLoaded_no_shim() {
  Assert.equal(Cu.isModuleLoaded("resource://test/esmified-6.jsm"), false);
  Assert.equal(Cu.isModuleLoaded("resource://test/esmified-6.js"), false);
  Assert.equal(Cu.isModuleLoaded("resource://test/esmified-6.jsm.js"), false);
  Assert.equal(Cu.loadedModules.includes("resource://test/esmified-6.jsm"), false);
  Assert.equal(Cu.loadedModules.includes("resource://test/esmified-6.js"), false);
  Assert.equal(Cu.loadedModules.includes("resource://test/esmified-6.jsm.js"), false);
  Assert.equal(Cu.isModuleLoaded("resource://test/esmified-6.sys.mjs"), false);
  Assert.equal(Cu.loadedModules.includes("resource://test/esmified-6.sys.mjs"), false);

  ChromeUtils.importModule("resource://test/esmified-6.sys.mjs");

  // Regardless of whether the ESM is loaded by shim or not,
  // query that accesses the ESM-ified module returns the existence of
  // ESM.
  Assert.equal(Cu.isModuleLoaded("resource://test/esmified-6.jsm"), true);
  Assert.equal(Cu.isModuleLoaded("resource://test/esmified-6.js"), true);
  Assert.equal(Cu.isModuleLoaded("resource://test/esmified-6.jsm.js"), true);
  Assert.equal(Cu.loadedModules.includes("resource://test/esmified-6.jsm"), true);

  // This is false because shim always use *.jsm.
  Assert.equal(Cu.loadedModules.includes("resource://test/esmified-6.js"), false);
  Assert.equal(Cu.loadedModules.includes("resource://test/esmified-6.jsm.js"), false);

  // This is false because Cu.isModuleLoaded does not support ESM directly
  // (bug 1768819)
  Assert.equal(Cu.isModuleLoaded("resource://test/esmified-6.sys.mjs"), false);
  Assert.equal(Cu.loadedModules.includes("resource://test/esmified-6.sys.mjs"), false);
});
