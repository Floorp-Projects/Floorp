// SPDX-License-Identifier: MPL-2.0

declare function add_task(fn: () => void | Promise<void>): void;

declare function registerCleanupFunction(
  fn: () => void | Promise<void>,
): void;

declare function ok(condition: unknown, message?: string): void;

declare function is(
  actual: unknown,
  expected: unknown,
  message?: string,
): void;

declare function isnot(
  actual: unknown,
  unexpected: unknown,
  message?: string,
): void;

declare function info(message: unknown): void;

declare function todo(condition: unknown, message?: string): void;

declare const Assert: {
  deepEqual(actual: unknown, expected: unknown, message?: string): void;
  equal(actual: unknown, expected: unknown, message?: string): void;
};

declare const BrowserTestUtils: {
  addTab(gBrowser: unknown, urlOrOptions?: unknown, options?: unknown): unknown;
  browserLoaded(
    browser: unknown,
    includeSubFrames?: unknown,
    wanted?: unknown,
  ): Promise<unknown>;
  browserStopped(browser: unknown): Promise<unknown>;
  openNewForegroundTab(
    gBrowser: unknown,
    opening?: unknown,
    waitForLoad?: unknown,
  ): Promise<unknown>;
  removeTab(tab: unknown, options?: unknown): Promise<void>;
  switchTab(gBrowser: unknown, tab: unknown): Promise<unknown>;
  withNewTab(opening: unknown, task: (browser: unknown) => unknown): Promise<
    unknown
  >;
};

declare function makeURI(uri: string): unknown;
