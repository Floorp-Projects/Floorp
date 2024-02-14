export function func3() {
  const { func3 } = ChromeUtils.importESModule("resource://test/non_shared_nest_import_non_shared_target_3.mjs", {
    global: "current",
  });

  const result = Cu.evalInSandbox(`
  const { func3 } = ChromeUtils.importESModule("resource://test/non_shared_nest_import_non_shared_target_3.mjs", {
    global: "current",
  });
  func3();
`, globalThis["sb"]);

  return func3() + result;
}
