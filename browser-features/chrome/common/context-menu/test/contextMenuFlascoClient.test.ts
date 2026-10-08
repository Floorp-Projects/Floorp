// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import type { ExperimentsClient as Client } from "../../../../modules/modules/experiments/Experiments.sys.mts";

const EXPERIMENT_ID = "context_menu_customization";
const POLICY_PREF = "floorp.experiments.participationPolicy";

async function testClientNotificationsAndPolicies(): Promise<void> {
  const { ExperimentsClient } = ChromeUtils.importESModule(
    "resource://noraneko/modules/experiments/Experiments.sys.mjs",
  ) as { ExperimentsClient: new () => Client };
  const prefix = "floorp.experiments.";
  const saved = new Map<string, string>();
  for (const name of Services.prefs.getChildList(prefix)) {
    if (Services.prefs.prefHasUserValue(name)) {
      saved.set(name, Services.prefs.getStringPref(name));
    }
  }
  const manifest = {
    experiments: [{
      id: EXPERIMENT_ID,
      rollout: 0,
      variants: [{ id: "control", weight: 0 }, { id: "enabled", weight: 1 }],
    }],
  };
  const manifestUri = `data:application/json,${
    encodeURIComponent(JSON.stringify(manifest))
  }`;
  const client = new ExperimentsClient();
  const states: Array<
    {
      pending: boolean;
      manifest: boolean;
      variant: string | null;
      status: string | undefined;
    }
  > = [];
  const unsubscribe = client.subscribe(() => {
    const entry = client.getAllExperiments()[0];
    states.push({
      pending: client.initializing,
      manifest: client.manifestAvailable,
      variant: entry?.currentVariantId ?? null,
      status: entry?.enrollmentStatus,
    });
  });
  try {
    Services.prefs.setStringPref("floorp.experiments.manifestUrl", manifestUri);
    Services.prefs.setStringPref("floorp.experiments.assignments.v1", "{}");
    Services.prefs.setStringPref("floorp.experiments.disabled", "[]");
    Services.prefs.setStringPref("floorp.experiments.forceEnrolled", "[]");
    Services.prefs.setStringPref(POLICY_PREF, "default");
    await client.init({ installId: "context-menu-flasco-test" });
    assertEquals(
      states[0].pending,
      true,
      "init immediately invalidates runtime availability",
    );
    assertEquals(
      states.at(-1)?.pending,
      false,
      "init completion notifies runtime consumers",
    );
    assertEquals(
      states.at(-1)?.variant,
      "control",
      "default respects rollout zero",
    );
    assertEquals(
      client.forceEnrollExperiment(EXPERIMENT_ID).success,
      true,
      "force enrollment succeeds",
    );
    assertEquals(
      states.at(-1)?.status,
      "force_enrolled",
      "force enrollment notifies after state commits",
    );
    assertEquals(
      states.at(-1)?.variant,
      "enabled",
      "force enrollment selects an experimental variant",
    );
    client.disableExperiment(EXPERIMENT_ID);
    assertEquals(
      states.at(-1)?.status,
      "disabled",
      "disable notifies after state commits",
    );
    client.enableExperiment(EXPERIMENT_ID);
    assertEquals(
      states.at(-1)?.status,
      "force_enrolled",
      "re-enable notifies after state commits",
    );
    client.removeForceEnrollment(EXPERIMENT_ID);
    assertEquals(
      states.at(-1)?.variant,
      "control",
      "removing forced enrollment restores rollout behavior",
    );
    Services.prefs.setStringPref(POLICY_PREF, "always");
    await client.init({ installId: "context-menu-flasco-test" });
    assertEquals(
      states.at(-1)?.variant,
      "enabled",
      "always opts in outside rollout",
    );
    Services.prefs.setStringPref(POLICY_PREF, "never");
    await client.init({ installId: "context-menu-flasco-test" });
    assertEquals(states.at(-1)?.variant, "control", "never opts out");
    Services.prefs.setStringPref(
      "floorp.experiments.manifestUrl",
      "data:application/json,invalid",
    );
    await client.init({ installId: "context-menu-flasco-test" });
    assertEquals(
      states.at(-1)?.pending,
      false,
      "failed fetch clears pending state",
    );
    assertEquals(
      states.at(-1)?.manifest,
      false,
      "failed fetch notifies unavailable manifest state",
    );
    client.clearCache();
    assertEquals(
      states.at(-1)?.variant,
      null,
      "cache clearing notifies after dropping assignments",
    );
    unsubscribe();
    const count = states.length;
    client.enableExperiment(EXPERIMENT_ID);
    assertEquals(
      states.length,
      count,
      "unsubscribe releases runtime consumers",
    );
  } finally {
    unsubscribe();
    for (const name of Services.prefs.getChildList(prefix)) {
      if (Services.prefs.prefHasUserValue(name)) {
        Services.prefs.clearUserPref(name);
      }
    }
    for (const [name, value] of saved) {
      Services.prefs.setStringPref(name, value);
    }
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [{
    name: "real Flasco client notifications and participation policies",
    fn: testClientNotificationsAndPolicies,
  }];
  await runTests("contextMenuFlascoClient.test.ts", tests);
}
