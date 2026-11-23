// SPDX-License-Identifier: MPL-2.0

import { runTestBootstrapWithFailureReporting } from "../../../startup/src/test_bootstrap_failure.ts";
import runBrowserTests from "./index.ts";

/** Loaded by the owning window after the packaged production core initializes. */
export default async function runPackagedTests(): Promise<void> {
  await runTestBootstrapWithFailureReporting(
    {
      getStringPref: (name, fallback) =>
        Services.prefs.getStringPref(name, fallback),
      setStringPref: (name, value) => Services.prefs.setStringPref(name, value),
      savePrefFile: (file) =>
        Services.prefs.savePrefFile(file as unknown as nsIFile),
    },
    () => runBrowserTests(true),
    (error) => Services.prefs.setStringPref("nora.startup.error", error),
  );
}
