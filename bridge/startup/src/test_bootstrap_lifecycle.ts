// SPDX-License-Identifier: MPL-2.0

import {
  runTestBootstrapWithFailureReporting,
  type TestBootstrapPrefs,
} from "./test_bootstrap_failure.ts";
import type {
  RegisterTestStartupObserver,
  TestBootstrapLifecycle,
  TestOwnerStartupWindow,
} from "./test_bootstrap_lifecycle_types.ts";

/** Start immediately to capture run ownership, without awaiting tests in a module. */
export function startTestBootstrap(
  prefs: TestBootstrapPrefs,
  lifecycle: TestBootstrapLifecycle,
): void {
  void runTestBootstrapWithFailureReporting(
    prefs,
    async () => {
      await lifecycle.initializeFeatures();
      await lifecycle.waitForOwnerStartup();
      await lifecycle.executeTests();
    },
    lifecycle.recordStartupError,
  ).catch((error) => {
    try {
      lifecycle.reportRejectedBootstrap(error);
    } catch {
      // The correlated failure has already been published by the wrapper.
    }
  });
}

/** Wait for the owner itself, rather than another browser window's startup. */
export function waitForTestOwnerStartup(
  win: TestOwnerStartupWindow,
  registerStartupObserver: RegisterTestStartupObserver,
  timeoutMs = 120_000,
): Promise<void> {
  const ready = () =>
    win.document.readyState === "complete" &&
    win.gBrowserInit?.delayedStartupFinished === true;
  if (win.closed) {
    return Promise.reject(
      new Error("The owner test window closed during startup"),
    );
  }
  if (ready()) return Promise.resolve();

  return new Promise<void>((resolve, reject) => {
    let settled = false;
    let unregister = () => {};
    let timeout: ReturnType<typeof setTimeout> | undefined;
    const cleanup = () => {
      win.removeEventListener("load", check);
      win.removeEventListener("unload", onClose);
      unregister();
      if (timeout !== undefined) clearTimeout(timeout);
    };
    const finish = (error?: unknown) => {
      if (settled) return;
      settled = true;
      cleanup();
      if (error) reject(error);
      else resolve();
    };
    const check = () => {
      if (win.closed) onClose();
      else if (ready()) finish();
    };
    const onClose = () =>
      finish(new Error("The owner test window closed during startup"));

    win.addEventListener("load", check);
    win.addEventListener("unload", onClose);
    try {
      unregister = registerStartupObserver((subject) => {
        if (subject === win) check();
      });
      if (settled) {
        unregister();
        return;
      }
      timeout = setTimeout(() => {
        finish(
          new Error(
            `The owner test window did not finish load and delayed startup within ${timeoutMs}ms (readyState=${win.document.readyState}, delayedStartupFinished=${
              win.gBrowserInit?.delayedStartupFinished === true
            })`,
          ),
        );
      }, timeoutMs);
      // Registration can race with the owner's final startup notification.
      check();
    } catch (error) {
      finish(error);
    }
  });
}
