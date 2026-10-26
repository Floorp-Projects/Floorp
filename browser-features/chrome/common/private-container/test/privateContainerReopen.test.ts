// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import { FloorpPrivateContainer } from "../browser-private-container.tsx";
import { PrivateContainer } from "../PrivateContainer.ts";

async function until(predicate: () => boolean, message: string): Promise<void> {
  const end = Date.now() + 15000;
  while (Date.now() < end) {
    if (predicate()) return;
    await new Promise((resolve) => globalThis.setTimeout(resolve, 25));
  }
  assert(predicate(), message);
}

async function testNativePrivateReopenPositionAndStaleContext(): Promise<void> {
  const gb = globalThis.gBrowser;
  const initial = new Set(gb.tabs);
  const originalSelected = gb.selectedTab;
  const originalContext = globalThis.TabContextMenu.contextTab;
  PrivateContainer.StartupCreatePrivateContainer();
  const privateId = PrivateContainer.getPrivateContainerUserContextId();
  assert(
    typeof privateId === "number" && privateId > 0,
    "Private container exists",
  );
  const initiallyHasPrivateTab = Array.from(initial).some((tab) =>
    Number(tab.getAttribute("usercontextid")) === privateId
  );
  let privateClearNotifications = 0;
  const clearObserver = {
    observe(_subject: unknown, _topic: string, data: string | null) {
      try {
        if (JSON.parse(data ?? "{}").userContextId === privateId) {
          privateClearNotifications++;
        }
      } catch {
        // Only the private-container origin-attribute notification is relevant.
      }
    },
  };
  const nonce = crypto.randomUUID();
  const urls = [0, 1, 2, 3].map((n) =>
    `about:blank#floorp-private-position-${nonce}-${n}`
  );
  const tabs = urls.map((url) =>
    gb.addTab(url, {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      userContextId: 0,
    })
  );
  Services.obs.addObserver(clearObserver, "clear-origin-attributes-data");
  try {
    await until(
      () =>
        tabs.every((tab, i) => tab.linkedBrowser?.currentURI?.spec === urls[i]),
      "Native fixture tabs finish navigation",
    );
    const target = tabs[2];
    gb.selectedTab = target;
    globalThis.TabContextMenu.contextTab = target;
    const before = Array.from(gb.tabs);
    const position = before.indexOf(target);
    FloorpPrivateContainer.reopenInPrivateContainer();
    await until(
      () => !gb.tabs.includes(target),
      "Private reopen removes its source tab",
    );
    const created = Array.from(gb.tabs).filter((tab) => !before.includes(tab));
    assertEquals(created.length, 1, "Private reopen creates one replacement");
    const replacement = created[0];
    assertEquals(
      Array.from(gb.tabs).indexOf(replacement),
      position,
      "Private replacement retains the native flat tab position",
    );
    assertEquals(
      gb.tabs[position - 1],
      before[position - 1],
      "Left neighbor remains adjacent",
    );
    assertEquals(
      gb.tabs[position + 1],
      before[position + 1],
      "Right neighbor remains adjacent",
    );
    assertEquals(
      Number(replacement.getAttribute("usercontextid")),
      privateId,
      "Replacement uses the private container",
    );
    assertEquals(
      gb.selectedTab,
      replacement,
      "Selected source transfers to replacement",
    );
    await until(
      () => !target.isConnected,
      "Original context tab is fully detached",
    );
    const after = Array.from(gb.tabs);
    globalThis.TabContextMenu.contextTab = target;
    FloorpPrivateContainer.reopenInPrivateContainer();
    assertEquals(
      gb.tabs.length,
      after.length,
      "Stale context creates no new tab",
    );
    assert(
      gb.tabs.every((tab, i) => tab === after[i]),
      "Stale context cannot reorder or remove current tabs",
    );
  } finally {
    try {
      globalThis.TabContextMenu.contextTab = originalContext;
      if (originalSelected.isConnected && gb.tabs.includes(originalSelected)) {
        gb.selectedTab = originalSelected;
      }
      for (const tab of Array.from(gb.tabs)) {
        if (!initial.has(tab)) gb.removeTab(tab, { animate: false });
      }
      await until(
        () => Array.from(gb.tabs).every((tab) => initial.has(tab)),
        "All native private-reopen fixture tabs close",
      );
      // Production removeDataIfPrivateContainerTabNotExist schedules each
      // TabClose clear after 400ms. Queue this barrier after all of this
      // fixture's closes, beyond that delay, so none of its SessionStore
      // clear-origin-attributes-data mutations can reach the next fixture.
      // ClearDataService sends that notification synchronously when called.
      await new Promise((resolve) => globalThis.setTimeout(resolve, 401));
      if (!initiallyHasPrivateTab) {
        assert(
          privateClearNotifications > 0,
          "The real private-container clear notification settles before the next test",
        );
      } else {
        assertEquals(
          privateClearNotifications,
          0,
          "An existing private tab keeps its container data after fixture cleanup",
        );
      }
    } finally {
      Services.obs.removeObserver(
        clearObserver,
        "clear-origin-attributes-data",
      );
    }
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("privateContainerReopen.test.ts", [{
    name:
      "native private-container reopen preserves position and rejects stale context",
    fn: testNativePrivateReopenPositionAndStaleContext,
  }]);
}
