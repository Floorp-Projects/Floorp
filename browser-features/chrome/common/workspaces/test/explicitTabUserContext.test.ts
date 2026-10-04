// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import { ExplicitTabUserContextOperations } from "../utils/explicit-tab-user-context.ts";

const rawTests: TestCase[] = [
  {
    name: "explicit tab context operation is one-shot",
    fn() {
      const operations = new ExplicitTabUserContextOperations();
      operations.run(0, () => {
        assertEquals(
          operations.consumeNext(),
          0,
          "first TabOpen should consume the operation",
        );
        assertEquals(
          operations.consumeNext(),
          null,
          "a second TabOpen must not reuse it",
        );
      });
      assertEquals(
        operations.consumeNext(),
        null,
        "operation should be cleaned up after callback",
      );
    },
  },
  {
    name: "nested explicit tab operations use LIFO ordering",
    fn() {
      const operations = new ExplicitTabUserContextOperations();
      const consumed: Array<number | null> = [];
      operations.run(1, () => {
        operations.run(2, () => {
          consumed.push(operations.consumeNext());
          consumed.push(operations.consumeNext());
        });
        consumed.push(operations.consumeNext());
      });
      assertEquals(
        consumed.join(","),
        "2,,1",
        "nested operations should not consume each other",
      );
    },
  },
  {
    name: "operations are isolated per workspace service instance",
    fn() {
      const firstWindow = new ExplicitTabUserContextOperations();
      const secondWindow = new ExplicitTabUserContextOperations();
      firstWindow.run(3, () => {
        assertEquals(
          secondWindow.consumeNext(),
          null,
          "another window must not see the operation",
        );
        assertEquals(
          firstWindow.consumeNext(),
          3,
          "own window should consume its operation",
        );
      });
    },
  },
  {
    name: "throwing open callbacks always clean up",
    fn() {
      const operations = new ExplicitTabUserContextOperations();
      try {
        operations.run(4, () => {
          throw new Error("expected open failure");
        });
      } catch {
        // Expected.
      }
      assertEquals(
        operations.consumeNext(),
        null,
        "failed opens must not leave a stale operation",
      );
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("explicitTabUserContext.test.ts", rawTests);
}
