// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { createPrefPoller } from "../../src/lib/rpc/prefObserver.ts";
import type {
  PrefKind,
  PrefReaders,
  PrefValue,
} from "../../src/types/prefObserver.ts";
import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";

async function checkPrefChanges(
  kind: PrefKind,
  initial: PrefValue,
  changed: PrefValue,
): Promise<void> {
  let value = initial;
  const readers: PrefReaders = {
    getStringPref: () =>
      Promise.resolve(typeof value === "string" ? value : null),
    getIntPref: () => Promise.resolve(typeof value === "number" ? value : null),
    getBoolPref: () =>
      Promise.resolve(typeof value === "boolean" ? value : null),
  };
  const changes: string[] = [];
  const poller = createPrefPoller(
    { name: "test-pref", kind },
    readers,
    (name) => changes.push(name),
  );
  await poller.poll();
  assertEquals(changes.length, 0, "the first value is a baseline");
  value = changed;
  await poller.poll();
  assertEquals(changes.join(","), "test-pref", `${kind} change is observed`);
  await poller.poll();
  assertEquals(changes.length, 1, "an unchanged preference does not notify");
  poller.stop();
}

async function testDisposalStopsPendingRead(): Promise<void> {
  let finish: (value: string | null) => void = () => {};
  let reads = 0;
  let changes = 0;
  const readers: PrefReaders = {
    getStringPref: () => {
      reads++;
      return reads === 1
        ? Promise.resolve("initial")
        : new Promise((resolve) => (finish = resolve));
    },
    getIntPref: () => Promise.resolve(null),
    getBoolPref: () => Promise.resolve(null),
  };
  const poller = createPrefPoller(
    { name: "test-pref" },
    readers,
    () => changes++,
  );
  await poller.poll();
  const pending = poller.poll();
  await poller.poll();
  assertEquals(reads, 2, "a slow read does not overlap the next tick");
  poller.stop();
  finish("changed");
  await pending;
  await poller.poll();
  assertEquals(changes, 0, "a disposed observer cannot publish a late value");
  assertEquals(reads, 2, "disposal prevents further reads");
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "string preferences are observed",
      fn: () => checkPrefChanges("string", "local", "sync"),
    },
    {
      name: "integer preferences are observed",
      fn: () => checkPrefChanges("int", 64, 2),
    },
    {
      name: "boolean preferences are observed",
      fn: () => checkPrefChanges("bool", false, true),
    },
    {
      name: "pending reads stop with the observer",
      fn: testDisposalStopsPendingRead,
    },
  ];
  await runTests("prefObserver.test.ts", tests);
}
