// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { createRoot } from "solid-js";
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
import type { TestWindow } from "./workspace-last-tab-test-types.ts";
import {
  WORKSPACE_DATA_PREF_NAME,
  WORKSPACE_TAB_ATTRIBUTION_ID,
} from "../utils/workspaces-static-names.ts";
import {
  type RestoreBackupResult,
  type WorkspacesBackupHost,
  WorkspacesBackupService,
} from "../utils/workspaces-backup-service.ts";
import {
  WorkspacesBackupController,
  type WorkspacesBackupFuncs,
} from "../utils/workspaces-backup-controller.ts";
import type { TWorkspaceID } from "../utils/type.ts";

/**
 * QA group g7: restore flag lifetime, workspaces-disabled capture and the
 * tabbrowser session-restore patch (test IDs N-10, R-04, R-05, S-01, S-02).
 */

type ServiceTestWindow = TestWindow & {
  workspacesBackupFuncs?: WorkspacesBackupFuncs;
};

interface SessionStoreApi {
  setWindowState(win: Window, state: string, overwrite: boolean): void;
}

const { SessionStore } = ChromeUtils.importESModule(
  "moz-src:///browser/components/sessionstore/SessionStore.sys.mjs",
) as { SessionStore: SessionStoreApi };
const { E10SUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/E10SUtils.sys.mjs",
) as { E10SUtils: { serializePrincipal(principal: unknown): string } };

const VALID_FILE = "backup-20260101-000000-0123abcd.jsonlz4";
const PREFIX = `https://example.com/floorp-qa-g7/${Date.now()}`;

class NullHost implements WorkspacesBackupHost {
  readonly supportsWorkspaceRestore = false;
  isWorkspaceID(_id: string): _id is TWorkspaceID {
    return false;
  }
  getRawWorkspace(): undefined {
    return undefined;
  }
  restoreWorkspaceState(): Promise<void> {
    return Promise.reject(new Error("workspaces-disabled"));
  }
}

/** The public restore API plus the private step the test replaces. */
interface RestoreProbe {
  runRestore(fileName: string): Promise<RestoreBackupResult>;
  restoreBackup(fileName: string): Promise<RestoreBackupResult>;
  isRestoreInFlight(): boolean;
}

interface Deferred {
  promise: Promise<RestoreBackupResult>;
  resolve(): void;
}

function deferred(): Deferred {
  let resolve = () => {};
  const promise = new Promise<RestoreBackupResult>((r) => {
    resolve = () => r({ success: true });
  });
  return { promise, resolve };
}

// N-10 (service side, 【既知の懸念】): after restore A outlives the 11-minute
// TTL and restore B starts, A finishing must not clear B's in-flight flag.
async function testStaleRestoreKeepsNewFlag(): Promise<void> {
  let service: WorkspacesBackupService | null = null;
  const dispose = createRoot((d) => {
    service = new WorkspacesBackupService(new NullHost());
    return d;
  });
  const realNow = Date.now;
  const gates: Deferred[] = [];
  try {
    const svc = service as unknown as RestoreProbe;
    svc.runRestore = () => {
      const gate = deferred();
      gates.push(gate);
      return gate.promise;
    };
    const restoreA = svc.restoreBackup(VALID_FILE);
    assert(svc.isRestoreInFlight(), "restore A is in flight");
    const blocked = await svc.restoreBackup(VALID_FILE);
    assertEquals(
      blocked.error,
      "restore-in-progress",
      "same-window second restore is refused",
    );
    Date.now = () => realNow.call(Date) + 12 * 60 * 1000;
    assert(!svc.isRestoreInFlight(), "restore A lapses after the TTL");
    const restoreB = svc.restoreBackup(VALID_FILE);
    assert(svc.isRestoreInFlight(), "restore B is in flight");
    gates[0].resolve();
    await restoreA;
    const stillB = svc.isRestoreInFlight();
    gates[1].resolve();
    await restoreB;
    assert(
      stillB,
      "stale restore A finishing must not clear restore B's flag " +
        "(restoreBackup() finally sets restoreInFlight=false unconditionally)",
    );
  } finally {
    Date.now = realNow;
    for (const gate of gates) gate.resolve();
    dispose();
  }
}

function backupPath(fileName: string): string {
  return PathUtils.join(PathUtils.profileDir, "workspaces", "backup", fileName);
}

// R-04
async function testDisabledCaptureHasNoFakeWorkspace(): Promise<void> {
  await withWindows(false, false, async (open) => {
    const win = (await open()) as ServiceTestWindow;
    const backup = win.workspacesBackupFuncs;
    assert(backup, "backup API exists while workspaces are disabled");
    assert(!win.workspacesFuncs, "workspaces are disabled");
    const storeBefore = Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME);
    const tab = win.gBrowser.addTab(`${PREFIX}/r04`, {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      inBackground: true,
    }) as XULElement;
    await settle();
    let fileName: string | null = null;
    try {
      const summary = await backup.backupNow();
      fileName = summary.fileName;
      assertEquals(summary.mode, "tabs", "disabled capture uses tabs mode");
      assertEquals(summary.workspaceCount, 0, "no workspaces are counted");
      const file = await IOUtils.readJSON(backupPath(fileName), {
        decompress: true,
      }) as {
        mode: string;
        workspaceState?: unknown;
        windows: { tabs: { workspaceId?: string; url?: string }[] }[];
      };
      const text = JSON.stringify(file);
      assert(
        !text.includes("__floorp_unassigned__"),
        "file has no synthetic unassigned workspace",
      );
      assertEquals(file.mode, "tabs", "file mode is tabs");
      assertEquals(file.workspaceState, undefined, "no workspace state");
      const tabs = file.windows.flatMap((w) => w.tabs);
      assert(tabs.length > 0, "tabs were captured");
      assert(
        tabs.every((t) => t.workspaceId === undefined),
        "captured tabs carry no workspace id",
      );
      assertEquals(
        Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME),
        storeBefore,
        "capture leaves the workspace store untouched",
      );
    } finally {
      if (fileName) await backup.deleteBackup(fileName);
      win.gBrowser.removeTab(tab, { animate: false });
    }
  }, false);
}

function sessionState(urls: string[]): string {
  const principal = E10SUtils.serializePrincipal(
    Services.scriptSecurityManager.getSystemPrincipal(),
  );
  return JSON.stringify({
    windows: [{
      tabs: urls.map((url) => ({
        entries: [{ url, triggeringPrincipal_base64: principal }],
        index: 1,
      })),
      selected: 1,
    }],
  });
}

function tabsWithUrl(win: TestWindow, url: string): XULElement[] {
  return (win.gBrowser.tabs as XULElement[]).filter((tab) =>
    (tab as unknown as { linkedBrowser: { currentURI: { spec: string } } })
        .linkedBrowser.currentURI.spec === url ||
    tab.getAttribute("label") === url
  );
}

/** Session restore into a window, the same Tabbrowser path as startup restore. */
async function restoreSession(win: TestWindow, urls: string[]) {
  SessionStore.setWindowState(win, sessionState(urls), false);
  await waitFor(
    () => urls.every((url) => tabsWithUrl(win, url).length === 1),
    "session tabs are created",
  );
  await settle();
  return urls.map((url) => tabsWithUrl(win, url)[0]);
}

// S-01
async function testDisabledSessionRestoreAddsNoWorkspaceId(): Promise<void> {
  await withWindows(false, false, async (open) => {
    const win = await open();
    const storeBefore = Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME);
    const defaultID = (JSON.parse(storeBefore) as { defaultID?: string })
      .defaultID;
    assert(defaultID, "the store has a default workspace to leak");
    const urls = [`${PREFIX}/s01-a`, `${PREFIX}/s01-b`];
    const tabs = await restoreSession(win, urls);
    for (const tab of tabs) {
      assert(
        !tab.hasAttribute(WORKSPACE_TAB_ATTRIBUTION_ID),
        `restored tab has no workspace id (got ${
          tab.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID)
        })`,
      );
    }
    assertEquals(
      Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME),
      storeBefore,
      "restore leaves the workspace store untouched",
    );
  }, false);
}

// S-02
async function testEnabledSessionRestoreUsesDefaultWorkspace(): Promise<void> {
  await withWindows(false, false, async (open) => {
    const win = await open();
    const store = JSON.parse(
      Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME),
    ) as { defaultID?: string };
    assert(store.defaultID, "the store has a default workspace");
    const urls = [`${PREFIX}/s02-a`, `${PREFIX}/s02-b`];
    const tabs = await restoreSession(win, urls);
    for (const tab of tabs) {
      assertEquals(
        tab.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID),
        store.defaultID,
        "restored tab without an id joins the default workspace",
      );
    }
  }, true);
}

type ServiceGetter = ConstructorParameters<
  typeof WorkspacesBackupController
>[0];
type ServiceLike = NonNullable<ReturnType<ServiceGetter>>;

interface ControllerProbe {
  detachedService: SchedulerProbe | null;
}

interface SchedulerProbe {
  disposed: boolean;
  timer: unknown;
}

// R-05: a detached (workspaces-disabled) service started before the real
// WorkspacesService exists is disposed on handover, leaving one scheduler.
async function testHandoverLeavesOneScheduler(): Promise<void> {
  let real: WorkspacesBackupService | null = null;
  let disposeReal = () => {};
  const controller = new WorkspacesBackupController(() =>
    real ? ({ backupService: real } as unknown as ServiceLike) : null
  );
  const probe = controller as unknown as ControllerProbe;
  try {
    controller.ensureStarted();
    controller.ensureStarted();
    const detached = probe.detachedService;
    assert(detached !== null, "disabled start creates a detached service");
    assert(detached!.timer !== null, "detached scheduler is armed");
    await controller.listBackups();
    assert(probe.detachedService === detached, "detached service is reused");

    real = createRoot((dispose) => {
      disposeReal = dispose;
      return new WorkspacesBackupService(new NullHost());
    });
    controller.ensureStarted();
    assertEquals(probe.detachedService, null, "handover drops detached");
    assert(detached!.disposed, "detached service is disposed");
    assertEquals(detached!.timer, null, "detached scheduler timer cleared");
    await controller.listBackups();
    assertEquals(probe.detachedService, null, "no new detached service");
    const realProbe = real as unknown as SchedulerProbe;
    assert(realProbe.timer !== null, "the real service scheduler is armed");
  } finally {
    controller.dispose();
    disposeReal();
    await settle();
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("workspacesBackupQaG7Service.test.ts", [
    {
      name: "N-10 stale restore keeps the newer restore's flag",
      fn: testStaleRestoreKeepsNewFlag,
    },
    {
      name: "R-04 disabled capture writes no fake unassigned workspace",
      fn: testDisabledCaptureHasNoFakeWorkspace,
    },
    {
      name: "R-05 handover from detached service leaves one scheduler",
      fn: testHandoverLeavesOneScheduler,
    },
    {
      name: "S-01 disabled session restore adds no workspace id",
      fn: testDisabledSessionRestoreAddsNoWorkspaceId,
    },
    {
      name: "S-02 enabled session restore uses the default workspace",
      fn: testEnabledSessionRestoreUsesDefaultWorkspace,
    },
  ]);
}
