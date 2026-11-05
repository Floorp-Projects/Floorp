// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  canInitializeWorkspace,
  waitForWorkspaceWindow,
} from "../window-ready.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

class FakeObservers {
  observer: nsIObserver | null = null;
  addObserver(observer: nsIObserver): void {
    this.observer = observer;
  }
  removeObserver(observer: nsIObserver): void {
    assertEquals(this.observer, observer, "remove the registered observer");
    this.observer = null;
  }
  notify(subject: unknown): void {
    if (typeof this.observer === "function") {
      this.observer(
        subject as nsISupports,
        "browser-delayed-startup-finished",
        "",
      );
    } else {
      this.observer?.observe(
        subject as nsISupports,
        "browser-delayed-startup-finished",
        "",
      );
    }
  }
}

function fixture() {
  const win = {
    closed: false,
    document: { readyState: "complete", getElementById: () => ({}) },
    gBrowser: undefined as undefined | {
      tabContainer: object;
      selectedTab: object;
    },
  };
  return { win, observers: new FakeObservers(), abort: new AbortController() };
}

async function testReadyAndForeignWindow() {
  const x = fixture();
  let settled = false;
  const pending = waitForWorkspaceWindow(
    x.win as unknown as Window,
    x.observers,
    x.abort.signal,
  )
    .then((ready) => {
      settled = true;
      return ready;
    });
  x.observers.notify({});
  await Promise.resolve();
  assert(!settled, "another window must not release this one");
  x.win.gBrowser = { tabContainer: {}, selectedTab: {} };
  x.observers.notify(x.win);
  assert(await pending, "own ready window initializes");
  assertEquals(x.observers.observer, null, "successful wait removes observer");
  assert(
    await waitForWorkspaceWindow(
      x.win as unknown as Window,
      x.observers,
      x.abort.signal,
    ),
    "already ready window initializes without waiting",
  );
  assertEquals(x.observers.observer, null, "fast path installs no observer");
}

async function testAbortAndMissingBrowser() {
  const x = fixture();
  const pending = waitForWorkspaceWindow(
    x.win as unknown as Window,
    x.observers,
    x.abort.signal,
  );
  x.abort.abort();
  assertEquals(await pending, false, "disposed owner cancels startup");
  assertEquals(x.observers.observer, null, "abort removes observer");
  const y = fixture();
  const missing = waitForWorkspaceWindow(
    y.win as unknown as Window,
    y.observers,
    y.abort.signal,
  );
  y.observers.notify(y.win);
  assertEquals(await missing, false, "no usable gBrowser does not initialize");
  assertEquals(y.observers.observer, null, "failed readiness removes observer");
  const ended = fixture();
  const endedWindow = Object.assign(ended.win, {
    gBrowserInit: { delayedStartupFinished: true },
  });
  assertEquals(
    await waitForWorkspaceWindow(
      endedWindow as unknown as Window,
      ended.observers,
      ended.abort.signal,
    ),
    false,
    "already-finished startup without chrome must not wait forever",
  );
  assertEquals(
    ended.observers.observer,
    null,
    "no stale observer after startup",
  );
}

async function testBrowserBeforeDocument() {
  const x = fixture();
  x.win.document.readyState = "loading";
  x.win.gBrowser = { tabContainer: {}, selectedTab: {} };
  let settled = false;
  const pending = waitForWorkspaceWindow(
    x.win as unknown as Window,
    x.observers,
    x.abort.signal,
  ).then((ready) => {
    settled = true;
    return ready;
  });
  await Promise.resolve();
  assert(!settled, "gBrowser alone must not initialize an incomplete document");
  x.win.document.readyState = "complete";
  x.observers.notify(x.win);
  assert(await pending, "initialization resumes after this window is ready");
}

async function testReadyThenOwnerDisposed() {
  const old = fixture();
  old.win.gBrowser = { tabContainer: {}, selectedTab: {} };
  const oldContinuation = waitForWorkspaceWindow(
    old.win as unknown as Window,
    old.observers,
    old.abort.signal,
  ).then((ready) => ready); // The prior caller only checked `!ready`.
  old.abort.abort();
  assertEquals(
    await oldContinuation,
    true,
    "prior guard allows disposed owner",
  );
  const x = fixture();
  x.win.gBrowser = { tabContainer: {}, selectedTab: {} };
  const continuation = waitForWorkspaceWindow(
    x.win as unknown as Window,
    x.observers,
    x.abort.signal,
  ).then((ready) => canInitializeWorkspace(ready, x.win, x.abort.signal));
  // Readiness is already resolved, but the awaiting continuation has not run.
  x.abort.abort();
  assertEquals(await continuation, false, "disposed owner must not initialize");
  const closed = fixture();
  closed.win.gBrowser = { tabContainer: {}, selectedTab: {} };
  const afterClose = waitForWorkspaceWindow(
    closed.win as unknown as Window,
    closed.observers,
    closed.abort.signal,
  ).then((ready) =>
    canInitializeWorkspace(ready, closed.win, closed.abort.signal)
  );
  closed.win.closed = true;
  assertEquals(await afterClose, false, "closed window must not initialize");
}

export async function runAllTests(): Promise<void> {
  await runTests("windowReady.test.ts", [
    { name: "own versus foreign window", fn: testReadyAndForeignWindow },
    { name: "abort and unavailable browser", fn: testAbortAndMissingBrowser },
    { name: "browser before document", fn: testBrowserBeforeDocument },
    { name: "owner disposed after ready", fn: testReadyThenOwnerDisposed },
  ]);
}
