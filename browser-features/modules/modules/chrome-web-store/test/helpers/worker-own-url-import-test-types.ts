// SPDX-License-Identifier: MPL-2.0
export type WorkerOwnImportKind = "classic" | "module";
export type WorkerOwnImportMode = "relative" | "public" | "mixed";
export interface WorkerOwnImportAddon {
  uninstall(): Promise<void>;
}
export interface WorkerOwnImportStartup {
  ok?: boolean;
  value?: string;
  sameNamespace?: boolean;
  evaluations?: number;
  deniedImports?: {
    url?: string;
    denied?: boolean;
    error?: string;
    name?: string;
  }[];
  foreignPrivateExecuted?: boolean;
}
export interface WorkerOwnImportReply {
  token?: string;
  phase?: string;
  location?: string;
  error?: string;
  stack?: string;
  resource?: {
    url?: string;
    status?: number;
    mime?: string;
    body?: string;
    error?: string;
  };
  value?: {
    startup?: WorkerOwnImportStartup;
    id?: string;
    observedURL?: string;
    location?: string;
  };
}
export interface WorkerOwnImportRow {
  kind: WorkerOwnImportKind;
  mode: WorkerOwnImportMode;
  id: string;
  parentProtocolBefore: object;
  parentProtocolAfter?: object;
  context?: string;
  actualNativeBase?: string;
  partialTitle?: unknown;
  caseError?: string;
  reply?: WorkerOwnImportReply;
  responseObserved?: boolean;
  consoleErrors?: object[];
}
export interface WorkerOwnImportReport {
  scope: string;
  qualified: false;
  parentProtocol: object;
  rows: WorkerOwnImportRow[];
  observationCompleted?: boolean;
}
