/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type {
  ModalCancelRequest,
  ModalRequestIdentity,
  ModalResultEnvelope,
  ModalShowRequest,
  ModalTerminalReason,
  TFormResult,
} from "../../chrome/common/modal-parent/utils/type.ts";
import { MODAL_TERMINAL_REASONS } from "../../chrome/common/modal-parent/utils/type.ts";

type IntervalHandle = number | ReturnType<typeof globalThis.setInterval>;

interface PendingActorRequest extends ModalRequestIdentity {
  win: Window;
  form: ModalShowRequest["form"];
  resolve(response: ModalResultEnvelope): void;
  settled: boolean;
  rendered: boolean;
  readyInterval: IntervalHandle | null;
  messageHandler: (event: MessageEvent) => void;
}

interface ModalPingResponse extends ModalRequestIdentity {
  ready: boolean;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function isRequestIdentity(
  value: unknown,
): value is ModalRequestIdentity & Record<string, unknown> {
  return isRecord(value) &&
    typeof value.requestId === "string" &&
    value.requestId.length > 0 &&
    Number.isSafeInteger(value.epoch) &&
    (value.epoch as number) > 0;
}

function isTerminalReason(value: unknown): value is ModalTerminalReason {
  return typeof value === "string" &&
    (MODAL_TERMINAL_REASONS as readonly string[]).includes(value);
}

function isShowRequest(value: unknown): value is ModalShowRequest {
  if (!isRequestIdentity(value) || !isRecord(value.form)) {
    return false;
  }
  return Array.isArray(value.form.forms) &&
    typeof value.form.title === "string";
}

function isCancelRequest(value: unknown): value is ModalCancelRequest {
  return isRequestIdentity(value) && isTerminalReason(value.reason);
}

function matchesRequest(
  request: ModalRequestIdentity,
  value: unknown,
): value is ModalRequestIdentity & Record<string, unknown> {
  return isRequestIdentity(value) &&
    request.requestId === value.requestId &&
    request.epoch === value.epoch;
}

function isSubmitMessage(
  request: ModalRequestIdentity,
  value: unknown,
): value is ModalResultEnvelope & { type: "nora-modal-submit" } {
  if (
    !matchesRequest(request, value) ||
    value.type !== "nora-modal-submit" ||
    (value.reason !== "submit" && value.reason !== "cancel")
  ) {
    return false;
  }

  if (value.reason === "cancel") {
    return value.result === null;
  }

  return isRecord(value.result) &&
    Object.values(value.result).every((entry) =>
      typeof entry === "string" || typeof entry === "number"
    );
}

function pageIsReady(win: Window): boolean {
  const doc = win.document as Document & { documentElement: HTMLElement };
  return doc?.documentElement?.dataset?.noraModalReady === "true";
}

export class ModalChildRequestController {
  private activeRequest: PendingActorRequest | null = null;

  public show(
    win: Window,
    value: unknown,
  ): Promise<ModalResultEnvelope | null> {
    if (!isShowRequest(value)) {
      return Promise.resolve(null);
    }

    const replaced = this.activeRequest;
    if (replaced) {
      this.settleOnce(replaced, "replacement", null);
    }

    let resolveRequest!: (response: ModalResultEnvelope) => void;
    const response = new Promise<ModalResultEnvelope>((resolve) => {
      resolveRequest = resolve;
    });

    const request: PendingActorRequest = {
      requestId: value.requestId,
      epoch: value.epoch,
      form: value.form,
      win,
      resolve: resolveRequest,
      settled: false,
      rendered: false,
      readyInterval: null,
      messageHandler: () => {},
    };

    request.messageHandler = (event: MessageEvent) => {
      if (!request.rendered || !isSubmitMessage(request, event.data)) {
        return;
      }
      this.settleOnce(request, event.data.reason, event.data.result);
    };

    // Register identity and terminal ingress before observing page readiness.
    this.activeRequest = request;
    win.addEventListener("message", request.messageHandler);

    if (pageIsReady(win)) {
      this.renderContent(request);
    } else {
      this.waitForReady(request);
    }

    return response;
  }

  public cancel(value: unknown): boolean {
    if (!isCancelRequest(value)) {
      return false;
    }
    const request = this.activeRequest;
    if (!request || !matchesRequest(request, value)) {
      return false;
    }
    return this.settleOnce(request, value.reason, null);
  }

  public ping(value: unknown): ModalPingResponse | null {
    if (!isRequestIdentity(value)) {
      return null;
    }
    const request = this.activeRequest;
    return {
      requestId: value.requestId,
      epoch: value.epoch,
      ready: request !== null &&
        matchesRequest(request, value) &&
        request.rendered &&
        pageIsReady(request.win),
    };
  }

  public destroy(): void {
    const request = this.activeRequest;
    if (request) {
      this.settleOnce(request, "dead", null);
    }
  }

  public getActiveRequestIdentity(): ModalRequestIdentity | null {
    return this.activeRequest
      ? {
        requestId: this.activeRequest.requestId,
        epoch: this.activeRequest.epoch,
      }
      : null;
  }

  private waitForReady(request: PendingActorRequest): void {
    let count = 0;
    request.readyInterval = request.win.setInterval(() => {
      if (!this.isActive(request)) {
        return;
      }
      if (pageIsReady(request.win)) {
        this.clearReadyInterval(request);
        this.renderContent(request);
        return;
      }
      if (count++ >= 50) {
        this.settleOnce(request, "timeout", null);
      }
    }, 100);
  }

  private renderContent(request: PendingActorRequest): void {
    if (!this.isActive(request)) {
      return;
    }
    request.rendered = true;
    try {
      request.win.postMessage({
        type: "nora-modal-init",
        requestId: request.requestId,
        epoch: request.epoch,
        form: request.form,
      }, "*");
    } catch {
      this.settleOnce(request, "actor-error", null);
    }
  }

  private settleOnce(
    request: PendingActorRequest,
    reason: ModalTerminalReason,
    result: TFormResult | null,
  ): boolean {
    if (!this.isActive(request)) {
      return false;
    }

    request.settled = true;
    this.activeRequest = null;
    this.clearReadyInterval(request);
    request.win.removeEventListener("message", request.messageHandler);

    try {
      request.win.postMessage({
        type: "nora-modal-remove",
        requestId: request.requestId,
        epoch: request.epoch,
        reason,
        result: null,
      }, "*");
    } catch {
      // A dead WindowGlobal cannot receive cleanup, but the query still settles.
    }

    request.resolve({
      requestId: request.requestId,
      epoch: request.epoch,
      reason,
      result,
    });
    return true;
  }

  private isActive(request: PendingActorRequest): boolean {
    return !request.settled && this.activeRequest === request &&
      this.activeRequest.requestId === request.requestId &&
      this.activeRequest.epoch === request.epoch;
  }

  private clearReadyInterval(request: PendingActorRequest): void {
    if (request.readyInterval === null) {
      return;
    }
    request.win.clearInterval(request.readyInterval);
    request.readyInterval = null;
  }
}

export class NRChromeModalChild extends JSWindowActorChild {
  private readonly requests = new ModalChildRequestController();

  receiveMessage(message: ReceiveMessageArgument) {
    const win = this.contentWindow as Window;
    switch (message.name) {
      case "NRChromeModal:show":
        return this.requests.show(win, message.data);
      case "NRChromeModal:cancel":
        return this.requests.cancel(message.data);
      case "NRChromeModal:ping":
        return this.requests.ping(message.data);
      default:
        return null;
    }
  }

  didDestroy(): void {
    this.requests.destroy();
  }

  handleEvent(_event: Event): void {
    // No-op
  }
}
