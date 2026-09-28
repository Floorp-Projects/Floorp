// SPDX-License-Identifier: MPL-2.0

import { assertEquals } from "@std/assert";
import {
  type MacAppShimEnrollment,
  resolveMacAppShimEnabled,
} from "./macAppShimExperiment.ts";

const enrolled: MacAppShimEnrollment = {
  id: "pwa_mac_app_shim",
  isActive: true,
  currentVariantId: "enabled",
  enrollmentStatus: "enrolled",
};

Deno.test("macOS App Shim follows the active Flasco assignment", () => {
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
});

Deno.test("an explicit about:config value overrides the Flasco assignment", () => {
  assertEquals(resolveMacAppShimEnabled("Darwin", false, enrolled), false);
  assertEquals(resolveMacAppShimEnabled("Darwin", true, null), true);
  assertEquals(resolveMacAppShimEnabled("Linux", true, enrolled), false);
});
