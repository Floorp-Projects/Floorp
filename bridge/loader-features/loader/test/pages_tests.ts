// SPDX-License-Identifier: MPL-2.0

type LazyModule = () => Promise<unknown>;
type EagerModule = Record<string, unknown>;

export function getPagesTests(): Record<string, LazyModule> {
  // Pages tests use eager loading to avoid @fs dynamic import issues in Firefox.
  // Keep this in a separate module so chrome/esm-only filtered runs do not
  // eagerly import unrelated pages tests.
  const pagesTests = import.meta.glob(
    "#features-pages/pages-*/**/*.test.{ts,mts,tsx,js,mjs,jsx}",
    { eager: true },
  ) as Record<string, EagerModule>;

  const lazyTests: Record<string, LazyModule> = {};
  for (const [path, mod] of Object.entries(pagesTests)) {
    lazyTests[path] = () => Promise.resolve(mod);
  }
  return lazyTests;
}
