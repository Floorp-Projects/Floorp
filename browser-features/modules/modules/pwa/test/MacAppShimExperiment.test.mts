// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  type MacAppShimEnrollment,
  resolveMacAppShimEnabled,
} from "#libs/pwa/macAppShimExperiment.ts";
import {
  assertEquals as assertHarnessEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";

function assertEquals(actual: boolean, expected: boolean): void {
  assertHarnessEquals(
    actual,
    expected,
    "Unexpected macOS App Shim gate result",
  );
}

const enrolled: MacAppShimEnrollment = {
  id: "pwa_mac_app_shim",
  isActive: true,
  currentVariantId: "enabled",
  enrollmentStatus: "enrolled",
};

export async function runAllTests(): Promise<void> {
  await runTests("MacAppShimExperiment.test.mts", [
    {
      name: "active Flasco enrollment enables only selected macOS users",
      fn() {
        assertEquals(resolveMacAppShimEnabled("Darwin", null, enrolled), true);
        assertEquals(
          resolveMacAppShimEnabled("Darwin", null, {
            ...enrolled,
            enrollmentStatus: "force_enrolled",
          }),
          true,
        );
        assertEquals(resolveMacAppShimEnabled("Darwin", null, null), false);
        assertEquals(
          resolveMacAppShimEnabled("Darwin", null, {
            ...enrolled,
            enrollmentStatus: "not_in_rollout",
            currentVariantId: null,
          }),
          false,
        );
        assertEquals(
          resolveMacAppShimEnabled("Darwin", null, {
            ...enrolled,
            enrollmentStatus: "disabled",
          }),
          false,
        );
        assertEquals(
          resolveMacAppShimEnabled("Darwin", null, {
            ...enrolled,
            currentVariantId: "control",
            enrollmentStatus: "control",
          }),
          false,
        );
        assertEquals(
          resolveMacAppShimEnabled("Darwin", null, {
            ...enrolled,
            isActive: false,
          }),
          false,
        );
        assertEquals(resolveMacAppShimEnabled("WINNT", null, enrolled), false);
      },
    },
    {
      name: "cached assignment survives a temporary manifest outage",
      fn() {
        const cached = {
          variantId: "enabled",
          disabled: false,
          optedOut: false,
        };
        assertEquals(
          resolveMacAppShimEnabled("Darwin", null, null, cached),
          true,
        );
        assertEquals(
          resolveMacAppShimEnabled("Darwin", null, null, {
            ...cached,
            disabled: true,
          }),
          false,
        );
        assertEquals(
          resolveMacAppShimEnabled("Darwin", null, null, {
            ...cached,
            optedOut: true,
          }),
          false,
        );
        assertEquals(
          resolveMacAppShimEnabled("Darwin", null, null, {
            ...cached,
            variantId: "control",
          }),
          false,
        );
      },
    },
    {
      name: "explicit about:config choice overrides Flasco",
      fn() {
        assertEquals(
          resolveMacAppShimEnabled("Darwin", false, enrolled),
          false,
        );
        assertEquals(resolveMacAppShimEnabled("Darwin", true, null), true);
        assertEquals(resolveMacAppShimEnabled("Linux", true, enrolled), false);
      },
    },
  ]);
}
