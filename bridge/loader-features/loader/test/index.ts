// SPDX-License-Identifier: MPL-2.0

/**
 * Browser-side test entry point.
 *
 * Loaded by chrome_root.ts when MODE === "test".
 * Discovers all @colocated-env browser test files via import.meta.glob,
 * runs them sequentially, and writes structured results to
 * globalThis.__TEST_RESULTS__ for the host-side collector to read
 * via Marionette.
 */

interface TestResult {
  file: string;
  ok: boolean;
  durationMs: number;
  mode: "import" | "runAllTests";
  error?: string;
}

interface TestState {
  status: "running" | "done" | "error";
  results: TestResult[];
  discoveredFiles: string[];
  error?: string;
}

const TEST_STATE_PREF = "nora.tests.state";

declare global {
  var __TEST_RESULTS__: TestState | undefined;
}

function setSharedStatePref(state: TestState): void {
  const payload = JSON.stringify(state);

  try {
    const maybeServices = (globalThis as Record<string, unknown>).Services as
      | { prefs?: { setStringPref: (name: string, value: string) => void } }
      | undefined;
    maybeServices?.prefs?.setStringPref(TEST_STATE_PREF, payload);
    return;
  } catch {
    // try fallback path below
  }

  try {
    const chromeUtils = (globalThis as Record<string, unknown>).ChromeUtils as
      | {
          importESModule: (specifier: string) => {
            Services?: {
              prefs?: { setStringPref: (name: string, value: string) => void };
            };
          };
        }
      | undefined;

    const servicesFromModule = chromeUtils?.importESModule(
      "resource://gre/modules/Services.sys.mjs",
    ).Services;
    servicesFromModule?.prefs?.setStringPref(TEST_STATE_PREF, payload);
  } catch {
    // Keep browser-side tests running even if prefs are unavailable.
  }
}

function publishState(state: TestState): void {
  globalThis.__TEST_RESULTS__ = state;
  setSharedStatePref(state);
}

// Keep this below host-side collection timeout defaults while giving slow
// test environments enough room.
const PER_TEST_TIMEOUT_MS = 120_000;

type LazyModule = () => Promise<unknown>;

function nowMs(): number {
  return globalThis.performance?.now() ?? Date.now();
}

function normalizeLoadedModule(value: unknown): Record<string, unknown> {
  if (value && typeof value === "object") {
    return value as Record<string, unknown>;
  }

  // Defensive normalization for unusual dynamic import return shapes.
  return {};
}

function withTimeout<T>(promise: Promise<T>, ms: number): Promise<T> {
  return new Promise<T>((resolve, reject) => {
    const timeoutId = setTimeout(() => {
      reject(new Error(`Test timed out after ${ms}ms`));
    }, ms);

    promise.then(
      (value) => {
        clearTimeout(timeoutId);
        resolve(value);
      },
      (error) => {
        clearTimeout(timeoutId);
        reject(error);
      },
    );
  });
}

async function runSingleTest(
  file: string,
  loader: LazyModule,
): Promise<TestResult> {
  const started = nowMs();
  let mode: TestResult["mode"] = "import";

  try {
    const loaded = await withTimeout(loader(), PER_TEST_TIMEOUT_MS);
    const mod = normalizeLoadedModule(loaded);
    const runAllTests = mod.runAllTests;

    if (typeof runAllTests === "function") {
      mode = "runAllTests";
      const result = await withTimeout(
        Promise.resolve((runAllTests as () => unknown)()),
        PER_TEST_TIMEOUT_MS,
      );
      if (result === false) {
        throw new Error("runAllTests returned false");
      }
      return {
        file,
        ok: true,
        durationMs: Math.round(nowMs() - started),
        mode,
      };
    }

    return {
      file,
      ok: true,
      durationMs: Math.round(nowMs() - started),
      mode,
    };
  } catch (e: unknown) {
    return {
      file,
      ok: false,
      durationMs: Math.round(nowMs() - started),
      mode,
      error: e instanceof Error ? e.message : String(e),
    };
  }
}

export default async function runBrowserTests(): Promise<void> {
  const results: TestResult[] = [];
  const discoveredFiles: string[] = [];
  publishState({ status: "running", results, discoveredFiles });

  try {
    // Chrome layer tests (via #features-chrome alias)
    const chromeTests = import.meta.glob(
      "#features-chrome/**/test/**/*.test.{ts,mts,tsx,js,mjs,jsx}",
    );

    // ESM layer tests (via #features-modules alias)
    const esmTests = import.meta.glob(
      "#features-modules/**/*.test.{ts,mts,tsx,js,mjs,jsx}",
    );

    // Pages layer tests (via #features-pages alias)
    const pagesTests = import.meta.glob(
      "#features-pages/pages-*/**/*.test.{ts,mts,tsx,js,mjs,jsx}",
    );

    const allTests = { ...chromeTests, ...esmTests, ...pagesTests };
    const entries = Object.entries(allTests) as Array<[string, LazyModule]>;
    discoveredFiles.push(
      ...entries.map(([file]) => file).sort((a, b) => a.localeCompare(b)),
    );
    publishState({ status: "running", results, discoveredFiles });

    console.log(`[nora@test] Found ${entries.length} browser test file(s).`);

    for (const [file, loader] of entries) {
      console.log(`[nora@test] Running ${file}`);
      const result = await runSingleTest(file, loader);
      results.push(result);

      if (result.ok) {
        console.log(
          `[nora@test] \u2713 ${file} (${result.mode}, ${result.durationMs}ms)`,
        );
      } else {
        console.error(
          `[nora@test] \u2717 ${file} (${result.durationMs}ms): ${result.error}`,
        );
      }
    }

    const passed = results.filter((r) => r.ok).length;
    const failed = results.length - passed;
    console.log(`[nora@test] Done: ${passed} passed, ${failed} failed`);

    publishState({ status: "done", results, discoveredFiles });
  } catch (e: unknown) {
    const msg = e instanceof Error ? e.message : String(e);
    console.error(`[nora@test] Fatal error: ${msg}`);
    publishState({
      status: "error",
      results,
      discoveredFiles,
      error: msg,
    });
  }
}
