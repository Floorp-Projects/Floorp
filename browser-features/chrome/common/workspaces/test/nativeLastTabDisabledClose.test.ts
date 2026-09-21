// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { runTests } from "../../../test/utils/test_harness.ts";
import { nativeHiddenTabTests } from "./workspace-last-tab-test-utils.ts";

export async function runAllTests(): Promise<void> {
  await runTests(
    "nativeLastTabDisabledClose.test.ts",
    nativeHiddenTabTests(false, [true]),
  );
}
