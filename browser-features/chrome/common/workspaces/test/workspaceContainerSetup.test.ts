// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  setupWorkspaceContainers,
  workspaceContainerNeedsReopen,
} from "../workspace-container-ui.ts";
import type {
  WorkspaceContainerBindingAPI,
  WorkspaceContainerPrompt,
} from "../workspace-container-ui-types.ts";
import type { WorkspaceContainerResolution } from "#modules/common/workspace-container-bindings.ts";
import type {
  WorkspaceContainerDefinition,
  WorkspaceContainerRef,
} from "#modules/common/workspace-container-types.ts";
import {
  assertEquals as assertValue,
  runTests,
} from "../../../test/utils/test_harness.ts";

const containerId = "8f716183-7ccb-4cb4-a867-d01421f4012d";
const ref: WorkspaceContainerRef = { kind: "container", containerId };
const definition: WorkspaceContainerDefinition = {
  version: 2,
  kind: "container",
  containerId,
  name: "Work",
  color: "blue",
  icon: "briefcase",
  modifiedAt: 1,
  modifiedBy: "079fdac8-0ad3-4940-b2b5-1cb13f18d272",
};
const local = {
  userContextId: 9,
  name: "Work",
  color: "blue",
  icon: "briefcase",
};

function fixture(reason: "confirmation" | "deleted") {
  const events: string[] = [];
  let current = true;
  let confirmation = true;
  let choice: number | null = 0;
  let acquired = true;
  const pending: WorkspaceContainerResolution = {
    status: "pending",
    reason,
    containerId,
    definition,
    candidates: [local],
  };
  const bindings: WorkspaceContainerBindingAPI = {
    isReady: () => true,
    getEpoch: () => 1,
    isCurrentEpoch: (epoch) => epoch === 1 && current,
    captureReference: () => ({ ref }),
    resolve: () => {
      events.push("resolve");
      return Promise.resolve(pending);
    },
    lookup: () => pending,
    listLocalContainers: () => [local],
    bind: (id, localId) => {
      events.push(`bind:${id}:${localId}`);
      return Promise.resolve();
    },
    create: (id) => {
      events.push(`create:${id}`);
      return Promise.resolve();
    },
    acquirePrompt: () => acquired,
    releasePrompt: () => {
      events.push("release");
    },
  };
  const prompt: WorkspaceContainerPrompt = {
    confirm: () => {
      events.push("confirm");
      return confirmation;
    },
    select: () => {
      events.push("select");
      return choice;
    },
    alert: () => {
      events.push("alert");
    },
  };
  return {
    events,
    bindings,
    prompt,
    setConfirmation: (value: boolean) => {
      confirmation = value;
    },
    setChoice: (value: number | null) => {
      choice = value;
    },
    setCurrent: (value: boolean) => {
      current = value;
    },
    setAcquired: (value: boolean) => {
      acquired = value;
    },
    actions: {
      resolveLegacy: (
        target: number | "new",
        scope: "default" | "tabs" | "all",
      ) => {
        events.push(`legacy:${target}:${scope}`);
        return Promise.resolve();
      },
      isCurrent: () => current,
      unsubscribe: () => {
        events.push("unsubscribe");
        return Promise.resolve();
      },
      retry: () => {
        events.push("retry");
        return Promise.resolve();
      },
    },
  };
}

function assertEquals(actual: unknown, expected: unknown): void {
  assertValue(
    actual,
    expected,
    "container operation must preserve the expected state",
  );
}

export async function runAllTests(): Promise<void> {
  await runTests("workspaceContainerSetup.test.ts", [
    {
      name:
        "legacy assignments require explicit scope and target instead of silently using none",
      fn: async () => {
        const f = fixture("confirmation");
        f.setChoice(2);
        await setupWorkspaceContainers(
          [{ kind: "legacy-unresolved" }],
          f.bindings,
          f.actions,
          f.prompt,
        );
        assertEquals(
          f.events.join(","),
          "resolve,select,select,legacy:new:all,retry,release",
        );
      },
    },
    {
      name: "canceling legacy recovery never applies a default container",
      fn: async () => {
        const f = fixture("confirmation");
        f.setChoice(null);
        await setupWorkspaceContainers(
          [{ kind: "legacy-unresolved" }],
          f.bindings,
          f.actions,
          f.prompt,
        );
        assertEquals(f.events.join(","), "resolve,select,release");
      },
    },
    {
      name:
        "one same-name candidate requires explicit confirmation only once per logical ref",
      fn: async () => {
        const f = fixture("confirmation");
        await setupWorkspaceContainers(
          [ref, ref],
          f.bindings,
          f.actions,
          f.prompt,
        );
        assertEquals(
          f.events.join(","),
          `resolve,confirm,bind:${containerId}:9,retry,release`,
        );
      },
    },
    {
      name: "cancel leaves the pending container untouched",
      fn: async () => {
        const f = fixture("confirmation");
        f.setConfirmation(false);
        await setupWorkspaceContainers([ref], f.bindings, f.actions, f.prompt);
        assertEquals(f.events.join(","), "resolve,confirm,release");
      },
    },
    {
      name:
        "deleted container is recreated only through an explicit create choice",
      fn: async () => {
        const f = fixture("deleted");
        f.setChoice(1);
        await setupWorkspaceContainers([ref], f.bindings, f.actions, f.prompt);
        assertEquals(
          f.events.join(","),
          `resolve,select,create:${containerId},retry,release`,
        );
      },
    },
    {
      name: "deleted container can stop only this workspace's sync",
      fn: async () => {
        const f = fixture("deleted");
        f.setChoice(2);
        await setupWorkspaceContainers([ref], f.bindings, f.actions, f.prompt);
        assertEquals(f.events.join(","), "resolve,select,unsubscribe,release");
      },
    },
    {
      name: "another window's prompt prevents duplicate resolution and dialogs",
      fn: async () => {
        const f = fixture("confirmation");
        f.setAcquired(false);
        await setupWorkspaceContainers([ref], f.bindings, f.actions, f.prompt);
        assertEquals(f.events.length, 0);
      },
    },
    {
      name:
        "account invalidation while confirming cannot bind into the new account",
      fn: async () => {
        const f = fixture("confirmation");
        f.prompt.confirm = () => {
          f.setCurrent(false);
          return true;
        };
        await setupWorkspaceContainers([ref], f.bindings, f.actions, f.prompt);
        assertEquals(f.events.join(","), "resolve,release");
      },
    },
    {
      name:
        "same numeric ID with a different container instance requires explicit reopen",
      fn: () => {
        assertEquals(
          workspaceContainerNeedsReopen(9, "old", {
            status: "resolved",
            userContextId: 9,
            instanceId: "new",
          }),
          true,
        );
        assertEquals(
          workspaceContainerNeedsReopen(9, "same", {
            status: "resolved",
            userContextId: 9,
            instanceId: "same",
          }),
          false,
        );
        assertEquals(
          workspaceContainerNeedsReopen(0, "", {
            status: "resolved",
            userContextId: 0,
          }),
          false,
        );
        assertEquals(
          workspaceContainerNeedsReopen(0, "", {
            status: "pending",
            reason: "deleted",
            candidates: [],
          }),
          true,
        );
      },
    },
  ]);
}
