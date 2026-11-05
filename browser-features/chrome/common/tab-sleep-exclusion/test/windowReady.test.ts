// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { startWhenBrowserReady } from "../window-ready.ts";
import { assertEquals, runTests } from "../../../test/utils/test_harness.ts";

class FakeObservers {
  observer: nsIObserver | null = null;
  addObserver(observer: nsIObserver): void {
    this.observer = observer;
  }
  removeObserver(observer: nsIObserver): void {
    assertEquals(this.observer, observer, "remove own observer");
    this.observer = null;
  }
  notify(subject: unknown): void {
    const o = this.observer;
    if (typeof o === "function") {
      o(subject as nsISupports, "browser-delayed-startup-finished", "");
    } else {o?.observe(
        subject as nsISupports,
        "browser-delayed-startup-finished",
        "",
      );}
  }
}
function fixture() {
  return {
    win: {
      closed: false,
      gBrowser: undefined as undefined | {
        tabs: unknown[];
        addTabsProgressListener(): void;
      },
    },
    observers: new FakeObservers(),
  };
}
function testWaitAndOtherWindow(): void {
  const x = fixture();
  let calls = 0;
  const cancel = startWhenBrowserReady(
    x.win as unknown as Window,
    x.observers,
    () => calls++,
  );
  assertEquals(calls, 0, "do not initialize with missing gBrowser");
  x.observers.notify({});
  assertEquals(calls, 0, "another window must not start this one");
  x.win.gBrowser = { tabs: [], addTabsProgressListener() {} };
  x.observers.notify(x.win);
  assertEquals(calls, 1, "own ready window initializes exactly once");
  assertEquals(x.observers.observer, null, "observer removed after startup");
  cancel();
}
function testImmediateAndCancelled(): void {
  const x = fixture();
  let calls = 0;
  x.win.gBrowser = { tabs: [], addTabsProgressListener() {} };
  startWhenBrowserReady(x.win as unknown as Window, x.observers, () => calls++);
  assertEquals(calls, 1, "already ready window initializes immediately");
  assertEquals(x.observers.observer, null, "no observer on fast path");
  const y = fixture();
  const cancel = startWhenBrowserReady(
    y.win as unknown as Window,
    y.observers,
    () => calls++,
  );
  cancel();
  y.win.gBrowser = { tabs: [], addTabsProgressListener() {} };
  y.observers.notify(y.win);
  assertEquals(calls, 1, "disposed owner cannot initialize");
  assertEquals(y.observers.observer, null, "cancel removes pending observer");
}
function testClosedWindow(): void {
  const x = fixture();
  let calls = 0;
  startWhenBrowserReady(x.win as unknown as Window, x.observers, () => calls++);
  x.win.closed = true;
  x.win.gBrowser = { tabs: [], addTabsProgressListener() {} };
  x.observers.notify(x.win);
  assertEquals(calls, 0, "closed window cannot initialize");
}
export async function runAllTests(): Promise<void> {
  await runTests("tab-sleep-window-ready.test.ts", [
    { name: "wait for own browser", fn: testWaitAndOtherWindow },
    { name: "fast path and disposal", fn: testImmediateAndCancelled },
    { name: "closed window", fn: testClosedWindow },
  ]);
}
