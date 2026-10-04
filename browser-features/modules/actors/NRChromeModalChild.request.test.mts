// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../chrome/test/utils/test_harness.ts";
import { ModalChildRequestController } from "./NRChromeModalChild.sys.mts";
import type {
  ModalRequestIdentity,
  ModalShowRequest,
} from "../../chrome/common/modal-parent/utils/type.ts";

class FakeModalWindow {
  readonly document = {
    documentElement: { dataset: {} as Record<string, string> },
  };
  readonly messages: unknown[] = [];
  private readonly listeners = new Set<(event: MessageEvent) => void>();
  private readonly intervals = new Map<number, () => void>();
  private readonly allIntervals = new Map<number, () => void>();
  private nextInterval = 1;
  throwOnPost = false;

  addEventListener(
    type: string,
    listener: (event: MessageEvent) => void,
  ): void {
    if (type === "message") {
      this.listeners.add(listener);
    }
  }

  removeEventListener(
    type: string,
    listener: (event: MessageEvent) => void,
  ): void {
    if (type === "message") {
      this.listeners.delete(listener);
    }
  }

  setInterval(callback: () => void, _delayMs: number): number {
    const handle = this.nextInterval++;
    this.intervals.set(handle, callback);
    this.allIntervals.set(handle, callback);
    return handle;
  }

  clearInterval(handle: number): void {
    this.intervals.delete(handle);
  }

  postMessage(message: unknown, _targetOrigin: string): void {
    if (this.throwOnPost) {
      throw new Error("WindowGlobal is gone");
    }
    this.messages.push(message);
  }

  dispatch(message: unknown): void {
    for (const listener of [...this.listeners]) {
      listener({ data: message } as MessageEvent);
    }
  }

  setReady(ready: boolean): void {
    if (ready) {
      this.document.documentElement.dataset.noraModalReady = "true";
    } else {
      delete this.document.documentElement.dataset.noraModalReady;
    }
  }

  fireIntervalEvenIfCleared(handle: number): void {
    this.allIntervals.get(handle)?.();
  }

  firstInterval(): number {
    const handle = this.allIntervals.keys().next().value;
    if (typeof handle !== "number") {
      throw new Error("no interval registered");
    }
    return handle;
  }

  listenerCount(): number {
    return this.listeners.size;
  }
}

function asWindow(win: FakeModalWindow): Window {
  return win as unknown as Window;
}

function showRequest(
  requestId: string,
  epoch: number,
): ModalShowRequest {
  return {
    requestId,
    epoch,
    form: { forms: [], title: requestId },
  };
}

function matchingMessages(
  win: FakeModalWindow,
  type: string,
  identity: ModalRequestIdentity,
): unknown[] {
  return win.messages.filter((value) => {
    const message = value as Record<string, unknown>;
    return message.type === type &&
      message.requestId === identity.requestId &&
      message.epoch === identity.epoch;
  });
}

async function testCancelBeforeReadyIsEffective(): Promise<void> {
  const controller = new ModalChildRequestController();
  const win = new FakeModalWindow();
  const request = showRequest("before-ready", 1);
  const result = controller.show(asWindow(win), request);
  const staleInterval = win.firstInterval();

  assertEquals(win.listenerCount(), 1, "submit listener is registered early");
  assertEquals(
    matchingMessages(win, "nora-modal-init", request).length,
    0,
    "init waits for readiness",
  );
  assert(
    controller.cancel({ ...request, reason: "cancel" }),
    "matching cancel is accepted before ready",
  );
  assertEquals((await result)?.reason, "cancel", "query settles as cancel");
  assertEquals((await result)?.result, null, "cancel result is null");
  assertEquals(win.listenerCount(), 0, "listener is removed on cancel");

  win.setReady(true);
  win.fireIntervalEvenIfCleared(staleInterval);
  assertEquals(
    matchingMessages(win, "nora-modal-init", request).length,
    0,
    "stale ready continuation cannot render cancelled request",
  );
}

async function testTaggedSubmitAndRemove(): Promise<void> {
  const controller = new ModalChildRequestController();
  const win = new FakeModalWindow();
  win.setReady(true);
  const request = showRequest("submit", 2);
  const result = controller.show(asWindow(win), request);

  assertEquals(
    matchingMessages(win, "nora-modal-init", request).length,
    1,
    "tagged init is posted",
  );
  win.dispatch({ type: "nora-modal-submit", result: { ignored: "yes" } });
  win.dispatch({
    type: "nora-modal-submit",
    requestId: "stale",
    epoch: 1,
    reason: "submit",
    result: { ignored: "yes" },
  });
  assertEquals(
    controller.getActiveRequestIdentity()?.requestId,
    request.requestId,
    "untagged and stale submit are ignored",
  );

  win.dispatch({
    type: "nora-modal-submit",
    requestId: request.requestId,
    epoch: request.epoch,
    reason: "submit",
    result: { accepted: "yes" },
  });
  assertEquals((await result)?.result?.accepted, "yes", "result is delivered");
  assertEquals(
    matchingMessages(win, "nora-modal-remove", request).length,
    1,
    "matching tagged remove is posted once",
  );

  win.dispatch({
    type: "nora-modal-submit",
    requestId: request.requestId,
    epoch: request.epoch,
    reason: "submit",
    result: { accepted: "twice" },
  });
  assertEquals(
    matchingMessages(win, "nora-modal-remove", request).length,
    1,
    "duplicate channel delivery cannot settle twice",
  );
}

async function testReplacementRejectsStaleContinuations(): Promise<void> {
  const controller = new ModalChildRequestController();
  const win = new FakeModalWindow();
  const firstRequest = showRequest("first", 3);
  const first = controller.show(asWindow(win), firstRequest);
  const staleInterval = win.firstInterval();

  win.setReady(true);
  const secondRequest = showRequest("second", 4);
  const second = controller.show(asWindow(win), secondRequest);
  assertEquals((await first)?.reason, "replacement", "old query is replaced");
  assertEquals((await first)?.result, null, "old query settles null");

  win.fireIntervalEvenIfCleared(staleInterval);
  win.dispatch({
    type: "nora-modal-submit",
    requestId: firstRequest.requestId,
    epoch: firstRequest.epoch,
    reason: "submit",
    result: { stale: "yes" },
  });
  assertEquals(
    controller.ping(firstRequest)?.ready,
    false,
    "stale ping cannot claim the active request",
  );
  assertEquals(
    controller.getActiveRequestIdentity()?.requestId,
    secondRequest.requestId,
    "stale ready and submit leave replacement active",
  );

  win.dispatch({
    type: "nora-modal-submit",
    requestId: secondRequest.requestId,
    epoch: secondRequest.epoch,
    reason: "cancel",
    result: null,
  });
  assertEquals((await second)?.reason, "cancel", "replacement can cancel");
}

async function testDestroyAndActorErrorAreMatching(): Promise<void> {
  const controller = new ModalChildRequestController();
  const win = new FakeModalWindow();
  win.setReady(true);
  const request = showRequest("destroy", 5);
  const result = controller.show(asWindow(win), request);
  controller.destroy();
  assertEquals(
    (await result)?.reason,
    "dead",
    "destroy settles active request",
  );
  assertEquals(win.listenerCount(), 0, "destroy tears down listener");

  const errorController = new ModalChildRequestController();
  const throwingWindow = new FakeModalWindow();
  throwingWindow.setReady(true);
  throwingWindow.throwOnPost = true;
  const error = await errorController.show(
    asWindow(throwingWindow),
    showRequest("actor-error", 6),
  );
  assertEquals(error?.reason, "actor-error", "post failure is normalized");
  assertEquals(error?.result, null, "post failure settles null");
}

const tests: TestCase[] = [
  {
    name: "cancel before ready is effective",
    fn: testCancelBeforeReadyIsEffective,
  },
  { name: "tagged submit and remove", fn: testTaggedSubmitAndRemove },
  {
    name: "replacement rejects stale continuations",
    fn: testReplacementRejectsStaleContinuations,
  },
  {
    name: "destroy and actor error are matching",
    fn: testDestroyAndActorErrorAreMatching,
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("NRChromeModalChild.request.test.mts", tests);
}
