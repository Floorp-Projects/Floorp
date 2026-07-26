// SPDX-License-Identifier: MPL-2.0

const TEST_RUN_ID_PREF = "nora.tests.run_id";
const TEST_STATE_PREF = "nora.tests.state";
const STARTUP_ERROR_LIMIT = 300;

export interface TestBootstrapPrefs {
  getStringPref(name: string, fallback?: string): string;
  setStringPref(name: string, value: string): void;
  savePrefFile(prefFile: null): void;
}

function captureTestRunId(prefs: TestBootstrapPrefs): string {
  try {
    const runId = prefs.getStringPref(TEST_RUN_ID_PREF, "");
    return runId.trim().length > 0 ? runId : "";
  } catch {
    return "";
  }
}

function describeStartupError(error: unknown): string {
  try {
    return String(error).slice(0, STARTUP_ERROR_LIMIT);
  } catch {
    return "Unknown startup error";
  }
}

function publishTestBootstrapFailure(
  prefs: TestBootstrapPrefs,
  runId: string,
  error: string,
): void {
  if (runId.length === 0) {
    return;
  }

  try {
    prefs.setStringPref(
      TEST_STATE_PREF,
      JSON.stringify({
        status: "error",
        results: [],
        discoveredFiles: [],
        runId,
        error,
      }),
    );
    prefs.savePrefFile(null);
  } catch {
    // Diagnostic persistence must never replace the original startup failure.
  }
}

export async function runTestBootstrapWithFailureReporting<T>(
  prefs: TestBootstrapPrefs,
  bootstrap: () => Promise<T>,
  recordStartupError: (error: string) => void,
): Promise<T> {
  // Capture once, before bootstrap performs its first loader import. The
  // resulting failure state must correlate with the host runner that launched
  // this browser, even if prefs change while startup is in progress.
  const runId = captureTestRunId(prefs);

  try {
    return await bootstrap();
  } catch (error) {
    const description = describeStartupError(error);
    try {
      recordStartupError(description);
    } catch {
      // Diagnostic marker failures must not replace the startup failure.
    }
    publishTestBootstrapFailure(prefs, runId, description);
    throw error;
  }
}
