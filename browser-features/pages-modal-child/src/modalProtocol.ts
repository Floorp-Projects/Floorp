import type {
  ModalRequestIdentity,
  ModalTerminalReason,
  TForm,
  TFormResult,
} from "../../chrome/common/modal-parent/utils/type.ts";
import { MODAL_TERMINAL_REASONS } from "../../chrome/common/modal-parent/utils/type.ts";

export interface NoraModalInitMessage extends ModalRequestIdentity {
  type: "nora-modal-init";
  form: TForm;
}

export interface NoraModalSubmitMessage extends ModalRequestIdentity {
  type: "nora-modal-submit";
  reason: "submit" | "cancel";
  result: TFormResult | null;
}

export interface NoraModalRemoveMessage extends ModalRequestIdentity {
  type: "nora-modal-remove";
  reason: ModalTerminalReason;
  result: null;
}

export interface ModalPageState extends ModalRequestIdentity {
  form: TForm;
}

export type NoraModalMessage =
  | NoraModalInitMessage
  | NoraModalSubmitMessage
  | NoraModalRemoveMessage;

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

export function isModalRequestIdentity(
  value: unknown,
): value is ModalRequestIdentity & Record<string, unknown> {
  if (!isRecord(value)) {
    return false;
  }

  return typeof value.requestId === "string" &&
    value.requestId.length > 0 &&
    Number.isSafeInteger(value.epoch) &&
    (value.epoch as number) > 0;
}

export function isModalTerminalReason(
  value: unknown,
): value is ModalTerminalReason {
  return typeof value === "string" &&
    (MODAL_TERMINAL_REASONS as readonly string[]).includes(value);
}

function isForm(value: unknown): value is TForm {
  return isRecord(value) &&
    Array.isArray(value.forms) &&
    typeof value.title === "string";
}

function isFormResult(value: unknown): value is TFormResult {
  if (!isRecord(value)) {
    return false;
  }

  return Object.values(value).every((entry) =>
    typeof entry === "string" || typeof entry === "number"
  );
}

export function isNoraModalInitMessage(
  value: unknown,
): value is NoraModalInitMessage {
  return isModalRequestIdentity(value) &&
    value.type === "nora-modal-init" &&
    isForm(value.form);
}

export function isNoraModalSubmitMessage(
  value: unknown,
): value is NoraModalSubmitMessage {
  if (
    !isModalRequestIdentity(value) ||
    value.type !== "nora-modal-submit" ||
    (value.reason !== "submit" && value.reason !== "cancel")
  ) {
    return false;
  }

  return value.reason === "submit"
    ? isFormResult(value.result)
    : value.result === null;
}

export function isNoraModalRemoveMessage(
  value: unknown,
): value is NoraModalRemoveMessage {
  return isModalRequestIdentity(value) &&
    value.type === "nora-modal-remove" &&
    isModalTerminalReason(value.reason) &&
    value.result === null;
}

export function matchesModalRequest(
  current: ModalRequestIdentity | null,
  incoming: ModalRequestIdentity,
): boolean {
  return current !== null &&
    current.requestId === incoming.requestId &&
    current.epoch === incoming.epoch;
}

export function shouldAcceptModalInit(
  current: ModalRequestIdentity | null,
  incoming: ModalRequestIdentity,
): boolean {
  if (current === null) {
    return true;
  }

  return incoming.epoch > current.epoch ||
    matchesModalRequest(current, incoming);
}

export function applyModalInit(
  current: ModalPageState | null,
  incoming: unknown,
): ModalPageState | null {
  if (
    !isNoraModalInitMessage(incoming) ||
    !shouldAcceptModalInit(current, incoming)
  ) {
    return current;
  }

  return {
    requestId: incoming.requestId,
    epoch: incoming.epoch,
    form: incoming.form,
  };
}

export function applyModalRemove(
  current: ModalPageState | null,
  incoming: unknown,
): ModalPageState | null {
  if (
    !isNoraModalRemoveMessage(incoming) ||
    !matchesModalRequest(current, incoming)
  ) {
    return current;
  }

  return null;
}

export function createModalSubmitMessage(
  identity: ModalRequestIdentity,
  result: TFormResult,
): NoraModalSubmitMessage {
  return {
    type: "nora-modal-submit",
    requestId: identity.requestId,
    epoch: identity.epoch,
    reason: "submit",
    result,
  };
}

export function createModalCancelMessage(
  identity: ModalRequestIdentity,
): NoraModalSubmitMessage {
  return {
    type: "nora-modal-submit",
    requestId: identity.requestId,
    epoch: identity.epoch,
    reason: "cancel",
    result: null,
  };
}

export function createModalRemoveMessage(
  identity: ModalRequestIdentity,
  reason: ModalTerminalReason,
): NoraModalRemoveMessage {
  return {
    type: "nora-modal-remove",
    requestId: identity.requestId,
    epoch: identity.epoch,
    reason,
    result: null,
  };
}
