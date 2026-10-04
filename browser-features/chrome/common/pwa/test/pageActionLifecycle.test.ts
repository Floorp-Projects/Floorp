// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { createRoot } from "@nora/preact-xul/lifetime";
import type { Signal } from "@preact/signals";
import { SsbPageAction } from "../SsbPageAction.tsx";
import type { PwaService } from "../pwaService.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

const flushPromises = async () => {
  await Promise.resolve();
  await Promise.resolve();
};

async function testStaleChecksAndDisposal(): Promise<void> {
  assert(
    document.getElementById("page-action-buttons"),
    "the native page action host is present",
  );
  assert(
    document.getElementById("star-button-box"),
    "the native insertion marker is present",
  );
  const manifests: Array<(value: boolean) => void> = [];
  const installations: Array<(value: boolean) => void> = [];
  const containers: Array<(value: boolean) => void> = [];
  const nativeUpdates: boolean[] = [];
  const service = {
    checkBrowserCanBeInstallAsPwa: () =>
      new Promise<boolean>((resolve) => manifests.push(resolve)),
    checkCurrentPageIsInstalled: () =>
      new Promise<boolean>((resolve) => installations.push(resolve)),
    checkPageIsInstalledForContainer: () =>
      new Promise<boolean>((resolve) => containers.push(resolve)),
    updateUIElements: (installed: boolean) => nativeUpdates.push(installed),
  } as unknown as PwaService;
  let dispose = () => {};
  let action!: SsbPageAction;
  try {
    createRoot((cleanup) => {
      dispose = cleanup;
      action = new SsbPageAction(service);
    });
    const check = Reflect.get(action, "onCheckPageHasManifest") as () =>
      Promise<void>;
    manifests[0](false);
    await flushPromises();
    assertEquals(installations.length, 1, "the first install query is pending");
    const latest = check.call(action);
    manifests[1](false);
    await flushPromises();
    installations[1](false);
    await latest;
    installations[0](true);
    await flushPromises();
    assertEquals(
      JSON.stringify(nativeUpdates),
      "[false]",
      "an older install result cannot overwrite the latest native UI",
    );

    const selectContainer = Reflect.get(action, "onContainerSelect") as (
      id: number,
    ) => void;
    const panelIsInstalled = Reflect.get(action, "panelIsInstalled") as Signal<
      boolean
    >;
    selectContainer(3);
    selectContainer(5);
    containers[1](true);
    await flushPromises();
    containers[0](false);
    await flushPromises();
    assertEquals(
      panelIsInstalled.peek(),
      true,
      "a slow prior container check cannot overwrite the selected container",
    );

    const pending = check.call(action);
    manifests[2](false);
    await flushPromises();
    selectContainer(7);
    dispose();
    installations[2](true);
    containers[2](false);
    await pending;
    await flushPromises();
    assertEquals(
      JSON.stringify(nativeUpdates),
      "[false]",
      "a disposed page action cannot update browser UI",
    );
    assertEquals(
      panelIsInstalled.peek(),
      true,
      "a disposed container check cannot publish state",
    );
  } finally {
    dispose();
    for (const resolve of manifests) resolve(false);
    for (const resolve of installations) resolve(false);
    for (const resolve of containers) resolve(false);
    await flushPromises();
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("pageActionLifecycle.test.ts", [{
    name:
      "PWA page action ignores stale checks and pending results after disposal",
    fn: testStaleChecksAndDisposal,
  }]);
}
