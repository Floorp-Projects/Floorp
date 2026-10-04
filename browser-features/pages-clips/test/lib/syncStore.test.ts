// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { rpc } from "../../src/lib/rpc/rpc.ts";
import { SYNC_STATE_PREF } from "../../src/lib/settings.ts";
import { rememberMerge } from "../../src/lib/syncStore.ts";
import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";

async function testFailedBookkeepingDoesNotDiscardDurableView(): Promise<void> {
  const original = rpc.setStringPref;
  let writtenPref = "";
  rpc.setStringPref = (name) => {
    writtenPref = name;
    return Promise.reject(new Error("sync-state pref is locked"));
  };
  try {
    // App calls this after saving the incoming clips. It must be allowed to
    // continue and reflect that durable snapshot even when bookkeeping fails.
    await rememberMerge({ clips: { incoming: 1 }, gone: {} });
    assertEquals(
      writtenPref,
      SYNC_STATE_PREF,
      "only sync bookkeeping is written",
    );
  } finally {
    rpc.setStringPref = original;
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [{
    name: "failed sync bookkeeping does not abort the durable view update",
    fn: testFailedBookkeepingDoesNotDiscardDurableView,
  }];
  await runTests("syncStore.test.ts", tests);
}
