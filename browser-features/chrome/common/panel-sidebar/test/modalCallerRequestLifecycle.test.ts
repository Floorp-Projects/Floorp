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
import { PanelSidebarAddModal } from "../components/panel-sidebar-modal.tsx";
import { panelSidebarData, setPanelSidebarData } from "../data/data.ts";
import type { Panel } from "../utils/type.ts";

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
    createRequestId: (epoch) => `panel-${epoch}`,
  };
  return new ModalManager(environment);
}

function createCaller(parent: ModalParent): PanelSidebarAddModal {
  const caller = Object.create(
    PanelSidebarAddModal.prototype,
  ) as PanelSidebarAddModal;
  const state = caller as unknown as {
    modalParent: ModalParent;
    createFormConfig(): object;
  };
  state.modalParent = parent;
  state.createFormConfig = () => ({ forms: [], title: "Panel" });
  return caller;
}

function panelResult(url: string): TFormResult {
  return {
    type: "web",
    width: "450",
    url,
    userContextId: "0",
    userAgent: "false",
  };
}

const parent = ModalParent.getInstance();
const parentState = parent as unknown as { modalManager: ModalManager | null };

async function testPanelSubmit(): Promise<void> {
  const actor = new CallerActor();
  const manager = createManager(actor);
  parentState.modalManager = manager;
  const shown = createCaller(parent).showAddPanelModal();
  const request = actor.requests[0];
  assert(request !== undefined, "panel caller reaches modal actor");
  const expected = panelResult("https://example.com");
  actor.submit(request, expected);
  assertEquals(await shown, expected, "panel submit returns public result");
  assert(
    panelSidebarData().some((panel: Panel) =>
      panel.url === "https://example.com"
    ),
    "real panel caller applies submitted result",
  );
  manager.dispose();
}

async function testPanelCancelReturnsNull(): Promise<void> {
  const actor = new CallerActor();
  const manager = createManager(actor);
  parentState.modalManager = manager;
  const shown = createCaller(parent).showAddPanelModal();
  parent.hideNoraModal();
  assertEquals(await shown, null, "panel public callback accepts null");
  manager.dispose();
}

async function testPanelRapidReplacement(): Promise<void> {
  const actor = new CallerActor();
  const manager = createManager(actor);
  parentState.modalManager = manager;
  const caller = createCaller(parent);
  const first = caller.showAddPanelModal();
  const firstRequest = actor.requests[0];
  const second = caller.showAddPanelModal();
  const secondRequest = actor.requests[1];
  assert(firstRequest !== undefined, "first panel caller reached actor");
  assert(secondRequest !== undefined, "replacement reached actor");
  assertEquals(await first, null, "replaced panel caller settles null");

  actor.submit(firstRequest, panelResult("https://stale.example"));
  const expected = panelResult("https://replacement.example");
  actor.submit(secondRequest, expected);
  assertEquals(await second, expected, "replacement panel remains active");
  assert(
    !panelSidebarData().some((panel: Panel) =>
      panel.url === "https://stale.example"
    ),
    "stale panel result is not applied",
  );
  manager.dispose();
}

const tests: TestCase[] = [
  { name: "panel submit", fn: testPanelSubmit },
  { name: "panel cancel returns null", fn: testPanelCancelReturnsNull },
  { name: "panel rapid replacement", fn: testPanelRapidReplacement },
];

export async function runAllTests(): Promise<void> {
  const savedManager = parentState.modalManager;
  const savedPanels = [...panelSidebarData()];
  try {
    await runTests("modalCallerRequestLifecycle.test.ts", tests);
  } finally {
    parentState.modalManager = savedManager;
    setPanelSidebarData(savedPanels);
  }
}
