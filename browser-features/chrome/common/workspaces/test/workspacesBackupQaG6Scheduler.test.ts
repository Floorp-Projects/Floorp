// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
// QA g6: scheduler timing with intercepted timers (L-01..L-05, L-09..L-11,
// L-13). Only timers armed by workspaces-backup-service are intercepted, so a
// test can inspect the requested delay and fire it without waiting minutes.
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
import {
  type WorkspacesBackupHost,
  WorkspacesBackupService,
} from "../utils/workspaces-backup-service.ts";
import type { WorkspacesBackupFuncs } from "../utils/workspaces-backup-controller.ts";
import {
  DEFAULT_WORKSPACE_BACKUP_CONFIG,
  type WorkspaceBackupConfig,
} from "../utils/backup-types.ts";
import {
  WORKSPACE_BACKUP_CONFIG_PREF_NAME,
  WORKSPACE_BACKUP_LAST_RUN_PREF_NAME,
  WORKSPACE_ENABLED_PREF_NAME,
} from "../utils/workspaces-static-names.ts";
import type { TWorkspaceID } from "../utils/type.ts";
import type { TestWindow } from "./workspace-last-tab-test-types.ts";

const MINUTE = 60_000;
const SERVICE_MODULE = "workspaces-backup-service";

const detachedHost: WorkspacesBackupHost = {
  supportsWorkspaceRestore: false,
  isWorkspaceID: (_id: string): _id is TWorkspaceID => false,
  getRawWorkspace: () => undefined,
  restoreWorkspaceState: () => Promise.reject(new Error("workspaces-disabled")),
};

interface ServiceInternals {
  timer: number | null;
  getBrowserWindows: () => Window[];
}

interface TimerWindow {
  setTimeout: (handler: () => void, delay?: number) => number;
  clearTimeout: (id?: number | null) => void;
}

interface FakeTimer {
  id: number;
  delay: number;
  handler: () => void;
  cleared: boolean;
  fired: boolean;
}

class ServiceTimers {
  readonly timers = new Map<number, FakeTimer>();
  private nextId = -1;
  private originalSet: TimerWindow["setTimeout"] | null = null;
  private originalClear: TimerWindow["clearTimeout"] | null = null;
  private target: TimerWindow | null = null;

  install(win: Window): void {
    const target = win as unknown as TimerWindow;
    const originalSet = target.setTimeout;
    const originalClear = target.clearTimeout;
    this.target = target;
    this.originalSet = originalSet;
    this.originalClear = originalClear;
    target.setTimeout = (handler: () => void, delay?: number): number => {
      if (!(new Error().stack ?? "").includes(SERVICE_MODULE)) {
        return originalSet.call(win, handler, delay);
      }
      const id = this.nextId--;
      this.timers.set(id, {
        id,
        delay: delay ?? 0,
        handler,
        cleared: false,
        fired: false,
      });
      return id;
    };
    target.clearTimeout = (id?: number | null): void => {
      const timer = typeof id === "number" ? this.timers.get(id) : undefined;
      if (timer) {
        timer.cleared = true;
        return;
      }
      originalClear.call(win, id);
    };
  }

  restore(): void {
    if (this.target && this.originalSet && this.originalClear) {
      this.target.setTimeout = this.originalSet;
      this.target.clearTimeout = this.originalClear;
    }
    this.target = null;
  }

  get(id: number | null): FakeTimer {
    const timer = id === null ? undefined : this.timers.get(id);
    assert(timer, `service timer ${id} was intercepted`);
    return timer;
  }

  pending(): FakeTimer[] {
    return [...this.timers.values()].filter((t) => !t.cleared && !t.fired);
  }
}

interface SavedPref {
  name: string;
  had: boolean;
  value: string;
}

const saveStringPref = (name: string): SavedPref => ({
  name,
  had: Services.prefs.prefHasUserValue(name),
  value: Services.prefs.getStringPref(name, ""),
});

const restoreStringPref = (saved: SavedPref): void => {
  if (saved.had) Services.prefs.setStringPref(saved.name, saved.value);
  else Services.prefs.clearUserPref(saved.name);
};

const setLastRun = (value: number): void =>
  Services.prefs.setStringPref(
    WORKSPACE_BACKUP_LAST_RUN_PREF_NAME,
    String(value),
  );

const readLastRun = (): number =>
  Number.parseInt(
    Services.prefs.getStringPref(WORKSPACE_BACKUP_LAST_RUN_PREF_NAME, "0"),
    10,
  );

const backupDirectory = (): string =>
  PathUtils.join(PathUtils.profileDir, "workspaces", "backup");

async function listBackupNames(): Promise<Set<string>> {
  const directory = backupDirectory();
  if (!(await IOUtils.exists(directory))) return new Set();
  const children = await IOUtils.getChildren(directory);
  return new Set(
    children
      .map((path) => PathUtils.filename(path))
      .filter((name) => /^backup-\d{8}-\d{6}-[0-9a-f]{8}\.jsonlz4$/.test(name)),
  );
}

async function newBackups(before: Set<string>): Promise<string[]> {
  return [...(await listBackupNames())].filter((name) => !before.has(name));
}

interface SchedulerContext {
  service: WorkspacesBackupService;
  internals: ServiceInternals;
  timers: ServiceTimers;
  before: Set<string>;
  /** Fires the service's current timer and waits for the next one. */
  fire(): Promise<FakeTimer | null>;
}

interface SchedulerOptions {
  config?: Partial<WorkspaceBackupConfig>;
  lastRun?: number;
  setup?: (internals: ServiceInternals) => void;
}

/**
 * Creates an isolated service with workspaces disabled (tabs-mode capture,
 * detached host) and intercepted timers. Restores prefs, deletes new backup
 * files and re-arms the windows' own schedulers with real timers.
 */
async function withScheduler(
  options: SchedulerOptions,
  check: (ctx: SchedulerContext) => Promise<void>,
): Promise<void> {
  const config = saveStringPref(WORKSPACE_BACKUP_CONFIG_PREF_NAME);
  const lastRun = saveStringPref(WORKSPACE_BACKUP_LAST_RUN_PREF_NAME);
  const hadEnabled = Services.prefs.prefHasUserValue(
    WORKSPACE_ENABLED_PREF_NAME,
  );
  const previousEnabled = Services.prefs.getBoolPref(
    WORKSPACE_ENABLED_PREF_NAME,
    true,
  );
  const before = await listBackupNames();
  const timers = new ServiceTimers();
  let disposeRoot: (() => void) | null = null;
  timers.install(window);
  try {
    setLastRun(Date.now());
    Services.prefs.setBoolPref(WORKSPACE_ENABLED_PREF_NAME, false);
    Services.prefs.setStringPref(
      WORKSPACE_BACKUP_CONFIG_PREF_NAME,
      JSON.stringify({ ...DEFAULT_WORKSPACE_BACKUP_CONFIG, ...options.config }),
    );
    if (options.lastRun !== undefined) setLastRun(options.lastRun);
    const service = createRoot((dispose) => {
      disposeRoot = dispose;
      return new WorkspacesBackupService(detachedHost);
    });
    const internals = service as unknown as ServiceInternals;
    options.setup?.(internals);
    const fire = async (): Promise<FakeTimer | null> => {
      const current = timers.get(internals.timer);
      current.fired = true;
      current.handler();
      await waitFor(
        () => internals.timer !== null && internals.timer !== current.id,
        "scheduler arms its next timer",
        30_000,
      );
      return internals.timer === null ? null : timers.get(internals.timer);
    };
    await check({ service, internals, timers, before, fire });
  } finally {
    const dispose: (() => void) | null = disposeRoot;
    if (dispose) (dispose as () => void)();
    timers.restore();
    // Keep lastRun fresh while the windows' schedulers react to the pref
    // restores below, so none of them backs up after cleanup.
    setLastRun(Date.now());
    if (hadEnabled) {
      Services.prefs.setBoolPref(WORKSPACE_ENABLED_PREF_NAME, previousEnabled);
    } else Services.prefs.clearUserPref(WORKSPACE_ENABLED_PREF_NAME);
    // Re-arm schedulers whose timers were intercepted: a config change makes
    // every live service call scheduleNext() with the real setTimeout.
    Services.prefs.setStringPref(
      WORKSPACE_BACKUP_CONFIG_PREF_NAME,
      JSON.stringify({ ...DEFAULT_WORKSPACE_BACKUP_CONFIG, retentionDays: 8 }),
    );
    restoreStringPref(config);
    await new Promise((resolve) => setTimeout(resolve, 1_500));
    for (const name of await newBackups(before)) {
      await IOUtils.remove(PathUtils.join(backupDirectory(), name));
    }
    restoreStringPref(lastRun);
    await settle();
  }
}

function assertDelayNear(
  actual: number,
  expected: number,
  toleranceMs: number,
  label: string,
): void {
  assert(
    Math.abs(actual - expected) <= toleranceMs,
    `${label}: delay ${actual} ms should be ~${expected} ms`,
  );
}

// L-03 + L-01: overdue at start -> first check after 5 s runs a backup, then
// the next run is one default interval (15 min) later.
async function testOverdueRunsFiveSecondsAfterStart(): Promise<void> {
  await withScheduler({ lastRun: Date.now() - 60 * MINUTE }, async (ctx) => {
    const first = ctx.timers.get(ctx.internals.timer);
    assertEquals(first.delay, 5_000, "first check is 5 s after start");
    const startedAt = Date.now();
    const next = await ctx.fire();
    const created = await newBackups(ctx.before);
    assertEquals(created.length, 1, "one scheduled backup is written");
    const summary = (await ctx.service.listBackups()).find((item) =>
      item.fileName === created[0]
    );
    assert(summary, "new backup is listed");
    assertEquals(summary.kind, "auto", "scheduled backup kind is auto");
    assertEquals(summary.mode, "tabs", "workspaces disabled -> tabs mode");
    assert(readLastRun() >= startedAt, "lastRun is updated by the run");
    assert(next, "next run is scheduled");
    assertDelayNear(next.delay, 15 * MINUTE, 5_000, "next run (L-01)");
  });
}

// L-02: a 5 minute interval schedules the following run 5 minutes later.
async function testFiveMinuteInterval(): Promise<void> {
  await withScheduler(
    { config: { intervalMinutes: 5 }, lastRun: Date.now() - 60 * MINUTE },
    async (ctx) => {
      const next = await ctx.fire();
      assertEquals((await newBackups(ctx.before)).length, 1, "backup ran");
      assert(next, "next run is scheduled");
      assertDelayNear(next.delay, 5 * MINUTE, 5_000, "5 min interval");
      const lastRun = Date.now() - 4 * MINUTE;
      setLastRun(lastRun);
      const after = await ctx.fire();
      assertEquals(
        (await newBackups(ctx.before)).length,
        1,
        "not due after 4 min -> no extra backup",
      );
      assert(after, "rescheduled");
      assertDelayNear(after.delay, 1 * MINUTE, 5_000, "remaining 1 min");
    },
  );
}

// L-04: within the interval at start -> the 5 s check does not back up and
// waits for the remaining time.
async function testNotDueAtStartup(): Promise<void> {
  await withScheduler({ lastRun: Date.now() - 1 * MINUTE }, async (ctx) => {
    assertEquals(ctx.timers.get(ctx.internals.timer).delay, 5_000, "5 s");
    const lastRunBefore = readLastRun();
    const next = await ctx.fire();
    assertEquals((await newBackups(ctx.before)).length, 0, "no backup");
    assertEquals(readLastRun(), lastRunBefore, "lastRun is untouched");
    assert(next, "next run is scheduled");
    assertDelayNear(next.delay, 14 * MINUTE, 6_000, "remaining ~14 min");
  });
}

// L-05 + L-06: disabled -> no timer; re-enable -> lastRun reset and a run
// is armed 1 s later, which backs up immediately.
async function testDisableAndReenable(): Promise<void> {
  await withScheduler(
    { config: { enabled: false }, lastRun: Date.now() - 60 * MINUTE },
    async (ctx) => {
      assertEquals(ctx.internals.timer, null, "disabled: no timer armed");
      const ownTimers = ctx.timers.pending().length;
      ctx.service.updateConfig({
        ...ctx.service.getConfig(),
        enabled: false,
        intervalMinutes: 5,
      });
      assertEquals(ctx.internals.timer, null, "still disabled: no timer");
      await settle();
      assertEquals((await newBackups(ctx.before)).length, 0, "no backup");
      setLastRun(Date.now());
      ctx.service.updateConfig({ ...ctx.service.getConfig(), enabled: true });
      assertEquals(readLastRun(), 0, "re-enable resets lastRun");
      const armed = ctx.timers.get(ctx.internals.timer);
      assertEquals(armed.delay, 1_000, "re-enable arms a 1 s check");
      assert(ctx.timers.pending().length > ownTimers, "timer was added");
      await ctx.fire();
      assertEquals(
        (await newBackups(ctx.before)).length,
        1,
        "re-enable backs up without waiting a full interval",
      );
      ctx.service.updateConfig({ ...ctx.service.getConfig(), enabled: false });
      assertEquals(ctx.internals.timer, null, "disable clears the timer");
      assert(armed.fired, "earlier timer fired");
    },
  );
}

// L-09: restore in progress in any window -> skip with a 5 minute backoff,
// lastRun untouched; manual backups are refused.
async function testSkipWhileRestoring(): Promise<void> {
  const host = window as Window & {
    workspacesBackupFuncs?: WorkspacesBackupFuncs;
  };
  const original = host.workspacesBackupFuncs;
  assert(original, "window backup API is available");
  host.workspacesBackupFuncs = { ...original, isRestoreInFlight: () => true };
  try {
    await withScheduler({ lastRun: Date.now() - 60 * MINUTE }, async (ctx) => {
      const lastRunBefore = readLastRun();
      const next = await ctx.fire();
      assertEquals((await newBackups(ctx.before)).length, 0, "skipped");
      assertEquals(readLastRun(), lastRunBefore, "lastRun is untouched");
      assert(next, "retry is scheduled");
      assertEquals(next.delay, 5 * MINUTE, "retry after 5 min backoff");
      let refused = "";
      try {
        await ctx.service.backupNow("manual");
      } catch (error) {
        refused = error instanceof Error ? error.message : String(error);
      }
      assertEquals(refused, "restore-in-progress", "manual backup refused");
    });
    await withScheduler(
      { config: { intervalMinutes: 1 }, lastRun: Date.now() - 60 * MINUTE },
      async (ctx) => {
        const next = await ctx.fire();
        assert(next, "retry is scheduled");
        assertEquals(next.delay, 1 * MINUTE, "backoff is capped by interval");
      },
    );
  } finally {
    host.workspacesBackupFuncs = original;
  }
  await withScheduler({ lastRun: Date.now() - 60 * MINUTE }, async (ctx) => {
    await ctx.fire();
    assertEquals(
      (await newBackups(ctx.before)).length,
      1,
      "after the restore ends the next check backs up",
    );
  });
}

// L-10: only non-restorable tabs -> no file, no error, next run after one
// interval (lastRun was already written before capture).
async function testNothingToBackUp(): Promise<void> {
  await withWindows(false, false, async (open) => {
    const win = await open();
    const initial = win.gBrowser.selectedTab as XULElement;
    const configTab = win.gBrowser.addTab("about:config", {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    }) as XULElement;
    win.gBrowser.selectedTab = configTab;
    win.gBrowser.removeTab(initial, { animate: false });
    await waitFor(
      () =>
        win.gBrowser.tabs.length === 1 &&
        win.gBrowser.selectedBrowser.currentURI?.spec === "about:config",
      "window only has about:config",
    );
    // Let the new window's own 5 s startup check pass while lastRun is fresh.
    setLastRun(Date.now());
    await new Promise((resolve) => setTimeout(resolve, 6_000));
    await withScheduler({}, async (ctx) => {
      ctx.internals.getBrowserWindows = () => [win];
      const runAt = Date.now();
      setLastRun(runAt - 60 * MINUTE);
      const next = await ctx.fire();
      assertEquals((await newBackups(ctx.before)).length, 0, "no file");
      assert(readLastRun() >= runAt, "lastRun advanced (attempt recorded)");
      assert(next, "scheduler keeps running");
      assertDelayNear(next.delay, 15 * MINUTE, 5_000, "next after interval");
    });
  }, false);
}

// L-11: lastRun in the future (clock moved back) does not stall: the next
// check treats it as due, backs up and repairs lastRun.
async function testClockMovedBack(): Promise<void> {
  await withScheduler(
    { lastRun: Date.now() + 24 * 60 * MINUTE },
    async (ctx) => {
      assertEquals(ctx.timers.get(ctx.internals.timer).delay, 5_000, "5 s");
      const next = await ctx.fire();
      assertEquals((await newBackups(ctx.before)).length, 1, "backup runs");
      assert(readLastRun() <= Date.now(), "lastRun is no longer in the future");
      assert(next, "next run is scheduled");
      assertDelayNear(next.delay, 15 * MINUTE, 5_000, "then normal interval");
    },
  );
}

// L-13: closing a window disposes its scheduler: its pending timer is cleared
// and later pref changes no longer reach it.
async function testWindowCloseStopsScheduler(): Promise<void> {
  const config = saveStringPref(WORKSPACE_BACKUP_CONFIG_PREF_NAME);
  const lastRun = saveStringPref(WORKSPACE_BACKUP_LAST_RUN_PREF_NAME);
  try {
    let closedWin: TestWindow | null = null;
    const timers = new ServiceTimers();
    await withWindows(false, false, async (open) => {
      const win = await open();
      closedWin = win;
      setLastRun(Date.now());
      timers.install(win);
      Services.prefs.setStringPref(
        WORKSPACE_BACKUP_CONFIG_PREF_NAME,
        JSON.stringify({
          ...DEFAULT_WORKSPACE_BACKUP_CONFIG,
          retentionDays: 9,
        }),
      );
      const armed = timers.pending();
      assert(armed.length >= 1, "window's scheduler re-armed on pref change");
      assert(
        armed.every((t) => t.delay === 1_000),
        "1 s re-check after change",
      );
      win.close();
      await waitFor(() => win.closed, "window closes");
      await settle();
      assert(
        armed.every((t) => t.cleared),
        "closing the window clears its scheduler timer",
      );
      const countAfterClose = timers.timers.size;
      Services.prefs.setStringPref(
        WORKSPACE_BACKUP_CONFIG_PREF_NAME,
        JSON.stringify({
          ...DEFAULT_WORKSPACE_BACKUP_CONFIG,
          retentionDays: 10,
        }),
      );
      await settle();
      assertEquals(
        timers.timers.size,
        countAfterClose,
        "closed window's observer no longer schedules",
      );
    });
    timers.restore();
    assert(closedWin, "window was opened");
  } finally {
    setLastRun(Date.now());
    restoreStringPref(config);
    await new Promise((resolve) => setTimeout(resolve, 1_500));
    restoreStringPref(lastRun);
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("workspacesBackupQaG6Scheduler.test.ts", [
    {
      name: "L-03/L-01 overdue start runs after 5 s, then every 15 min",
      fn: testOverdueRunsFiveSecondsAfterStart,
    },
    { name: "L-02 5 minute interval", fn: testFiveMinuteInterval },
    { name: "L-04 not due at startup", fn: testNotDueAtStartup },
    { name: "L-05/L-06 disable and re-enable", fn: testDisableAndReenable },
    { name: "L-09 skip while restoring", fn: testSkipWhileRestoring },
    { name: "L-10 nothing to back up", fn: testNothingToBackUp },
    { name: "L-11 clock moved back", fn: testClockMovedBack },
    {
      name: "L-13 window close stops scheduler",
      fn: testWindowCloseStopsScheduler,
    },
  ]);
}
