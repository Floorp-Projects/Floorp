// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import { tabSessionState, workspaceSessionLabel } from "../session-info.ts";
import type { SessionIndicatorTab } from "../types.ts";
import { getWorkspaceSessionIdentityService } from "../../workspaces/utils/workspace-sessions.ts";

function testActualBrowserContextAndPending(): void {
  const identities = getWorkspaceSessionIdentityService();
  const identity = identities.create(
    "Actual session badge",
    "fingerprint",
    "blue",
  );
  const tab = document.createXULElement(
    "tab",
  ) as unknown as SessionIndicatorTab;
  const browser = document.createXULElement("box");
  Reflect.set(tab, "linkedBrowser", browser);
  try {
    // A stale tab attribute must not override the actual browser context.
    tab.setAttribute("usercontextid", String(identity.userContextId));
    browser.setAttribute("usercontextid", "0");
    const exception = tabSessionState(tab, identity.userContextId);
    assertEquals(exception.actualId, 0, "actual browser is authoritative");
    assert(
      exception.exceptional && !exception.pending,
      "standard exception is visible",
    );
    assert(
      workspaceSessionLabel("Workspace", identity.userContextId).includes(
        "Actual session badge",
      ),
      "workspace label names its default session",
    );
    browser.setAttribute("usercontextid", String(identity.userContextId));
    assertEquals(
      tabSessionState(tab, identity.userContextId).exceptional,
      false,
      "matching context has no exception",
    );
    Reflect.set(browser, "browsingContext", {
      originAttributes: { userContextId: 0 },
    });
    assertEquals(
      tabSessionState(tab, identity.userContextId).actualId,
      0,
      "native origin attributes override an old browser attribute",
    );
    Reflect.set(browser, "browsingContext", {
      originAttributes: { userContextId: 2147483600 },
    });
    const removed = tabSessionState(tab, identity.userContextId);
    assert(
      removed.unavailable && !removed.pending,
      "removing a definition does not pretend an already loaded tab is paused",
    );
    tab.setAttribute("floorpPendingSessionId", "2147483600");
    const held = tabSessionState(tab, identity.userContextId);
    assert(held.pending, "a placeholder is visibly pending");
    assertEquals(
      held.actualId,
      2147483600,
      "placeholder names the original unavailable context",
    );
  } finally {
    identities.remove(identity.userContextId);
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("sessionInfo.test.ts", [{
    name:
      "indicators use the actual browser and retain pending session identity",
    fn: testActualBrowserContextAndPending,
  }]);
}
