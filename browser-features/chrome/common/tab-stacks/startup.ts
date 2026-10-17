// SPDX-License-Identifier: MPL-2.0

import { createRoot, getOwner, onCleanup } from "solid-js";
import type { TabStacksChrome, TabStacksStartupOptions } from "./types.ts";

/** SessionStore readiness is process-wide; browser chrome readiness is per window. */
export function initializeTabStacksWhenReady(
  options: TabStacksStartupOptions,
): void {
  const owner = getOwner();
  let disposed = false;
  let started = false;
  let disposeInitialization: (() => void) | undefined;
  let stopWaiting = () => {};
  onCleanup(() => {
    disposed = true;
    stopWaiting();
    disposeInitialization?.();
  });

  const mount = (chrome: TabStacksChrome) => {
    if (disposed || options.window.closed || started) return;
    started = true;
    stopWaiting();
    // Promise callbacks have no Solid owner. Give rendering and subscriptions a
    // root with the original context, and explicitly link its disposal to the
    // component root (createRoot's owner argument alone does not do that).
    createRoot((dispose) => {
      let childDisposed = false;
      disposeInitialization = () => {
        if (childDisposed) return;
        childDisposed = true;
        dispose();
      };
      try {
        options.initialize(chrome);
      } catch (error) {
        try {
          disposeInitialization();
        } catch (cleanupError) {
          options.logger.error("Failed to clean up tab stacks:", cleanupError);
        }
        throw error;
      }
    }, owner);
  };

  if (options.window.closed) return;
  const chrome = options.getChrome();
  if (chrome) {
    mount(chrome);
    return;
  }

  const resume = () => {
    stopWaiting();
    if (disposed || options.window.closed || started) return;
    // The original references may have been absent or replaced during restore.
    const readyChrome = options.getChrome();
    if (readyChrome) mount(readyChrome);
    else options.logger.warn("Browser chrome not ready after delayed startup.");
  };
  const reportFailure = (error: unknown) => {
    if (!disposed && !options.window.closed) {
      options.logger.error(
        "Failed to initialize tab stacks after startup:",
        error,
      );
    }
  };
  const startup = options.window.delayedStartupPromise ??
    options.window.gBrowserInit?.delayedStartupPromise;
  options.logger.info(
    "Deferring tab stacks until this window's delayed startup.",
  );
  if (startup) {
    startup.then(resume).catch(reportFailure);
    return;
  }

  // Support runtimes exposing the observer notification instead of the promise.
  // Filter by the owning window: another restored window can finish first.
  if (options.window.gBrowserInit?.delayedStartupFinished) {
    resume();
    return;
  }
  const onStartup = (subject: nsISupports | null) => {
    if ((subject as unknown) !== options.window) return;
    try {
      resume();
    } catch (error) {
      reportFailure(error);
    }
  };
  stopWaiting = () => {
    Services.obs.removeObserver(onStartup, "browser-delayed-startup-finished");
    stopWaiting = () => {};
  };
  Services.obs.addObserver(onStartup, "browser-delayed-startup-finished");
}
