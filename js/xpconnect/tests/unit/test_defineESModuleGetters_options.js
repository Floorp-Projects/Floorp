/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function testShared() {
  const lazy1 = {};
  const lazy2 = {};

  ChromeUtils.defineESModuleGetters(lazy1, {
    GetX: "resource://test/esm_lazy-1.sys.mjs",
  });

  ChromeUtils.defineESModuleGetters(lazy2, {
    GetX: "resource://test/esm_lazy-1.sys.mjs",
  }, {
    global: "shared",
  });

  Assert.equal(lazy1.GetX, lazy2.GetX);

  const ns = ChromeUtils.importESModule("resource://test/esm_lazy-1.sys.mjs");

  Assert.equal(ns.GetX, lazy1.GetX);
  Assert.equal(ns.GetX, lazy2.GetX);
});

add_task(async function testDevTools() {
  const lazy = {};

  ChromeUtils.defineESModuleGetters(lazy, {
    GetX: "resource://test/esm_lazy-1.sys.mjs",
  }, {
    global: "devtools",
  });

  lazy.GetX; // delazify before import.

  const ns = ChromeUtils.importESModule("resource://test/esm_lazy-1.sys.mjs", {
    global: "devtools",
  });

  Assert.equal(ns.GetX, lazy.GetX);
});

add_task(async function testSandbox() {
  const uri = "http://example.com/";
  const window = createContentWindow(uri);
  const sandboxOpts = {
    sandboxPrototype: window,
    wantGlobalProperties: ["ChromeUtils"],
  };
  const sb = new Cu.Sandbox(uri, sandboxOpts);

  const result = Cu.evalInSandbox(`
  const lazy = {};

  ChromeUtils.defineESModuleGetters(lazy, {
    GetX: "resource://test/esm_lazy-1.sys.mjs",
  }, {
    global: "current",
  });

  lazy.GetX; // delazify before import.

  const ns = ChromeUtils.importESModule("resource://test/esm_lazy-1.sys.mjs", {
    global: "current",
  });

  ns.GetX == lazy.GetX;
`, sb);

  Assert.ok(result);
});

add_task(async function testWindow() {
  const win1 = createChromeWindow();

  const result = win1.eval(`
  const lazy = {};

  ChromeUtils.defineESModuleGetters(lazy, {
    GetX: "resource://test/esm_lazy-1.sys.mjs",
  }, {
    global: "current",
  });

  lazy.GetX; // delazify before import.

  const ns = ChromeUtils.importESModule("resource://test/esm_lazy-1.sys.mjs", {
    global: "current",
  });

  ns.GetX == lazy.GetX;
`);

  Assert.ok(result);
});
