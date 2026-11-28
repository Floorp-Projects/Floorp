// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
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
import { launchDevPorts } from "#modules/common/dev-port-offset.ts";

/**
 * QA group g7: NRWorkspaces actor bridge, trust checks and the process-wide
 * mutating lock (test IDs N-01..N-04, N-06, N-08, N-10, O-01..O-08).
 */

const PARENT_URI = "resource://noraneko/actors/NRWorkspacesParent.sys.mjs";
const BACKUP_EXPORTS = [
  "NRWorkspacesBackupNow",
  "NRWorkspacesListBackups",
  "NRWorkspacesRestoreBackup",
  "NRWorkspacesDeleteBackup",
  "NRWorkspacesDeleteAllBackups",
];
const VALID_FILE = "backup-20260101-000000-0123abcd.jsonlz4";

interface BridgeResult {
  requestId?: string | null;
  success: boolean;
  error?: string;
  data?: unknown;
}

interface ChildActor {
  backupNow(): Promise<BridgeResult>;
  listBackups(): Promise<BridgeResult>;
}

interface ParentActorLike {
  receiveMessage(message: { name: string; data: unknown }): void;
}

interface StubFuncs {
  backupNow(): Promise<unknown>;
  listBackups(): Promise<unknown>;
  restoreBackup(fileName: string): Promise<unknown>;
  deleteBackup(fileName: string): Promise<unknown>;
  deleteAllBackups(): Promise<unknown>;
  isRestoreInFlight(): boolean;
}

type ActorTestWindow = TestWindow & { workspacesBackupFuncs?: StubFuncs };

interface Deferred {
  promise: Promise<void>;
  resolve(): void;
}

function deferred(): Deferred {
  let resolve = () => {};
  const promise = new Promise<void>((r) => {
    resolve = r;
  });
  return { promise, resolve };
}

/** Funcs double: every call is recorded; held operations wait for release. */
class Stub implements StubFuncs {
  calls: string[] = [];
  holds = new Map<string, Deferred>();

  hold(op: string): Deferred {
    const gate = deferred();
    this.holds.set(op, gate);
    return gate;
  }

  releaseAll(): void {
    for (const gate of this.holds.values()) gate.resolve();
    this.holds.clear();
  }

  private async run(op: string, arg?: string): Promise<unknown> {
    this.calls.push(arg === undefined ? op : `${op}:${arg}`);
    const gate = this.holds.get(op);
    if (gate) await gate.promise;
    return op === "listBackups" ? [] : { success: true, op };
  }

  backupNow() {
    return this.run("backupNow");
  }
  listBackups() {
    return this.run("listBackups");
  }
  restoreBackup(fileName: string) {
    return this.run("restoreBackup", fileName);
  }
  deleteBackup(fileName: string) {
    return this.run("deleteBackup", fileName);
  }
  deleteAllBackups() {
    return this.run("deleteAllBackups");
  }
  isRestoreInFlight() {
    return false;
  }
}

function installStub(win: ActorTestWindow): Stub {
  const stub = new Stub();
  win.workspacesBackupFuncs = stub;
  return stub;
}

const parentModule = ChromeUtils.importESModule(PARENT_URI) as {
  NRWorkspacesParent: { prototype: object };
};

/** Parent actor instance with a faked document, window and message port. */
class FakeParent {
  private readonly actor: ParentActorLike;
  private readonly waiting = new Map<string, (result: BridgeResult) => void>();
  private seq = 0;

  constructor(uri: string, topChromeWindow: Window | null) {
    this.actor = Object.create(
      parentModule.NRWorkspacesParent.prototype,
    ) as ParentActorLike;
    const documentURI = Services.io.newURI(uri);
    Object.defineProperty(this.actor, "manager", { value: { documentURI } });
    Object.defineProperty(this.actor, "browsingContext", {
      value: { topChromeWindow, currentURI: documentURI },
    });
    Object.defineProperty(this.actor, "sendAsyncMessage", {
      value: (_name: string, data: BridgeResult) => {
        const id = data.requestId ?? "";
        this.waiting.get(id)?.(data);
        this.waiting.delete(id);
      },
    });
  }

  send(
    name: string,
    data: Record<string, unknown> = {},
  ): Promise<BridgeResult> {
    const requestId = `qa-g7-${++this.seq}-${Math.random()}`;
    const result = new Promise<BridgeResult>((resolve) => {
      this.waiting.set(requestId, resolve);
    });
    this.actor.receiveMessage({
      name: `NRWorkspaces:${name}`,
      data: { ...data, requestId },
    });
    return result;
  }
}

/** Wait until the promise settles, failing after `ms`. */
async function within<T>(promise: Promise<T>, ms: number, what: string) {
  let timer: ReturnType<typeof setTimeout> | undefined;
  try {
    return await Promise.race([
      promise,
      new Promise<never>((_, reject) => {
        timer = setTimeout(() => reject(new Error(`timed out: ${what}`)), ms);
      }),
    ]);
  } finally {
    clearTimeout(timer);
  }
}

async function openPage(win: TestWindow, url: string): Promise<XULElement> {
  const tab = win.gBrowser.addTab(url, {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  }) as XULElement;
  win.gBrowser.selectedTab = tab;
  const browser = (tab as unknown as { linkedBrowser: XULBrowserLike })
    .linkedBrowser;
  await waitFor(
    () =>
      browser.currentURI.spec !== "about:blank" &&
      !browser.webProgress?.isLoadingDocument,
    `${url} loads`,
  );
  await settle();
  return tab;
}

interface XULBrowserLike {
  currentURI: { spec: string };
  isRemoteBrowser: boolean;
  webProgress?: { isLoadingDocument: boolean };
  contentWindow:
    | Window & {
      windowGlobalChild: { getActor(name: string): unknown };
    }
    | null;
  browsingContext: {
    currentWindowGlobal: { getActor(name: string): unknown } | null;
  };
  messageManager: {
    addMessageListener(name: string, fn: (m: { data: unknown }) => void): void;
    removeMessageListener(
      name: string,
      fn: (m: { data: unknown }) => void,
    ): void;
    loadFrameScript(url: string, delayed: boolean): void;
  };
  loadURI(uri: unknown, params: Record<string, unknown>): void;
}

function browserOf(tab: XULElement): XULBrowserLike {
  return (tab as unknown as { linkedBrowser: XULBrowserLike }).linkedBrowser;
}

/** typeof of the given globals in the page, via a frame script (works for remote and parent pages). */
async function contentTypes(
  tab: XULElement,
  names: string[],
): Promise<Record<string, string>> {
  const browser = browserOf(tab);
  const msg = `qa-g7:types:${Math.random().toString(36).slice(2)}`;
  return await within(
    new Promise<Record<string, string>>((resolve) => {
      const listener = (m: { data: unknown }) => {
        browser.messageManager.removeMessageListener(msg, listener);
        resolve(m.data as Record<string, string>);
      };
      browser.messageManager.addMessageListener(msg, listener);
      const script = `
        const out = { href: content.location.href };
        for (const n of ${
        JSON.stringify(names)
      }) out[n] = typeof content.wrappedJSObject[n];
        sendAsyncMessage(${JSON.stringify(msg)}, out);`;
      browser.messageManager.loadFrameScript(
        `data:,${encodeURIComponent(script)}`,
        false,
      );
    }),
    10_000,
    "frame script reply",
  );
}

function childActor(tab: XULElement): ChildActor {
  const browser = browserOf(tab);
  assert(
    !browser.isRemoteBrowser && browser.contentWindow,
    "about:hub runs in the parent process in this build",
  );
  return browser.contentWindow!.windowGlobalChild.getActor(
    "NRWorkspaces",
  ) as unknown as ChildActor;
}

const childModule = ChromeUtils.importESModule(
  "resource://noraneko/actors/NRWorkspacesChild.sys.mjs",
) as { NRWorkspacesChild: object };

type TimerFn = (handler: () => void, ms?: number) => number;

interface TimerHost {
  setTimeout?: TimerFn;
  clearTimeout?: (id?: number) => void;
}

/**
 * Runs `fn` with setTimeout/clearTimeout defined on the actor module global.
 * The shared sys.mjs global has no timers, so this simulates the missing
 * Timer.sys.mjs import and checks the logic behind it.
 */
function withActorTimers(fn: () => Promise<void>): () => Promise<void> {
  return async () => {
    const host = Cu.getGlobalForObject(
      childModule.NRWorkspacesChild,
    ) as TimerHost;
    const added = typeof host.setTimeout !== "function";
    if (added) {
      host.setTimeout = setTimeout.bind(globalThis) as TimerFn;
      host.clearTimeout = clearTimeout.bind(globalThis);
    }
    try {
      await fn();
    } finally {
      if (added) {
        delete host.setTimeout;
        delete host.clearTimeout;
      }
    }
  };
}

// O-01
async function testHubExportsBridge(): Promise<void> {
  await withWindows(false, false, async (open) => {
    const win = (await open()) as ActorTestWindow;
    const stub = installStub(win);
    const tab = await openPage(win, "about:hub");
    const types = await contentTypes(tab, BACKUP_EXPORTS);
    for (const name of BACKUP_EXPORTS) {
      assertEquals(types[name], "function", `about:hub exposes ${name}`);
    }
    const result = await within(
      childActor(tab).listBackups(),
      10_000,
      "list through the real actor pair",
    );
    assert(result.success, `list succeeds: ${JSON.stringify(result)}`);
    assert(stub.calls.includes("listBackups"), "request reaches window funcs");
  });
}

// O-02
async function testOtherAboutPagesHaveNoBridge(): Promise<void> {
  await withWindows(false, false, async (open) => {
    const win = (await open()) as ActorTestWindow;
    for (const url of ["about:newtab", "about:preferences", "about:about"]) {
      const tab = await openPage(win, url);
      const types = await contentTypes(tab, [
        ...BACKUP_EXPORTS,
        "NRInitializeWorkspaces",
      ]);
      for (const name of BACKUP_EXPORTS) {
        assertEquals(types[name], "undefined", `${url} lacks ${name}`);
      }
      assertEquals(
        types.NRInitializeWorkspaces,
        "function",
        `${url} keeps the initialize bridge (actor did load)`,
      );
      const parent = new FakeParent(url, win);
      const stub = installStub(win);
      const result = await parent.send("ListBackups");
      assertEquals(result.error, "untrusted-document", `${url} is rejected`);
      assertEquals(stub.calls.length, 0, "no funcs were called");
    }
  });
}

// O-03
async function testWebPagesHaveNoBridge(): Promise<void> {
  await withWindows(false, false, async (open) => {
    const win = (await open()) as ActorTestWindow;
    const tab = await openPage(win, "https://example.com/floorp-qa-g7");
    const types = await contentTypes(tab, BACKUP_EXPORTS);
    for (const name of BACKUP_EXPORTS) {
      assertEquals(types[name], "undefined", `web page lacks ${name}`);
    }
    let actorError = "";
    try {
      browserOf(tab).browsingContext.currentWindowGlobal?.getActor(
        "NRWorkspaces",
      );
    } catch (error) {
      actorError = String(error);
    }
    assert(actorError !== "", "actor is not instantiated for https pages");
    for (const url of ["https://example.com/", "http://example.com/hub"]) {
      const stub = installStub(win);
      const result = await new FakeParent(url, win).send("BackupNow");
      assertEquals(result.error, "untrusted-document", `${url} rejected`);
      assertEquals(stub.calls.length, 0, "no funcs were called");
    }
  });
}

// O-04
async function testSpoofedHubUrls(): Promise<void> {
  await withWindows(false, false, async (open) => {
    const win = (await open()) as ActorTestWindow;
    const cases: [string, boolean][] = [
      ["about:hubfoo", false],
      ["about:hubfoo?x=1", false],
      ["about:hub-backup", false],
      ["about:hu", false],
      ["about:hub", true],
      ["about:hub?x=1", true],
      ["about:hub#/backup", true],
      ["about:hub?#/x", true],
      ["chrome://noraneko-settings/content/index.html", true],
      ["chrome://noraneko-settingsfoo/content/index.html", false],
      ["chrome://noraneko/content/index.html", false],
    ];
    for (const [url, trusted] of cases) {
      const stub = installStub(win);
      const result = await new FakeParent(url, win).send("ListBackups");
      if (trusted) {
        assert(result.success, `${url} is trusted: ${JSON.stringify(result)}`);
      } else {
        assertEquals(result.error, "untrusted-document", `${url} rejected`);
        assertEquals(stub.calls.length, 0, `${url} reaches no funcs`);
      }
    }
  });
}

// O-05
async function testDevPortTrust(): Promise<void> {
  await withWindows(false, false, async (open) => {
    const win = (await open()) as ActorTestWindow;
    const ports = launchDevPorts();
    const trusted = (["settings", "newtab", "welcome", "notes"] as const).map(
      (name) => ports.port(name),
    );
    const untrusted = [
      ports.port("loader"),
      ports.port("designs"),
      ports.port("profileManager"),
      9999,
      80,
    ];
    const cases: [string, boolean][] = [
      ...trusted.map((
        p,
      ): [string, boolean] => [`http://localhost:${p}/`, true]),
      ...untrusted.map((p): [string, boolean] => [
        `http://localhost:${p}/`,
        false,
      ]),
      [`https://localhost:${trusted[0]}/`, false],
      [`http://127.0.0.1:${trusted[0]}/`, false],
      [`http://example.com:${trusted[0]}/`, false],
    ];
    for (const [url, ok] of cases) {
      const stub = installStub(win);
      const result = await new FakeParent(url, win).send("ListBackups");
      if (ok) {
        assert(result.success, `${url} trusted: ${JSON.stringify(result)}`);
      } else {
        assertEquals(result.error, "untrusted-document", `${url} rejected`);
        assertEquals(stub.calls.length, 0, `${url} reaches no funcs`);
      }
    }
  });
}

// O-06
async function testMissingFileName(): Promise<void> {
  await withWindows(false, false, async (open) => {
    const win = (await open()) as ActorTestWindow;
    const stub = installStub(win);
    const parent = new FakeParent("about:hub", win);
    const bad: Record<string, unknown>[] = [
      {},
      { fileName: "" },
      { fileName: 42 },
      { fileName: null },
    ];
    for (const name of ["RestoreBackup", "DeleteBackup"]) {
      for (const data of bad) {
        const result = await parent.send(name, data);
        assertEquals(
          result.error,
          "invalid-file-name",
          `${name} ${JSON.stringify(data)} is rejected`,
        );
      }
    }
    assertEquals(stub.calls.length, 0, "funcs never see a bad file name");
    const after = await parent.send("BackupNow");
    assert(after.success, "the lock is released after validation errors");
  });
}

// O-07
async function testNavigationSettlesPending(): Promise<void> {
  await withWindows(false, false, async (open) => {
    const win = (await open()) as ActorTestWindow;
    const stub = installStub(win);
    const gate = stub.hold("backupNow");
    const tab = await openPage(win, "about:hub");
    const actor = childActor(tab);
    try {
      const pending = actor.backupNow();
      await waitFor(
        () => stub.calls.includes("backupNow"),
        "request reaches the parent",
      );
      browserOf(tab).loadURI(Services.io.newURI("about:blank"), {
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
      });
      const result = await within(pending, 10_000, "pending request settles");
      assertEquals(result.success, false, "navigated request fails");
      assertEquals(result.error, "actor-destroyed", "error is actor-destroyed");
    } finally {
      gate.resolve();
    }
    await settle();
    const after = await new FakeParent("about:hub", win).send("BackupNow");
    assert(after.success, "the lock is released once the operation ends");
  });
}

// O-08
async function testRequestTimeout(): Promise<void> {
  await withWindows(false, false, async (open) => {
    const win = (await open()) as ActorTestWindow;
    const stub = installStub(win);
    const gate = stub.hold("listBackups");
    try {
      const tab = await openPage(win, "about:hub");
      const started = Date.now();
      const result = await within(
        childActor(tab).listBackups(),
        45_000,
        "list times out",
      );
      const elapsed = Date.now() - started;
      assertEquals(result.error, "timeout", "unanswered list times out");
      assert(
        elapsed >= 29_000 && elapsed < 40_000,
        `timeout is about 30s (${elapsed}ms)`,
      );
    } finally {
      gate.resolve();
    }
  });
}

// N-01..N-04, N-06
async function testMutatingLockAcrossWindows(): Promise<void> {
  await withWindows(false, false, async (open) => {
    const winA = (await open()) as ActorTestWindow;
    const winB = (await open()) as ActorTestWindow;
    const stubA = installStub(winA);
    const stubB = installStub(winB);
    const gate = stubA.hold("restoreBackup");
    const parentA = new FakeParent("about:hub", winA);
    const parentB = new FakeParent("about:hub", winB);
    try {
      const restoreA = parentA.send("RestoreBackup", { fileName: VALID_FILE });
      await waitFor(
        () => stubA.calls.length === 1,
        "first restore reaches window A",
      );
      winA.focus();
      for (
        const [name, data] of [
          ["RestoreBackup", { fileName: VALID_FILE }],
          ["BackupNow", {}],
          ["DeleteBackup", { fileName: VALID_FILE }],
          ["DeleteAllBackups", {}],
        ] as [string, Record<string, unknown>][]
      ) {
        const result = await parentB.send(name, data);
        assertEquals(
          result.error,
          "restore-in-progress",
          `${name} from window B is locked out`,
        );
      }
      const list = await parentB.send("ListBackups");
      assert(list.success, "listing is never locked");
      assertEquals(
        stubB.calls.join(","),
        "listBackups",
        "only the list reached window B",
      );
      gate.resolve();
      const resultA = await within(restoreA, 5_000, "restore A finishes");
      assert(resultA.success, "first restore succeeds");
      assertEquals(
        stubA.calls.join(","),
        `restoreBackup:${VALID_FILE}`,
        "restore ran in the requesting window A only",
      );
      const after = await parentB.send("BackupNow");
      assert(after.success, "lock is released after the restore");
    } finally {
      stubA.releaseAll();
      stubB.releaseAll();
    }
  });
}

// N-10 (parent side): a stale lock holder must not release a newer lock.
async function testStaleLockHolderKeepsNewLock(): Promise<void> {
  await withWindows(false, false, async (open) => {
    const win = (await open()) as ActorTestWindow;
    const stub = installStub(win);
    const parent = new FakeParent("about:hub", win);
    const moduleDate = (Cu.getGlobalForObject(
      parentModule.NRWorkspacesParent,
    ) as { Date: DateConstructor }).Date;
    const realNow = moduleDate.now;
    const gateA = stub.hold("restoreBackup");
    let gateB: Deferred | null = null;
    try {
      const restoreA = parent.send("RestoreBackup", { fileName: VALID_FILE });
      await waitFor(() => stub.calls.length === 1, "restore A starts");
      const shift = 12 * 60 * 1000;
      moduleDate.now = () => realNow.call(moduleDate) + shift;
      gateB = stub.hold("backupNow");
      const backupB = parent.send("BackupNow");
      await waitFor(
        () => stub.calls.length === 2,
        "operation B acquires the lapsed lock",
      );
      gateA.resolve();
      await within(restoreA, 5_000, "stale restore A finishes");
      const third = await parent.send("DeleteAllBackups");
      assertEquals(
        third.error,
        "restore-in-progress",
        "stale holder A did not release B's lock",
      );
      gateB.resolve();
      await within(backupB, 5_000, "B finishes");
      const fourth = await parent.send("DeleteAllBackups");
      assert(fourth.success, "B released its own lock");
    } finally {
      moduleDate.now = realNow;
      gateA.resolve();
      gateB?.resolve();
    }
  });
}

export async function runAllTests(): Promise<void> {
  await runTests("workspacesBackupQaG7Actor.test.ts", [
    {
      name: "O-01 about:hub exposes the backup bridge",
      fn: testHubExportsBridge,
    },
    {
      name: "O-02 other about: pages have no backup bridge",
      fn: testOtherAboutPagesHaveNoBridge,
    },
    {
      name: "O-03 web pages have no backup bridge",
      fn: testWebPagesHaveNoBridge,
    },
    { name: "O-04 spoofed hub URLs are untrusted", fn: testSpoofedHubUrls },
    { name: "O-05 only hub dev ports are trusted", fn: testDevPortTrust },
    {
      name: "O-06 missing fileName is invalid-file-name",
      fn: testMissingFileName,
    },
    {
      name: "O-07 navigation settles pending requests",
      fn: testNavigationSettlesPending,
    },
    {
      name: "O-08 unanswered request times out at 30s",
      fn: testRequestTimeout,
    },
    {
      name: "O-01 (actor timers shimmed) list round-trips through the actors",
      fn: withActorTimers(testHubExportsBridge),
    },
    {
      name: "O-07 (actor timers shimmed) navigation settles pending requests",
      fn: withActorTimers(testNavigationSettlesPending),
    },
    {
      name: "O-08 (actor timers shimmed) unanswered request times out at 30s",
      fn: withActorTimers(testRequestTimeout),
    },
    {
      name: "N-01..N-04/N-06 mutating lock across windows",
      fn: testMutatingLockAcrossWindows,
    },
    {
      name: "N-10 stale lock holder keeps the newer lock",
      fn: testStaleLockHolderKeepsNewLock,
    },
  ]);
}
