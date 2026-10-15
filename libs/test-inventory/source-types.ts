// SPDX-License-Identifier: MPL-2.0

export interface TestSourceManifest {
  schemaVersion: 1;
  scope: "vite-test-source-input";
  runId: string;
  expiresAtMs: number;
  sources: Array<{ id: string; sha256: string }>;
}

export interface TestSourceContext {
  schemaVersion: 1;
  runId: string;
  manifestSha256: string;
}

export interface TestSourceProof extends TestSourceContext {
  id: string;
  sourceSha256: string;
}

export interface SourceBuffer {
  bytes: Uint8Array;
  sha256: string;
}

export interface SourceFileStat {
  isFile(): boolean;
  isSymbolicLink(): boolean;
  dev: number;
  ino: number;
  size: number;
  mtimeMs: number;
  ctimeMs: number;
}

export interface SourceFileHandle {
  stat(): Promise<SourceFileStat>;
  read(buffer: Uint8Array): Promise<{ bytesRead: number }>;
  close(): Promise<void>;
}

export interface SourceFileOperations {
  lstat(filename: string): Promise<SourceFileStat>;
  open(filename: string, flags: number): Promise<SourceFileHandle>;
}
