// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import Workspaces from "../../workspaces/index.ts";
import type { BookmarkBrowserWindow } from "./bookmark-workspace-container-test-types.ts";
import { loadBookmarkURI } from "../layout/dom-manipulator.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

async function checkBookmarkContainer(
  modifier: "ctrlKey" | "metaKey" | "shiftKey",
  useContainer = true,
  privateWindow = false,
): Promise<void> {
  const ctx = Workspaces.getCtx(window);
  assert(ctx, "the browser initializes Workspaces before this test");
  const { ContextualIdentityService } = ChromeUtils.importESModule(
    "moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs",
  );
  const cid: number = useContainer
    ? ContextualIdentityService.create(
      "Bookmark regression",
      "fingerprint",
      "blue",
    )
      .userContextId
    : 0;
  const originalResolver = ctx.getCurrentWorkspaceUserContextId;
  const originalGetCtx = Workspaces.getCtx;
  let targetWindow = window as BookmarkBrowserWindow;
  let originalTab: XULElement | undefined;
  let previousTabs: Set<XULElement> | undefined;
  try {
    if (privateWindow) {
      targetWindow = (globalThis as unknown as {
        OpenBrowserWindow(options: { private: boolean }): BookmarkBrowserWindow;
      }).OpenBrowserWindow({ private: true });
      const deadline = Date.now() + 30_000;
      while (
        !targetWindow.gBrowserInit?.delayedStartupFinished &&
        Date.now() < deadline
      ) {
        await new Promise((resolve) => setTimeout(resolve, 50));
      }
      assert(
        targetWindow.gBrowserInit?.delayedStartupFinished,
        "private window initializes",
      );
      // Supply a real public workspace identity even for this private target;
      // the bookmark policy must reject it before creating the browser.
      Workspaces.getCtx = () => ctx;
    }
    const browserTabs = targetWindow.gBrowser;
    originalTab = browserTabs.selectedTab;
    previousTabs = new Set(browserTabs.tabs);
    ctx.getCurrentWorkspaceUserContextId = () => cid;
    assert(
      loadBookmarkURI(
        "data:text/html,bookmark-container-regression",
        new MouseEvent("click", {
          [modifier]: true,
        }),
        targetWindow,
      ),
      "the bookmark opens successfully",
    );
    const tab = browserTabs.tabs.find((candidate) =>
      !previousTabs?.has(candidate)
    );
    assert(tab?.linkedBrowser, "the bookmark creates a content browser");
    const browser = tab.linkedBrowser;
    const expectedCID = privateWindow ? 0 : cid;
    assert(browser.browsingContext, "the bookmark has a browsing context");
    assertEquals(
      Number(tab.getAttribute("usercontextid") || 0),
      expectedCID,
      "tab identity",
    );
    assertEquals(
      Number(browser.getAttribute("usercontextid") || 0),
      expectedCID,
      "browser identity",
    );
    assertEquals(
      browser.browsingContext.originAttributes.userContextId,
      expectedCID,
      "the browser is created in the workspace jar",
    );
    const deadline = Date.now() + 15_000;
    while (
      browser.currentURI.spec !==
        "data:text/html,bookmark-container-regression" && Date.now() < deadline
    ) {
      await new Promise((resolve) => setTimeout(resolve, 50));
    }
    assertEquals(
      browser.currentURI.spec,
      "data:text/html,bookmark-container-regression",
      "bookmark content loads",
    );
    const principal = browser.contentPrincipal as nsIPrincipal | undefined;
    assert(principal, "loaded bookmark content has a principal");
    assertEquals(
      principal.originAttributes.userContextId,
      expectedCID,
      "the content principal uses the same jar",
    );
    assertEquals(
      principal.originAttributes.privateBrowsingId,
      privateWindow ? 1 : 0,
      "bookmark preserves the target's browsing privacy",
    );
  } finally {
    Workspaces.getCtx = originalGetCtx;
    ctx.getCurrentWorkspaceUserContextId = originalResolver;
    if (privateWindow && targetWindow !== window) {
      targetWindow.close();
    } else if (previousTabs && originalTab) {
      for (const tab of [...gBrowser.tabs]) {
        if (!previousTabs.has(tab)) gBrowser.removeTab(tab);
      }
      gBrowser.selectedTab = originalTab;
    }
    if (cid) ContextualIdentityService.remove(cid);
  }
}

const tests: TestCase[] = [
  ...(["ctrlKey", "metaKey", "shiftKey"] as const).map((modifier) => ({
    name: `${modifier} bookmark creates its browser in the workspace container`,
    fn: () => checkBookmarkContainer(modifier),
  })),
  {
    name: "a workspace without a container uses the default jar",
    fn: () => checkBookmarkContainer("ctrlKey", false),
  },
  {
    name:
      "private modifier-click bookmarks keep context 0 and private origin attributes",
    fn: () => checkBookmarkContainer("ctrlKey", true, true),
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("bookmarkWorkspaceContainer.test.ts", tests);
}
