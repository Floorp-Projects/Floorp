/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function testSandbox() {
  const uri = "http://example.com/";
  const window = createContentWindow(uri);
  const sandboxOpts = {
    sandboxPrototype: window,
    wantGlobalProperties: ["ChromeUtils"],
  };
  const sb = new Cu.Sandbox(uri, sandboxOpts);

  Cu.evalInSandbox(`
globalThis["loaded"] = [];
var ns = ChromeUtils.importESModule("resource://test/non_shared_1.mjs", {
  global: "current",
});
`, sb);

  Assert.equal(Cu.evalInSandbox(`ns.getCounter();`, sb), 0);
  Cu.evalInSandbox(`ns.incCounter();`, sb);
  Assert.equal(Cu.evalInSandbox(`ns.getCounter();`, sb), 1);

  Assert.equal(Cu.evalInSandbox(`globalThis["loaded"].join(",")`, sb), "2,1");
});

add_task(async function testNoWindowSandbox() {
  // Sandbox without window doesn't have ScriptLoader, and Sandbox's
  // ModuleLoader cannot be created.
  const systemPrincipal = Components.Constructor(
    "@mozilla.org/systemprincipal;1",
    "nsIPrincipal"
  )();
  const sandboxOpts = {
    wantGlobalProperties: ["ChromeUtils"],
  };

  const sb = new Cu.Sandbox(systemPrincipal, sandboxOpts);

  let caught = false;
  try {
  Cu.evalInSandbox(`
ChromeUtils.importESModule("resource://test/non_shared_1.mjs", {
  global: "current",
});
`, sb);
  } catch (e) {
    caught = true;
    Assert.stringMatches(e.message, /No ModuleLoader found/);
  }
  Assert.ok(caught);
});

add_task(async function testWindow() {
  const win1 = createChromeWindow();

  win1.eval(`
globalThis["loaded"] = [];
var ns = ChromeUtils.importESModule("resource://test/non_shared_1.mjs", {
  global: "current",
});
`);

  Assert.equal(win1.eval(`ns.getCounter();`), 0);
  win1.eval(`ns.incCounter();`);
  Assert.equal(win1.eval(`ns.getCounter();`), 1);

  Assert.equal(win1.eval(`globalThis["loaded"].join(",")`), "2,1");
});

add_task(async function testReImport() {
  // Re-importing the same module should return the same thing.

  const uri = "http://example.com/";
  const window = createContentWindow(uri);
  const sandboxOpts = {
    sandboxPrototype: window,
    wantGlobalProperties: ["ChromeUtils"],
  };
  const sb = new Cu.Sandbox(uri, sandboxOpts);

  Cu.evalInSandbox(`
globalThis["loaded"] = [];
var ns = ChromeUtils.importESModule("resource://test/non_shared_1.mjs", {
  global: "current",
});
`, sb);

  Assert.equal(Cu.evalInSandbox(`ns.getCounter();`, sb), 0);
  Cu.evalInSandbox(`ns.incCounter();`, sb);
  Assert.equal(Cu.evalInSandbox(`ns.getCounter();`, sb), 1);

  Assert.equal(Cu.evalInSandbox(`globalThis["loaded"].join(",")`, sb), "2,1");

  Cu.evalInSandbox(`
var ns2 = ChromeUtils.importESModule("resource://test/non_shared_1.mjs", {
  global: "current",
});
`, sb);

  // The counter should be shared, and also not reset.
  Assert.equal(Cu.evalInSandbox(`ns2.getCounter();`, sb), 1);
  Cu.evalInSandbox(`ns2.incCounter();`, sb);
  Assert.equal(Cu.evalInSandbox(`ns.getCounter();`, sb), 2);
  Assert.equal(Cu.evalInSandbox(`ns2.getCounter();`, sb), 2);

  // The top-level script shouldn't be executed twice.
  Assert.equal(Cu.evalInSandbox(`globalThis["loaded"].join(",")`, sb), "2,1");
});

add_task(async function testNotFound() {
  // Importing non-existent file should throw error.

  const uri = "http://example.com/";
  const window = createContentWindow(uri);
  const sandboxOpts = {
    sandboxPrototype: window,
    wantGlobalProperties: ["ChromeUtils"],
  };
  const sb = new Cu.Sandbox(uri, sandboxOpts);

  let caught = false;
  try {
  Cu.evalInSandbox(`
ChromeUtils.importESModule("resource://test/not_found.mjs", {
  global: "current",
});
`, sb);
  } catch (e) {
    caught = true;
    Assert.stringMatches(e.message, /Failed to load/);
  }
  Assert.ok(caught);
});

add_task(async function testParseError() {
  // Parse error should be thrown.

  const uri = "http://example.com/";
  const window = createContentWindow(uri);
  const sandboxOpts = {
    sandboxPrototype: window,
    wantGlobalProperties: ["ChromeUtils"],
  };
  const sb = new Cu.Sandbox(uri, sandboxOpts);

  let caught = false;
  try {
  Cu.evalInSandbox(`
ChromeUtils.importESModule("resource://test/es6module_parse_error.js", {
  global: "current",
});
`, sb);
  } catch (e) {
    caught = true;
    Assert.stringMatches(e.message, /unexpected token/);
  }
  Assert.ok(caught);
});

add_task(async function testParseErrorInImport() {
  // Parse error in imported module should be thrown.

  const uri = "http://example.com/";
  const window = createContentWindow(uri);
  const sandboxOpts = {
    sandboxPrototype: window,
    wantGlobalProperties: ["ChromeUtils"],
  };
  const sb = new Cu.Sandbox(uri, sandboxOpts);

  let caught = false;
  try {
  Cu.evalInSandbox(`
ChromeUtils.importESModule("resource://test/es6module_parse_error_in_import.js", {
  global: "current",
});
`, sb);
  } catch (e) {
    caught = true;
    Assert.stringMatches(e.message, /unexpected token/);
  }
  Assert.ok(caught);
});

add_task(async function testImportError() {
  // Error for nested import should be thrown.

  const uri = "http://example.com/";
  const window = createContentWindow(uri);
  const sandboxOpts = {
    sandboxPrototype: window,
    wantGlobalProperties: ["ChromeUtils"],
  };
  const sb = new Cu.Sandbox(uri, sandboxOpts);

  let caught = false;
  try {
  Cu.evalInSandbox(`
ChromeUtils.importESModule("resource://test/es6module_import_error.js", {
  global: "current",
});
`, sb);
  } catch (e) {
    caught = true;
    Assert.stringMatches(e.message, /doesn't provide an export named/);
  }
  Assert.ok(caught);
});

add_task(async function testExecutionError() {
  // Error while execution the top-level script should be thrown.

  const uri = "http://example.com/";
  const window = createContentWindow(uri);
  const sandboxOpts = {
    sandboxPrototype: window,
    wantGlobalProperties: ["ChromeUtils"],
  };
  const sb = new Cu.Sandbox(uri, sandboxOpts);

  let caught = false;
  try {
  Cu.evalInSandbox(`
ChromeUtils.importESModule("resource://test/es6module_throws.js", {
  global: "current",
});
`, sb);
  } catch (e) {
    caught = true;
    Assert.stringMatches(e.message, /foobar/);
  }
  Assert.ok(caught);

  // Re-import should throw the same error.

  caught = false;
  try {
  Cu.evalInSandbox(`
ChromeUtils.importESModule("resource://test/es6module_throws.js", {
  global: "current",
});
`, sb);
  } catch (e) {
    caught = true;
    Assert.stringMatches(e.message, /foobar/);
  }
  Assert.ok(caught);
});

add_task(async function testImportNestShared() {
  // Importing system ESM should work.

  const win1 = createChromeWindow();

  const result = win1.eval(`
const { func1 } = ChromeUtils.importESModule("resource://test/non_shared_nest_import_shared_1.mjs", {
  global: "current",
});
func1();
`);

  Assert.equal(result, 27);
});

add_task(async function testImportNestNonSharedSame() {
  // For the same global, nested import for non-shared global is allowed while
  // executing top-level script.

  const win1 = createChromeWindow();

  const result = win1.eval(`
const { func } = ChromeUtils.importESModule("resource://test/non_shared_nest_import_non_shared_1.mjs", {
  global: "current",
});
func();
`);
  Assert.equal(result, 10);
});

add_task(async function testImportNestNonSharedDifferent() {
  // For the different globals, nested import for non-shared global isn't
  // allowed while executing top-level script.

  const win1 = createChromeWindow();

  const uri = "http://example.com/";
  const window = createContentWindow(uri);
  const sandboxOpts = {
    sandboxPrototype: window,
    wantGlobalProperties: ["ChromeUtils"],
  };
  win1.sb = new Cu.Sandbox(uri, sandboxOpts);

  let caught = false;
  try {
  win1.eval(`
ChromeUtils.importESModule("resource://test/non_shared_nest_import_non_shared_2.mjs", {
  global: "current",
});
`);
  } catch (e) {
    caught = true;
    Assert.stringMatches(e.message, /cannot be used for different global/);
  }
  Assert.ok(caught);
});

add_task(async function testImportNestNonSharedAfterImport() {
  // Nested import for non-shared global is allowed after the import, both for
  // the same and different globals.

  const win1 = createChromeWindow();

  const uri = "http://example.com/";
  const window = createContentWindow(uri);
  const sandboxOpts = {
    sandboxPrototype: window,
    wantGlobalProperties: ["ChromeUtils"],
  };
  win1.sb = new Cu.Sandbox(uri, sandboxOpts);

  const result = win1.eval(`
const { func3 } = ChromeUtils.importESModule("resource://test/non_shared_nest_import_non_shared_3.mjs", {
  global: "current",
});

// Nested import happens here.
func3();
`);
  Assert.equal(result, 22);
});

add_task(async function testIsolationWithSandbox() {
  // Modules should be isolated for each sandbox.

  const uri = "http://example.com/";
  const window = createContentWindow(uri);
  const sandboxOpts = {
    sandboxPrototype: window,
    wantGlobalProperties: ["ChromeUtils"],
  };
  const sb1 = new Cu.Sandbox(uri, sandboxOpts);
  const sb2 = new Cu.Sandbox(uri, sandboxOpts);
  const sb3 = new Cu.Sandbox(uri, sandboxOpts);

  // Verify modules in 2 sandboxes are isolated.

  Cu.evalInSandbox(`
globalThis["loaded"] = [];
var ns = ChromeUtils.importESModule("resource://test/non_shared_1.mjs", {
  global: "current",
});
`, sb1);
  Cu.evalInSandbox(`
globalThis["loaded"] = [];
var ns = ChromeUtils.importESModule("resource://test/non_shared_1.mjs", {
  global: "current",
});
`, sb2);

  Assert.equal(Cu.evalInSandbox(`ns.getCounter();`, sb1), 0);
  Cu.evalInSandbox(`ns.incCounter();`, sb1);
  Assert.equal(Cu.evalInSandbox(`ns.getCounter();`, sb1), 1);

  Assert.equal(Cu.evalInSandbox(`globalThis["loaded"].join(",")`, sb1), "2,1");

  Assert.equal(Cu.evalInSandbox(`ns.getCounter();`, sb2), 0);
  Cu.evalInSandbox(`ns.incCounter();`, sb2);
  Assert.equal(Cu.evalInSandbox(`ns.getCounter();`, sb2), 1);

  Assert.equal(Cu.evalInSandbox(`globalThis["loaded"].join(",")`, sb2), "2,1");

  // Verify importing after any modification to different global doesn't affect.

  const ns3 = Cu.evalInSandbox(`
globalThis["loaded"] = [];
var ns = ChromeUtils.importESModule("resource://test/non_shared_1.mjs", {
  global: "current",
});
`, sb3);

  Assert.equal(Cu.evalInSandbox(`ns.getCounter();`, sb3), 0);
  Cu.evalInSandbox(`ns.incCounter();`, sb3);
  Assert.equal(Cu.evalInSandbox(`ns.getCounter();`, sb3), 1);

  Assert.equal(Cu.evalInSandbox(`globalThis["loaded"].join(",")`, sb3), "2,1");

  // Verify yet another modification are still isolated.

  Assert.equal(Cu.evalInSandbox(`ns.getCounter();`, sb1), 1);
  Cu.evalInSandbox(`ns.incCounter();`, sb1);
  Assert.equal(Cu.evalInSandbox(`ns.getCounter();`, sb1), 2);

  Assert.equal(Cu.evalInSandbox(`ns.getCounter();`, sb2), 1);
  Cu.evalInSandbox(`ns.incCounter();`, sb2);
  Cu.evalInSandbox(`ns.incCounter();`, sb2);
  Assert.equal(Cu.evalInSandbox(`ns.getCounter();`, sb2), 3);

  Assert.equal(Cu.evalInSandbox(`ns.getCounter();`, sb3), 1);
  Cu.evalInSandbox(`ns.incCounter();`, sb3);
  Cu.evalInSandbox(`ns.incCounter();`, sb3);
  Cu.evalInSandbox(`ns.incCounter();`, sb3);
  Assert.equal(Cu.evalInSandbox(`ns.getCounter();`, sb3), 4);

  // Verify the module's `globalThis` points the target global.

  Cu.evalInSandbox(`ns.putCounter();`, sb1);
  Cu.evalInSandbox(`ns.putCounter();`, sb2);
  Cu.evalInSandbox(`ns.putCounter();`, sb3);

  const counter1 = Cu.evalInSandbox(`globalThis["counter"]`, sb1);
  Assert.equal(counter1, 2);
  const counter2 = Cu.evalInSandbox(`globalThis["counter"]`, sb2);
  Assert.equal(counter2, 3);
  const counter3 = Cu.evalInSandbox(`globalThis["counter"]`, sb3);
  Assert.equal(counter3, 4);
});

add_task(async function testIsolationWithWindow() {
  // Modules should be isolated for each window.

  const win1 = createChromeWindow();
  const win2 = createChromeWindow();
  const win3 = createChromeWindow();

  // Verify modules in 2 sandboxes are isolated.

  win1.eval(`
globalThis["loaded"] = [];
var ns = ChromeUtils.importESModule("resource://test/non_shared_1.mjs", {
  global: "current",
});
`);
  win2.eval(`
globalThis["loaded"] = [];
var ns = ChromeUtils.importESModule("resource://test/non_shared_1.mjs", {
  global: "current",
});
`);

  Assert.equal(win1.eval(`ns.getCounter();`), 0);
  win1.eval(`ns.incCounter();`);
  Assert.equal(win1.eval(`ns.getCounter();`), 1);

  Assert.equal(win1.eval(`globalThis["loaded"].join(",")`), "2,1");

  Assert.equal(win2.eval(`ns.getCounter();`), 0);
  win2.eval(`ns.incCounter();`);
  Assert.equal(win2.eval(`ns.getCounter();`), 1);

  Assert.equal(win2.eval(`globalThis["loaded"].join(",")`), "2,1");

  // Verify importing after any modification to different global doesn't affect.

  const ns3 = win3.eval(`
globalThis["loaded"] = [];
var ns = ChromeUtils.importESModule("resource://test/non_shared_1.mjs", {
  global: "current",
});
`);

  Assert.equal(win3.eval(`ns.getCounter();`), 0);
  win3.eval(`ns.incCounter();`);
  Assert.equal(win3.eval(`ns.getCounter();`), 1);

  Assert.equal(win3.eval(`globalThis["loaded"].join(",")`), "2,1");

  // Verify yet another modification are still isolated.

  Assert.equal(win1.eval(`ns.getCounter();`), 1);
  win1.eval(`ns.incCounter();`);
  Assert.equal(win1.eval(`ns.getCounter();`), 2);

  Assert.equal(win2.eval(`ns.getCounter();`), 1);
  win2.eval(`ns.incCounter();`);
  win2.eval(`ns.incCounter();`);
  Assert.equal(win2.eval(`ns.getCounter();`), 3);

  Assert.equal(win3.eval(`ns.getCounter();`), 1);
  win3.eval(`ns.incCounter();`);
  win3.eval(`ns.incCounter();`);
  win3.eval(`ns.incCounter();`);
  Assert.equal(win3.eval(`ns.getCounter();`), 4);

  // Verify the module's `globalThis` points the target global.

  win1.eval(`ns.putCounter();`);
  win2.eval(`ns.putCounter();`);
  win3.eval(`ns.putCounter();`);

  const counter1 = win1.eval(`globalThis["counter"]`);
  Assert.equal(counter1, 2);
  const counter2 = win2.eval(`globalThis["counter"]`);
  Assert.equal(counter2, 3);
  const counter3 = win3.eval(`globalThis["counter"]`);
  Assert.equal(counter3, 4);
});

add_task(async function testSyncImportBeforeAsyncImportTopLevel() {
  const window = createChromeWindow();

  window.eval(`
globalThis["loaded"] = [];
var ns = ChromeUtils.importESModule("resource://test/non_shared_1.mjs", {
  global: "current",
});
`);

  Assert.equal(window.eval(`ns.getCounter();`), 0);
  window.eval(`ns.incCounter();`);
  Assert.equal(window.eval(`ns.getCounter();`), 1);

  Assert.equal(window.eval(`globalThis["loaded"].join(",")`), "2,1");

  window.eval(`
var ns2 = null;
const nsPromise = import("resource://test/non_shared_1.mjs");
nsPromise.then(v => { ns2 = v; });
`);

  Services.tm.spinEventLoopUntil(
    "Wait until dynamic import finishes",
    () => window.eval(`ns2 !== null`)
  );

  Assert.equal(window.eval(`ns2.getCounter();`), 1);
  window.eval(`ns2.incCounter();`);
  Assert.equal(window.eval(`ns2.getCounter();`), 2);
  Assert.equal(window.eval(`ns.getCounter();`), 2);

  Assert.equal(window.eval(`globalThis["loaded"].join(",")`), "2,1");
});

add_task(async function testSyncImportBeforeAsyncImportDependency() {
  const window = createChromeWindow();

  window.eval(`
globalThis["loaded"] = [];
var ns = ChromeUtils.importESModule("resource://test/non_shared_1.mjs", {
  global: "current",
});
`);

  Assert.equal(window.eval(`ns.getCounter();`), 0);
  window.eval(`ns.incCounter();`);
  Assert.equal(window.eval(`ns.getCounter();`), 1);

  Assert.equal(window.eval(`globalThis["loaded"].join(",")`), "2,1");

  window.eval(`
var ns2 = null;
const nsPromise = import("resource://test/import_non_shared_1.mjs");
nsPromise.then(v => { ns2 = v; });
`);

  Services.tm.spinEventLoopUntil(
    "Wait until dynamic import finishes",
    () => window.eval(`ns2 !== null`)
  );

  Assert.equal(window.eval(`ns2.getCounter();`), 1);
  window.eval(`ns2.incCounter();`);
  Assert.equal(window.eval(`ns2.getCounter();`), 2);
  Assert.equal(window.eval(`ns.getCounter();`), 2);

  Assert.equal(window.eval(`globalThis["loaded"].join(",")`), "2,1");
});

add_task(async function testSyncImportAfterAsyncImportTopLevel() {
  const window = createChromeWindow();

  window.eval(`
var ns = null;
globalThis["loaded"] = [];
const nsPromise = import("resource://test/non_shared_1.mjs");
nsPromise.then(v => { ns = v; });
`);

  Services.tm.spinEventLoopUntil(
    "Wait until dynamic import finishes",
    () => window.eval(`ns !== null`)
  );

  Assert.equal(window.eval(`ns.getCounter();`), 0);
  window.eval(`ns.incCounter();`);
  Assert.equal(window.eval(`ns.getCounter();`), 1);

  Assert.equal(window.eval(`globalThis["loaded"].join(",")`), "2,1");

  window.eval(`
var ns2 = ChromeUtils.importESModule("resource://test/non_shared_1.mjs", {
  global: "current",
});
`);

  Assert.equal(window.eval(`ns2.getCounter();`), 1);
  window.eval(`ns2.incCounter();`);
  Assert.equal(window.eval(`ns2.getCounter();`), 2);
  Assert.equal(window.eval(`ns.getCounter();`), 2);

  Assert.equal(window.eval(`globalThis["loaded"].join(",")`), "2,1");
});

add_task(async function testSyncImportAfterAsyncImportDependency() {
  const window = createChromeWindow();

  window.eval(`
var ns = null;
globalThis["loaded"] = [];
const nsPromise = import("resource://test/non_shared_1.mjs");
nsPromise.then(v => { ns = v; });
`);

  Services.tm.spinEventLoopUntil(
    "Wait until dynamic import finishes",
    () => window.eval(`ns !== null`)
  );

  Assert.equal(window.eval(`ns.getCounter();`), 0);
  window.eval(`ns.incCounter();`);
  Assert.equal(window.eval(`ns.getCounter();`), 1);

  Assert.equal(window.eval(`globalThis["loaded"].join(",")`), "2,1");

  window.eval(`
var ns2 = ChromeUtils.importESModule("resource://test/import_non_shared_1.mjs", {
  global: "current",
});
`);

  Assert.equal(window.eval(`ns2.getCounter();`), 1);
  window.eval(`ns2.incCounter();`);
  Assert.equal(window.eval(`ns2.getCounter();`), 2);
  Assert.equal(window.eval(`ns.getCounter();`), 2);

  Assert.equal(window.eval(`globalThis["loaded"].join(",")`), "2,1");
});

add_task(async function testSyncImportWhileAsyncImportTopLevel() {
  const window = createChromeWindow();

  window.eval(`
var ns = null;
globalThis["loaded"] = [];
const nsPromise = import("resource://test/non_shared_1.mjs");
nsPromise.then(v => { ns = v; });
`);

  window.eval(`
var ns2 = ChromeUtils.importESModule("resource://test/non_shared_1.mjs", {
  global: "current",
});
`);

  Assert.equal(window.eval(`ns2.getCounter();`), 0);
  window.eval(`ns2.incCounter();`);
  Assert.equal(window.eval(`ns2.getCounter();`), 1);

  Services.tm.spinEventLoopUntil(
    "Wait until dynamic import finishes",
    () => window.eval(`ns !== null`)
  );

  Assert.equal(window.eval(`ns.getCounter();`), 1);
  window.eval(`ns.incCounter();`);
  Assert.equal(window.eval(`ns.getCounter();`), 2);
  Assert.equal(window.eval(`ns2.getCounter();`), 2);

  Assert.equal(window.eval(`globalThis["loaded"].join(",")`), "2,1");
});

add_task(async function testSyncImportWhileAsyncImportDependency() {
  const window = createChromeWindow();

  window.eval(`
var ns = null;
globalThis["loaded"] = [];
const nsPromise = import("resource://test/non_shared_1.mjs");
nsPromise.then(v => { ns = v; });
`);

  window.eval(`
var ns2 = ChromeUtils.importESModule("resource://test/import_non_shared_1.mjs", {
  global: "current",
});
`);

  Assert.equal(window.eval(`ns2.getCounter();`), 0);
  window.eval(`ns2.incCounter();`);
  Assert.equal(window.eval(`ns2.getCounter();`), 1);

  Services.tm.spinEventLoopUntil(
    "Wait until dynamic import finishes",
    () => window.eval(`ns !== null`)
  );

  Assert.equal(window.eval(`ns.getCounter();`), 1);
  window.eval(`ns.incCounter();`);
  Assert.equal(window.eval(`ns.getCounter();`), 2);
  Assert.equal(window.eval(`ns2.getCounter();`), 2);

  Assert.equal(window.eval(`globalThis["loaded"].join(",")`), "2,1");
});

add_task(async function testSyncImportBeforeAsyncImportTLA() {
  // Top-level-await is not supported by sync import.

  const window = createChromeWindow();

  let caught = false;

  try {
    window.eval(`
ChromeUtils.importESModule("resource://test/es6module_top_level_await.js", {
  global: "current",
});
`);
  } catch (e) {
    caught = true;
    Assert.stringMatches(e.message, /top level await is not supported/);
  }
  Assert.ok(caught);

  window.eval(`
var ns2 = null;
const nsPromise = import("resource://test/es6module_top_level_await.js");
nsPromise.then(v => { ns2 = v; });
`);

  Services.tm.spinEventLoopUntil(
    "Wait until dynamic import finishes",
    () => window.eval(`ns2 !== null`)
  );

  Assert.equal(window.eval(`ns2.foo();`), 10);
});

add_task(async function testSyncImportAfterAsyncImportTLA() {
  // Top-level-await is not supported by sync import, but if the module is
  // already imported, the existing module namespace is returned.

  const window = createChromeWindow();

  window.eval(`
var ns2 = null;
const nsPromise = import("resource://test/es6module_top_level_await.js");
nsPromise.then(v => { ns2 = v; });
`);

  Services.tm.spinEventLoopUntil(
    "Wait until dynamic import finishes",
    () => window.eval(`ns2 !== null`)
  );

  Assert.equal(window.eval(`ns2.foo();`), 10);

  window.eval(`
var ns = ChromeUtils.importESModule("resource://test/es6module_top_level_await.js", {
  global: "current",
});
`);

  Assert.equal(window.eval(`ns.foo();`), 10);
  Assert.equal(window.eval(`ns2.foo == ns.foo;`), true);
});

add_task(async function testSyncImportWhileAsyncImportTLA() {
  // Top-level-await is not supported by sync import, but if the module is
  // already fetching, ChromeUtils.importESModule waits for it and, the
  // async-imported module namespace is returned.

  const window = createChromeWindow();

  window.eval(`
var ns2 = null;
const nsPromise = import("resource://test/es6module_top_level_await.js");
nsPromise.then(v => { ns2 = v; });
`);

  window.eval(`
var ns = ChromeUtils.importESModule("resource://test/es6module_top_level_await.js", {
  global: "current",
});
`);

  Services.tm.spinEventLoopUntil(
    "Wait until dynamic import finishes",
    () => window.eval(`ns2 !== null`)
  );

  Assert.equal(window.eval(`ns2.foo();`), 10);
  Assert.equal(window.eval(`ns.foo();`), 10);
  Assert.equal(window.eval(`ns2.foo == ns.foo;`), true);
});
