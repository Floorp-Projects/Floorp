// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
//
// QA group g1 (PR #3159): backup privacy cases B-01 .. B-12.

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
  WORKSPACE_BACKUP_CONFIG_PREF_NAME,
  WORKSPACE_BACKUP_LAST_RUN_PREF_NAME,
  WORKSPACE_TAB_ATTRIBUTION_ID,
} from "../utils/workspaces-static-names.ts";
import type { WorkspacesBackupFuncs } from "../utils/workspaces-backup-controller.ts";
import { formatBackupFileName } from "../utils/workspaces-backup-helpers.ts";
import type {
  WorkspaceBackupFile,
  WorkspaceBackupTab,
} from "../utils/backup-types.ts";
import type { TestWindow } from "./workspace-last-tab-test-types.ts";

type BackupTestWindow = TestWindow & {
  workspacesBackupFuncs?: WorkspacesBackupFuncs;
  workspacesFuncs?: TestWindow["workspacesFuncs"] & {
    isWorkspaceID(id: string): boolean;
  };
};

interface SessionStoreApi {
  getTabState(tab: XULElement): string;
  setTabState(tab: XULElement, state: string): void;
}

interface Identity {
  userContextId: number;
  floorpPrivateContainer?: boolean;
}

interface IdentityService {
  _identities: Identity[] | null;
  ensureDataReady(): void;
  create(name: string, icon: string, color: string): Identity;
  remove(userContextId: number): boolean;
}

interface PrivateBrowsingUtilsApi {
  isWindowPrivate(window: Window): boolean;
}

const { SessionStore } = ChromeUtils.importESModule(
  "moz-src:///browser/components/sessionstore/SessionStore.sys.mjs",
) as { SessionStore: SessionStoreApi };
const { ContextualIdentityService } = ChromeUtils.importESModule(
  "moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs",
) as { ContextualIdentityService: IdentityService };
const { PrivateBrowsingUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
) as { PrivateBrowsingUtils: PrivateBrowsingUtilsApi };

const RUN = Date.now();
const PREFIX = `https://example.com/floorp-qa-g1-privacy/${RUN}`;
const BACKUP_DIR = PathUtils.join(PathUtils.profileDir, "workspaces", "backup");

function browserWindows(): BackupTestWindow[] {
  const windows: BackupTestWindow[] = [];
  const enumerator = Services.wm.getEnumerator("navigator:browser");
  while (enumerator.hasMoreElements()) {
    const win = enumerator.getNext() as BackupTestWindow;
    if (!win.closed && win.gBrowser) windows.push(win);
  }
  return windows;
}

function urlTab(
  win: Window & { gBrowser: TestWindow["gBrowser"] },
  url: string,
  userContextId = 0,
): XULElement {
  const tab = win.gBrowser.addTab("about:blank", {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    inBackground: true,
    userContextId,
  }) as XULElement;
  SessionStore.setTabState(
    tab,
    JSON.stringify({
      entries: [{ url, title: url }],
      index: 1,
      userContextId,
    }),
  );
  return tab;
}

function tabHolds(tab: XULElement, url: string): boolean {
  return SessionStore.getTabState(tab).includes(url);
}

async function readBackup(fileName: string): Promise<WorkspaceBackupFile> {
  const bytes = await IOUtils.read(PathUtils.join(BACKUP_DIR, fileName), {
    decompress: true,
  });
  return JSON.parse(new TextDecoder().decode(bytes)) as WorkspaceBackupFile;
}

function allTabs(file: WorkspaceBackupFile): WorkspaceBackupTab[] {
  return file.windows.flatMap((window) => window.tabs);
}

async function listDir(): Promise<Set<string>> {
  return new Set(
    (await IOUtils.getChildren(BACKUP_DIR).catch(() => [] as string[])).map((
      path,
    ) => PathUtils.filename(path)),
  );
}

async function newFiles(before: Set<string>): Promise<string[]> {
  return [...await listDir()].filter((name) => !before.has(name));
}

async function rejection(promise: Promise<unknown>): Promise<string> {
  try {
    await promise;
  } catch (error) {
    // Errors thrown in another window's global fail `instanceof Error`.
    const message = (error as { message?: unknown } | null)?.message;
    return typeof message === "string" ? message : String(error);
  }
  return "(resolved)";
}

async function withBackupCleanup(check: () => Promise<void>): Promise<void> {
  const harnessWindows = browserWindows();
  const attributions = new Map<XULElement, string | null>();
  for (const win of harnessWindows) {
    for (const tab of win.gBrowser.tabs) {
      attributions.set(
        tab as XULElement,
        (tab as XULElement).getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID),
      );
    }
  }
  const before = await listDir();
  try {
    await check();
  } finally {
    for (const win of browserWindows()) {
      if (!harnessWindows.includes(win)) win.close();
    }
    for (const name of await newFiles(before)) {
      await IOUtils.remove(PathUtils.join(BACKUP_DIR, name), {
        ignoreAbsent: true,
      });
    }
    for (const [tab, value] of attributions) {
      if (!tab.isConnected) continue;
      if (value === null) tab.removeAttribute(WORKSPACE_TAB_ATTRIBUTION_ID);
      else tab.setAttribute(WORKSPACE_TAB_ATTRIBUTION_ID, value);
    }
  }
}

function withTestWindow(
  check: (
    win: BackupTestWindow,
    backup: WorkspacesBackupFuncs,
  ) => Promise<void>,
  workspaceEnabled = true,
): Promise<void> {
  return withBackupCleanup(() =>
    withWindows(false, false, async (open) => {
      const win = (await open()) as BackupTestWindow;
      const backup = win.workspacesBackupFuncs;
      assert(backup, "backup API is available");
      await check(win, backup);
    }, workspaceEnabled)
  );
}

/** Ensures a Floorp private container exists for the duration of `check`. */
async function withPrivateContainer(
  check: (userContextId: number) => Promise<void>,
): Promise<void> {
  ContextualIdentityService.ensureDataReady();
  const identities = ContextualIdentityService._identities ?? [];
  const existing = identities.find((identity) =>
    identity.floorpPrivateContainer === true
  );
  let created: Identity | null = null;
  if (!existing) {
    created = ContextualIdentityService.create(
      "QA g1 private",
      "fingerprint",
      "purple",
    );
    const stored = (ContextualIdentityService._identities ?? []).find((
      identity,
    ) => identity.userContextId === created?.userContextId);
    assert(stored, "created identity is stored");
    stored.floorpPrivateContainer = true;
  }
  try {
    await check((existing ?? created as Identity).userContextId);
  } finally {
    if (created) ContextualIdentityService.remove(created.userContextId);
  }
}

// B-01
async function testPrivateWindowExcluded(): Promise<void> {
  await withTestWindow(async (win, backup) => {
    const url = `${PREFIX}/b01-private`;
    const privateWin = (globalThis as unknown as {
      OpenBrowserWindow(options: { private: boolean }): BackupTestWindow;
    }).OpenBrowserWindow({ private: true });
    try {
      await waitFor(
        () => Boolean(privateWin.gBrowserInit?.delayedStartupFinished),
        "private window starts",
        30_000,
      );
      assert(
        PrivateBrowsingUtils.isWindowPrivate(privateWin),
        "window is private",
      );
      urlTab(privateWin, url);
      urlTab(win, `${PREFIX}/b01-normal`);
      await settle();
      const file = await readBackup((await backup.backupNow()).fileName);
      const text = JSON.stringify(file);
      assert(!text.includes(url), "B-01 private window tab is excluded");
      assert(text.includes(`${PREFIX}/b01-normal`), "B-01 normal tab saved");
    } finally {
      privateWin.close();
    }
  });
}

// B-02 (alternative: every window reported private, scheduler forced due)
async function testOnlyPrivateWindowsSkip(): Promise<void> {
  await withTestWindow(async () => {
    const hadLastRun = Services.prefs.prefHasUserValue(
      WORKSPACE_BACKUP_LAST_RUN_PREF_NAME,
    );
    const previousLastRun = Services.prefs.getStringPref(
      WORKSPACE_BACKUP_LAST_RUN_PREF_NAME,
      "0",
    );
    const hadConfig = Services.prefs.prefHasUserValue(
      WORKSPACE_BACKUP_CONFIG_PREF_NAME,
    );
    const previousConfig = Services.prefs.getStringPref(
      WORKSPACE_BACKUP_CONFIG_PREF_NAME,
      "",
    );
    const original = PrivateBrowsingUtils.isWindowPrivate;
    const before = await listDir();
    try {
      PrivateBrowsingUtils.isWindowPrivate = () => true;
      Services.prefs.setStringPref(WORKSPACE_BACKUP_LAST_RUN_PREF_NAME, "0");
      // Any config change reschedules every window's service in 1 s.
      Services.prefs.setStringPref(
        WORKSPACE_BACKUP_CONFIG_PREF_NAME,
        JSON.stringify({
          enabled: true,
          intervalMinutes: 15,
          retentionDays: 7,
          maxPreRestoreBackups: 10,
          qa: RUN,
        }),
      );
      await new Promise((resolve) => setTimeout(resolve, 3_000));
      assertEquals(
        (await newFiles(before)).length,
        0,
        "B-02 no backup while only private windows exist",
      );
      assertEquals(
        Services.prefs.getStringPref(WORKSPACE_BACKUP_LAST_RUN_PREF_NAME, "0"),
        "0",
        "B-02 lastRun is not advanced, so the run is retried later",
      );
      PrivateBrowsingUtils.isWindowPrivate = original;
      await new Promise((resolve) => setTimeout(resolve, 3_000));
      assertEquals(
        (await newFiles(before)).length,
        0,
        "B-02 retry is backed off (not every second)",
      );
    } finally {
      PrivateBrowsingUtils.isWindowPrivate = original;
      Services.prefs.setStringPref(
        WORKSPACE_BACKUP_LAST_RUN_PREF_NAME,
        String(Date.now()),
      );
      if (hadConfig) {
        Services.prefs.setStringPref(
          WORKSPACE_BACKUP_CONFIG_PREF_NAME,
          previousConfig,
        );
      } else Services.prefs.clearUserPref(WORKSPACE_BACKUP_CONFIG_PREF_NAME);
      await new Promise((resolve) => setTimeout(resolve, 1_500));
      if (hadLastRun) {
        Services.prefs.setStringPref(
          WORKSPACE_BACKUP_LAST_RUN_PREF_NAME,
          previousLastRun,
        );
      } else Services.prefs.clearUserPref(WORKSPACE_BACKUP_LAST_RUN_PREF_NAME);
    }
  });
}

// B-05 / B-09
async function testPrivateContainerAndNoHistoryExcluded(): Promise<void> {
  await withPrivateContainer((privateId) =>
    withTestWindow(async (win, backup) => {
      urlTab(win, `${PREFIX}/b05-private-container`, privateId);
      const noHistory = urlTab(win, `${PREFIX}/b09-no-history`);
      noHistory.setAttribute("floorp-disablehistory", "true");
      urlTab(win, `${PREFIX}/b05-normal`);
      await settle();
      const file = await readBackup((await backup.backupNow()).fileName);
      const text = JSON.stringify(file);
      assert(
        !text.includes("b05-private-container"),
        "B-05 private container tab URL/title absent",
      );
      assert(
        allTabs(file).every((tab) => tab.userContextId !== privateId),
        "B-05 no tab uses the private container",
      );
      assert(!text.includes("b09-no-history"), "B-09 no-history tab absent");
      assert(text.includes(`${PREFIX}/b05-normal`), "normal tab is saved");
    })
  );
}

// B-06
async function testNoPrivateContainer(): Promise<void> {
  ContextualIdentityService.ensureDataReady();
  const flagged = (ContextualIdentityService._identities ?? []).filter((
    identity,
  ) => identity.floorpPrivateContainer === true);
  for (const identity of flagged) identity.floorpPrivateContainer = false;
  try {
    await withTestWindow(async (win, backup) => {
      urlTab(win, `${PREFIX}/b06`);
      await settle();
      const file = await readBackup((await backup.backupNow()).fileName);
      assert(
        JSON.stringify(file).includes(`${PREFIX}/b06`),
        "B-06 backup succeeds without a private container",
      );
    });
  } finally {
    for (const identity of flagged) identity.floorpPrivateContainer = true;
  }
}

// B-07
async function testPrivateContainerLookupThrows(): Promise<void> {
  await withTestWindow(async (win, backup) => {
    urlTab(win, `${PREFIX}/b07`);
    await settle();
    ContextualIdentityService.ensureDataReady();
    const before = await listDir();
    const target = ContextualIdentityService as unknown as Record<
      string,
      unknown
    >;
    const value = target._identities;
    Object.defineProperty(target, "_identities", {
      configurable: true,
      get() {
        throw new Error("qa-g1 injected identity failure");
      },
    });
    let message: string;
    try {
      message = await rejection(backup.backupNow());
    } finally {
      Object.defineProperty(target, "_identities", {
        configurable: true,
        writable: true,
        enumerable: true,
        value,
      });
    }
    assertEquals(
      message,
      "private-container-detection-failed",
      "B-07 capture fails closed",
    );
    assertEquals((await newFiles(before)).length, 0, "B-07 no file written");
  });
}

// B-08
async function testWindowPrivacyThrows(): Promise<void> {
  await withTestWindow(async (win, backup) => {
    urlTab(win, `${PREFIX}/b08`);
    await settle();
    const before = await listDir();
    const original = PrivateBrowsingUtils.isWindowPrivate;
    let message: string;
    try {
      PrivateBrowsingUtils.isWindowPrivate = () => {
        throw new Error("qa-g1 injected privacy failure");
      };
      message = await rejection(backup.backupNow());
    } finally {
      PrivateBrowsingUtils.isWindowPrivate = original;
    }
    assertEquals(message, "private-detection-failed", "B-08 fails closed");
    assertEquals((await newFiles(before)).length, 0, "B-08 no file written");
  });
}

// B-10 / B-11
async function testRestoreKeepsExemptTabs(): Promise<void> {
  await withPrivateContainer((privateId) =>
    withTestWindow(async (win, backup) => {
      const normal = urlTab(win, `${PREFIX}/b10-normal`);
      await settle();
      const summary = await backup.backupNow();
      const privateTab = urlTab(win, `${PREFIX}/b10-private`, privateId);
      const noHistory = urlTab(win, `${PREFIX}/b11-no-history`);
      noHistory.setAttribute("floorp-disablehistory", "true");
      await settle();

      const result = await backup.restoreBackup(summary.fileName);
      assert(result.success, `restore succeeds: ${JSON.stringify(result)}`);
      await settle();
      assert(!normal.isConnected, "covered tab is replaced");
      assert(
        [...win.gBrowser.tabs].some((tab) =>
          tabHolds(tab as XULElement, `${PREFIX}/b10-normal`)
        ),
        "covered tab is restored",
      );
      assert(privateTab.isConnected, "B-10 private container tab stays open");
      assertEquals(
        privateTab.getAttribute("usercontextid"),
        String(privateId),
        "B-10 tab keeps its container",
      );
      assert(noHistory.isConnected, "B-11 no-history tab stays open");
    })
  );
}

// B-12 (tampered file)
async function testTamperedPrivateContainerTab(): Promise<void> {
  await withPrivateContainer((privateId) =>
    withTestWindow(async (win, backup) => {
      const url = `${PREFIX}/b12-tampered`;
      const file: WorkspaceBackupFile = {
        version: 2,
        kind: "manual",
        mode: "tabs",
        capturedAt: Date.now(),
        windows: [{
          isFocused: true,
          tabs: [{
            state: null,
            title: "tampered",
            url,
            pinned: false,
            isSelected: false,
            userContextId: privateId,
            lastShownWorkspaceId: null,
          }],
        }],
      };
      const fileName = formatBackupFileName(file.capturedAt, "b12b12b1");
      await IOUtils.makeDirectory(BACKUP_DIR, { createAncestors: true });
      await IOUtils.write(
        PathUtils.join(BACKUP_DIR, fileName),
        new TextEncoder().encode(JSON.stringify(file)),
        { compress: true },
      );
      const result = await backup.restoreBackup(fileName);
      await settle();
      const restored = [...win.gBrowser.tabs].filter((tab) =>
        tabHolds(tab as XULElement, url) ||
        (tab as XULElement & {
            linkedBrowser?: { currentURI?: { spec: string } };
          })
            .linkedBrowser?.currentURI?.spec === url
      ) as XULElement[];
      console.log(
        "[qa-g1] B-12 result",
        JSON.stringify(result),
        "restored",
        restored.length,
        "usercontextid",
        restored.map((tab) => tab.getAttribute("usercontextid")).join(","),
        "privateId",
        privateId,
      );
      const restoredContexts = restored.map((tab) =>
        tab.getAttribute("usercontextid")
      );
      for (const tab of restored) {
        win.gBrowser.removeTab(tab, { animate: false });
      }
      assert(result.success, `B-12 restore result: ${JSON.stringify(result)}`);
      // Capture never writes private-container tabs; a tampered file is
      // honored and the tab opens in the (ephemeral) private container.
      assertEquals(restored.length, 1, "B-12 tampered tab is restored once");
      assertEquals(
        restoredContexts[0],
        String(privateId),
        "B-12 tab opens in the private container, so nothing is persisted",
      );
    }, false)
  );
}

export async function runAllTests(): Promise<void> {
  await runTests("workspacesBackupQaG1Privacy.test.ts", [
    { name: "B-01 private window excluded", fn: testPrivateWindowExcluded },
    { name: "B-02 only private windows skip", fn: testOnlyPrivateWindowsSkip },
    {
      name: "B-05/B-09 private container and no-history excluded",
      fn: testPrivateContainerAndNoHistoryExcluded,
    },
    { name: "B-06 no private container", fn: testNoPrivateContainer },
    {
      name: "B-07 private container lookup throws",
      fn: testPrivateContainerLookupThrows,
    },
    { name: "B-08 window privacy throws", fn: testWindowPrivacyThrows },
    {
      name: "B-10/B-11 restore keeps exempt tabs",
      fn: testRestoreKeepsExemptTabs,
    },
    {
      name: "B-12 tampered private container tab",
      fn: testTamperedPrivateContainerTab,
    },
  ]);
}
