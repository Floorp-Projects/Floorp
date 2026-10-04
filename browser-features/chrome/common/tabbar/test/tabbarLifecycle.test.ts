// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { signal } from "@preact/signals";
import { addDisposer, createRoot, rootEffect } from "@nora/preact-xul/lifetime";
import { initializeTabbarWhenReady } from "../lifecycle.ts";
import { assertEquals, runTests } from "../../../test/utils/test_harness.ts";

const nextTimer = () =>
  new Promise((resolve) => globalThis.setTimeout(resolve));

export async function runAllTests(): Promise<void> {
  await runTests("tabbarLifecycle.test.ts", [
    {
      name: "disposing before session readiness cancels delayed setup",
      async fn() {
        let ready = () => {};
        const pending = new Promise<void>((resolve) => ready = resolve);
        let setups = 0;
        const dispose = createRoot((cleanup) => {
          initializeTabbarWhenReady(pending, () => setups++, 0);
          return cleanup;
        });
        dispose();
        ready();
        await nextTimer();
        assertEquals(setups, 0, "a disposed feature must not initialize late");
      },
    },
    {
      name: "disposing during the delay cancels the scheduled setup",
      async fn() {
        let setups = 0;
        const dispose = createRoot((cleanup) => {
          initializeTabbarWhenReady(Promise.resolve(), () => setups++, 0);
          return cleanup;
        });
        await Promise.resolve();
        dispose();
        await nextTimer();
        assertEquals(setups, 0, "the pending timer must be cancelled");
      },
    },
    {
      name: "delayed effects and cleanups belong to the feature lifetime",
      async fn() {
        const state = signal(0);
        const observed: number[] = [];
        let cleanups = 0;
        const dispose = createRoot((cleanup) => {
          initializeTabbarWhenReady(Promise.resolve(), () => {
            rootEffect(() => {
              observed.push(state.value);
            });
            addDisposer(() => cleanups++);
          }, 0);
          return cleanup;
        });
        try {
          await Promise.resolve();
          await nextTimer();
          state.value = 1;
          assertEquals(observed.join(","), "0,1", "setup effect stays live");
          dispose();
          state.value = 2;
          dispose();
          assertEquals(observed.join(","), "0,1", "disposed effect is stopped");
          assertEquals(cleanups, 1, "deferred resources are disposed once");
        } finally {
          dispose();
        }
      },
    },
  ]);
}
