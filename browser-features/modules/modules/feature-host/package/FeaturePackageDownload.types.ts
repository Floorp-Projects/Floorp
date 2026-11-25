// SPDX-License-Identifier: MPL-2.0

import type {
  FeaturePackageErrorCode,
  FeaturePackagePolicy,
  VerifiedFeaturePackage,
} from "./types.ts";

export interface FeaturePackageDownloadRequest {
  readonly origin: string;
  readonly packageSha256: string;
  readonly policy: FeaturePackagePolicy;
  readonly signal?: AbortSignal;
}

export type FeaturePackageDownloadErrorCode =
  | "invalid-request"
  | "invalid-endpoint"
  | "invalid-policy"
  | "busy"
  | "aborted"
  | "timeout"
  | "network-failure"
  | "unacceptable-response"
  | "size-limit"
  | "package-rejected"
  | "expected-package-mismatch"
  | "unsupported-platform"
  | "cleanup-unproven";

export interface FeaturePackageDownloadDiagnostics {
  readonly inFlight: number;
  readonly reading: number;
  readonly verifying: number;
  readonly cleanupPending: number;
  readonly blocked: number;
}

export interface FeaturePackageDownloaderPlatform {
  fetch(url: string, options: RequestInit): Promise<Response>;
  now(): number;
  setTimeout(callback: () => void, delayMs: number): number;
  clearTimeout(handle: number): void;
}

export type FeaturePackageDownloadVerifierCode = FeaturePackageErrorCode;

export interface PreparedRequest {
  readonly url: string;
  readonly packageSha256: string;
  readonly policy: FeaturePackagePolicy;
  readonly callerSignal?: AbortSignal;
}

export interface Settled<T> {
  readonly fulfilled: boolean;
  readonly value?: T;
  readonly reason?: unknown;
}

export interface CancelResult {
  readonly fulfilled: boolean;
  readonly reason?: unknown;
}

export interface DownloadContext {
  readonly resolve: (bytes: Uint8Array) => void;
  readonly reject: (error: Error) => void;
  readonly promise: Promise<Uint8Array>;
  prepared?: PreparedRequest;
  controller?: AbortController;
  dependentSignal?: AbortSignal;
  signalListener?: EventListener;
  listenerInstalled: boolean;
  timerId: number | null;
  startTime: number;
  deadline: number;
  lastTime: number;
  terminalError: Error | null;
  publicSettled: boolean;
  cancelRequested: boolean;
  body: ReadableStream<Uint8Array> | null;
  reader: ReadableStreamDefaultReader<Uint8Array> | null;
  readerClosed: Promise<Settled<void>> | null;
  readPromise: Promise<ReadableStreamReadResult<Uint8Array>> | null;
  readerDone: boolean;
  readerCancelStarted: boolean;
  bodyCancelStarted: boolean;
  cancelPromises: Promise<CancelResult>[];
  fetchPromise: Promise<Response> | null;
  verifyPromise: Promise<VerifiedFeaturePackage> | null;
  contentLength: number | null;
  cleanupFailure: boolean;
  cleanupPending: boolean;
  reading: boolean;
  verifying: boolean;
  blocked: boolean;
  released: boolean;
}
