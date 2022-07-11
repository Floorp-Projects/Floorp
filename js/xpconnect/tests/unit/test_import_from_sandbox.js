"use strict";

function makeSandbox() {
  return Cu.Sandbox(
    Services.scriptSecurityManager.getSystemPrincipal(),
    {
      wantXrays: false,
      wantGlobalProperties: ["ChromeUtils"],
      sandboxName: `Sandbox type used for ext-*.js  ExtensionAPI subscripts`,
    }
  );
}

// This test will fail (and should be removed) once the JSM shim is dropped.
add_task(function test_import_from_sandbox_using_shim() {
  let sandbox = makeSandbox();
  Object.assign(sandbox, {
    injected1: ChromeUtils.import("resource://test/esmified-1.jsm"),
  });

  Assert.equal(Cu.isModuleLoaded("resource://test/esmified-2.jsm"), false);

  Services.scriptloader.loadSubScript(
    `data:,
      "use strict";

      const shimmed1 = ChromeUtils.import("resource://test/esmified-1.jsm");
      const shimmed2 = ChromeUtils.import("resource://test/esmified-2.jsm");

      this.testResults = {
        shimmed1: shimmed1.obj.value,
        injected1: injected1.obj.value,
        sameInstance1: injected1 === shimmed1,
        shimmed2: shimmed2.obj.value,
      };
    `,
    sandbox
  );
  let tr = sandbox.testResults;

  Assert.equal(tr.injected1, 10, "Injected esmified-1.mjs has correct value.");
  Assert.equal(tr.shimmed1, 10, "Shim-imported esmified-1.jsm correct value.");
  Assert.ok(tr.sameInstance1, "Injected and imported are the same instance.");
  Assert.equal(tr.shimmed2, 10, "Shim-imported esmified-2.jsm correct value.");

  Assert.equal(Cu.isModuleLoaded("resource://test/esmified-2.jsm"), true);
});

// This tests the ESMification transition for extension API scripts.
add_task(function test_import_from_sandbox_transition() {
  let sandbox = makeSandbox();

  Object.assign(sandbox, {
    injected3: ChromeUtils.importESModule("resource://test/esmified-3.sys.mjs"),
  });

  Services.scriptloader.loadSubScript("resource://test/api_script.js", sandbox);
  let tr = sandbox.testResults;

  Assert.equal(tr.injected3, 16, "Injected esmified-3.mjs has correct value.");
  Assert.equal(tr.module3, 16, "Iimported esmified-3.mjs has correct value.");
  Assert.ok(tr.sameInstance3, "Injected and imported are the same instance.");
  Assert.equal(tr.module4, 14, "Iimported esmified-4.mjs has correct value.");
});

// Same as above, just using a PrecompiledScript.
add_task(async function test_import_from_sandbox_transition() {
  let sandbox = makeSandbox();

  Object.assign(sandbox, {
    injected3: ChromeUtils.importESModule("resource://test/esmified-3.sys.mjs"),
  });

  let script = await ChromeUtils.compileScript("resource://test/api_script.js");
  script.executeInGlobal(sandbox);
  let tr = sandbox.testResults;

  Assert.equal(tr.injected3, 22, "Injected esmified-3.mjs has correct value.");
  Assert.equal(tr.module3, 22, "Iimported esmified-3.mjs has correct value.");
  Assert.ok(tr.sameInstance3, "Injected and imported are the same instance.");
  Assert.equal(tr.module4, 18, "Iimported esmified-4.mjs has correct value.");
});
