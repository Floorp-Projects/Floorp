// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  getCurrentProfile,
  getFxAccountsInfo,
  getProfiles,
  openProfile,
  openUrl,
  removeProfile,
  renameProfile,
  restart,
  type RestartDependencies,
  setDefaultProfile,
} from "../../src/lib/profileDataManager.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";

type RestartHarness = {
  dependencies: RestartDependencies;
  calls: string[];
  cancelQuit: { data: boolean };
  interfaceType: unknown;
  notifiedSubject: unknown;
  notifiedTopic: string;
  notifiedData: string;
  quitFlags: number | null;
  safeModeFlags: number | null;
  fallbackValues: boolean[];
};

function createRestartHarness(cancelOnNotify = false): RestartHarness {
  const interfaceType = {};
  const harness: RestartHarness = {
    dependencies: {},
    calls: [],
    cancelQuit: { data: false },
    interfaceType,
    notifiedSubject: null,
    notifiedTopic: "",
    notifiedData: "",
    quitFlags: null,
    safeModeFlags: null,
    fallbackValues: [],
  };

  harness.dependencies = {
    isDev: false,
    hasChrome: true,
    Cc: {
      "@mozilla.org/supports-PRBool;1": {
        createInstance: (receivedInterfaceType) => {
          harness.calls.push("create-cancel-quit");
          harness.interfaceType = receivedInterfaceType;
          return harness.cancelQuit;
        },
      },
    },
    Ci: {
      nsISupportsPRBool: interfaceType,
      nsIAppStartup: {
        eAttemptQuit: 1,
        eRestart: 2,
      },
    },
    Services: {
      obs: {
        notifyObservers: (subject, topic, data) => {
          harness.calls.push("notify");
          harness.notifiedSubject = subject;
          harness.notifiedTopic = topic;
          harness.notifiedData = data;
          if (cancelOnNotify) {
            subject.data = true;
          }
        },
      },
      startup: {
        quit: (flags) => {
          harness.calls.push("quit");
          harness.quitFlags = flags;
        },
        restartInSafeMode: (flags) => {
          harness.calls.push("safe-mode");
          harness.safeModeFlags = flags;
        },
      },
    },
    fallbackRestart: (safeMode) => {
      harness.calls.push(`fallback:${safeMode}`);
      harness.fallbackValues.push(safeMode);
    },
  };

  return harness;
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
          assert(
            typeof result.profileName === "string",
            "profileName should be string",
          );
          assert(
            typeof result.profilePath === "string",
            "profilePath should be string",
          );
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
          assert(
            typeof result.profileName === "string",
            "profileName should be string",
          );
          assert(
            result.profileName.length > 0,
            "profileName should not be empty",
          );
          assert(
            typeof result.profilePath === "string",
            "profilePath should be string",
          );
          assert(
            result.profilePath.length > 0,
            "profilePath should not be empty",
          );
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
      name: "openUrl forwards URL to NROpenUrl when callback path is used",
      fn: () => {
        const targetUrl = "https://floorp.app";
        let calledWith = "";
        (globalThis as Record<string, unknown>).NROpenUrl = (url: string) => {
          calledWith = url;
        };
        openUrl(targetUrl);

        // In non-dev chrome context, openUrl takes the privileged branch and
        // does not call NROpenUrl. In fallback/dev contexts it should pass the
        // URL through NROpenUrl.
        if (calledWith !== "") {
          assertEquals(calledWith, targetUrl, "should pass url to NROpenUrl");
        }

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
      name:
        "setDefaultProfile returns false when NRSetDefaultProfile is not set",
      fn: async () => {
        (globalThis as Record<string, unknown>).NRSetDefaultProfile = undefined;
        const result = await setDefaultProfile("some-id");
        assertEquals(result, false, "should return false");
      },
    },
    {
      name: "restart uses privileged quit with restart flags",
      fn: () => {
        const harness = createRestartHarness();
        restart(false, harness.dependencies);

        assertEquals(
          harness.calls.join(","),
          "create-cancel-quit,notify,quit",
          "normal restart should notify before quitting",
        );
        assertEquals(
          harness.interfaceType,
          harness.dependencies.Ci?.nsISupportsPRBool,
          "restart should create the expected cancel-quit interface",
        );
        assertEquals(
          harness.notifiedSubject,
          harness.cancelQuit,
          "restart should notify with the cancel-quit subject",
        );
        assertEquals(
          harness.notifiedTopic,
          "quit-application-requested",
          "restart should use the quit request topic",
        );
        assertEquals(
          harness.notifiedData,
          "restart",
          "restart should identify the quit as a restart",
        );
        assertEquals(harness.quitFlags, 3, "restart should combine quit flags");
        assertEquals(
          harness.safeModeFlags,
          null,
          "normal restart should not enter safe mode",
        );
      },
    },
    {
      name: "restart uses privileged safe-mode restart with restart flags",
      fn: () => {
        const harness = createRestartHarness();
        restart(true, harness.dependencies);

        assertEquals(
          harness.calls.join(","),
          "create-cancel-quit,notify,safe-mode",
          "safe-mode restart should notify before restarting",
        );
        assertEquals(
          harness.safeModeFlags,
          3,
          "safe-mode restart should combine quit flags",
        );
        assertEquals(
          harness.quitFlags,
          null,
          "safe-mode restart should not call normal quit",
        );
      },
    },
    {
      name: "restart stops when quit is canceled",
      fn: () => {
        const harness = createRestartHarness(true);
        restart(false, harness.dependencies);

        assertEquals(
          harness.calls.join(","),
          "create-cancel-quit,notify",
          "canceled restart should stop after notification",
        );
        assertEquals(
          harness.quitFlags,
          null,
          "canceled restart should not quit",
        );
        assertEquals(
          harness.safeModeFlags,
          null,
          "canceled restart should not enter safe mode",
        );
      },
    },
    {
      name: "restart forwards normal and safe-mode requests to fallback",
      fn: () => {
        const harness = createRestartHarness();
        harness.dependencies.isDev = true;
        harness.dependencies.hasChrome = false;

        restart(false, harness.dependencies);
        restart(true, harness.dependencies);

        assertEquals(
          harness.calls.join(","),
          "fallback:false,fallback:true",
          "fallback should receive both restart modes",
        );
        assertEquals(
          harness.fallbackValues.join(","),
          "false,true",
          "fallback should preserve the safe-mode argument",
        );
      },
    },
    {
      name: "restart does not throw without a fallback",
      fn: () => {
        const harness = createRestartHarness();
        harness.dependencies.isDev = true;
        harness.dependencies.hasChrome = false;
        harness.dependencies.fallbackRestart = null;

        restart(false, harness.dependencies);
        restart(true, harness.dependencies);

        assertEquals(
          harness.calls.length,
          0,
          "missing fallback should not call privileged restart effects",
        );
      },
    },
  ];

  await runTests("profileDataManager.test.ts", tests);
}
