// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import {
  MAX_BACKGROUND_TIMER_DELAY,
  nextBackgroundDelay,
} from "../../../src/features/newtab/backgroundTiming.ts";
import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";

const tests: TestCase[] = [
  {
    name: "Zero and invalid slideshow intervals never schedule automatic updates",
    fn: () => {
      for (const seconds of [0, -1, NaN, Infinity]) {
        assertEquals(nextBackgroundDelay(seconds, 0), null, "No initial timer");
        assertEquals(
          nextBackgroundDelay(seconds, 10_000),
          null,
          "No later timer",
        );
      }
    },
  },
  {
    name: "Fractional seconds retain their timing and update only at their deadline",
    fn: () => {
      assertEquals(
        nextBackgroundDelay(0.5, 0),
        500,
        "Keep half-second settings",
      );
      assertEquals(
        nextBackgroundDelay(0.5, 125),
        375,
        "Wait for the remainder",
      );
      assertEquals(
        nextBackgroundDelay(0.5, 499),
        1,
        "An early timer is not due",
      );
      assertEquals(nextBackgroundDelay(0.5, 500), 0, "Update at the deadline");
      assertEquals(nextBackgroundDelay(0.5, 550), 0, "A delayed timer is due");
      assertEquals(
        nextBackgroundDelay(0.0001, 0),
        0.1,
        "Preserve sub-ms timing",
      );
    },
  },
  {
    name: "Long intervals use safe timer chunks without becoming due early",
    fn: () => {
      const seconds = (MAX_BACKGROUND_TIMER_DELAY + 25_000) / 1000;
      assertEquals(
        nextBackgroundDelay(seconds, 0),
        MAX_BACKGROUND_TIMER_DELAY,
        "Never overflow Gecko's signed 32-bit timer delay",
      );
      assertEquals(
        nextBackgroundDelay(seconds, MAX_BACKGROUND_TIMER_DELAY),
        25_000,
        "The first chunk must not trigger an early image request",
      );
      assertEquals(
        nextBackgroundDelay(seconds, MAX_BACKGROUND_TIMER_DELAY + 24_999),
        1,
        "Wait until the full interval has elapsed",
      );
      assertEquals(
        nextBackgroundDelay(seconds, MAX_BACKGROUND_TIMER_DELAY + 25_000),
        0,
        "Request the next image only at the full deadline",
      );
      assertEquals(
        nextBackgroundDelay(Number.MAX_VALUE, MAX_BACKGROUND_TIMER_DELAY),
        MAX_BACKGROUND_TIMER_DELAY,
        "Even seconds-to-ms overflow must never produce a fast timer",
      );
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("backgroundTiming.test.ts", tests);
}
