// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  assertStringArrayEqual,
  runTests,
} from "../../../test/utils/test_harness.ts";
import {
  registerWebPanelBrowserId,
  unregisterWebPanelBrowserId,
  WEB_PANEL_BROWSER_IDS_SHARED_KEY,
} from "../utils/web-panel-browser-registry.ts";
import type { WritableSharedDataLike } from "../utils/web-panel-types.ts";

class FakeSharedData implements WritableSharedDataLike {
  values = new Map<string, unknown>();
  flushes = 0;

  get(key: string): unknown {
    return this.values.get(key);
  }
  set(key: string, value: unknown): void {
    this.values.set(key, value);
  }
  delete(key: string): void {
    this.values.delete(key);
  }
  flush(): void {
    this.flushes++;
  }
}

function ids(sharedData: FakeSharedData): string[] {
  const value = sharedData.get(WEB_PANEL_BROWSER_IDS_SHARED_KEY);
  return Array.isArray(value) ? value.map(String) : [];
}

function testIgnoresInvalidBrowserIds(): void {
  const sharedData = new FakeSharedData();
  for (const id of [0, -1, Number.NaN, 1.5]) {
    registerWebPanelBrowserId(id, sharedData);
  }
  assert(
    !sharedData.values.has(WEB_PANEL_BROWSER_IDS_SHARED_KEY),
    "uninitialized or invalid browserIds are never published",
  );
  assertEquals(sharedData.flushes, 0, "nothing is flushed");
}

function testRegisterAddsAndFlushes(): void {
  const sharedData = new FakeSharedData();
  registerWebPanelBrowserId(7, sharedData);
  registerWebPanelBrowserId(9, sharedData);
  assertStringArrayEqual(ids(sharedData), ["7", "9"], "both ids are listed");
  assertEquals(sharedData.flushes, 2, "every change is flushed immediately");
}

function testRegisterIsIdempotent(): void {
  const sharedData = new FakeSharedData();
  registerWebPanelBrowserId(7, sharedData);
  registerWebPanelBrowserId(7, sharedData);
  assertStringArrayEqual(ids(sharedData), ["7"], "id is listed once");
  assertEquals(sharedData.flushes, 1, "no-op registration does not flush");
}

function testUnregisterRemovesAndDeletesWhenEmpty(): void {
  const sharedData = new FakeSharedData();
  registerWebPanelBrowserId(7, sharedData);
  registerWebPanelBrowserId(9, sharedData);
  unregisterWebPanelBrowserId(7, sharedData);
  assertStringArrayEqual(ids(sharedData), ["9"], "only 9 remains");
  unregisterWebPanelBrowserId(9, sharedData);
  assert(
    !sharedData.values.has(WEB_PANEL_BROWSER_IDS_SHARED_KEY),
    "an empty list removes the key",
  );
  unregisterWebPanelBrowserId(9, sharedData);
  assertEquals(
    sharedData.flushes,
    2,
    "only registrations flush; removals wait for the idle flush",
  );
}

function testIgnoresMalformedValues(): void {
  const sharedData = new FakeSharedData();
  sharedData.set(WEB_PANEL_BROWSER_IDS_SHARED_KEY, ["x", 3, null]);
  registerWebPanelBrowserId(4, sharedData);
  assertStringArrayEqual(ids(sharedData), ["3", "4"], "non-numbers dropped");
}

export async function runAllTests(): Promise<void> {
  await runTests("webPanelBrowserRegistry.test.ts", [
    { name: "ignores invalid browserIds", fn: testIgnoresInvalidBrowserIds },
    { name: "register adds and flushes", fn: testRegisterAddsAndFlushes },
    { name: "register is idempotent", fn: testRegisterIsIdempotent },
    {
      name: "unregister removes ids and the empty key",
      fn: testUnregisterRemovesAndDeletesWhenEmpty,
    },
    { name: "ignores malformed values", fn: testIgnoresMalformedValues },
  ]);
}
