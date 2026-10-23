// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import { WorkspacesService } from "../workspacesService.ts";
import type { WorkspaceManageModal } from "../workspace-modal.tsx";
import { enabled, setEnabled } from "../data/config.ts";
import type { TWorkspace, TWorkspaceID } from "../utils/type.ts";
import type { TFormResult } from "../../modal-parent/utils/type.ts";
import { getWorkspaceSessionIdentities } from "../utils/workspace-sessions.ts";
import { workspacesDataStore } from "../data/data.ts";

const workspaceId = "11111111-1111-4111-8111-111111111111" as TWorkspaceID;
const workspace: TWorkspace = {
  name: "Existing project",
  userContextId: 0,
  isSelected: null,
  isDefault: null,
};

const snapshotWorkspaceDefinitions = () =>
  JSON.stringify([
    ...(workspacesDataStore.data as unknown as Map<TWorkspaceID, TWorkspace>)
      .entries(),
  ]);

function createCaller() {
  const pending: Array<(result: TFormResult | null) => void> = [];
  const caller = Object.create(
    WorkspacesService.prototype,
  ) as WorkspacesService;
  caller.getSelectedWorkspaceID = () => workspaceId;
  caller.getRawWorkspace = () => workspace;
  const show = () =>
    new Promise<TFormResult | null>((resolve) => {
      pending.push(resolve);
    });
  caller.modalCtx = {
    showCreateWorkspacesModal: show,
    showWorkspacesModal: show,
  } as unknown as WorkspaceManageModal;
  return { caller, pending };
}

async function assertDisabledSubmissionDoesNothing(
  edit: boolean,
): Promise<void> {
  const savedEnabled = enabled();
  const beforeIdentities = getWorkspaceSessionIdentities().length;
  const beforeDefinitions = snapshotWorkspaceDefinitions();
  const beforeOrder = workspacesDataStore.order.join(",");
  try {
    setEnabled(true);
    const { caller, pending } = createCaller();
    const operation = edit
      ? caller.manageWorkspaceFromDialog(workspaceId)
      : caller.createWorkspaceFromDialog();
    assertEquals(pending.length, 1, "Enabled UI opens a confirmation form");
    setEnabled(false);
    pending[0]({ name: "Must not be created", userContextId: "new" });
    assertEquals(
      await operation,
      null,
      "Disabling cancels the submitted result",
    );
    assertEquals(
      getWorkspaceSessionIdentities().length,
      beforeIdentities,
      "No new container is created after disable",
    );
    assertEquals(
      snapshotWorkspaceDefinitions(),
      beforeDefinitions,
      "No workspace definitions change",
    );
    assertEquals(
      workspacesDataStore.order.join(","),
      beforeOrder,
      "Workspace order stays intact",
    );
  } finally {
    setEnabled(savedEnabled);
  }
}

const tests: TestCase[] = [
  {
    name: "Disabled workspace creation does not open a modal",
    async fn() {
      const savedEnabled = enabled();
      try {
        setEnabled(false);
        const { caller, pending } = createCaller();
        assertEquals(
          await caller.createWorkspaceFromDialog(),
          null,
          "No create",
        );
        assertEquals(
          await caller.manageWorkspaceFromDialog(workspaceId),
          null,
          "No edit",
        );
        assertEquals(pending.length, 0, "Disabled feature opens no form");
      } finally {
        setEnabled(savedEnabled);
      }
    },
  },
  {
    name: "Disabling while creating ignores the later modal submission",
    fn: () => assertDisabledSubmissionDoesNothing(false),
  },
  {
    name: "Disabling while editing ignores the later modal submission",
    fn: () => assertDisabledSubmissionDoesNothing(true),
  },
  {
    name: "Cancelling a create form leaves data and containers untouched",
    async fn() {
      const savedEnabled = enabled();
      const beforeIdentities = getWorkspaceSessionIdentities().length;
      const beforeDefinitions = workspacesDataStore.data;
      const beforeOrder = workspacesDataStore.order.join(",");
      try {
        setEnabled(true);
        const { caller, pending } = createCaller();
        const operation = caller.createWorkspaceFromDialog();
        assert(pending[0] !== undefined, "The confirmation form is pending");
        pending[0](null);
        assertEquals(await operation, null, "Cancellation returns null");
        assertEquals(
          getWorkspaceSessionIdentities().length,
          beforeIdentities,
          "No orphan container",
        );
        assertEquals(
          workspacesDataStore.data,
          beforeDefinitions,
          "No draft workspace",
        );
        assertEquals(
          workspacesDataStore.order.join(","),
          beforeOrder,
          "No order entry",
        );
      } finally {
        setEnabled(savedEnabled);
      }
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("workspaceSessionLifecycle.test.ts", tests);
}
