// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import {
  type ModalActorLike,
  ModalManager,
  type ModalManagerEnvironment,
} from "../modalManager.tsx";
import type {
  ModalRequestIdentity,
  ModalResultEnvelope,
  ModalShowRequest,
  ModalTerminalReason,
  TFormResult,
} from "../utils/type.ts";

type DeferredQuery = {
  request: ModalShowRequest;
  resolve(value: unknown): void;
  reject(reason: unknown): void;
};

class FakeActor implements ModalActorLike {
  readonly sent: Array<{ name: string; data: unknown }> = [];
  readonly shows = new Map<string, DeferredQuery>();
  pingMode: "alive" | "dead" | "silent" | "reject" = "alive";
  throwOnShow = false;

  sendQuery(name: string, data: unknown): Promise<unknown> {
    this.sent.push({ name, data });
    if (name === "NRChromeModal:show") {
      if (this.throwOnShow) {
        throw new Error("actor threw");
      }
      const request = data as ModalShowRequest;
      return new Promise((resolve, reject) => {
        this.shows.set(request.requestId, { request, resolve, reject });
      });
    }
    if (name === "NRChromeModal:ping") {
      const identity = data as ModalRequestIdentity;
      if (this.pingMode === "silent") {
        return new Promise(() => {});
      }
      if (this.pingMode === "reject") {
        return Promise.reject(new Error("actor unavailable"));
      }
      return Promise.resolve({
        ...identity,
        ready: this.pingMode === "alive",
      });
    }
    return Promise.resolve(true);
  }

  finish(
    identity: ModalRequestIdentity,
    reason: ModalTerminalReason,
    result: TFormResult | null,
  ): void {
    const response: ModalResultEnvelope = { ...identity, reason, result };
    this.shows.get(identity.requestId)?.resolve(response);
  }

  fail(identity: ModalRequestIdentity): void {
    this.shows.get(identity.requestId)?.reject(new Error("query failed"));
  }
}

class FakeClock {
  private nextHandle = 1;
  private readonly timers = new Map<number, () => void>();
  private readonly allCallbacks = new Map<number, () => void>();

  setTimer = (callback: () => void, _delayMs: number): number => {
    const handle = this.nextHandle++;
    this.timers.set(handle, callback);
    this.allCallbacks.set(handle, callback);
    return handle;
  };

  clearTimer = (handle: number | ReturnType<typeof globalThis.setTimeout>) => {
    this.timers.delete(Number(handle));
  };

  runNext(): number {
    const entry = this.timers.entries().next().value as
      | [number, () => void]
      | undefined;
    if (!entry) {
      throw new Error("no pending timer");
    }
    const [handle, callback] = entry;
    this.timers.delete(handle);
    callback();
    return handle;
  }

  runEvenIfCleared(handle: number): void {
    const callback = this.allCallbacks.get(handle);
    if (!callback) {
      throw new Error(`unknown timer ${handle}`);
    }
    callback();
  }

  firstHandle(): number {
    const handle = this.allCallbacks.keys().next().value;
    if (typeof handle !== "number") {
      throw new Error("no timer was registered");
    }
    return handle;
  }
}

type ManagerHarness = {
  actor: FakeActor;
  clock: FakeClock;
  manager: ModalManager;
  visible: boolean;
  hiddenCount: number;
  keydown: ((event: KeyboardEvent) => void) | null;
};

function createHarness(actor = new FakeActor()): ManagerHarness {
  const clock = new FakeClock();
  const state: ManagerHarness = {
    actor,
    clock,
    manager: null as unknown as ModalManager,
    visible: false,
    hiddenCount: 0,
    keydown: null,
  };
  const environment: Partial<ModalManagerEnvironment> = {
    getContainer: () => ({ focus: () => {} }),
    getActor: () => actor,
    setVisible: (visible) => {
      state.visible = visible;
      if (!visible) {
        state.hiddenCount++;
      }
    },
    setSize: () => {},
    focusWindow: () => {},
    notifyHidden: () => {},
    addKeydownListener: (listener) => {
      state.keydown = listener;
    },
    removeKeydownListener: (listener) => {
      if (state.keydown === listener) {
        state.keydown = null;
      }
    },
    setTimer: clock.setTimer,
    clearTimer: clock.clearTimer,
    createRequestId: (epoch) => `request-${epoch}`,
  };
  state.manager = new ModalManager(environment, {
    watchdogDelayMs: 10,
    pingTimeoutMs: 10,
  });
  return state;
}

const form = { forms: [], title: "lifecycle" };
const size = { width: 400, height: 300 };

async function flushPromises(): Promise<void> {
  await Promise.resolve();
  await Promise.resolve();
}

function testRegistersBeforeActorContinuation(): void {
  const harness = createHarness();
  void harness.manager.show(form, size);
  const active = harness.manager.getActiveRequestIdentity();
  const query = harness.actor.sent[0];
  assert(active !== null, "request must be active synchronously");
  assertEquals(query.name, "NRChromeModal:show", "show query is sent");
  assertEquals(
    (query.data as ModalShowRequest).requestId,
    active.requestId,
    "the actor receives the registered request identity",
  );
  harness.manager.hide();
  harness.manager.dispose();
}

async function testRapidReplacementIgnoresStaleCompletion(): Promise<void> {
  const harness = createHarness();
  const first = harness.manager.show(form, size);
  const firstIdentity = harness.manager.getActiveRequestIdentity();
  assert(firstIdentity !== null, "first request is active");
  const staleWatchdog = harness.clock.firstHandle();

  const second = harness.manager.show(form, size);
  const secondIdentity = harness.manager.getActiveRequestIdentity();
  assert(secondIdentity !== null, "replacement is active");
  assertEquals(await first, null, "old caller settles once with null");
  assert(harness.visible, "replacement keeps the overlay visible");

  const replacementCancel = harness.actor.sent.find((entry) =>
    entry.name === "NRChromeModal:cancel" &&
    (entry.data as ModalRequestIdentity).requestId === firstIdentity.requestId
  );
  assert(replacementCancel !== undefined, "old actor request is targeted");
  assertEquals(
    (replacementCancel.data as { reason: string }).reason,
    "replacement",
    "replacement cancellation has an explicit reason",
  );

  harness.clock.runEvenIfCleared(staleWatchdog);
  harness.actor.finish(firstIdentity, "submit", { stale: "value" });
  await flushPromises();
  assertEquals(
    harness.manager.getActiveRequestIdentity()?.requestId,
    secondIdentity.requestId,
    "stale timer and result cannot clear the replacement",
  );
  assert(harness.visible, "stale completion cannot hide the replacement");

  const expected = { current: "value" };
  harness.actor.finish(secondIdentity, "submit", expected);
  assertEquals(await second, expected, "replacement result is delivered");
  assertEquals(harness.visible, false, "matching submit hides the overlay");
  assertEquals(harness.hiddenCount, 1, "only matching completion hides once");
  harness.manager.dispose();
}

async function testEscapeAndProgrammaticHideAreTargeted(): Promise<void> {
  const escapeHarness = createHarness();
  const escaped = escapeHarness.manager.show(form, size);
  const escapeIdentity = escapeHarness.manager.getActiveRequestIdentity();
  assert(escapeIdentity !== null, "escape request is active");
  escapeHarness.keydown?.({ key: "Escape" } as KeyboardEvent);
  assertEquals(await escaped, null, "Escape settles null");
  const escapeCancel = escapeHarness.actor.sent.find((entry) =>
    entry.name === "NRChromeModal:cancel"
  );
  assertEquals(
    (escapeCancel?.data as { reason: string }).reason,
    "escape",
    "Escape sends a targeted reason",
  );
  escapeHarness.manager.dispose();

  const hideHarness = createHarness();
  const hidden = hideHarness.manager.show(form, size);
  hideHarness.manager.hide("hide");
  assertEquals(await hidden, null, "programmatic hide settles null");
  const hideCancel = hideHarness.actor.sent.find((entry) =>
    entry.name === "NRChromeModal:cancel"
  );
  assertEquals(
    (hideCancel?.data as { reason: string }).reason,
    "hide",
    "programmatic hide sends a targeted reason",
  );
  hideHarness.manager.dispose();
}

async function testBackdropIsMatchingCancellation(): Promise<void> {
  const harness = createHarness();
  const shown = harness.manager.show(form, size);
  const target = {};
  harness.manager.handleBackdropClick({
    target,
    currentTarget: target,
  } as unknown as MouseEvent);
  assertEquals(await shown, null, "backdrop settles null");
  const cancel = harness.actor.sent.find((entry) =>
    entry.name === "NRChromeModal:cancel"
  );
  assertEquals(
    (cancel?.data as { reason: string }).reason,
    "backdrop",
    "backdrop sends a targeted reason",
  );
  harness.manager.dispose();
}

async function testDeadSilentAndActorErrorsSettleOnce(): Promise<void> {
  const deadActor = new FakeActor();
  deadActor.pingMode = "dead";
  const deadHarness = createHarness(deadActor);
  const dead = deadHarness.manager.show(form, size);
  deadHarness.clock.runNext();
  await flushPromises();
  assertEquals(await dead, null, "dead child settles null");
  assertEquals(deadHarness.hiddenCount, 1, "dead child hides once");
  deadHarness.manager.dispose();

  const silentActor = new FakeActor();
  silentActor.pingMode = "silent";
  const silentHarness = createHarness(silentActor);
  const silent = silentHarness.manager.show(form, size);
  silentHarness.clock.runNext();
  silentHarness.clock.runNext();
  await flushPromises();
  assertEquals(await silent, null, "silent child watchdog settles null");
  assertEquals(silentHarness.hiddenCount, 1, "timeout hides once");
  silentHarness.manager.dispose();

  const throwingActor = new FakeActor();
  throwingActor.throwOnShow = true;
  const throwingHarness = createHarness(throwingActor);
  assertEquals(
    await throwingHarness.manager.show(form, size),
    null,
    "actor throw settles null",
  );
  assertEquals(throwingHarness.hiddenCount, 1, "actor throw hides once");
  throwingHarness.manager.dispose();

  const rejectingActor = new FakeActor();
  const rejectingHarness = createHarness(rejectingActor);
  const rejected = rejectingHarness.manager.show(form, size);
  const rejectedIdentity = rejectingHarness.manager
    .getActiveRequestIdentity();
  assert(rejectedIdentity !== null, "rejecting request is active");
  rejectingActor.fail(rejectedIdentity);
  assertEquals(await rejected, null, "actor query rejection settles null");
  assertEquals(
    rejectingHarness.hiddenCount,
    1,
    "actor query rejection hides once",
  );
  rejectingHarness.manager.dispose();
}

const tests: TestCase[] = [
  {
    name: "request is registered before actor continuation",
    fn: testRegistersBeforeActorContinuation,
  },
  {
    name: "rapid replacement ignores stale completion",
    fn: testRapidReplacementIgnoresStaleCompletion,
  },
  {
    name: "Escape and programmatic hide are targeted",
    fn: testEscapeAndProgrammaticHideAreTargeted,
  },
  {
    name: "backdrop is matching cancellation",
    fn: testBackdropIsMatchingCancellation,
  },
  {
    name: "dead silent and actor errors settle once",
    fn: testDeadSilentAndActorErrorsSettleOnce,
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("modalRequestLifecycle.test.ts", tests);
}
