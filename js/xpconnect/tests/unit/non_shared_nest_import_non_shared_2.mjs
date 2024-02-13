Cu.evalInSandbox(`
ChromeUtils.importESModule("resource://test/non_shared_nest_import_non_shared_target_2.mjs", {
  global: "current",
});
`, globalThis["sb"]);
