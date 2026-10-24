// SPDX-License-Identifier: MPL-2.0
export type WorkerScriptType = "classic" | "module";

export type WorkerMessageMode =
  | "promise"
  | "reject"
  | "sync"
  | "async"
  | "none"
  | "multiple"
  | "late"
  | "object";

export interface WorkerMessageResult {
  id: number;
  outcome: "resolved" | "rejected" | "timeout";
  undefinedResult?: boolean;
  value?: unknown;
  name?: string;
  message?: string;
}

export interface WorkerMessageEvidence {
  scriptType: WorkerScriptType;
  mode: WorkerMessageMode;
  result: WorkerMessageResult;
}
