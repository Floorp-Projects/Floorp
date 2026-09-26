// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { planAppQuit, supportsAppLifecycle } from "#libs/pwa/appLifecycle.ts";
import type {
  AppLifecycleCapabilities,
  AppLifecycleSnapshot,
} from "#libs/pwa/appLifecycleTypes.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";

const capabilities: AppLifecycleCapabilities = {
  protocolVersion: 1,
  sharedProfile: true,
  nativeWindowOwnership: true,
  independentAppQuit: true,
  backgroundSession: true,
};

const running: AppLifecycleSnapshot = {
  windows: [
    { id: "browser-1", kind: "browser" },
    { id: "a-1", kind: "web-app", appId: "a" },
    { id: "a-2", kind: "web-app", appId: "a" },
    { id: "b-1", kind: "web-app", appId: "b" },
    { id: "browser-2", kind: "browser" },
  ],
  pendingLaunches: [
    { id: "launch-a", appId: "a" },
    { id: "launch-b", appId: "b" },
  ],
};

export async function runAllTests(): Promise<void> {
  await runTests("appLifecycle.test.mts", [
    {
      name: "browser quit keeps both apps and pending launches when enabled",
      fn: () => {
        const plan = planAppQuit(
          { kind: "browser" },
          running,
          true,
          capabilities,
        );
        assert(plan.kind === "close-windows", "close the browser windows");
        assertEquals(
          plan.windowIds.join(","),
          "browser-1,browser-2",
          "only browser windows close",
        );
        assertEquals(plan.cancelLaunchIds.length, 0, "launches remain active");
        assert(plan.retainRuntimeAfterClose, "shared session stays alive");
      },
    },
    {
      name: "browser quit setting can choose global shutdown",
      fn: () => {
        const plan = planAppQuit(
          { kind: "browser" },
          running,
          false,
          capabilities,
        );
        assert(plan.kind === "quit-runtime", "request global shutdown");
        assertEquals(plan.reason, "quit-all", "normal quit reason");
      },
    },
    {
      name:
        "app quit closes all windows of that app and cancels only its launches",
      fn: () => {
        for (const keepRunning of [true, false]) {
          const plan = planAppQuit(
            { kind: "web-app", appId: "a" },
            running,
            keepRunning,
            capabilities,
          );
          assert(plan.kind === "close-windows", "request an app-scoped close");
          assertEquals(
            plan.windowIds.join(","),
            "a-1,a-2",
            "both app windows close",
          );
          assertEquals(
            plan.cancelLaunchIds.join(","),
            "launch-a",
            "only matching launch cancels",
          );
          assert(
            plan.retainRuntimeAfterClose,
            "other apps and browser remain alive",
          );
        }
      },
    },
    {
      name: "restart and OS shutdown always use global termination",
      fn: () => {
        for (const reason of ["restart", "os-shutdown", "quit-all"] as const) {
          for (const backend of [capabilities, null]) {
            const plan = planAppQuit(
              { kind: "runtime", reason },
              running,
              true,
              backend,
            );
            assert(
              plan.kind === "quit-runtime",
              "global requests cannot be intercepted",
            );
            assertEquals(
              plan.reason,
              reason,
              "preserve restart and shutdown intent",
            );
          }
        }
      },
    },
    {
      name: "unsupported backends leave browser and app quit unhandled",
      fn: () => {
        for (const backend of [null, { ...capabilities, protocolVersion: 2 }]) {
          assertEquals(
            planAppQuit({ kind: "browser" }, running, true, backend).kind,
            "unhandled",
            "no browser interception",
          );
          assertEquals(
            planAppQuit({ kind: "web-app", appId: "a" }, running, true, backend)
              .kind,
            "unhandled",
            "no app interception",
          );
        }
        for (
          const capability of [
            "sharedProfile",
            "nativeWindowOwnership",
            "independentAppQuit",
            "backgroundSession",
          ] as const
        ) {
          assert(
            !supportsAppLifecycle({ ...capabilities, [capability]: false }),
            `${capability} is required`,
          );
        }
      },
    },
    {
      name:
        "a pending launch keeps the shared runtime alive before its first window",
      fn: () => {
        const plan = planAppQuit(
          { kind: "browser" },
          {
            windows: [{ id: "browser", kind: "browser" }],
            pendingLaunches: [{ id: "launch-a", appId: "a" }],
          },
          true,
          capabilities,
        );
        assert(
          plan.kind === "close-windows" && plan.retainRuntimeAfterClose,
          "launch survives browser quit",
        );
      },
    },
    {
      name: "last app closes before an idle runtime may be released",
      fn: () => {
        const plan = planAppQuit(
          { kind: "web-app", appId: "a" },
          {
            windows: [{ id: "a-1", kind: "web-app", appId: "a" }],
            pendingLaunches: [],
          },
          true,
          capabilities,
        );
        assert(
          plan.kind === "close-windows",
          "allow beforeunload before shutdown",
        );
        assert(
          !plan.retainRuntimeAfterClose,
          "idle runtime can exit after close completes",
        );
      },
    },
    {
      name: "quitting an app before it opens cancels its pending launch",
      fn: () => {
        const plan = planAppQuit(
          { kind: "web-app", appId: "a" },
          {
            windows: [],
            pendingLaunches: [{ id: "launch-a", appId: "a" }],
          },
          true,
          capabilities,
        );
        assert(
          plan.kind === "close-windows",
          "launch cancellation is an action",
        );
        assertEquals(plan.windowIds.length, 0, "no unowned windows close");
        assertEquals(
          plan.cancelLaunchIds.join(","),
          "launch-a",
          "launch is canceled",
        );
        assert(!plan.retainRuntimeAfterClose, "no orphaned runtime");
      },
    },
    {
      name: "unknown app quit has no effect and cannot stop the shared runtime",
      fn: () => {
        assertEquals(
          planAppQuit(
            { kind: "web-app", appId: "missing" },
            running,
            true,
            capabilities,
          ).kind,
          "noop",
          "stale app command is ignored",
        );
      },
    },
    {
      name: "browser quit without running apps uses normal global shutdown",
      fn: () => {
        assertEquals(
          planAppQuit(
            { kind: "browser" },
            {
              windows: [{ id: "browser", kind: "browser" }],
              pendingLaunches: [],
            },
            true,
            capabilities,
          ).kind,
          "quit-runtime",
          "do not leave an idle background browser",
        );
      },
    },
    {
      name:
        "repeated browser quit after its windows close does not terminate apps",
      fn: () => {
        assertEquals(
          planAppQuit(
            { kind: "browser" },
            {
              windows: [{ id: "a-1", kind: "web-app", appId: "a" }],
              pendingLaunches: [],
            },
            true,
            capabilities,
          ).kind,
          "noop",
          "background apps remain open",
        );
      },
    },
    {
      name: "planning a canceled close does not mutate the next quit request",
      fn: () => {
        const before = JSON.stringify(running);
        planAppQuit(
          { kind: "web-app", appId: "a" },
          running,
          true,
          capabilities,
        );
        assertEquals(JSON.stringify(running), before, "snapshot is immutable");
        const plan = planAppQuit(
          { kind: "web-app", appId: "a" },
          running,
          true,
          capabilities,
        );
        assert(
          plan.kind === "close-windows",
          "retry close after user canceled beforeunload",
        );
        assertEquals(plan.windowIds.length, 2, "all windows are still tracked");
      },
    },
  ]);
}
