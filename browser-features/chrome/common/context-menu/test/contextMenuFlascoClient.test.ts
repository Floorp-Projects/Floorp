// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import type { ExperimentsClient as Client } from "../../../../modules/modules/experiments/Experiments.sys.mts";
import { ContextMenuRuntime } from "../runtime.ts";

const EXPERIMENT_ID = "context_menu_customization";
const POLICY_PREF = "floorp.experiments.participationPolicy";

async function waitUntil(
  condition: () => boolean,
  message: string,
): Promise<void> {
  const deadline = Date.now() + 5000;
  while (!condition() && Date.now() < deadline) {
    await new Promise<void>((resolve) => globalThis.setTimeout(resolve, 25));
  }
  assert(condition(), message);
}

async function testClientRuntimeBoundaries(
  policy: "default" | "always" | "never",
  rollout: number,
): Promise<void> {
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
  const enabledPref = "floorp.contextMenu.enabled";
  const configPref = "floorp.contextMenu.config";
  const hadEnabled = Services.prefs.prefHasUserValue(enabledPref);
  const oldEnabled = Services.prefs.getBoolPref(enabledPref, true);
  const hadConfig = Services.prefs.prefHasUserValue(configPref);
  const oldConfig = Services.prefs.getStringPref(configPref, "");
  const config = JSON.stringify({ schemaVersion: 1, surfaces: {} });
  const runtimes: ContextMenuRuntime[] = [];
  const counts = { creations: 0, attachments: 0, destructions: 0 };
  let client = new ExperimentsClient();
  const startRuntime = () => {
    const runtime = new ContextMenuRuntime({
      window,
      experiments: client,
      createController: () => {
        counts.creations++;
        return {
          attach: () => counts.attachments++,
          destroy: () => counts.destructions++,
        };
      },
    });
    runtimes.push(runtime);
    runtime.start();
    runtime.start();
    return runtime;
  };
  try {
    Services.prefs.setBoolPref(enabledPref, true);
    Services.prefs.setStringPref(configPref, config);
    Services.prefs.setStringPref(POLICY_PREF, policy);
    Services.prefs.setStringPref("floorp.experiments.assignments.v1", "{}");
    Services.prefs.setStringPref("floorp.experiments.disabled", "[]");
    Services.prefs.setStringPref("floorp.experiments.forceEnrolled", "[]");
    const start = Date.now() + 1500;
    const manifest = {
      experiments: [
        {
          id: EXPERIMENT_ID,
          rollout,
          start: new Date(start).toISOString(),
          end: new Date(start + 5000).toISOString(),
          variants: [{ id: "control", weight: 0 }, {
            id: "enabled",
            weight: 1,
          }],
        },
        {
          id: "unrelated_boundary_test",
          rollout: 100,
          start: new Date(start).toISOString(),
          variants: [{ id: "enabled", weight: 1 }],
        },
      ],
    };
    Services.prefs.setStringPref(
      "floorp.experiments.manifestUrl",
      `data:application/json,${encodeURIComponent(JSON.stringify(manifest))}`,
    );
    await client.init({ installId: "context-menu-boundary-test" });
    assertEquals(
      client.getAllExperiments()[0].currentVariantId,
      null,
      "initializing before start leaves no assignment",
    );
    let runtime = startRuntime();
    assertEquals(counts.creations, 0, "before start creates no customizer");
    const initiallyEnabled = policy === "always" ||
      (policy === "default" && rollout === 100);
    if (policy === "never") {
      await waitUntil(
        () => Date.now() >= start + 100,
        "start boundary arrives",
      );
      assertEquals(
        client.getAllExperiments()[0].currentVariantId,
        null,
        "never does not create an assignment at start",
      );
    } else {
      await waitUntil(
        () => client.getAllExperiments()[0].currentVariantId !== null,
        "start boundary creates the real client's missing assignment",
      );
      assertEquals(
        client.getAllExperiments()[0].currentVariantId,
        initiallyEnabled ? "enabled" : "control",
        "start boundary respects participation and rollout",
      );
    }
    assertEquals(
      counts.creations,
      initiallyEnabled ? 1 : 0,
      "start creates a controller only for participants",
    );
    assertEquals(counts.attachments, counts.creations, "no duplicate attach");
    assertEquals(
      Object.hasOwn(client.assignments, "unrelated_boundary_test"),
      false,
      "the boundary does not initialize unrelated experiments",
    );
    const assignment = JSON.stringify(client.assignments[EXPERIMENT_ID]);
    runtime.destroy();
    client = new ExperimentsClient();
    await client.init({ installId: "context-menu-boundary-test" });
    runtime = startRuntime();
    assertEquals(
      counts.creations,
      initiallyEnabled ? 2 : 0,
      "restarting the real client after start restores only eligible runtimes",
    );
    if (policy !== "never") {
      assertEquals(
        JSON.stringify(client.assignments[EXPERIMENT_ID]),
        assignment,
        "restart reuses the saved assignment",
      );
    }
    if (!initiallyEnabled) {
      Services.prefs.setStringPref(POLICY_PREF, "always");
      await client.init({ installId: "context-menu-boundary-test" });
      assertEquals(counts.creations, 1, "opt-in after start activates");
    }
    const beforeClear = counts.creations;
    client.clearCache();
    assertEquals(
      client.getAllExperiments()[0].currentVariantId,
      null,
      "cache clearing does not recreate an assignment from the old manifest",
    );
    assertEquals(
      counts.destructions,
      beforeClear,
      "cache clearing releases the active runtime",
    );
    runtime.destroy();
    runtime = startRuntime();
    assertEquals(
      counts.creations,
      beforeClear,
      "runtime restart after cache clearing waits for client initialization",
    );
    await client.init({ installId: "context-menu-boundary-test" });
    assertEquals(
      counts.creations,
      beforeClear + 1,
      "successful initialization after cache clearing resumes once",
    );
    const beforeDisable = counts.creations;
    client.disableExperiment(EXPERIMENT_ID);
    assertEquals(
      counts.destructions,
      beforeDisable,
      "disable releases runtime",
    );
    if (rollout === 0) {
      Services.prefs.setStringPref(POLICY_PREF, "default");
      await client.init({ installId: "context-menu-boundary-test" });
    }
    client.enableExperiment(EXPERIMENT_ID);
    if (rollout === 0) {
      assertEquals(
        client.getAllExperiments()[0].currentVariantId,
        "control",
        "re-enable recalculates an assignment retained while disabled",
      );
      assertEquals(
        counts.creations,
        beforeDisable,
        "a default-policy rollout exclusion cannot revive the old overlay",
      );
      Services.prefs.setStringPref(POLICY_PREF, "always");
      await client.init({ installId: "context-menu-boundary-test" });
    }
    assertEquals(
      counts.creations,
      beforeDisable + 1,
      "re-enable activates once",
    );
    Services.prefs.setStringPref(POLICY_PREF, "never");
    assertEquals(counts.destructions, counts.creations, "opt-out is immediate");
    await client.init({ installId: "context-menu-boundary-test" });
    assertEquals(
      client.getAllExperiments()[0].currentVariantId,
      "control",
      "opt-out recalculates the control assignment",
    );
    Services.prefs.setStringPref(POLICY_PREF, "always");
    await client.init({ installId: "context-menu-boundary-test" });
    assertEquals(counts.creations, beforeDisable + 2, "opt-in resumes once");
    await waitUntil(
      () => counts.destructions === counts.creations,
      "end boundary stops the real-client runtime",
    );
    assertEquals(
      client.getAllExperiments()[0].isActive,
      false,
      "the real client reports the experiment expired",
    );
    const afterEnd = counts.creations;
    runtime.destroy();
    client = new ExperimentsClient();
    await client.init({ installId: "context-menu-boundary-test" });
    startRuntime();
    assertEquals(counts.creations, afterEnd, "restart after end stays native");
    assertEquals(
      client.getAllExperiments()[0].currentVariantId,
      null,
      "expired assignments are not revived",
    );
    assertEquals(
      Services.prefs.getStringPref(configPref),
      config,
      "start, restart, opt-out, and end preserve the saved customization",
    );
  } finally {
    for (const runtime of runtimes) runtime.destroy();
    for (const name of Services.prefs.getChildList(prefix)) {
      if (Services.prefs.prefHasUserValue(name)) {
        Services.prefs.clearUserPref(name);
      }
    }
    for (const [name, value] of saved) {
      Services.prefs.setStringPref(name, value);
    }
    if (hadEnabled) Services.prefs.setBoolPref(enabledPref, oldEnabled);
    else Services.prefs.clearUserPref(enabledPref);
    if (hadConfig) Services.prefs.setStringPref(configPref, oldConfig);
    else Services.prefs.clearUserPref(configPref);
  }
}

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
    const retainedAssignment = Services.prefs.getStringPref(
      "floorp.experiments.assignments.v1",
    );
    await new Promise<void>((resolve) => globalThis.setTimeout(resolve, 25));
    client.enableExperiment(EXPERIMENT_ID);
    assertEquals(
      Services.prefs.getStringPref("floorp.experiments.assignments.v1"),
      retainedAssignment,
      "enabling an already enabled experiment does not rewrite its assignment",
    );
    client.disableExperiment(EXPERIMENT_ID);
    Services.prefs.setStringPref(POLICY_PREF, "default");
    await client.init({ installId: "context-menu-flasco-test" });
    assertEquals(
      client.getAllExperiments()[0].currentVariantId,
      "enabled",
      "initialization retains the disabled assignment",
    );
    client.enableExperiment(EXPERIMENT_ID);
    assertEquals(
      states.at(-1)?.variant,
      "control",
      "re-enable publishes the recalculated default-policy assignment",
    );
    Services.prefs.setStringPref(POLICY_PREF, "always");
    await client.init({ installId: "context-menu-flasco-test" });
    Services.prefs.setStringPref(POLICY_PREF, "never");
    await client.init({ installId: "context-menu-flasco-test" });
    assertEquals(states.at(-1)?.variant, "control", "never opts out");
    Services.prefs.setStringPref(POLICY_PREF, "always");
    await client.init({ installId: "context-menu-flasco-test" });
    client.disableExperiment(EXPERIMENT_ID);
    Services.prefs.setStringPref(POLICY_PREF, "default");
    await client.init({ installId: "context-menu-flasco-test" });
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
    assertEquals(
      client.getAllExperiments()[0].currentVariantId,
      "enabled",
      "a failed refresh retains the old public assignment",
    );
    assertEquals(
      client.getAllExperiments()[0].isReady,
      false,
      "a failed refresh exposes the stale snapshot as unavailable to settings",
    );
    client.enableExperiment(EXPERIMENT_ID);
    assertEquals(
      client.getAllExperiments()[0].currentVariantId,
      null,
      "re-enable without a manifest invalidates the retained assignment",
    );
    Services.prefs.setStringPref("floorp.experiments.manifestUrl", manifestUri);
    await client.init({ installId: "context-menu-flasco-test" });
    assertEquals(
      client.getAllExperiments()[0].currentVariantId,
      "control",
      "retry evaluates the current rollout instead of the old enabled variant",
    );
    assertEquals(
      client.getAllExperiments()[0].isReady,
      true,
      "successful retry exposes a ready settings snapshot",
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
  const tests: TestCase[] = [
    {
      name: "real Flasco client notifications and participation policies",
      fn: testClientNotificationsAndPolicies,
    },
    {
      name: "real client future start, restart, opt-out, and end",
      fn: () => testClientRuntimeBoundaries("default", 100),
    },
    {
      name: "real client future start respects rollout exclusion",
      fn: () => testClientRuntimeBoundaries("default", 0),
    },
    {
      name: "real client opt-in starts outside rollout and expires",
      fn: () => testClientRuntimeBoundaries("always", 0),
    },
    {
      name: "real client opt-out before start stays stopped until opt-in",
      fn: () => testClientRuntimeBoundaries("never", 100),
    },
  ];
  await runTests("contextMenuFlascoClient.test.ts", tests);
}
