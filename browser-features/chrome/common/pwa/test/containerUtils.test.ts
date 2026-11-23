// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { getPublicContainerOptions } from "../containerUtils.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

function withContainerExperimentEnabled(fn: () => void): void {
  const { PwaContainerExperiment } = ChromeUtils.importESModule(
    "resource://noraneko/modules/pwa/PwaContainerExperiment.sys.mjs",
  );
  const originalIsEnabled = PwaContainerExperiment.isEnabled;
  try {
    PwaContainerExperiment.isEnabled = () => true;
    fn();
  } finally {
    PwaContainerExperiment.isEnabled = originalIsEnabled;
  }
}

const tests: TestCase[] = [
  {
    name: "getPublicContainerOptions includes no-container entry first",
    fn() {
      const options = getPublicContainerOptions();
      assert(
        options.length >= 1,
        "must return at least the no-container option",
      );
      assertEquals(
        options[0].userContextId,
        0,
        "first option must be no container",
      );
      assert(
        options[0].label.length > 0,
        "no-container label must not be empty",
      );
    },
  },
  {
    name: "getPublicContainerOptions uses unique userContextId values",
    fn() {
      const options = getPublicContainerOptions();
      const ids = options.map((option) => option.userContextId);
      const uniqueIds = new Set(ids);
      assertEquals(
        ids.length,
        uniqueIds.size,
        "container option ids must be unique",
      );
    },
  },
  {
    name: "public containers use their localized browser labels",
    fn() {
      withContainerExperimentEnabled(() => {
        const { ContextualIdentityService } = ChromeUtils.importESModule(
          "moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs",
        );
        const identities = ContextualIdentityService.getPublicIdentities() as {
          userContextId: number;
        }[];
        const options = getPublicContainerOptions();
        assert(identities.length > 0, "browser must have public containers");
        assertEquals(
          options.length,
          identities.length + 1,
          "all containers are included",
        );
        for (const identity of identities) {
          const option = options.find((entry) =>
            entry.userContextId === identity.userContextId
          );
          assert(option, "container must have an option");
          assert(option.label?.length > 0, "container label must not be empty");
          assertEquals(
            option.label,
            ContextualIdentityService.getUserContextLabel(
              identity.userContextId,
            ),
            "option must match the browser's localized container label",
          );
        }
      });
    },
  },
  {
    name:
      "legacy and Fluent default containers and custom containers have labels",
    fn() {
      withContainerExperimentEnabled(() => {
        const { ContextualIdentityService } = ChromeUtils.importESModule(
          "moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs",
        );
        const originalGetPublicIdentities =
          ContextualIdentityService.getPublicIdentities;
        const originalGetUserContextLabel =
          ContextualIdentityService.getUserContextLabel;
        const labels = new Map([
          [1, "個人"],
          [2, "仕事"],
          [5, "Private Container"],
        ]);
        try {
          ContextualIdentityService.getPublicIdentities = () => [
            { userContextId: 1, l10nID: "userContextPersonal.label" },
            { userContextId: 2, l10nId: "user-context-work" },
            { userContextId: 5, name: "Private Container" },
          ];
          ContextualIdentityService.getUserContextLabel = (id: number) =>
            labels.get(id) ?? "";

          const options = getPublicContainerOptions();
          assertEquals(options.length, 4, "all options must remain available");
          assertEquals(options[0].userContextId, 0, "no-container stays first");
          assert(options[0].label.length > 0, "no-container keeps its label");
          for (const [id, label] of labels) {
            assertEquals(
              options.find((option) => option.userContextId === id)?.label,
              label,
              `container ${id} must have its display label`,
            );
          }
        } finally {
          ContextualIdentityService.getPublicIdentities =
            originalGetPublicIdentities;
          ContextualIdentityService.getUserContextLabel =
            originalGetUserContextLabel;
        }
      });
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("containerUtils.test.ts", tests);
}
