// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
// QA group g8 (PR #3159): backup performance with many tabs and files.
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
import { formatBackupFileName } from "../utils/workspaces-backup-helpers.ts";
import { WORKSPACE_TAB_ATTRIBUTION_ID } from "../utils/workspaces-static-names.ts";
import type { WorkspacesBackupFuncs } from "../utils/workspaces-backup-controller.ts";
import type { TestWindow } from "./workspace-last-tab-test-types.ts";

type BackupTestWindow = TestWindow & {
  workspacesBackupFuncs?: WorkspacesBackupFuncs;
};

interface SessionStoreApi {
  getTabState(tab: XULElement): string;
  setTabState(tab: XULElement, state: string): void;
  setWindowState(win: Window, state: string, overwrite: boolean): void;
}

const { SessionStore } = ChromeUtils.importESModule(
  "moz-src:///browser/components/sessionstore/SessionStore.sys.mjs",
) as { SessionStore: SessionStoreApi };
const { E10SUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/E10SUtils.sys.mjs",
) as { E10SUtils: { serializePrincipal(principal: unknown): string } };

const PREFIX = `https://example.com/floorp-backup-qa-g8/${Date.now()}`;
const BACKUP_DIR = PathUtils.join(PathUtils.profileDir, "workspaces", "backup");

const measurements: string[] = [];

function log(message: string): void {
  measurements.push(message);
  void IOUtils.writeUTF8(
    PathUtils.join(PathUtils.tempDir, "floorp-qa-g8-perf.txt"),
    measurements.join("\n"),
  );
  console.log(`[QA-G8 perf] ${message}`);
}

function recentBackupErrors(): string {
  const storage =
    (Cc as Record<string, { getService(iface: unknown): unknown }>)[
      "@mozilla.org/consoleAPI-storage;1"
    ].getService(Ci.nsIConsoleAPIStorage) as {
      getEvents(): { level?: string; arguments?: unknown[] }[];
    };
  return storage.getEvents()
    .filter((event) => event.level === "error")
    .map((event) =>
      (event.arguments ?? []).map((arg) =>
        arg instanceof Error ? arg.message : String(arg)
      ).join(" ")
    )
    .filter((text) => /Workspaces/.test(text))
    .slice(-5)
    .join("\n");
}

/** Collects garbage left by earlier tests so the delta reflects the restore. */
async function minimizeMemory(): Promise<void> {
  const manager = Cc["@mozilla.org/memory-reporter-manager;1"].getService(
    Ci.nsIMemoryReporterManager,
  ) as { minimizeMemoryUsage(callback: () => void): void };
  await new Promise<void>((resolve) => manager.minimizeMemoryUsage(resolve));
}

function residentBytes(): number {
  const manager = Cc["@mozilla.org/memory-reporter-manager;1"].getService(
    Ci.nsIMemoryReporterManager,
  ) as { resident: number };
  return manager.resident;
}

function browserWindows(): BackupTestWindow[] {
  const windows: BackupTestWindow[] = [];
  const enumerator = Services.wm.getEnumerator("navigator:browser");
  while (enumerator.hasMoreElements()) {
    const win = enumerator.getNext() as BackupTestWindow;
    if (!win.closed) windows.push(win);
  }
  return windows;
}

/** Adds pending (not yet loaded) tabs in one SessionStore batch. */
async function addPendingTabs(
  win: BackupTestWindow,
  count: number,
  tag: string,
): Promise<XULElement[]> {
  const before = new Set([...win.gBrowser.tabs]);
  const tabs = Array.from({ length: count }, (_, i) => {
    const url = `${PREFIX}/${tag}/${i}`;
    return {
      entries: [{
        url,
        title: `QA ${tag} ${i}`,
        triggeringPrincipal_base64: E10SUtils.serializePrincipal(
          Services.scriptSecurityManager.createContentPrincipal(
            Services.io.newURI(url),
            {},
          ),
        ),
      }],
      index: 1,
    };
  });
  SessionStore.setWindowState(
    win,
    JSON.stringify({ windows: [{ tabs, selected: 1 }] }),
    false,
  );
  await waitFor(
    () => win.gBrowser.tabs.length >= before.size + count,
    `${count} pending tabs are added`,
    60_000,
  );
  return [...win.gBrowser.tabs].filter((tab) =>
    !before.has(tab)
  ) as XULElement[];
}

/** Measures the largest gap between event-loop turns while `work` runs. */
async function measureJank<T>(
  work: () => Promise<T>,
): Promise<{ result: T; durationMs: number; maxGapMs: number }> {
  let last = performance.now();
  let maxGapMs = 0;
  let running = true;
  const tick = () => {
    const now = performance.now();
    maxGapMs = Math.max(maxGapMs, now - last);
    last = now;
    if (running) setTimeout(tick, 0);
  };
  setTimeout(tick, 0);
  const start = performance.now();
  try {
    const result = await work();
    return { result, durationMs: performance.now() - start, maxGapMs };
  } finally {
    running = false;
  }
}

/** Removes backups and restores tab attributions changed by the test. */
async function withBackupCleanup(
  check: (funcs: WorkspacesBackupFuncs) => Promise<void>,
): Promise<void> {
  const harnessWindows = browserWindows();
  const attributions = new Map<XULElement, string | null>();
  for (const win of harnessWindows) {
    for (const tab of win.gBrowser.tabs) {
      const xulTab = tab as XULElement;
      attributions.set(
        xulTab,
        xulTab.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID),
      );
    }
  }
  const funcs = harnessWindows[0]?.workspacesBackupFuncs;
  assert(funcs, "harness window exposes the backup API");
  const before = new Set(
    (await IOUtils.exists(BACKUP_DIR))
      ? (await IOUtils.getChildren(BACKUP_DIR)).map((p) =>
        PathUtils.filename(p)
      )
      : [],
  );
  try {
    await check(funcs!);
  } finally {
    for (const win of browserWindows()) {
      if (!harnessWindows.includes(win)) win.close();
    }
    if (await IOUtils.exists(BACKUP_DIR)) {
      for (const path of await IOUtils.getChildren(BACKUP_DIR)) {
        if (!before.has(PathUtils.filename(path))) await IOUtils.remove(path);
      }
    }
    for (const [tab, value] of attributions) {
      if (!tab.isConnected) continue;
      if (value === null) tab.removeAttribute(WORKSPACE_TAB_ATTRIBUTION_ID);
      else tab.setAttribute(WORKSPACE_TAB_ATTRIBUTION_ID, value);
    }
  }
}

async function testBackup500Tabs(): Promise<void> {
  await withBackupCleanup(() =>
    withWindows(false, false, async (open) => {
      const win = (await open()) as BackupTestWindow;
      const backup = win.workspacesBackupFuncs;
      assert(backup, "backup API");
      await addPendingTabs(win, 500, "u01");
      await settle();
      const run = await measureJank(() => backup!.backupNow());
      log(
        `U-01 500 tabs: backupNow ${run.durationMs.toFixed(0)} ms, ` +
          `max event-loop gap ${run.maxGapMs.toFixed(0)} ms, ` +
          `file ${run.result.fileSize} B, tabs ${run.result.tabCount}`,
      );
      assert(run.result.tabCount >= 500, `captured ${run.result.tabCount}`);
      assert(run.durationMs < 5000, `backup took ${run.durationMs} ms`);
      assert(run.maxGapMs < 1000, `UI blocked for ${run.maxGapMs} ms`);
    })
  );
}

async function testRestore500TabsLazily(): Promise<void> {
  await withBackupCleanup(() =>
    withWindows(false, false, async (open) => {
      const win = (await open()) as BackupTestWindow;
      const backup = win.workspacesBackupFuncs;
      assert(backup, "backup API while workspaces are disabled");
      const tabs = await addPendingTabs(win, 500, "u02");
      win.focus();
      await settle();
      const summary = await backup!.backupNow();
      assertEquals(summary.mode, "tabs", "tabs mode capture");
      for (const tab of tabs) win.gBrowser.removeTab(tab, { animate: false });
      await settle();
      await minimizeMemory();
      const memBefore = residentBytes();
      const run = await measureJank(() =>
        backup!.restoreBackup(summary.fileName)
      );
      if (!run.result.success) {
        log(
          `U-02 restore failed: ${
            JSON.stringify(run.result)
          } windows=${browserWindows().length}\n${recentBackupErrors()}`,
        );
      }
      assert(run.result.success, `restore: ${JSON.stringify(run.result)}`);
      await settle();
      await minimizeMemory();
      const restored = [...win.gBrowser.tabs].filter((tab) =>
        SessionStore.getTabState(tab as XULElement).includes(`${PREFIX}/u02/`)
      ) as XULElement[];
      const inserted = restored.filter((tab) =>
        (tab as XULElement & { linkedPanel?: string }).linkedPanel
      ).length;
      const pending = restored.filter((tab) =>
        tab.hasAttribute("pending")
      )
        .length;
      const memDelta = residentBytes() - memBefore;
      log(
        `U-02 500 tabs: restore ${run.durationMs.toFixed(0)} ms, ` +
          `max gap ${
            run.maxGapMs.toFixed(0)
          } ms, restored ${restored.length}, ` +
          `browsers inserted ${inserted}, pending ${pending}, resident delta ` +
          `${(memDelta / 1048576).toFixed(1)} MiB`,
      );
      assertEquals(restored.length, 500, "all tabs restored");
      assert(inserted <= 5, `only a few tabs get a browser (got ${inserted})`);
      assert(run.durationMs < 30_000, `restore took ${run.durationMs} ms`);
      assert(run.maxGapMs < 1000, `UI blocked for ${run.maxGapMs} ms`);
      assert(
        memDelta < 512 * 1048576,
        `resident memory grew ${memDelta} bytes`,
      );
    }, false)
  );
}

async function testList100Backups(): Promise<void> {
  await withBackupCleanup(() =>
    withWindows(false, false, async (open) => {
      const win = (await open()) as BackupTestWindow;
      const backup = win.workspacesBackupFuncs;
      assert(backup, "backup API");
      await addPendingTabs(win, 50, "u03");
      await settle();
      const source = await backup!.backupNow();
      const sourcePath = PathUtils.join(BACKUP_DIR, source.fileName);
      for (let i = 1; i < 100; i++) {
        const name = formatBackupFileName(
          source.capturedAt - i * 1000,
          `9${i.toString(16).padStart(7, "0")}`,
        );
        await IOUtils.copy(sourcePath, PathUtils.join(BACKUP_DIR, name));
      }
      const run = await measureJank(() => backup!.listBackups());
      log(
        `U-03 ${run.result.length} backups (${source.fileSize} B each): ` +
          `listBackups ${run.durationMs.toFixed(0)} ms, max gap ` +
          `${run.maxGapMs.toFixed(0)} ms`,
      );
      assert(run.result.length >= 100, `listed ${run.result.length}`);
      assert(run.durationMs < 3000, `listing took ${run.durationMs} ms`);
    })
  );
}

async function testScrollDuringBackup1000Tabs(): Promise<void> {
  await withBackupCleanup(() =>
    withWindows(false, false, async (open) => {
      const win = (await open()) as BackupTestWindow;
      const backup = win.workspacesBackupFuncs;
      assert(backup, "backup API");
      await addPendingTabs(win, 1000, "u04");
      win.focus();
      win.gBrowser.selectedTab = win.gBrowser.tabs[0];
      win.focus();
      await settle();
      const scrollbox = (win.gBrowser.tabContainer as unknown as {
        arrowScrollbox: XULElement & {
          scrollByIndex(index: number, instant?: boolean): void;
        };
      }).arrowScrollbox;
      const scrollFrames = async (work: Promise<unknown> | null) => {
        let maxFrameMs = 0;
        let frames = 0;
        let done = false;
        void work?.finally(() => {
          done = true;
        });
        const until = performance.now() + 1500;
        let last = performance.now();
        while (work ? !done : performance.now() < until) {
          // A background window may throttle rAF; never wait more than 250 ms.
          await new Promise<void>((resolve) => {
            const timer = setTimeout(resolve, 250);
            win.requestAnimationFrame(() => {
              clearTimeout(timer);
              resolve();
            });
          });
          const now = performance.now();
          maxFrameMs = Math.max(maxFrameMs, now - last);
          last = now;
          frames++;
          scrollbox.scrollByIndex(frames % 40 < 20 ? 1 : -1, true);
        }
        return { maxFrameMs, frames };
      };
      const baseline = await scrollFrames(null);
      const start = performance.now();
      const during = await scrollFrames(backup!.backupNow());
      const durationMs = performance.now() - start;
      log(
        `U-04 1000 tabs: backup ${durationMs.toFixed(0)} ms, ` +
          `baseline max frame ${baseline.maxFrameMs.toFixed(0)} ms ` +
          `(${baseline.frames} frames), during backup max frame ` +
          `${during.maxFrameMs.toFixed(0)} ms (${during.frames} frames)`,
      );
      assert(
        during.maxFrameMs < 500,
        `scrolling froze for ${during.maxFrameMs} ms during the backup`,
      );
    })
  );
}

export async function runAllTests(): Promise<void> {
  try {
    await runPerfTests();
  } finally {
    // Measurements are only visible in the browser console otherwise.
    await IOUtils.writeUTF8(
      PathUtils.join(PathUtils.tempDir, "floorp-qa-g8-perf.txt"),
      measurements.join("\n"),
    );
  }
}

async function runPerfTests(): Promise<void> {
  await runTests("workspacesBackupQaG8Perf.test.ts", [
    { name: "U-01 backup of 500 tabs", fn: testBackup500Tabs },
    { name: "U-02 restore of 500 tabs is lazy", fn: testRestore500TabsLazily },
    { name: "U-03 listing 100 backups", fn: testList100Backups },
    {
      name: "U-04 tab strip scroll while backing up 1000 tabs",
      fn: testScrollDuringBackup1000Tabs,
    },
  ]);
}
