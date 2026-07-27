// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { getPublicContainerOptions } from "../containerUtils.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

const tests: TestCase[] = [
  {
    name: "getPublicContainerOptions includes no-container entry first",
    fn() {
      const options = getPublicContainerOptions();
      assert(options.length >= 1, "must return at least the no-container option");
      assertEquals(
        options[0].userContextId,
        0,
        "first option must be no container",
      );
      assert(
        options[0].label.length > 0,
        "no-container label must not be empty",
      );
    },
  },
  {
    name: "getPublicContainerOptions uses unique userContextId values",
    fn() {
      const options = getPublicContainerOptions();
      const ids = options.map((option) => option.userContextId);
      const uniqueIds = new Set(ids);
      assertEquals(
        ids.length,
        uniqueIds.size,
        "container option ids must be unique",
      );
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("containerUtils.test.ts", tests);
}
