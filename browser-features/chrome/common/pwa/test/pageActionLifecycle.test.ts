// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { createRootHMR } from "@nora/solid-xul";
import { SsbPageAction } from "../SsbPageAction.tsx";
import type { PwaService } from "../pwaService.ts";
import type { PageActionTestInstance } from "./types.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

const MANIFEST_TOPIC = "nora-pwa-check-page-has-manifest";
const timerWindow: Window = window;

function manifestObserverCount(): number {
  let count = 0;
  const observers = Services.obs.enumerateObservers(MANIFEST_TOPIC);
  while (observers.hasMoreElements()) {
    observers.getNext();
    count++;
  }
  return count;
}

function tabSelectListenerCount(): number {
  return Cc["@mozilla.org/eventlistenerservice;1"].getService(
    Ci.nsIEventListenerService,
  ).getListenerInfoFor(gBrowser.tabContainer).filter((info) =>
    info.type === "TabSelect"
  ).length;
}

function deferredBoolean() {
  let complete: (value: boolean) => void = () => {};
  const promise = new Promise<boolean>((resolve) => complete = resolve);
  return { promise, complete };
}

async function testPendingManifestCheckCannotReviveClosedOwner(): Promise<
  void
> {
  for (const pendingStage of ["manifest", "installed"]) {
    const observers = manifestObserverCount();
    const tabListeners = tabSelectListenerCount();
    const pending = deferredBoolean();
    const target = new EventTarget();
    let firstChecks = 0;
    let installedChecks = 0;
    let uiUpdates = 0;
    const service = {
      checkBrowserCanBeInstallAsPwa: () => {
        firstChecks++;
        return pendingStage === "manifest"
          ? pending.promise
          : Promise.resolve(false);
      },
      checkCurrentPageIsInstalled: () => {
        installedChecks++;
        return pending.promise;
      },
      updateUIElements: () => uiUpdates++,
    } as unknown as PwaService;
    let instance: PageActionTestInstance | undefined;
    const dispose = createRootHMR(
      (cleanup) => {
        instance = new SsbPageAction(
          service,
        ) as unknown as PageActionTestInstance;
        return cleanup;
      },
      undefined,
      target,
    );
    try {
      assertEquals(
        manifestObserverCount(),
        observers + 1,
        "constructor owns one manifest observer",
      );
      assertEquals(
        tabSelectListenerCount(),
        tabListeners + 1,
        "constructor owns one TabSelect listener",
      );
      await Promise.resolve();
      if (pendingStage === "installed") {
        assertEquals(
          installedChecks,
          1,
          "second asynchronous stage has started",
        );
      }
      target.dispatchEvent(new Event("unload"));
      dispose();
      assertEquals(
        manifestObserverCount(),
        observers,
        "unload releases the exact native observer",
      );
      assertEquals(
        tabSelectListenerCount(),
        tabListeners,
        "unload releases the exact tab listener",
      );
      assert(instance, "real page action was constructed");
      assertEquals(
        instance.disposed,
        true,
        "unload marks page action disposed",
      );
      const checksBeforeLateCall = firstChecks;
      await instance.onCheckPageHasManifest();
      assertEquals(
        firstChecks,
        checksBeforeLateCall,
        "late callbacks cannot start a new manifest check",
      );
      pending.complete(true);
      await Promise.resolve();
      await Promise.resolve();
      assertEquals(
        uiUpdates,
        0,
        "pending completion cannot update closed-window UI",
      );
      if (pendingStage === "manifest") {
        assertEquals(
          installedChecks,
          0,
          "closed first await cannot start a second browser operation",
        );
      }
    } finally {
      dispose();
      pending.complete(false);
    }
  }
}

async function waitFor(check: () => boolean, label: string): Promise<void> {
  for (let attempt = 0; attempt < 300; attempt++) {
    if (check()) return;
    await new Promise<void>((resolve) => timerWindow.setTimeout(resolve, 50));
  }
  throw new Error(`Timed out waiting for PWA lifecycle: ${label}`);
}

async function testNativeWindowCloseReleasesManifestObserver(): Promise<void> {
  const pref = "floorp.browser.ssb.enabled";
  const hadPref = Services.prefs.prefHasUserValue(pref);
  const previous = Services.prefs.getBoolPref(pref, true);
  const openWindow = (globalThis as unknown as { OpenBrowserWindow(): Window })
    .OpenBrowserWindow;
  let otherWindow: Window | null = null;
  try {
    Services.prefs.setBoolPref(pref, true);
    const baseline = manifestObserverCount();
    for (let cycle = 0; cycle < 3; cycle++) {
      otherWindow = openWindow();
      await waitFor(
        () => manifestObserverCount() === baseline + 1,
        "new native window page action",
      );
      otherWindow.close();
      otherWindow = null;
      await waitFor(
        () => manifestObserverCount() === baseline,
        "closed native window page action",
      );
    }
  } finally {
    otherWindow?.close();
    if (hadPref) Services.prefs.setBoolPref(pref, previous);
    else Services.prefs.clearUserPref(pref);
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("pageActionLifecycle.test.ts", [
    {
      name:
        "real page action constructor unload removes native listeners and invalidates either pending await",
      fn: testPendingManifestCheckCannotReviveClosedOwner,
    },
    {
      name: "three native browser-window closes release manifest observers",
      fn: testNativeWindowCloseReleasesManifestObserver,
    },
  ]);
}
