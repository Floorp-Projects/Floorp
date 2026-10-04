// SPDX-License-Identifier: MPL-2.0

import type { TestRunSharedData } from "./test_run_owner_types.ts";

const TEST_RUN_OWNER_KEY = "floorp/test-run-owner";

export function claimTestRunOwnership(sharedData: TestRunSharedData): boolean {
  // This map belongs to the browser process, unlike module globals (one per
  // window) or prefs (which survive restarting the browser). Claim before any
  // asynchronous initialization and retain ownership after completion/failure,
  // so a window opened by a test cannot restart the suite or replace its result.
  if (sharedData.has(TEST_RUN_OWNER_KEY)) return false;
  sharedData.set(TEST_RUN_OWNER_KEY, true);
  return true;
}
