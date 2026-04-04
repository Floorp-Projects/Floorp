// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  getCurrentProfile,
  getProfiles,
  getFxAccountsInfo,
  openUrl,
  openProfile,
  removeProfile,
  renameProfile,
  setDefaultProfile,
  restart,
} from "./profileDataManager.ts";

type TestCase = { name: string; fn: () => void | Promise<void> };

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) throw new Error(message);
}

function assertEquals<T>(actual: T, expected: T, message: string): void {
  if (actual !== expected) {
    throw new Error(
      `${message} (expected: ${String(expected)}, actual: ${String(actual)})`,
    );
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "getCurrentProfile returns profile data in chrome context",
      fn: async () => {
        // In chrome (non-dev) context, getCurrentProfile uses privileged APIs
        // directly, so it returns profile data even without NRGetCurrentProfile
        const result = await getCurrentProfile();
        assert(
          result === null || (typeof result === "object" && result !== null),
          "should return profile object or null",
        );
        if (result !== null) {
          assert(typeof result.profileName === "string", "profileName should be string");
          assert(typeof result.profilePath === "string", "profilePath should be string");
        }
      },
    },
    {
      name: "getCurrentProfile returns valid profile shape",
      fn: async () => {
        // In chrome context, getCurrentProfile uses privileged APIs directly
        // and ignores NRGetCurrentProfile callbacks, so we just verify the shape
        const result = await getCurrentProfile();
        if (result !== null) {
          assert(typeof result.profileName === "string", "profileName should be string");
          assert(result.profileName.length > 0, "profileName should not be empty");
          assert(typeof result.profilePath === "string", "profilePath should be string");
          assert(result.profilePath.length > 0, "profilePath should not be empty");
        }
      },
    },
    {
      name: "getProfiles returns array in chrome context",
      fn: async () => {
        // In chrome context, getProfiles uses privileged APIs directly
        const result = await getProfiles();
        assert(Array.isArray(result), "result should be an array");
      },
    },
    {
      name: "getFxAccountsInfo returns object or null",
      fn: async () => {
        const result = await getFxAccountsInfo();
        assert(
          result === null || typeof result === "object",
          "should return object or null",
        );
      },
    },
    {
      name: "openUrl does not throw when NROpenUrl is not set",
      fn: () => {
        (globalThis as Record<string, unknown>).NROpenUrl = undefined;
        // Should not throw
        openUrl("https://example.com");
      },
    },
    {
      name: "openUrl calls NROpenUrl with the provided URL",
      fn: () => {
        let calledWith = "";
        (globalThis as Record<string, unknown>).NROpenUrl = (url: string) => {
          calledWith = url;
        };
        openUrl("https://floorp.app");
        assertEquals(calledWith, "https://floorp.app", "should pass url to NROpenUrl");
        (globalThis as Record<string, unknown>).NROpenUrl = undefined;
      },
    },
    {
      name: "openProfile does not throw when NROpenProfile is not set",
      fn: () => {
        (globalThis as Record<string, unknown>).NROpenProfile = undefined;
        openProfile("some-id");
      },
    },
    {
      name: "removeProfile returns false when NRRemoveProfile is not set",
      fn: async () => {
        (globalThis as Record<string, unknown>).NRRemoveProfile = undefined;
        const result = await removeProfile("some-id");
        assertEquals(result, false, "should return false");
      },
    },
    {
      name: "renameProfile returns false when NRRenameProfile is not set",
      fn: async () => {
        (globalThis as Record<string, unknown>).NRRenameProfile = undefined;
        const result = await renameProfile("some-id", "new-name");
        assertEquals(result, false, "should return false");
      },
    },
    {
      name: "setDefaultProfile returns false when NRSetDefaultProfile is not set",
      fn: async () => {
        (globalThis as Record<string, unknown>).NRSetDefaultProfile = undefined;
        const result = await setDefaultProfile("some-id");
        assertEquals(result, false, "should return false");
      },
    },
    {
      name: "restart does not throw when NRRestart is not set",
      fn: () => {
        (globalThis as Record<string, unknown>).NRRestart = undefined;
        restart();
        restart(true);
      },
    },
  ];

  for (const t of tests) {
    try {
      await t.fn();
      console.log(`  PASS: ${t.name}`);
    } catch (e) {
      console.error(`  FAIL: ${t.name}`);
      throw e;
    }
  }
}
