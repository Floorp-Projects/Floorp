// SPDX-License-Identifier: MPL-2.0

const tests = import.meta.glob("./**/*.test.ts");

(() => {
  for (const [path, module] of Object.entries(tests)) {
    (() => {
      module().catch((e) => {
        console.debug(`[nora@test] Failed to run test ${path}:`, e);
      });
    })();
  }
})();
