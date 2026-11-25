// SPDX-License-Identifier: MPL-2.0

export async function discoverTestModules(includePages: boolean) {
  const tests = {
    ...import.meta.glob(
      "#features-chrome/**/test/**/*.test.{ts,mts,tsx,js,mjs,jsx}",
    ),
    ...import.meta.glob(
      "#features-modules/**/*.test.{ts,mts,tsx,js,mjs,jsx}",
    ),
  };
  if (includePages) {
    Object.assign(tests, (await import("./pages_tests.ts")).getPagesTests());
  }
  return tests;
}
