// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { getScrollTargetContext } from "../utils/scroll-target.ts";
import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

interface TestContext {
  readonly top: TestContext;
}

function rootContext(): TestContext {
  return {
    get top() {
      return this;
    },
  };
}

const selected = rootContext();
const foreign = rootContext();
const frame = { top: selected };
const nestedFrame = { top: frame.top };

const tests: TestCase[] = [
  {
    name: "no focused content falls back to the selected tab",
    fn: () =>
      assertEquals(
        getScrollTargetContext(selected, null),
        selected,
        "null focus",
      ),
  },
  {
    name: "unavailable focus falls back to the selected tab",
    fn: () =>
      assertEquals(
        getScrollTargetContext(selected, undefined),
        selected,
        "undefined focus",
      ),
  },
  {
    name: "focused selected document remains the target",
    fn: () =>
      assertEquals(
        getScrollTargetContext(selected, selected),
        selected,
        "root focus",
      ),
  },
  {
    name: "focused iframe receives scroll commands",
    fn: () =>
      assertEquals(
        getScrollTargetContext(selected, frame),
        frame,
        "focused frame",
      ),
  },
  {
    name: "focused nested descendant receives scroll commands",
    fn: () =>
      assertEquals(
        getScrollTargetContext(selected, nestedFrame),
        nestedFrame,
        "nested frame",
      ),
  },
  {
    name: "another tab or window cannot redirect the command",
    fn: () =>
      assertEquals(
        getScrollTargetContext(selected, foreign),
        selected,
        "foreign root",
      ),
  },
  {
    name: "a descendant of another tab is not a target",
    fn: () =>
      assertEquals(
        getScrollTargetContext(selected, { top: foreign }),
        selected,
        "foreign frame",
      ),
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("scrollTarget.test.ts", tests);
}
