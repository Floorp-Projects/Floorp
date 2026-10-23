// SPDX-License-Identifier: MPL-2.0

import {
  startTestBootstrap,
  waitForTestOwnerStartup,
} from "./test_bootstrap_lifecycle.ts";
import type { TestOwnerStartupWindow } from "./test_bootstrap_lifecycle_types.ts";
import { claimTestRunOwnership } from "./test_run_owner.ts";

const HTTP_LOADER_PREF = "nora.dev.allow_http_loader";

async function importWithRetry(
  url: string,
  timeoutMs = 120_000,
  intervalMs = 500,
) {
  const startedAt = Date.now();
  let lastError: unknown;

  while (Date.now() - startedAt < timeoutMs) {
    try {
      return await import(`${url}?t=${Date.now()}`);
    } catch (error) {
      lastError = error;
      await new Promise((resolve) => setTimeout(resolve, intervalMs));
    }
  }

  throw lastError ?? new Error(`Failed to import module: ${url}`);
}

function setStartupMarker(key: string, value: string) {
  try {
    Services.prefs.setStringPref(key, value);
  } catch {
    // Diagnostics should never block startup flow.
  }
}

function assertHttpLoaderAllowed(): void {
  try {
    const allowed = Services.prefs.getBoolPref(HTTP_LOADER_PREF, false);
    if (allowed) {
      return;
    }
  } catch {
    // fall through to throw below
  }

  throw new Error(
    `Refusing to load privileged startup modules over HTTP without ${HTTP_LOADER_PREF}=true`,
  );
}

const isTestOwner = import.meta.env.MODE === "test" &&
  claimTestRunOwnership(Services.ppmm.sharedData);
if (import.meta.env.MODE !== "test" || isTestOwner) {
  setStartupMarker("nora.startup.mode", import.meta.env.MODE);
  setStartupMarker("nora.startup.loader", "");
  setStartupMarker("nora.startup.test", "");
  setStartupMarker("nora.startup.error", "");
}
if (import.meta.env.MODE === "dev") {
  assertHttpLoaderAllowed();
  try {
    //! Do not write `core/index.ts` as `core`
    //! This causes HMR error
    const loaderModule = await importWithRetry(
      "http://localhost:5181/loader/index.ts",
    );
    await loaderModule.default();
    setStartupMarker("nora.startup.loader", "loaded");
  } catch (error) {
    setStartupMarker("nora.startup.error", String(error).slice(0, 300));
    throw error;
  }
} else if (import.meta.env.MODE === "test") {
  Reflect.set(window, "__NORA_TEST_FEATURES_READY__", false);
  if (!isTestOwner) {
    // Test-created windows need the features under test, but must not consume
    // the host control request, run tests, or overwrite the owner's diagnostics.
    assertHttpLoaderAllowed();
    const loaderModule = await importWithRetry(
      "http://localhost:5181/loader/index.ts",
    );
    await loaderModule.default();
    Reflect.set(window, "__NORA_TEST_FEATURES_READY__", true);
  } else {
    // Test execution is scheduled independently of startup module evaluation.
    // Capture the run now, then require the actual owner window's readiness.
    startTestBootstrap(
      {
        getStringPref: (name, fallback) =>
          Services.prefs.getStringPref(name, fallback),
        setStringPref: (name, value) =>
          Services.prefs.setStringPref(name, value),
        // Gecko accepts null to flush the default prefs.js, but the generated
        // nsIPrefService type still declares this parameter as nsIFile only.
        savePrefFile: (prefFile) =>
          Services.prefs.savePrefFile(prefFile as unknown as nsIFile),
      },
      {
        async initializeFeatures() {
          assertHttpLoaderAllowed();

          const loaderModule = await importWithRetry(
            "http://localhost:5181/loader/index.ts",
          );
          await loaderModule.default();
          Reflect.set(window, "__NORA_TEST_FEATURES_READY__", true);
          setStartupMarker("nora.startup.loader", "loaded");
        },
        async waitForOwnerStartup() {
          const owner = window as unknown as TestOwnerStartupWindow;
          const checkpoint = (phase: string) => {
            setStartupMarker(
              "nora.startup.test.owner_state",
              JSON.stringify({
                phase,
                at: Date.now(),
                readyState: document.readyState,
                delayedStartupFinished:
                  owner.gBrowserInit?.delayedStartupFinished === true,
                featuresReady:
                  Reflect.get(window, "__NORA_TEST_FEATURES_READY__") === true,
              }),
            );
            try {
              Services.prefs.savePrefFile(null as unknown as nsIFile);
            } catch {
              // A diagnostic flush must not block native startup.
            }
          };
          checkpoint("waiting-for-owner-startup");
          await waitForTestOwnerStartup(owner, (observer) => {
            Services.obs.addObserver(
              observer,
              "browser-delayed-startup-finished",
            );
            return () =>
              Services.obs.removeObserver(
                observer,
                "browser-delayed-startup-finished",
              );
          });
          checkpoint("owner-startup-ready");
        },
        async executeTests() {
          const testModule = await importWithRetry(
            "http://localhost:5181/loader/test/index.ts",
          );
          await testModule.default();
          setStartupMarker("nora.startup.test", "loaded");

          // Keep the browser alive while the host collects the final state.
          const keepaliveId = setInterval(() => {}, 60_000);
          setTimeout(() => clearInterval(keepaliveId), 600_000);
        },
        recordStartupError: (error) =>
          setStartupMarker("nora.startup.error", error),
        reportRejectedBootstrap: (error) =>
          console.error("[noraneko] Test bootstrap failed:", error),
      },
    );
  }
} else {
  //@ts-expect-error TS cannot find the module from chrome that is inner in Firefox
  await (await import("chrome://noraneko/content/core.js")).default();
}
