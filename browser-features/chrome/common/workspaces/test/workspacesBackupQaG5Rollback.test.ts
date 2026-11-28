// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
//
// QA group g5 (I: rollback / failure handling) for workspace backup v2.
// Faults are injected by shadowing SessionStore / gBrowser methods for the
// duration of one restore; every shadow is removed in a finally block.
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
  WORKSPACE_DATA_PREF_NAME,
  WORKSPACE_TAB_ATTRIBUTION_ID,
} from "../utils/workspaces-static-names.ts";
import { formatBackupFileName } from "../utils/workspaces-backup-helpers.ts";
import type { WorkspacesBackupFuncs } from "../utils/workspaces-backup-controller.ts";
import type { WorkspaceBackupSummary } from "../utils/backup-types.ts";
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
  readonly promiseInitialized: Promise<void>;
}

const { SessionStore } = ChromeUtils.importESModule(
  "moz-src:///browser/components/sessionstore/SessionStore.sys.mjs",
) as { SessionStore: SessionStoreApi };
const { E10SUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/E10SUtils.sys.mjs",
) as { E10SUtils: { serializePrincipal(principal: unknown): string } };

const PREFIX = `https://example.com/floorp-backup-qa-g5/${Date.now()}`;

const backupDir = (): string =>
  PathUtils.join(PathUtils.profileDir, "workspaces", "backup");

function openTab(win: BackupTestWindow, url: string): XULElement {
  const tab = win.gBrowser.addTab("about:blank", {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    inBackground: true,
  }) as XULElement;
  SessionStore.setTabState(
    tab,
    JSON.stringify({
      entries: [{
        url,
        triggeringPrincipal_base64: E10SUtils.serializePrincipal(
          Services.scriptSecurityManager.createContentPrincipal(
            Services.io.newURI(url),
            {},
          ),
        ),
      }],
      index: 1,
    }),
  );
  return tab;
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

function tabUrls(tab: XULElement): string[] {
  const state = JSON.parse(SessionStore.getTabState(tab)) as {
    entries?: { url?: string }[];
  };
  return (state.entries ?? []).map((entry) => entry.url ?? "");
}

interface StoreSnapshot {
  order: string[];
  defaultID: string;
  ids: string[];
}

function readStore(): StoreSnapshot {
  const raw = JSON.parse(
    Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME, "{}"),
  ) as { order?: string[]; defaultID?: string; data?: [string, unknown][] };
  return {
    order: raw.order ?? [],
    defaultID: raw.defaultID ?? "",
    ids: (raw.data ?? []).map(([id]) => id).sort(),
  };
}

/** Same cleanup contract as workspacesBackupRoundTrip.test.ts. */
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
  const funcs = harnessWindows[0]?.workspacesBackupFuncs;
  assert(funcs, "harness window exposes the backup API");
  const before = new Set(
    (await funcs.listBackups()).map((backup) => backup.fileName),
  );
  const beforeFiles = new Set(
    (await IOUtils.getChildren(backupDir()).catch(() => [] as string[]))
      .map((path) => PathUtils.filename(path)),
  );
  try {
    await check();
  } finally {
    for (const win of browserWindows()) {
      if (!harnessWindows.includes(win)) win.close();
    }
    for (const backup of await funcs.listBackups()) {
      if (!before.has(backup.fileName)) {
        await funcs.deleteBackup(backup.fileName);
      }
    }
    // Hand-written files (corrupt / oversized) are not listed.
    for (const path of await IOUtils.getChildren(backupDir())) {
      if (!beforeFiles.has(PathUtils.filename(path))) {
        await IOUtils.remove(path, { ignoreAbsent: true });
      }
    }
    for (const [tab, value] of attributions) {
      if (!tab.isConnected) continue;
      if (value === null) tab.removeAttribute(WORKSPACE_TAB_ATTRIBUTION_ID);
      else tab.setAttribute(WORKSPACE_TAB_ATTRIBUTION_ID, value);
    }
  }
}

interface Scenario {
  win: BackupTestWindow;
  backup: WorkspacesBackupFuncs;
  workspaces: NonNullable<BackupTestWindow["workspacesFuncs"]>;
  summary: WorkspaceBackupSummary;
  /** Tabs of the state the backup holds. */
  capturedUrls: string[];
  /** State right before the restore under test. */
  tabs: XULElement[];
  selectedTab: XULElement;
  selectedWorkspace: string;
  attributions: (string | null)[];
  store: StoreSnapshot;
}

/**
 * Backup: WS1 {a1, a2}, WS2 {b1 (selected)}. Then the live state moves on to
 * an extra WS3 {c1 (selected)} so a rollback has something to return to.
 */
async function prepareScenario(win: BackupTestWindow): Promise<Scenario> {
  const backup = win.workspacesBackupFuncs;
  const workspaces = win.workspacesFuncs;
  assert(backup && workspaces, "backup and workspace APIs are available");
  const tag = Math.random().toString(16).slice(2, 8);
  const urls = {
    a1: `${PREFIX}/${tag}/a1`,
    a2: `${PREFIX}/${tag}/a2`,
    b1: `${PREFIX}/${tag}/b1`,
    c1: `${PREFIX}/${tag}/c1`,
  };
  openTab(win, urls.a1);
  openTab(win, urls.a2);
  const ws2 = workspaces.createNoNameWorkspace();
  workspaces.changeWorkspace(ws2);
  await settle();
  const b1 = openTab(win, urls.b1);
  win.gBrowser.selectedTab = b1;
  await settle();
  const summary = await backup.backupNow();
  assertEquals(summary.mode, "workspaces", "capture uses workspaces mode");

  const ws3 = workspaces.createNoNameWorkspace();
  workspaces.changeWorkspace(ws3);
  await settle();
  const c1 = openTab(win, urls.c1);
  win.gBrowser.selectedTab = c1;
  await settle();
  const tabs = [...win.gBrowser.tabs] as XULElement[];
  return {
    win,
    backup,
    workspaces,
    summary,
    capturedUrls: [urls.a1, urls.a2, urls.b1],
    tabs,
    selectedTab: win.gBrowser.selectedTab as XULElement,
    selectedWorkspace: workspaces.getSelectedWorkspaceID(),
    attributions: tabs.map((tab) =>
      tab.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID)
    ),
    store: readStore(),
  };
}

/** I-01 / I-02 / I-03: everything is back to the pre-restore state. */
async function assertRolledBack(s: Scenario, label: string): Promise<void> {
  await settle();
  const tabs = [...s.win.gBrowser.tabs] as XULElement[];
  assertEquals(tabs.length, s.tabs.length, `${label}: tab count is unchanged`);
  assert(
    tabs.every((tab, index) => tab === s.tabs[index]),
    `${label}: the same tab elements remain in the same order`,
  );
  assert(
    tabs.every((tab, index) =>
      tab.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID) === s.attributions[index]
    ),
    `${label}: tab workspace attribution is unchanged`,
  );
  assertEquals(
    s.win.gBrowser.selectedTab,
    s.selectedTab,
    `${label}: the previously selected tab is selected again`,
  );
  assertEquals(
    s.workspaces.getSelectedWorkspaceID(),
    s.selectedWorkspace,
    `${label}: the previous workspace is current again`,
  );
  const store = readStore();
  assertEquals(
    JSON.stringify(store.order),
    JSON.stringify(s.store.order),
    `${label}: workspace order is unchanged`,
  );
  assertEquals(
    store.defaultID,
    s.store.defaultID,
    `${label}: default workspace is unchanged`,
  );
  assertEquals(
    JSON.stringify(store.ids),
    JSON.stringify(s.store.ids),
    `${label}: no restored workspace is left behind`,
  );
  assertEquals(s.backup.isRestoreInFlight(), false, `${label}: not in flight`);
}

/** Shadows a method on an object instance and returns the undo function. */
function shadow<T extends object, K extends keyof T>(
  target: T,
  key: K,
  replacement: T[K],
): () => void {
  const own = Object.getOwnPropertyDescriptor(target, key);
  Object.defineProperty(target, key, {
    configurable: true,
    writable: true,
    value: replacement,
  });
  return () => {
    if (own) Object.defineProperty(target, key, own);
    else delete (target as Record<PropertyKey, unknown>)[key as PropertyKey];
  };
}

/**
 * Captures console.error calls. The restore runs in the test window, which
 * has its own `console`, so pass that window's console as `target`.
 */
async function captureErrors<T>(
  run: () => Promise<T>,
  target: Console = console,
): Promise<{ value: T; errors: string[] }> {
  const original = target.error;
  const errors: string[] = [];
  target.error = (...args: unknown[]) => {
    errors.push(
      args.map((arg) => arg instanceof Error ? arg.message : String(arg))
        .join(" "),
    );
    original.apply(target, args);
  };
  try {
    return { value: await run(), errors };
  } finally {
    target.error = original;
  }
}

function runScenario(
  check: (s: Scenario) => Promise<void>,
): Promise<void> {
  return withBackupCleanup(() =>
    withWindows(false, false, async (open) => {
      const win = (await open()) as BackupTestWindow;
      await check(await prepareScenario(win));
    })
  );
}

// I-01/I-02/I-03: one tab's SessionStore.setTabState throws.
async function testI01SetTabStateFailureRollsBack(): Promise<void> {
  await runScenario(async (s) => {
    const original = SessionStore.setTabState.bind(SessionStore);
    let injected = 0;
    const undo = shadow(
      SessionStore,
      "setTabState",
      (tab: XULElement, state: string) => {
        if (state.includes(s.capturedUrls[1])) {
          injected++;
          throw new Error("qa-g5 injected setTabState failure");
        }
        original(tab, state);
      },
    );
    let result;
    try {
      ({ value: result } = await captureErrors(
        () => s.backup.restoreBackup(s.summary.fileName),
        s.win.console,
      ));
    } finally {
      undo();
    }
    assertEquals(injected, 1, "the fault fired for exactly one tab");
    assertEquals(result.success, false, "restore reports failure");
    assertEquals(result.error, "restore-partial-failure", "error code");
    await assertRolledBack(s, "setTabState failure");
    const pre = (await s.backup.listBackups()).filter((b) =>
      b.kind === "pre-restore"
    );
    assert(pre.length > 0, "the pre-restore safety copy is kept");
  });
}

// I-05: gBrowser.addTab throws for the second restored tab.
async function testI05AddTabFailureRollsBack(): Promise<void> {
  await runScenario(async (s) => {
    const gBrowser = s.win.gBrowser as unknown as {
      addTab(url: string, options: object): XULElement;
    };
    const original = gBrowser.addTab.bind(gBrowser);
    let calls = 0;
    const undo = shadow(gBrowser, "addTab", (url: string, options: object) => {
      calls++;
      if (calls === 2) throw new Error("qa-g5 injected addTab failure");
      return original(url, options);
    });
    let result;
    try {
      ({ value: result } = await captureErrors(
        () => s.backup.restoreBackup(s.summary.fileName),
        s.win.console,
      ));
    } finally {
      undo();
    }
    assert(calls >= 2, "addTab was called for restored tabs");
    assertEquals(result.success, false, "restore reports failure");
    assertEquals(result.error, "restore-partial-failure", "error code");
    await assertRolledBack(s, "addTab failure");
  });
}

// I-06: restoreTabsIntoWindow itself throws (its SessionStore init rejects).
async function testI06RestoreTabsIntoWindowThrows(): Promise<void> {
  await runScenario(async (s) => {
    const proto = Object.getPrototypeOf(SessionStore) as object;
    const descriptor = Object.getOwnPropertyDescriptor(
      proto,
      "promiseInitialized",
    );
    assert(descriptor?.get, "SessionStore.promiseInitialized is a getter");
    const originalGet = descriptor.get;
    let injected = 0;
    Object.defineProperty(SessionStore, "promiseInitialized", {
      configurable: true,
      get() {
        // Only the call made from restoreTabsIntoWindow fails; the
        // pre-restore capture before it must still succeed.
        if (new Error().stack?.includes("restoreTabsIntoWindow")) {
          injected++;
          return Promise.reject(new Error("qa-g5 injected init failure"));
        }
        return originalGet.call(SessionStore);
      },
    });
    let result;
    try {
      ({ value: result } = await captureErrors(
        () => s.backup.restoreBackup(s.summary.fileName),
        s.win.console,
      ));
    } finally {
      delete (SessionStore as unknown as Record<string, unknown>)
        .promiseInitialized;
    }
    // ensureSessionStore reads the getter twice (truthiness check + await).
    assert(injected >= 1, "the fault fired inside restoreTabsIntoWindow");
    assertEquals(result.success, false, "restore reports failure");
    // The rejection comes from another realm (here the test, in production
    // SessionStore.sys.mjs), so `instanceof Error` in the service is false and
    // the code becomes String(error), e.g. "Error: ...". The UI shows its
    // generic message for it either way.
    assert(
      result.error?.includes("qa-g5 injected init failure"),
      `the underlying error is surfaced: ${result.error}`,
    );
    await assertRolledBack(s, "restoreTabsIntoWindow throws");
  });
}

// I-07: a post-commit step (closing a replaced tab) throws. The restore is
// not rolled back; the tab that could not be closed stays on a restored
// workspace and the remaining steps still run.
async function testI07PostCommitStepFailure(): Promise<void> {
  await runScenario(async (s) => {
    const gBrowser = s.win.gBrowser as unknown as {
      removeTab(tab: XULElement, options?: object): void;
    };
    const original = gBrowser.removeTab.bind(gBrowser);
    const stuck = s.selectedTab;
    let injected = 0;
    const undo = shadow(
      gBrowser,
      "removeTab",
      (tab: XULElement, options?: object) => {
        if (tab === stuck) {
          injected++;
          throw new Error("qa-g5 injected removeTab failure");
        }
        original(tab, options);
      },
    );
    let outcome;
    try {
      outcome = await captureErrors(
        () => s.backup.restoreBackup(s.summary.fileName),
        s.win.console,
      );
    } finally {
      undo();
    }
    await settle();
    assertEquals(injected, 1, "the replaced tab close failed once");
    assert(
      outcome.value.success,
      `restore still reports success: ${JSON.stringify(outcome.value)}`,
    );
    assert(
      outcome.errors.some((text) =>
        text.includes("failed to close replaced tab")
      ),
      "the failure is logged",
    );
    assert(stuck.isConnected, "the tab that failed to close is still open");
    const restoredOrder = readStore().order;
    assert(
      !restoredOrder.some((id) => s.store.order.includes(id)),
      "the store was replaced by the restored workspaces",
    );
    const current = s.workspaces.getSelectedWorkspaceID();
    assert(restoredOrder.includes(current), "a restored workspace is current");
    assertEquals(
      stuck.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID),
      current,
      "the unclosed tab is re-attributed to the current restored workspace",
    );
    for (const url of s.capturedUrls) {
      assertEquals(
        ([...s.win.gBrowser.tabs] as XULElement[]).filter((tab) =>
          tabUrls(tab).includes(url)
        ).length,
        1,
        `restored tab ${url} exists once`,
      );
    }
  });
}

// I-04 (alternative to manual): another window edits the workspace store
// while this window's restore is failing; the rollback must keep that edit.
async function testI04OtherWindowEditSurvivesRollback(): Promise<void> {
  await withBackupCleanup(() =>
    withWindows(false, false, async (open) => {
      const win = (await open()) as BackupTestWindow;
      const other = (await open()) as BackupTestWindow;
      const s = await prepareScenario(win);
      const otherFuncs = other.workspacesFuncs;
      assert(otherFuncs, "second window has workspaces");
      let createdInOther: string | null = null;
      const original = SessionStore.setTabState.bind(SessionStore);
      const undo = shadow(
        SessionStore,
        "setTabState",
        (tab: XULElement, state: string) => {
          if (state.includes(s.capturedUrls[2])) {
            // Runs synchronously inside the restore, before the rollback.
            createdInOther = otherFuncs.createNoNameWorkspace();
            throw new Error("qa-g5 injected setTabState failure");
          }
          original(tab, state);
        },
      );
      let result;
      try {
        ({ value: result } = await captureErrors(
          () => s.backup.restoreBackup(s.summary.fileName),
          s.win.console,
        ));
      } finally {
        undo();
      }
      await settle();
      assertEquals(result.error, "restore-partial-failure", "restore fails");
      assert(createdInOther, "the other window created a workspace");
      const id = createdInOther as string;
      await waitFor(
        () => readStore().ids.includes(id),
        "the other window's workspace is still in the persisted store",
      );
      assert(
        otherFuncs.isWorkspaceID(id),
        "the other window still knows its workspace",
      );
      assert(
        s.workspaces.isWorkspaceID(id),
        "the restoring window sees the other window's workspace",
      );
      const store = readStore();
      assertEquals(
        store.order.filter((wsId) => !s.store.order.includes(wsId)).join(","),
        id,
        "only the other window's workspace was added; restored ones are gone",
      );
    })
  );
}

// I-08: the file disappears between listing and restoring.
async function testI08MissingFile(): Promise<void> {
  await runScenario(async (s) => {
    await IOUtils.remove(PathUtils.join(backupDir(), s.summary.fileName));
    const before = (await s.backup.listBackups()).length;
    const result = await s.backup.restoreBackup(s.summary.fileName);
    assertEquals(result.success, false, "restore fails");
    assertEquals(result.error, "backup-not-found", "error code");
    assertEquals(
      (await s.backup.listBackups()).length,
      before,
      "no pre-restore copy is written for a missing file",
    );
    await assertRolledBack(s, "missing file");
  });
}

// I-09: corrupt content with a canonical file name.
async function testI09CorruptFile(): Promise<void> {
  await runScenario(async (s) => {
    const cases: [string, Uint8Array, boolean][] = [
      ["plain text", new TextEncoder().encode("not a backup"), false],
      ["lz4 of invalid JSON", new TextEncoder().encode("{oops"), true],
      [
        "lz4 of unknown version",
        new TextEncoder().encode(JSON.stringify({ version: 999 })),
        true,
      ],
    ];
    for (const [label, bytes, compress] of cases) {
      const name = formatBackupFileName(
        Date.now() - 60_000,
        Math.random().toString(16).slice(2, 10).padEnd(8, "0"),
      );
      await IOUtils.write(PathUtils.join(backupDir(), name), bytes, {
        compress,
      });
      const before = (await s.backup.listBackups()).length;
      const result = await s.backup.restoreBackup(name);
      assertEquals(result.success, false, `${label}: restore fails`);
      assertEquals(result.error, "backup-not-found", `${label}: error code`);
      assertEquals(
        (await s.backup.listBackups()).length,
        before,
        `${label}: no pre-restore copy is written`,
      );
      await assertRolledBack(s, label);
    }
  });
}

// I-10: oversized compressed file and oversized declared decompressed size.
async function testI10HugeFile(): Promise<void> {
  await runScenario(async (s) => {
    const big = formatBackupFileName(Date.now() - 60_000, "a1b2c3d4");
    const bigBytes = new Uint8Array(64 * 1024 * 1024 + 1);
    bigBytes.set(new TextEncoder().encode("mozLz40\0"));
    await IOUtils.write(PathUtils.join(backupDir(), big), bigBytes);
    const bigResult = await s.backup.restoreBackup(big);
    assertEquals(bigResult.error, "backup-too-large", "64 MiB + 1 file");

    // A tiny file whose mozLz4 header claims 300 MiB once decompressed.
    const bomb = formatBackupFileName(Date.now() - 50_000, "a1b2c3d5");
    const header = new Uint8Array(12 + 16);
    header.set(new TextEncoder().encode("mozLz40\0"));
    new DataView(header.buffer).setUint32(8, 300 * 1024 * 1024, true);
    await IOUtils.write(PathUtils.join(backupDir(), bomb), header);
    const bombResult = await s.backup.restoreBackup(bomb);
    assertEquals(
      bombResult.error,
      "backup-too-large",
      "declared decompressed size above 256 MiB",
    );
    const listed = (await s.backup.listBackups()).map((b) => b.fileName);
    assert(
      !listed.includes(big) && !listed.includes(bomb),
      "oversized files are not listed",
    );
    await assertRolledBack(s, "huge file");
  });
}

export async function runAllTests(): Promise<void> {
  await runTests("workspacesBackupQaG5Rollback.test.ts", [
    {
      name: "I-01/I-02/I-03 setTabState failure rolls back",
      fn: testI01SetTabStateFailureRollsBack,
    },
    {
      name: "I-04 other window edit survives rollback",
      fn: testI04OtherWindowEditSurvivesRollback,
    },
    {
      name: "I-05 addTab failure rolls back",
      fn: testI05AddTabFailureRollsBack,
    },
    {
      name: "I-06 restoreTabsIntoWindow throws",
      fn: testI06RestoreTabsIntoWindowThrows,
    },
    { name: "I-07 post-commit step failure", fn: testI07PostCommitStepFailure },
    { name: "I-08 missing file", fn: testI08MissingFile },
    { name: "I-09 corrupt file", fn: testI09CorruptFile },
    { name: "I-10 oversized file", fn: testI10HugeFile },
  ]);
}
