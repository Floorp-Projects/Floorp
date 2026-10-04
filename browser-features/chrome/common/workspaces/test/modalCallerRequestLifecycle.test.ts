// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import ModalParent from "../../modal-parent/index.ts";
import {
  type ModalActorLike,
  ModalManager,
  type ModalManagerEnvironment,
} from "../../modal-parent/modalManager.tsx";
import type {
  ModalRequestIdentity,
  ModalShowRequest,
  TFormResult,
} from "../../modal-parent/utils/type.ts";
import { WorkspaceManageModal } from "../workspace-modal.tsx";
import type { TWorkspaceID } from "../utils/type.ts";

class CallerActor implements ModalActorLike {
  readonly requests: ModalShowRequest[] = [];
  private readonly resolvers = new Map<string, (value: unknown) => void>();

  sendQuery(name: string, data: unknown): Promise<unknown> {
    if (name !== "NRChromeModal:show") {
      return Promise.resolve(true);
    }
    const request = data as ModalShowRequest;
    this.requests.push(request);
    return new Promise((resolve) => {
      this.resolvers.set(request.requestId, resolve);
    });
  }

  submit(identity: ModalRequestIdentity, result: TFormResult): void {
    this.resolvers.get(identity.requestId)?.({
      ...identity,
      reason: "submit",
      result,
    });
  }
}

function createManager(actor: CallerActor): ModalManager {
  let timer = 0;
  const environment: Partial<ModalManagerEnvironment> = {
    getContainer: () => ({ focus: () => {} }),
    getActor: () => actor,
    setVisible: () => {},
    setSize: () => {},
    focusWindow: () => {},
    notifyHidden: () => {},
    addKeydownListener: () => {},
    removeKeydownListener: () => {},
    setTimer: () => ++timer,
    clearTimer: () => {},
    createRequestId: (epoch) => `workspace-${epoch}`,
  };
  return new ModalManager(environment);
}

function createCaller(parent: ModalParent): WorkspaceManageModal {
  const caller = Object.create(
    WorkspaceManageModal.prototype,
  ) as WorkspaceManageModal;
  const state = caller as unknown as {
    ctx: {
      getRawWorkspace(id: unknown): object | null;
      getSelectedWorkspaceID(): TWorkspaceID;
    };
    modalParent: ModalParent;
    createFormConfig(workspace: object): object;
  };
  state.ctx = {
    getRawWorkspace: () => ({ id: "workspace", name: "Workspace" }),
    getSelectedWorkspaceID: () => "workspace" as TWorkspaceID,
  };
  state.modalParent = parent;
  state.createFormConfig = () => ({ forms: [], title: "Workspace" });
  return caller;
}

const parent = ModalParent.getInstance();
const parentState = parent as unknown as { modalManager: ModalManager | null };
const workspaceId = "workspace" as TWorkspaceID;

async function testWorkspaceSubmit(): Promise<void> {
  const actor = new CallerActor();
  const manager = createManager(actor);
  parentState.modalManager = manager;
  const caller = createCaller(parent);
  const shown = caller.showWorkspacesModal(workspaceId);
  const request = actor.requests[0];
  assert(request !== undefined, "workspace caller reaches modal actor");
  const expected = { name: "Renamed" };
  actor.submit(request, expected);
  assertEquals(await shown, expected, "workspace submit returns public result");
  manager.dispose();
}

async function testWorkspaceCancelReturnsNull(): Promise<void> {
  const actor = new CallerActor();
  const manager = createManager(actor);
  parentState.modalManager = manager;
  const shown = createCaller(parent).showWorkspacesModal(workspaceId);
  parent.hideNoraModal();
  assertEquals(await shown, null, "workspace public callback accepts null");
  manager.dispose();
}

async function testWorkspaceRapidReplacement(): Promise<void> {
  const actor = new CallerActor();
  const manager = createManager(actor);
  parentState.modalManager = manager;
  const caller = createCaller(parent);
  const first = caller.showWorkspacesModal(workspaceId);
  const firstRequest = actor.requests[0];
  const second = caller.showWorkspacesModal(workspaceId);
  const secondRequest = actor.requests[1];
  assert(firstRequest !== undefined, "first caller reached actor");
  assert(secondRequest !== undefined, "replacement reached actor");
  assertEquals(await first, null, "replaced workspace caller settles null");

  actor.submit(firstRequest, { stale: "ignored" });
  const expected = { name: "Replacement" };
  actor.submit(secondRequest, expected);
  assertEquals(await second, expected, "replacement remains active");
  manager.dispose();
}

const tests: TestCase[] = [
  { name: "workspace submit", fn: testWorkspaceSubmit },
  { name: "workspace cancel returns null", fn: testWorkspaceCancelReturnsNull },
  { name: "workspace rapid replacement", fn: testWorkspaceRapidReplacement },
];

export async function runAllTests(): Promise<void> {
  const savedManager = parentState.modalManager;
  try {
    await runTests("modalCallerRequestLifecycle.test.ts", tests);
  } finally {
    parentState.modalManager = savedManager;
  }
}
