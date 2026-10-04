// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import type { NRExperimemmtParentFunctions } from "../../../../modules/common/defines.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";
import { withExperimentChangeNotifications } from "../../../src/lib/rpc/experiments.ts";

function fakeExperimentsApi() {
  const calls: string[] = [];
  let response: { success: boolean; error?: string } = { success: true };
  let failure: Error | null = null;
  const mutate = (name: string, id?: string) => {
    calls.push(`${name}:${id ?? ""}`);
    return failure ? Promise.reject(failure) : Promise.resolve(response);
  };
  const all: Awaited<
    ReturnType<NRExperimemmtParentFunctions["getAllExperiments"]>
  > = [];
  const active: Awaited<
    ReturnType<NRExperimemmtParentFunctions["getActiveExperiments"]>
  > = [];
  const api: NRExperimemmtParentFunctions = {
    getAllExperiments: () => Promise.resolve(all),
    getActiveExperiments: () => Promise.resolve(active),
    disableExperiment: (id) => mutate("disable", id),
    enableExperiment: (id) => mutate("enable", id),
    forceEnrollExperiment: (id) => mutate("force", id),
    removeForceEnrollment: (id) => mutate("remove", id),
    clearExperimentCache: () => mutate("clear"),
    reinitializeExperiments: () => mutate("reinitialize"),
  };
  return {
    api,
    calls,
    all,
    active,
    response: () => response,
    fail: (error: Error) => {
      failure = error;
    },
    deny: () => {
      response = { success: false, error: "unchanged" };
    },
  };
}

const tests: TestCase[] = [
  {
    name: "Experiment reads pass through without notifying availability",
    fn: async () => {
      const fake = fakeExperimentsApi();
      let notifications = 0;
      const api = withExperimentChangeNotifications(
        fake.api,
        () => notifications++,
      );
      assertEquals(
        await api.getAllExperiments(),
        fake.all,
        "available metadata is preserved",
      );
      assertEquals(
        await api.getActiveExperiments(),
        fake.active,
        "active metadata is preserved",
      );
      assertEquals(notifications, 0, "reads do not trigger a refresh loop");
    },
  },
  {
    name:
      "All successful enrollment and cache mutations notify once and preserve results",
    fn: async () => {
      const fake = fakeExperimentsApi();
      let notifications = 0;
      const api = withExperimentChangeNotifications(
        fake.api,
        () => notifications++,
      );
      const results = [
        await api.disableExperiment("flasco-id"),
        await api.enableExperiment("flasco-id"),
        await api.forceEnrollExperiment("flasco-id"),
        await api.removeForceEnrollment("flasco-id"),
        await api.clearExperimentCache(),
        await api.reinitializeExperiments(),
      ];
      assert(
        results.every((result) => result === fake.response()),
        "original response objects are preserved",
      );
      assertEquals(
        fake.calls.join(","),
        "disable:flasco-id,enable:flasco-id,force:flasco-id,remove:flasco-id,clear:,reinitialize:",
        "method arguments pass through",
      );
      assertEquals(
        notifications,
        6,
        "each successful mutation emits one change",
      );
    },
  },
  {
    name: "Failed and rejected mutations do not announce a successful change",
    fn: async () => {
      const fake = fakeExperimentsApi();
      let notifications = 0;
      const api = withExperimentChangeNotifications(
        fake.api,
        () => notifications++,
      );
      fake.deny();
      const denied = await api.forceEnrollExperiment("flasco-id");
      assertEquals(
        denied,
        fake.response(),
        "unsuccessful response is preserved",
      );
      const failure = new Error("RPC unavailable");
      fake.fail(failure);
      let rejected: unknown = null;
      try {
        await api.reinitializeExperiments();
      } catch (error) {
        rejected = error;
      }
      assertEquals(rejected, failure, "RPC rejection is preserved");
      assertEquals(notifications, 0, "no false change notification is sent");
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("experiments.test.ts", tests);
}
