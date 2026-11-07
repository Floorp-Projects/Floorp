// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { createDeferredTabUpdater } from "../deferred-update.ts";
import { assertEquals, runTests } from "../../../test/utils/test_harness.ts";
const wait = () => new Promise<void>((resolve) => setTimeout(resolve, 160));
async function testNormalAndDisposed() {
  const seen: string[] = [];
  const updater = createDeferredTabUpdater((tab: string) => seen.push(tab));
  updater.schedule("live");
  await wait();
  assertEquals(seen.join(","), "live", "live owner applies update");
  updater.schedule("pending-one");
  updater.schedule("pending-two");
  updater.dispose();
  updater.schedule("after-dispose");
  await wait();
  assertEquals(
    seen.join(","),
    "live",
    "dispose cancels both pending updates and rejects new ones",
  );
  updater.dispose();
}
export async function runAllTests(): Promise<void> {
  await runTests("tab-sleep-deferred-update.test.ts", [
    { name: "cancel delayed updates on disposal", fn: testNormalAndDisposed },
  ]);
}
