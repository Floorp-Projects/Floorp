// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { getPanelMountTargets } from "../components/mount-target.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

function fakeWindow(): Window {
  return Object.assign(new EventTarget(), {
    closed: false,
  }) as unknown as Window;
}

async function testLateWindowTargets() {
  const events = new EventTarget();
  let readyState = "loading";
  const parent = document.createElement("div");
  const before = document.createElement("div");
  let installed = false;
  let releaseSidebar = () => {};
  const sidebarReady = new Promise<void>((resolve) => releaseSidebar = resolve);
  const doc = {
    get readyState() {
      return readyState;
    },
    addEventListener: events.addEventListener.bind(events),
    removeEventListener: events.removeEventListener.bind(events),
    getElementById(id: string) {
      return installed ? (id === "browser" ? parent : before) : null;
    },
  } as unknown as Document;
  // This is the original ordering: a null reference captured before the DOM
  // exists stays null even after the sidebar promise resolves.
  const capturedBeforeStartup = doc.getElementById("browser");
  let settled = false;
  const pending = getPanelMountTargets(doc, sidebarReady, fakeWindow()).then(
    (v) => {
      settled = true;
      return v;
    },
  );
  parent.appendChild(before);
  installed = true;
  readyState = "interactive";
  events.dispatchEvent(new Event("DOMContentLoaded"));
  await Promise.resolve();
  assert(!settled, "must also wait for this window's sidebar initialization");
  releaseSidebar();
  const targets = await pending;
  assert(targets, "ready window should have targets");
  assertEquals(
    capturedBeforeStartup,
    null,
    "early captured reference is stale",
  );
  assertEquals(targets.parent, parent, "parent is looked up after readiness");
  assertEquals(targets.before, before, "marker belongs to the same window");
}

async function testAlreadyReadyAndInvalidMarker() {
  const parent = document.createElement("div");
  const before = document.createElement("div");
  parent.appendChild(before);
  const doc = {
    readyState: "complete",
    getElementById: (id: string) => id === "browser" ? parent : before,
  } as unknown as Document;
  const targets = await getPanelMountTargets(
    doc,
    Promise.resolve(),
    fakeWindow(),
  );
  assert(targets, "ready window should have targets");
  assertEquals(targets.parent, parent, "ready window mounts immediately");
  before.remove();
  let failed = false;
  try {
    await getPanelMountTargets(doc, Promise.resolve(), fakeWindow());
  } catch {
    failed = true;
  }
  assert(failed, "foreign or missing marker must not be inserted");
}

async function testClosedAndRejectedStartup() {
  const events = new EventTarget();
  const doc = {
    readyState: "loading",
    addEventListener: events.addEventListener.bind(events),
    removeEventListener: events.removeEventListener.bind(events),
    getElementById: () => null,
  } as unknown as Document;
  const win = fakeWindow();
  const pending = getPanelMountTargets(doc, new Promise<void>(() => {}), win);
  win.dispatchEvent(new Event("unload"));
  assertEquals(await pending, null, "unloaded window must stop waiting");

  let reject = (_error: Error) => {};
  const sidebarReady = new Promise<void>((_resolve, fail) => reject = fail);
  const failed = getPanelMountTargets(doc, sidebarReady, fakeWindow());
  reject(new Error("sidebar initialization failed"));
  let failure = "";
  try {
    await failed;
  } catch (error) {
    failure = String(error);
  }
  assert(
    failure.includes("sidebar initialization failed"),
    "sidebar rejection must be observed even while the DOM is loading",
  );
}

export async function runAllTests(): Promise<void> {
  await runTests("panelSidebarMount.test.ts", [
    { name: "late document and sidebar mount", fn: testLateWindowTargets },
    {
      name: "ready and invalid mount targets",
      fn: testAlreadyReadyAndInvalidMarker,
    },
    {
      name: "closed window and early sidebar rejection",
      fn: testClosedAndRejectedStartup,
    },
  ]);
}
