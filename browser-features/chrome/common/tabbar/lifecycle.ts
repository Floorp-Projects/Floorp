// SPDX-License-Identifier: MPL-2.0

import { addDisposer, createRoot } from "@nora/preact-xul/lifetime";

/** Keep delayed tabbar setup and all resources it creates under the owner. */
export function initializeTabbarWhenReady(
  ready: Promise<unknown>,
  initialize: () => void,
  delayMs = 1000,
): void {
  let disposed = false;
  let timer: ReturnType<typeof globalThis.setTimeout> | undefined;
  let disposeSetup: (() => void) | undefined;
  addDisposer(() => {
    disposed = true;
    globalThis.clearTimeout(timer);
    disposeSetup?.();
  });

  ready.then(() => {
    if (disposed) return;
    timer = globalThis.setTimeout(() => {
      timer = undefined;
      if (disposed) return;
      disposeSetup = createRoot((dispose) => {
        initialize();
        return dispose;
      });
    }, delayMs);
  }).catch((error) => {
    console.error("[TabBar] Failed to wait for session initialization:", error);
  });
}
