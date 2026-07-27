export interface TFormItem {
  type:
    | "text"
    | "number"
    | "textarea"
    | "select"
    | "dropdown"
    | "checkbox"
    | "radio"
    | "url";
  id: string;
  label?: string;
  value?: string | number;
  required?: boolean;
  classList?: string;
  placeholder?: string;
  rows?: number;
  maxLength?: number;
  options?: Array<{
    label: string;
    value: string;
    icon?: string;
  }>;
  when?: {
    id: string;
    value: string | string[];
  };
  onInput?: (value: string) => string;
}

export interface TForm {
  forms: TFormItem[];
  title: string;
  submitLabel?: string;
  cancelLabel?: string;
}

export interface TFormResult {
  [key: string]: string | number;
}

export const MODAL_TERMINAL_REASONS = [
  "submit",
  "cancel",
  "escape",
  "backdrop",
  "hide",
  "replacement",
  "timeout",
  "dead",
  "actor-error",
  "remove",
] as const;

export type ModalTerminalReason = typeof MODAL_TERMINAL_REASONS[number];

export interface ModalRequestIdentity {
  requestId: string;
  epoch: number;
}

export interface ModalShowRequest extends ModalRequestIdentity {
  form: TForm;
}

export interface ModalCancelRequest extends ModalRequestIdentity {
  reason: ModalTerminalReason;
}

export interface ModalResultEnvelope extends ModalRequestIdentity {
  reason: ModalTerminalReason;
  result: TFormResult | null;
}
