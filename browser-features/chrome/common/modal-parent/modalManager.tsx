/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { onCleanup } from "solid-js";
import {
  type ModalSize,
  modalSize,
  setModalSize,
  setModalVisible,
} from "./data/data.ts";
import type {
  ModalCancelRequest,
  ModalRequestIdentity,
  ModalResultEnvelope,
  ModalShowRequest,
  ModalTerminalReason,
  TForm,
  TFormResult,
} from "./utils/type.ts";
import { MODAL_TERMINAL_REASONS } from "./utils/type.ts";

export interface ModalActorLike {
  sendQuery(message: string, data: unknown): Promise<unknown>;
}

interface BrowsingContextLike {
  currentWindowGlobal: {
    getActor(name: string): ModalActorLike;
  };
}

interface FocusableElement {
  focus(): void;
}

type TimerHandle = number | ReturnType<typeof globalThis.setTimeout>;

export interface ModalManagerEnvironment {
  getContainer(): FocusableElement | null;
  getActor(): ModalActorLike | null;
  getSize(): ModalSize;
  setVisible(visible: boolean): void;
  setSize(size: ModalSize): void;
  focusWindow(): void;
  notifyHidden(): void;
  addKeydownListener(listener: (event: KeyboardEvent) => void): void;
  removeKeydownListener(listener: (event: KeyboardEvent) => void): void;
  setTimer(callback: () => void, delayMs: number): TimerHandle;
  clearTimer(handle: TimerHandle): void;
  createRequestId(epoch: number): string;
}

export const MODAL_WATCHDOG_DELAY_MS = 8_000;
export const MODAL_PING_TIMEOUT_MS = 3_000;

type ModalProbeResult = "alive" | "dead" | "actor-error" | "timeout";

interface ModalPingResponse extends ModalRequestIdentity {
  ready: boolean;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function matchesIdentity(
  expected: ModalRequestIdentity,
  actual: unknown,
): actual is ModalRequestIdentity & Record<string, unknown> {
  return isRecord(actual) &&
    actual.requestId === expected.requestId &&
    actual.epoch === expected.epoch;
}

function isTerminalReason(value: unknown): value is ModalTerminalReason {
  return typeof value === "string" &&
    (MODAL_TERMINAL_REASONS as readonly string[]).includes(value);
}

function isResultEnvelope(
  expected: ModalRequestIdentity,
  value: unknown,
): value is ModalResultEnvelope {
  if (!matchesIdentity(expected, value) || !isTerminalReason(value.reason)) {
    return false;
  }

  if (value.reason !== "submit") {
    return value.result === null;
  }

  return isRecord(value.result) &&
    Object.values(value.result).every((entry) =>
      typeof entry === "string" || typeof entry === "number"
    );
}

function isPingResponse(
  expected: ModalRequestIdentity,
  value: unknown,
): value is ModalPingResponse {
  return matchesIdentity(expected, value) && typeof value.ready === "boolean";
}

function defaultEnvironment(): ModalManagerEnvironment {
  return {
    getContainer: () =>
      document?.getElementById("modal-parent-container") as
        | (HTMLElement & FocusableElement)
        | null,
    getActor: () => {
      const browser = document?.getElementById("modal-child-browser") as
        | (XULElement & { browsingContext: BrowsingContextLike })
        | null;
      if (!browser?.browsingContext?.currentWindowGlobal) {
        return null;
      }
      return browser.browsingContext.currentWindowGlobal.getActor(
        "NRChromeModal",
      );
    },
    getSize: modalSize,
    setVisible: setModalVisible,
    setSize: (size) => setModalSize(size),
    focusWindow: () => globalThis.focus(),
    notifyHidden: () => {
      try {
        Services.obs.notifyObservers({}, "nora:modal:hide", "");
      } catch {
        // Compatibility notification is best-effort and never owns cleanup.
      }
    },
    addKeydownListener: (listener) =>
      globalThis.addEventListener("keydown", listener),
    removeKeydownListener: (listener) =>
      globalThis.removeEventListener("keydown", listener),
    setTimer: (callback, delayMs) => globalThis.setTimeout(callback, delayMs),
    clearTimer: (handle) => globalThis.clearTimeout(handle),
    createRequestId: (epoch) => {
      if (typeof globalThis.crypto?.randomUUID === "function") {
        return globalThis.crypto.randomUUID();
      }
      return `nora-modal-${Date.now()}-${epoch}`;
    },
  };
}

export function probeModalChildAlive(
  actor: ModalActorLike,
  identity: ModalRequestIdentity,
  pingTimeoutMs: number = MODAL_PING_TIMEOUT_MS,
  timers: Pick<ModalManagerEnvironment, "setTimer" | "clearTimer"> =
    defaultEnvironment(),
): Promise<ModalProbeResult> {
  return new Promise((resolve) => {
    let finished = false;
    const finish = (result: ModalProbeResult) => {
      if (finished) {
        return;
      }
      finished = true;
      timers.clearTimer(timeout);
      resolve(result);
    };

    const timeout = timers.setTimer(() => finish("timeout"), pingTimeoutMs);

    const request: ModalRequestIdentity = {
      requestId: identity.requestId,
      epoch: identity.epoch,
    };

    try {
      actor.sendQuery("NRChromeModal:ping", request).then(
        (response: unknown) => {
          if (!isPingResponse(identity, response)) {
            finish("dead");
            return;
          }
          finish(response.ready ? "alive" : "dead");
        },
        () => finish("actor-error"),
      );
    } catch {
      finish("actor-error");
    }
  });
}

interface PendingModal extends ModalRequestIdentity {
  actor: ModalActorLike;
  resolve(result: TFormResult | null): void;
  settled: boolean;
  watchdog: TimerHandle | null;
}

export class ModalManager {
  private static get targetParent(): HTMLElement | null {
    return document?.getElementById("main-window") as HTMLElement | null;
  }

  private readonly environment: ModalManagerEnvironment;
  private readonly handleKeydown: (event: KeyboardEvent) => void;
  private pending: PendingModal | null = null;
  private epoch = 0;
  private disposed = false;

  public readonly watchdogDelayMs: number;
  public readonly pingTimeoutMs: number;

  constructor(
    environment: Partial<ModalManagerEnvironment> = {},
    timings: {
      watchdogDelayMs?: number;
      pingTimeoutMs?: number;
    } = {},
  ) {
    this.environment = { ...defaultEnvironment(), ...environment };
    this.watchdogDelayMs = timings.watchdogDelayMs ?? MODAL_WATCHDOG_DELAY_MS;
    this.pingTimeoutMs = timings.pingTimeoutMs ?? MODAL_PING_TIMEOUT_MS;
    this.handleKeydown = (event: KeyboardEvent) => {
      if (event.key === "Escape" && this.pending !== null) {
        this.hide("escape");
      }
    };
    this.environment.addKeydownListener(this.handleKeydown);
    onCleanup(() => this.dispose());
  }

  public show(
    form: TForm,
    options: { width: number; height: number },
  ): Promise<TFormResult | null> {
    if (this.disposed) {
      return Promise.resolve(null);
    }

    const replaced = this.pending;
    if (replaced) {
      this.settleOnce(replaced, "replacement", null, true, false);
    }

    const epoch = ++this.epoch;
    let requestId: string;
    try {
      requestId = this.environment.createRequestId(epoch);
    } catch {
      this.hideOverlay();
      return Promise.resolve(null);
    }
    const identity: ModalRequestIdentity = { requestId, epoch };

    const container = this.environment.getContainer();
    if (!container) {
      this.hideOverlay();
      return Promise.resolve(null);
    }

    let actor: ModalActorLike | null;
    try {
      actor = this.environment.getActor();
    } catch {
      actor = null;
    }
    if (!actor) {
      this.hideOverlay();
      return Promise.resolve(null);
    }

    let safeForm: TForm;
    try {
      safeForm = JSON.parse(JSON.stringify(form)) as TForm;
    } catch {
      this.hideOverlay();
      return Promise.resolve(null);
    }

    this.environment.setVisible(true);
    this.environment.setSize({
      width: options.width,
      height: options.height,
    });
    container.focus();

    return new Promise((resolve) => {
      const pending: PendingModal = {
        ...identity,
        actor,
        resolve,
        settled: false,
        watchdog: null,
      };

      // Ownership is established before the actor query or any asynchronous work.
      this.pending = pending;

      pending.watchdog = this.environment.setTimer(() => {
        void probeModalChildAlive(
          actor,
          identity,
          this.pingTimeoutMs,
          this.environment,
        ).then((probeResult) => {
          if (probeResult === "alive") {
            return;
          }
          const reason: ModalTerminalReason = probeResult === "actor-error"
            ? "actor-error"
            : probeResult === "timeout"
            ? "timeout"
            : "dead";
          this.settleOnce(pending, reason, null, true, true);
        });
      }, this.watchdogDelayMs);

      const request: ModalShowRequest = {
        ...identity,
        form: safeForm,
      };

      try {
        actor.sendQuery("NRChromeModal:show", request).then(
          (response: unknown) => {
            if (!isResultEnvelope(identity, response)) {
              this.settleOnce(pending, "dead", null, true, true);
              return;
            }
            this.settleOnce(
              pending,
              response.reason,
              response.result,
              response.reason !== "submit",
              true,
            );
          },
          () => {
            this.settleOnce(pending, "actor-error", null, true, true);
          },
        );
      } catch {
        this.settleOnce(pending, "actor-error", null, true, true);
      }
    });
  }

  public hide(reason: ModalTerminalReason = "hide"): void {
    const pending = this.pending;
    if (!pending) {
      this.hideOverlay();
      return;
    }
    this.settleOnce(pending, reason, null, true, true);
  }

  public setModalSize(newSize: ModalSize): void {
    this.environment.setSize({ ...this.environment.getSize(), ...newSize });
  }

  public handleBackdropClick(event: MouseEvent): void {
    if (event.target === event.currentTarget && this.pending !== null) {
      this.hide("backdrop");
    }
  }

  public getActiveRequestIdentity(): ModalRequestIdentity | null {
    return this.pending
      ? { requestId: this.pending.requestId, epoch: this.pending.epoch }
      : null;
  }

  public dispose(): void {
    if (this.disposed) {
      return;
    }
    this.disposed = true;
    this.environment.removeKeydownListener(this.handleKeydown);
    const pending = this.pending;
    if (pending) {
      this.settleOnce(pending, "remove", null, true, true);
    }
  }

  private settleOnce(
    request: PendingModal,
    reason: ModalTerminalReason,
    result: TFormResult | null,
    cancelActor: boolean,
    hideOverlay: boolean,
  ): boolean {
    if (
      request.settled ||
      this.pending !== request ||
      this.pending.requestId !== request.requestId ||
      this.pending.epoch !== request.epoch
    ) {
      return false;
    }

    request.settled = true;
    this.pending = null;
    if (request.watchdog !== null) {
      this.environment.clearTimer(request.watchdog);
      request.watchdog = null;
    }

    if (cancelActor) {
      this.cancelActor(request, reason);
    }
    if (hideOverlay) {
      this.hideOverlay();
    }
    request.resolve(result);
    return true;
  }

  private cancelActor(
    request: PendingModal,
    reason: ModalTerminalReason,
  ): void {
    const cancel: ModalCancelRequest = {
      requestId: request.requestId,
      epoch: request.epoch,
      reason,
    };
    try {
      request.actor.sendQuery("NRChromeModal:cancel", cancel).catch(() => {});
    } catch {
      // The actor may already be dead. Cancellation never owns another request.
    }
  }

  private hideOverlay(): void {
    this.environment.setVisible(false);
    this.environment.setSize({ width: 600, height: 800 });
    this.environment.focusWindow();
    this.environment.notifyHidden();
  }

  public static get parentElement(): HTMLElement | null {
    return this.targetParent;
  }
}
