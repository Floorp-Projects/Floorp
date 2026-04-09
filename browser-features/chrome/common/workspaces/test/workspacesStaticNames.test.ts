// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  WORKSPACE_DATA_PREF_NAME,
  WORKSPACE_ENABLED_PREF_NAME,
  WORKSPACE_LAST_SHOW_ID,
  WORKSPACE_PENDING_EXIT_PREF_NAME,
  WORKSPACE_TAB_ATTRIBUTION_ID,
  WORKSPACED_CONFIG_PREF_NAME,
  WORKSPACES_CHANGED_OBSERVER_TOPIC,
  WORKSPACES_INIT_OBSERVER_TOPIC,
} from "../utils/workspaces-static-names.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

function testWorkspacePrefNamesHaveExpectedPrefix(): void {
  const prefNames = [
    WORKSPACE_DATA_PREF_NAME,
    WORKSPACE_ENABLED_PREF_NAME,
    WORKSPACED_CONFIG_PREF_NAME,
    WORKSPACE_PENDING_EXIT_PREF_NAME,
  ];

  for (const pref of prefNames) {
    assert(
      pref.startsWith("floorp.workspaces"),
      `invalid pref prefix: ${pref}`,
    );
  }
}

function testWorkspaceIdentifiersAreStable(): void {
  assertEquals(
    WORKSPACE_TAB_ATTRIBUTION_ID,
    "floorpWorkspaceId",
    "workspace tab attribution id should remain stable",
  );
  assertEquals(
    WORKSPACE_LAST_SHOW_ID,
    "floorpWorkspaceLastShowId",
    "workspace last-show id should remain stable",
  );
}

function testObserverTopicsAreUniqueAndNamespaced(): void {
  const topics = [
    WORKSPACES_INIT_OBSERVER_TOPIC,
    WORKSPACES_CHANGED_OBSERVER_TOPIC,
  ];

  assertEquals(
    new Set(topics).size,
    topics.length,
    "observer topics should be unique",
  );
  for (const topic of topics) {
    assert(
      topic.startsWith("floorp.workspaces."),
      `invalid topic namespace: ${topic}`,
    );
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "workspace pref names have expected prefix",
      fn: testWorkspacePrefNamesHaveExpectedPrefix,
    },
    {
      name: "workspace identifiers are stable",
      fn: testWorkspaceIdentifiersAreStable,
    },
    {
      name: "observer topics are unique and namespaced",
      fn: testObserverTopicsAreUniqueAndNamespaced,
    },
  ];

  await runTests("workspacesStaticNames.test.ts", tests);
}
