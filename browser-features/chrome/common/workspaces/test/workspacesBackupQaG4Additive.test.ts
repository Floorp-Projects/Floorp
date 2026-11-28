// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
// QA group g4 (G-*): additive restore (workspaces disabled, or tabs-mode file).
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import {
  settle,
  waitFor,
  withWindows,
} from "./workspace-last-tab-test-utils.ts";
import {
  WORKSPACE_LAST_SHOW_ID,
  WORKSPACE_TAB_ATTRIBUTION_ID,
} from "../utils/workspaces-static-names.ts";
import { formatBackupFileName } from "../utils/workspaces-backup-helpers.ts";
import type { WorkspacesBackupFuncs } from "../utils/workspaces-backup-controller.ts";
import type { RestoreBackupResult } from "../utils/workspaces-backup-service.ts";
import type {
  WorkspaceBackupFile,
  WorkspaceBackupTab,
  WorkspaceBackupWindow,
} from "../utils/backup-types.ts";
import type { TestWindow } from "./workspace-last-tab-test-types.ts";

type QaWindow = TestWindow & {
  workspacesBackupFuncs?: WorkspacesBackupFuncs;
  workspacesFuncs?: TestWindow["workspacesFuncs"] & {
    isWorkspaceID(id: string): boolean;
  };
};

interface SessionStoreApi {
  getTabState(tab: XULElement): string;
}

const { SessionStore } = ChromeUtils.importESModule(
  "moz-src:///browser/components/sessionstore/SessionStore.sys.mjs",
) as { SessionStore: SessionStoreApi };

const PREFIX = `https://example.com/floorp-qa-g4-additive/${Date.now()}`;

function backupDir(): string {
  return PathUtils.join(PathUtils.profileDir, "workspaces", "backup");
}

function shortId(): string {
  return crypto.randomUUID().replace(/-/g, "").slice(0, 8);
}

/** Writes a hand-crafted backup file the way the service would. */
async function writeBackup(file: WorkspaceBackupFile): Promise<string> {
  const fileName = formatBackupFileName(file.capturedAt, shortId());
  await IOUtils.makeDirectory(backupDir(), { createAncestors: true });
  await IOUtils.write(
    PathUtils.join(backupDir(), fileName),
    new TextEncoder().encode(JSON.stringify(file)),
    { compress: true },
  );
  return fileName;
}

function tabsFile(windows: WorkspaceBackupWindow[]): WorkspaceBackupFile {
  return {
    version: 2,
    kind: "manual",
    mode: "tabs",
    capturedAt: Date.now() - 60_000,
    windows,
  };
}

function tab(
  url: string,
  extra: Partial<WorkspaceBackupTab> = {},
): WorkspaceBackupTab {
  return {
    state: { entries: [{ url, title: url }], index: 1 },
    title: url,
    url,
    pinned: false,
    isSelected: false,
    userContextId: 0,
    lastShownWorkspaceId: null,
    ...extra,
  };
}

function tabUrls(t: XULElement): string[] {
  const urls: string[] = [];
  try {
    const state = JSON.parse(SessionStore.getTabState(t)) as {
      entries?: { url?: string }[];
      userTypedValue?: string;
    };
    for (const entry of state.entries ?? []) urls.push(entry.url ?? "");
    if (state.userTypedValue) urls.push(state.userTypedValue);
  } catch {
    // ignore
  }
  const spec = (t as unknown as {
    linkedBrowser?: { currentURI?: { spec?: string } };
  }).linkedBrowser?.currentURI?.spec;
  if (spec) urls.push(spec);
  return urls;
}

function findTabs(win: QaWindow, url: string): XULElement[] {
  return [...win.gBrowser.tabs].filter((t) =>
    tabUrls(t as XULElement).includes(url)
  ) as XULElement[];
}

function browserWindows(): QaWindow[] {
  const windows: QaWindow[] = [];
  const enumerator = Services.wm.getEnumerator("navigator:browser");
  while (enumerator.hasMoreElements()) {
    const win = enumerator.getNext() as QaWindow;
    if (!win.closed) windows.push(win);
  }
  return windows;
}

function windowWithUrl(url: string): QaWindow | undefined {
  return browserWindows().find((win) => findTabs(win, url).length > 0);
}

async function preRestoreCount(funcs: WorkspacesBackupFuncs): Promise<number> {
  return (await funcs.listBackups()).filter((b) => b.kind === "pre-restore")
    .length;
}

/**
 * Removes every backup file, extra window and workspace attribution change a
 * case leaves behind.
 */
async function withCleanup(
  check: (track: (fileName: string) => void) => Promise<void>,
): Promise<void> {
  const harnessWindows = browserWindows();
  const attributions = new Map<XULElement, string | null>();
  for (const win of harnessWindows) {
    for (const t of win.gBrowser.tabs) {
      attributions.set(
        t as XULElement,
        (t as XULElement).getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID),
      );
    }
  }
  const funcs = harnessWindows[0]?.workspacesBackupFuncs;
  assert(funcs, "harness window exposes the backup API");
  const before = new Set((await funcs.listBackups()).map((b) => b.fileName));
  const tracked = new Set<string>();
  try {
    await check((fileName) => tracked.add(fileName));
  } finally {
    for (const win of browserWindows()) {
      if (!harnessWindows.includes(win)) win.close();
    }
    for (const backup of await funcs.listBackups()) {
      if (!before.has(backup.fileName)) tracked.add(backup.fileName);
    }
    for (const fileName of tracked) await funcs.deleteBackup(fileName);
    for (const [t, value] of attributions) {
      if (!t.isConnected) continue;
      if (value === null) t.removeAttribute(WORKSPACE_TAB_ATTRIBUTION_ID);
      else t.setAttribute(WORKSPACE_TAB_ATTRIBUTION_ID, value);
    }
  }
}

function assertOk(result: RestoreBackupResult, label: string): void {
  assert(
    result.success,
    `${label}: restore succeeds ${JSON.stringify(result)}`,
  );
}

// G-03 / G-04 / G-10 / G-12 / G-14 with a real workspaces-mode capture.
async function testWorkspacesFileWhileDisabled(): Promise<void> {
  await withCleanup(async (track) => {
    let fileName = "";
    const urlA = `${PREFIX}/ws-a`;
    const urlB = `${PREFIX}/ws-b-hidden`;
    await withWindows(false, false, async (open) => {
      const win = (await open()) as QaWindow;
      const ws = win.workspacesFuncs;
      const backup = win.workspacesBackupFuncs;
      assert(ws && backup, "workspaces enabled for capture");
      const a = win.gBrowser.addTab(urlA, {
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
        inBackground: true,
      }) as XULElement;
      const first = ws.getSelectedWorkspaceID();
      ws.createNoNameWorkspace();
      await settle();
      win.gBrowser.addTab(urlB, {
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
        inBackground: true,
      });
      ws.changeWorkspace(first);
      await settle();
      await waitFor(
        () => findTabs(win, urlB).some((t) => t.hasAttribute("hidden")),
        "the other workspace's tab is hidden at capture time",
      );
      assert(!a.hasAttribute("hidden"), "current workspace tab visible");
      const summary = await backup.backupNow();
      assertEquals(summary.mode, "workspaces", "capture is workspaces mode");
      fileName = summary.fileName;
      track(fileName);
    }, true);

    await withWindows(false, false, async (open) => {
      const win = (await open()) as QaWindow;
      const backup = win.workspacesBackupFuncs;
      assert(backup && !win.workspacesFuncs, "workspaces disabled");
      const existing = [...win.gBrowser.tabs] as XULElement[];
      const preBefore = await preRestoreCount(backup);
      const result = await backup.restoreBackup(fileName);
      assertOk(result, "G-03 workspaces file while disabled");
      await settle();
      for (const t of existing) {
        assert(t.isConnected, "G-04 existing tabs are not closed");
      }
      for (const url of [urlA, urlB]) {
        const found = windowWithUrl(url);
        assert(found, `G-03 ${url} restored into some window`);
        for (const t of findTabs(found, url)) {
          if (existing.includes(t)) continue;
          assert(
            !t.hasAttribute(WORKSPACE_TAB_ATTRIBUTION_ID),
            "G-10 no workspace attribution",
          );
          assert(!t.hasAttribute("hidden"), "G-14 restored tab is not hidden");
        }
      }
      assertEquals(
        await preRestoreCount(backup),
        preBefore,
        "G-12 additive restore writes no pre-restore copy",
      );
    }, false);
  });
}

// G-05 / G-06 / G-08: two windows, the second focused.
async function testMultiWindowFocused(): Promise<void> {
  await withCleanup(async (track) => {
    await withWindows(false, false, async (open) => {
      const win = (await open()) as QaWindow;
      const backup = win.workspacesBackupFuncs;
      assert(backup, "backup API");
      const unfocused = [`${PREFIX}/mw-u1`, `${PREFIX}/mw-u2`];
      const focused = [`${PREFIX}/mw-f1`];
      const fileName = await writeBackup(tabsFile([
        {
          isFocused: false,
          tabs: unfocused.map((u, i) => tab(u, { isSelected: i === 1 })),
        },
        {
          isFocused: true,
          tabs: focused.map((u) => tab(u, { isSelected: true })),
        },
      ]));
      track(fileName);
      const windowsBefore = browserWindows();
      const existing = [...win.gBrowser.tabs] as XULElement[];
      const result = await backup.restoreBackup(fileName);
      assertOk(result, "G-08 two-window restore");
      await settle();
      assertEquals(
        findTabs(win, focused[0]).length,
        1,
        "G-05 focused window goes to current window",
      );
      assertEquals(
        findTabs(win, unfocused[0]).length,
        0,
        "G-05 other window tabs not in current window",
      );
      const opened = browserWindows().filter((w) => !windowsBefore.includes(w));
      assertEquals(opened.length, 1, "G-05 exactly one new window opened");
      const other = opened[0];
      for (const u of unfocused) {
        assertEquals(findTabs(other, u).length, 1, `G-05 ${u} in new window`);
      }
      assertEquals(
        other.gBrowser.tabs.length,
        unfocused.length,
        "G-06 the new window's initial blank tab was removed",
      );
      for (const t of existing) {
        assert(t.isConnected, "G-04 existing tabs kept");
      }
    }, false);
  });
}

// G-09: no focused window recorded.
async function testMultiWindowNoFocus(): Promise<void> {
  await withCleanup(async (track) => {
    await withWindows(false, false, async (open) => {
      const win = (await open()) as QaWindow;
      const backup = win.workspacesBackupFuncs;
      assert(backup, "backup API");
      const firstUrl = `${PREFIX}/nf-first`;
      const secondUrl = `${PREFIX}/nf-second`;
      const fileName = await writeBackup(tabsFile([
        { isFocused: false, tabs: [tab(firstUrl)] },
        { isFocused: false, tabs: [tab(secondUrl)] },
      ]));
      track(fileName);
      const windowsBefore = browserWindows();
      assertOk(await backup.restoreBackup(fileName), "G-09");
      await settle();
      assertEquals(
        findTabs(win, firstUrl).length,
        1,
        "G-09 first captured window lands in current window",
      );
      const opened = browserWindows().filter((w) => !windowsBefore.includes(w));
      assertEquals(opened.length, 1, "G-09 one new window");
      assertEquals(
        findTabs(opened[0], secondUrl).length,
        1,
        "G-09 second window in new window",
      );
    }, false);
  });
}

// G-07: a tab failing in a newly opened window keeps its placeholder tab.
async function testNewWindowPartialFailure(): Promise<void> {
  await withCleanup(async (track) => {
    await withWindows(false, false, async (open) => {
      const win = (await open()) as QaWindow;
      const backup = win.workspacesBackupFuncs;
      assert(backup, "backup API");
      const okUrl = `${PREFIX}/pf-ok`;
      const failUrl = `${PREFIX}/pf-fail`;
      const fileName = await writeBackup(tabsFile([
        { isFocused: true, tabs: [tab(`${PREFIX}/pf-current`)] },
        {
          isFocused: false,
          tabs: [tab(okUrl), tab(failUrl, { pinned: true })],
        },
      ]));
      track(fileName);
      // Inject a pinTab failure into the window the restore opens.
      const observer = (subject: nsISupports | null): void => {
        const opened = subject as unknown as QaWindow;
        (opened.gBrowser as unknown as { pinTab(t: XULElement): void }).pinTab =
          () => {
            throw new Error("qa-injected-pin-failure");
          };
      };
      Services.obs.addObserver(observer, "browser-delayed-startup-finished");
      let result: RestoreBackupResult;
      const windowsBefore = browserWindows();
      try {
        result = await backup.restoreBackup(fileName);
      } finally {
        Services.obs.removeObserver(
          observer,
          "browser-delayed-startup-finished",
        );
      }
      await settle();
      assert(!result.success, "G-07 restore reports failure");
      assertEquals(result.error, "restore-partial-failure", "G-07 error code");
      const opened = browserWindows().filter((w) => !windowsBefore.includes(w));
      assertEquals(opened.length, 1, "G-07 new window opened");
      assertEquals(
        opened[0].gBrowser.tabs.length,
        3,
        "G-07 placeholder tab kept next to the two restored tabs",
      );
    }, false);
  });
}

// G-11 / G-13: workspaces enabled + tabs file -> current workspace, flattened.
async function testTabsFileWhileEnabled(): Promise<void> {
  await withCleanup(async (track) => {
    await withWindows(false, false, async (open) => {
      const win = (await open()) as QaWindow;
      const ws = win.workspacesFuncs;
      const backup = win.workspacesBackupFuncs;
      assert(ws && backup, "workspaces enabled");
      const current = ws.getSelectedWorkspaceID();
      const urls = [`${PREFIX}/en-1`, `${PREFIX}/en-2`];
      const crafted = tabsFile([
        {
          isFocused: true,
          tabs: [
            tab(urls[0], {
              lastShownWorkspaceId:
                current as WorkspaceBackupTab["lastShownWorkspaceId"],
            }),
          ],
        },
        { isFocused: false, tabs: [tab(urls[1])] },
      ]);
      const state = crafted.windows[0].tabs[0].state as Record<string, unknown>;
      state.attributes = { [WORKSPACE_LAST_SHOW_ID]: current };
      const fileName = await writeBackup(crafted);
      track(fileName);
      const windowsBefore = browserWindows();
      const preBefore = await preRestoreCount(backup);
      assertOk(await backup.restoreBackup(fileName), "G-11");
      await settle();
      assertEquals(
        browserWindows().length,
        windowsBefore.length,
        "G-13 tabs flattened into current window",
      );
      for (const u of urls) {
        const found = findTabs(win, u);
        assertEquals(found.length, 1, `G-13 ${u} restored in current window`);
        const t = found[0];
        assertEquals(
          t.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID),
          current,
          "G-13 assigned to current workspace",
        );
        assert(
          !t.hasAttribute(WORKSPACE_LAST_SHOW_ID),
          "G-11 no lastShown attribute",
        );
        assert(!t.hasAttribute("hidden"), "G-13 visible in current workspace");
      }
      assertEquals(
        await preRestoreCount(backup),
        preBefore,
        "G-12 no pre-restore copy for a tabs file",
      );
    }, true);
  });
}

export async function runAllTests(): Promise<void> {
  await runTests("workspacesBackupQaG4Additive.test.ts", [
    {
      name: "G-03/04/10/12/14 workspaces file restored while disabled",
      fn: testWorkspacesFileWhileDisabled,
    },
    {
      name: "G-05/06/08 multi-window restore with focused window",
      fn: testMultiWindowFocused,
    },
    {
      name: "G-09 multi-window restore without focused window",
      fn: testMultiWindowNoFocus,
    },
    {
      name: "G-07 partial failure in a new window keeps placeholder",
      fn: testNewWindowPartialFailure,
    },
    {
      name: "G-11/13 tabs file while workspaces enabled",
      fn: testTabsFileWhileEnabled,
    },
  ]);
}
