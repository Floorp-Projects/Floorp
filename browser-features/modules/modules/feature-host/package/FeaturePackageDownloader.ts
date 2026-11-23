// SPDX-License-Identifier: MPL-2.0

import {
  FEATURE_PACKAGE_MAX_BYTES,
  FeaturePackageError,
  snapshotFeaturePackagePolicy,
  verifySignedFeaturePackage,
} from "./FeaturePackageVerifier.ts";
import type {
  DownloadContext,
  FeaturePackageDownloadDiagnostics,
  FeaturePackageDownloaderPlatform,
  FeaturePackageDownloadErrorCode,
  FeaturePackageDownloadRequest,
  FeaturePackageDownloadVerifierCode,
  PreparedRequest,
} from "./FeaturePackageDownload.types.ts";
import type { FeaturePackagePolicy, VerifiedFeaturePackage } from "./types.ts";

const MAX_IN_FLIGHT = 4;
const DEADLINE_MS = 20_000;
const MAX_EMPTY_READS = 16;
const ALLOWED_RESPONSE_TYPES = new Set(["basic", "cors"]);
const FIXED_ABORT_REASON = "feature package acquisition stopped";
const HASH_PATTERN = /^[0-9a-f]{64}$/;
const ALLOWED_REQUEST_KEYS = new Set([
  "origin",
  "packageSha256",
  "policy",
  "signal",
]);
const ERROR_MESSAGES: Record<FeaturePackageDownloadErrorCode, string> = {
  "invalid-request": "Package download request is invalid.",
  "invalid-endpoint": "Package download endpoint is invalid.",
  "invalid-policy": "Package download policy is invalid.",
  busy: "Package download capacity is busy.",
  aborted: "Package download was aborted.",
  timeout: "Package download deadline expired.",
  "network-failure": "Package download failed.",
  "unacceptable-response": "Package download response is unacceptable.",
  "size-limit": "Package download exceeds the allowed size.",
  "package-rejected": "Downloaded package was rejected.",
  "expected-package-mismatch": "Downloaded package identity does not match.",
  "unsupported-platform": "Required package download support is unavailable.",
  "cleanup-unproven": "Package download cleanup could not be established.",
};
const PREPARATION_ERROR_CODES = new Set(Object.keys(ERROR_MESSAGES));
const SAFE_VERIFIER_CODES = new Set<FeaturePackageDownloadVerifierCode>([
  "invalid-envelope",
  "invalid-policy",
  "invalid-metadata",
  "invalid-source",
  "invalid-signing-key",
  "unsupported-crypto",
  "unsupported-version",
  "untrusted-key",
  "invalid-signature",
  "source-digest-mismatch",
  "feature-mismatch",
  "host-incompatible",
  "runtime-incompatible",
  "capability-denied",
  "sequence-too-low",
  "size-limit",
]);

const nativeAbortSignalAny = globalThis.AbortSignal?.any;
const nativeAbortedGetter = globalThis.AbortSignal
  ? Object.getOwnPropertyDescriptor(AbortSignal.prototype, "aborted")?.get
  : undefined;
const nativeAddEventListener = globalThis.EventTarget?.prototype
  .addEventListener;
const nativeRemoveEventListener = globalThis.EventTarget?.prototype
  .removeEventListener;

export class FeaturePackageDownloadError extends Error {
  constructor(
    readonly code: FeaturePackageDownloadErrorCode,
    readonly verifierCode?: FeaturePackageDownloadVerifierCode,
  ) {
    super(ERROR_MESSAGES[code]);
    this.name = "FeaturePackageDownloadError";
  }
}

function makeContext(): DownloadContext {
  let resolve!: (bytes: Uint8Array) => void;
  let reject!: (error: Error) => void;
  const promise = new Promise<Uint8Array>((resolvePromise, rejectPromise) => {
    resolve = resolvePromise;
    reject = rejectPromise;
  });
  return {
    resolve,
    reject,
    promise,
    listenerInstalled: false,
    timerId: null,
    startTime: Number.NaN,
    deadline: Number.NaN,
    lastTime: Number.NaN,
    terminalError: null,
    publicSettled: false,
    cancelRequested: false,
    body: null,
    reader: null,
    readerClosed: null,
    readPromise: null,
    readerDone: false,
    readerCancelStarted: false,
    bodyCancelStarted: false,
    cancelPromises: [],
    fetchPromise: null,
    verifyPromise: null,
    contentLength: null,
    cleanupFailure: false,
    cleanupPending: false,
    reading: false,
    verifying: false,
    blocked: false,
    released: false,
  };
}

function downloadError(
  code: FeaturePackageDownloadErrorCode,
  verifierCode?: FeaturePackageDownloadVerifierCode,
): FeaturePackageDownloadError {
  return new FeaturePackageDownloadError(code, verifierCode);
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function snapshotRequest(input: unknown): PreparedRequest {
  let record: Record<string, unknown>;
  try {
    if (!isRecord(input)) throw downloadError("invalid-request");
    record = input;
  } catch {
    throw downloadError("invalid-request");
  }
  let ownKeys: PropertyKey[];
  try {
    ownKeys = Reflect.ownKeys(record);
  } catch {
    throw downloadError("invalid-request");
  }
  if (
    ownKeys.some((key) =>
      typeof key !== "string" || !ALLOWED_REQUEST_KEYS.has(key)
    )
  ) throw downloadError("invalid-request");
  try {
    if (
      !Object.hasOwn(record, "origin") ||
      !Object.hasOwn(record, "packageSha256") ||
      !Object.hasOwn(record, "policy")
    ) throw downloadError("invalid-request");
  } catch {
    throw downloadError("invalid-request");
  }

  let origin: unknown;
  let packageSha256: unknown;
  let policyInput: unknown;
  let callerSignal: unknown;
  try {
    origin = record.origin;
    packageSha256 = record.packageSha256;
    policyInput = record.policy;
    callerSignal = Object.hasOwn(record, "signal") ? record.signal : undefined;
  } catch {
    throw downloadError("invalid-request");
  }
  if (
    typeof origin !== "string" || typeof packageSha256 !== "string" ||
    !HASH_PATTERN.test(packageSha256)
  ) throw downloadError("invalid-request");

  let policy: FeaturePackagePolicy;
  try {
    policy = snapshotFeaturePackagePolicy(policyInput as FeaturePackagePolicy);
  } catch {
    throw downloadError("invalid-policy");
  }

  if (origin.length === 0 || origin.length > 2_048) {
    throw downloadError("invalid-endpoint");
  }
  let parsedOrigin: URL;
  try {
    parsedOrigin = new URL(origin);
  } catch {
    throw downloadError("invalid-endpoint");
  }
  if (
    parsedOrigin.protocol !== "https:" || parsedOrigin.origin !== origin ||
    parsedOrigin.username !== "" || parsedOrigin.password !== ""
  ) throw downloadError("invalid-endpoint");

  let callerAbortSignal: AbortSignal | undefined;
  if (callerSignal !== undefined) {
    if (typeof nativeAbortedGetter !== "function") {
      throw downloadError("unsupported-platform");
    }
    try {
      const aborted = Reflect.apply(
        nativeAbortedGetter,
        callerSignal,
        [],
      ) as unknown;
      if (typeof aborted !== "boolean") throw downloadError("invalid-request");
      callerAbortSignal = callerSignal as AbortSignal;
    } catch {
      throw downloadError("invalid-request");
    }
  }

  return {
    url:
      `${origin}/floorp-feature-packages/v1/${policy.featureId}/${packageSha256}.json`,
    packageSha256,
    policy,
    ...(callerAbortSignal ? { callerSignal: callerAbortSignal } : {}),
  };
}

function readSignalAborted(signal: AbortSignal): boolean | null {
  if (typeof nativeAbortedGetter !== "function") return null;
  try {
    const value = Reflect.apply(nativeAbortedGetter, signal, []) as unknown;
    return typeof value === "boolean" ? value : null;
  } catch {
    return null;
  }
}

function trimHttpOws(value: string): string {
  return value.replace(/^[\t ]+|[\t ]+$/g, "");
}

function contentTypeIsAllowed(value: string | null): boolean {
  if (typeof value !== "string" || value.length > 128) return false;
  return /^application\/json(?:[\t ]*;[\t ]*charset[\t ]*=[\t ]*(?:utf-8|"utf-8"))?$/i
    .test(value) || /^application\/octet-stream$/i.test(value);
}

function contentEncodingIsAllowed(value: string | null): boolean {
  return value === null || (value.length <= 32 &&
    trimHttpOws(value).toLowerCase() === "identity");
}

function parseContentLength(value: string | null): number | null | false {
  if (value === null) return null;
  if (value.length > 7 || !/^(?:0|[1-9][0-9]{0,6})$/.test(value)) return false;
  const length = Number(value);
  return length <= FEATURE_PACKAGE_MAX_BYTES ? length : false;
}

export class FeaturePackageDownloader {
  readonly #platform: FeaturePackageDownloaderPlatform;
  readonly #contexts = new Set<DownloadContext>();
  #reading = 0;
  #verifying = 0;
  #cleanupPending = 0;
  #blocked = 0;

  constructor(platform: FeaturePackageDownloaderPlatform) {
    this.#platform = platform;
  }

  get diagnosticsForTests(): FeaturePackageDownloadDiagnostics {
    return Object.freeze({
      inFlight: this.#contexts.size,
      reading: this.#reading,
      verifying: this.#verifying,
      cleanupPending: this.#cleanupPending,
      blocked: this.#blocked,
    });
  }

  downloadFeaturePackageBytes(
    request: FeaturePackageDownloadRequest,
  ): Promise<Uint8Array> {
    if (this.#contexts.size >= MAX_IN_FLIGHT) {
      return Promise.reject(downloadError("busy"));
    }
    const context = makeContext();
    this.#contexts.add(context);
    try {
      context.prepared = snapshotRequest(request);
      if (
        typeof nativeAbortSignalAny !== "function" ||
        typeof nativeAddEventListener !== "function" ||
        typeof nativeRemoveEventListener !== "function" ||
        typeof AbortController !== "function"
      ) throw downloadError("unsupported-platform");
      const startTime = this.#platform.now();
      if (!Number.isFinite(startTime)) {
        throw downloadError("unsupported-platform");
      }
      context.startTime = startTime;
      context.lastTime = startTime;
      context.deadline = startTime + DEADLINE_MS;
      if (!Number.isFinite(context.deadline)) {
        throw downloadError("unsupported-platform");
      }
      context.controller = new AbortController();
      try {
        context.dependentSignal = Reflect.apply(
          nativeAbortSignalAny,
          AbortSignal,
          [
            context.prepared.callerSignal
              ? [context.prepared.callerSignal]
              : [],
          ],
        ) as AbortSignal;
      } catch {
        throw downloadError("unsupported-platform");
      }
      context.signalListener = () => this.#terminate(context, "aborted");
      Reflect.apply(nativeAddEventListener, context.dependentSignal, [
        "abort",
        context.signalListener,
        { once: true },
      ]);
      context.listenerInstalled = true;
      this.#scheduleDeadline(context);
      const initiallyAborted = readSignalAborted(context.dependentSignal);
      if (initiallyAborted === null) {
        throw downloadError("unsupported-platform");
      }
      if (initiallyAborted) this.#terminate(context, "aborted");
    } catch (error: unknown) {
      this.#disposeMonitoring(context);
      const failure = context.cleanupFailure
        ? downloadError("cleanup-unproven")
        : this.#preparationError(error);
      if (context.cleanupFailure) this.#block(context);
      else this.#release(context);
      this.#settleReject(context, failure);
      return context.promise;
    }
    void this.#run(context).catch(() => {
      this.#disposeMonitoring(context);
      this.#requestCancellation(context);
      context.cleanupFailure = true;
      this.#block(context);
      if (!context.publicSettled) {
        this.#settleReject(context, downloadError("cleanup-unproven"));
      }
      void this.#awaitCancellations(context).catch(() => {});
    });
    return context.promise;
  }

  #preparationError(error: unknown): FeaturePackageDownloadError {
    try {
      if (error instanceof FeaturePackageDownloadError) {
        const descriptor = Object.getOwnPropertyDescriptor(error, "code");
        const code = descriptor?.value;
        if (
          typeof code === "string" && PREPARATION_ERROR_CODES.has(code)
        ) return downloadError(code as FeaturePackageDownloadErrorCode);
      }
    } catch {
      return downloadError("unsupported-platform");
    }
    return downloadError("unsupported-platform");
  }

  #readTime(context: DownloadContext): number | null {
    try {
      const now = this.#platform.now();
      if (!Number.isFinite(now) || now < context.lastTime) return null;
      context.lastTime = now;
      return now;
    } catch {
      return null;
    }
  }

  #scheduleDeadline(context: DownloadContext): void {
    const now = this.#readTime(context);
    if (now === null) throw downloadError("unsupported-platform");
    const remaining = context.deadline - now;
    if (remaining <= 0) {
      this.#terminate(context, "timeout");
      return;
    }
    try {
      context.timerId = this.#platform.setTimeout(
        () => this.#deadlineTimerFired(context),
        Math.max(1, Math.ceil(remaining)),
      );
    } catch {
      throw downloadError("unsupported-platform");
    }
  }

  #deadlineTimerFired(context: DownloadContext): void {
    context.timerId = null;
    if (context.terminalError) return;
    const now = this.#readTime(context);
    if (now === null) {
      this.#terminate(context, "unsupported-platform");
    } else if (now >= context.deadline) {
      this.#terminate(context, "timeout");
    } else {
      try {
        this.#scheduleDeadline(context);
      } catch {
        this.#terminate(context, "unsupported-platform");
      }
    }
  }

  #checkBoundary(context: DownloadContext): boolean {
    if (context.terminalError) return false;
    const now = this.#readTime(context);
    if (now === null) {
      this.#terminate(context, "unsupported-platform");
      return false;
    }
    if (now >= context.deadline) {
      this.#terminate(context, "timeout");
      return false;
    }
    const dependentSignal = context.dependentSignal;
    const aborted = dependentSignal ? readSignalAborted(dependentSignal) : null;
    if (aborted === null) {
      this.#terminate(context, "unsupported-platform");
      return false;
    }
    if (aborted) {
      this.#terminate(context, "aborted");
      return false;
    }
    return true;
  }

  #terminate(
    context: DownloadContext,
    requestedCode: FeaturePackageDownloadErrorCode,
    verifierCode?: FeaturePackageDownloadVerifierCode,
  ): void {
    if (context.terminalError) return;
    let code = requestedCode;
    let safeVerifierCode = verifierCode;
    const now = this.#readTime(context);
    if (now === null) {
      code = "unsupported-platform";
      safeVerifierCode = undefined;
    } else if (now >= context.deadline) {
      code = "timeout";
      safeVerifierCode = undefined;
    } else if (context.dependentSignal) {
      const aborted = readSignalAborted(context.dependentSignal);
      if (aborted === null) {
        code = "unsupported-platform";
        safeVerifierCode = undefined;
      } else if (aborted) {
        code = "aborted";
        safeVerifierCode = undefined;
      }
    }
    context.terminalError = downloadError(code, safeVerifierCode);
    this.#disposeMonitoring(context);
    this.#requestCancellation(context);
    this.#settleReject(context, context.terminalError);
  }

  #disposeMonitoring(context: DownloadContext): void {
    if (
      context.listenerInstalled && context.dependentSignal &&
      context.signalListener
    ) {
      try {
        Reflect.apply(nativeRemoveEventListener, context.dependentSignal, [
          "abort",
          context.signalListener,
        ]);
      } catch {
        context.cleanupFailure = true;
      }
      context.listenerInstalled = false;
    }
    if (context.timerId !== null) {
      try {
        this.#platform.clearTimeout(context.timerId);
      } catch {
        context.cleanupFailure = true;
      }
      context.timerId = null;
    }
  }

  #requestCancellation(context: DownloadContext): void {
    context.cancelRequested = true;
    if (context.reader && !context.readerDone) this.#startReaderCancel(context);
    else if (context.body && !context.reader) this.#startBodyCancel(context);
    if (context.controller) {
      try {
        if (!context.controller.signal.aborted) {
          context.controller.abort(FIXED_ABORT_REASON);
        }
      } catch {
        context.cleanupFailure = true;
      }
    }
  }

  #startReaderCancel(context: DownloadContext): void {
    if (!context.reader || context.readerDone || context.readerCancelStarted) {
      return;
    }
    context.readerCancelStarted = true;
    let cancellation: Promise<void>;
    try {
      cancellation = context.reader.cancel(FIXED_ABORT_REASON);
    } catch (reason: unknown) {
      context.cancelPromises.push(
        Promise.resolve({ fulfilled: false, reason }),
      );
      return;
    }
    context.cancelPromises.push(cancellation.then(
      () => ({ fulfilled: true }),
      (reason: unknown) => ({ fulfilled: false, reason }),
    ));
  }

  #startBodyCancel(context: DownloadContext): void {
    if (!context.body || context.bodyCancelStarted) return;
    context.bodyCancelStarted = true;
    let cancellation: Promise<void>;
    try {
      cancellation = context.body.cancel(FIXED_ABORT_REASON);
    } catch (reason: unknown) {
      context.cancelPromises.push(
        Promise.resolve({ fulfilled: false, reason }),
      );
      return;
    }
    context.cancelPromises.push(cancellation.then(
      () => ({ fulfilled: true }),
      (reason: unknown) => ({ fulfilled: false, reason }),
    ));
  }

  #settleReject(
    context: DownloadContext,
    error: Error,
  ): void {
    if (context.publicSettled) return;
    context.publicSettled = true;
    context.reject(error);
  }

  #release(context: DownloadContext): void {
    if (context.released || context.blocked) return;
    context.released = true;
    this.#contexts.delete(context);
  }

  #block(context: DownloadContext): void {
    if (context.blocked || context.released) return;
    context.blocked = true;
    this.#blocked++;
  }

  #attachResponseBody(context: DownloadContext, response: Response): void {
    try {
      context.body = response.body;
    } catch {
      context.cleanupFailure = true;
      return;
    }
    if (!context.body) return;
    try {
      const reader = context.body.getReader();
      context.reader = reader;
      const closed = reader.closed;
      context.readerClosed = closed.then(
        () => ({ fulfilled: true }),
        (reason: unknown) => ({ fulfilled: false, reason }),
      );
    } catch {
      context.cleanupFailure = true;
      if (context.reader) this.#startReaderCancel(context);
      else this.#startBodyCancel(context);
      return;
    }
    if (context.cancelRequested) this.#startReaderCancel(context);
  }

  #readResponseHeaders(
    context: DownloadContext,
    response: Response,
  ): number | null | false {
    const prepared = context.prepared;
    if (!prepared) return false;
    try {
      if (
        response.status !== 200 || response.url !== prepared.url ||
        response.redirected !== false ||
        !ALLOWED_RESPONSE_TYPES.has(response.type)
      ) return false;
      const headers = response.headers;
      if (!contentTypeIsAllowed(headers.get("content-type"))) return false;
      if (!contentEncodingIsAllowed(headers.get("content-encoding"))) {
        return false;
      }
      return parseContentLength(headers.get("content-length"));
    } catch {
      return false;
    }
  }

  async #readBody(context: DownloadContext): Promise<Uint8Array | null> {
    const reader = context.reader;
    if (!reader) return null;
    const storage = new Uint8Array(FEATURE_PACKAGE_MAX_BYTES);
    let length = 0;
    let emptyReads = 0;
    for (;;) {
      if (!this.#checkBoundary(context)) return null;
      let pendingRead: Promise<ReadableStreamReadResult<Uint8Array>>;
      try {
        pendingRead = reader.read();
        context.readPromise = pendingRead;
      } catch {
        this.#terminate(context, "network-failure");
        return null;
      }
      let item: ReadableStreamReadResult<Uint8Array>;
      try {
        item = await pendingRead;
      } catch {
        context.readPromise = null;
        this.#terminate(context, "network-failure");
        return null;
      }
      context.readPromise = null;
      if (!this.#checkBoundary(context)) return null;
      if (item.done) {
        context.readerDone = true;
        break;
      }
      const chunk = item.value;
      if (!(chunk instanceof Uint8Array)) {
        this.#terminate(context, "unacceptable-response");
        return null;
      }
      const chunkLength = chunk.byteLength;
      if (!Number.isSafeInteger(chunkLength) || chunkLength < 0) {
        this.#terminate(context, "unacceptable-response");
        return null;
      }
      if (chunkLength === 0) {
        emptyReads++;
        if (emptyReads > MAX_EMPTY_READS) {
          this.#terminate(context, "unacceptable-response");
          return null;
        }
        continue;
      }
      emptyReads = 0;
      if (length + chunkLength > FEATURE_PACKAGE_MAX_BYTES) {
        this.#terminate(context, "size-limit");
        return null;
      }
      storage.set(chunk, length);
      length += chunkLength;
    }
    if (context.contentLength !== null && context.contentLength !== length) {
      this.#terminate(context, "unacceptable-response");
      return null;
    }
    return storage.slice(0, length);
  }

  #verifierFailure(error: unknown): FeaturePackageDownloadError {
    if (error instanceof FeaturePackageError) {
      if (error.code === "unsupported-crypto") {
        return downloadError("unsupported-platform");
      }
      if (SAFE_VERIFIER_CODES.has(error.code)) {
        return downloadError("package-rejected", error.code);
      }
    }
    return downloadError("package-rejected");
  }

  async #run(context: DownloadContext): Promise<void> {
    let successfulBytes: Uint8Array | null = null;
    try {
      if (!this.#checkBoundary(context)) return;
      const prepared = context.prepared;
      const controller = context.controller;
      if (!prepared || !controller) {
        this.#terminate(context, "unsupported-platform");
        return;
      }
      let fetchPromise: Promise<Response>;
      try {
        fetchPromise = this.#platform.fetch(prepared.url, {
          method: "GET",
          credentials: "omit",
          redirect: "error",
          referrerPolicy: "no-referrer",
          cache: "no-store",
          signal: controller.signal,
        });
        context.fetchPromise = fetchPromise;
      } catch {
        this.#terminate(context, "network-failure");
        return;
      }
      let response: Response;
      try {
        response = await fetchPromise;
      } catch {
        context.fetchPromise = null;
        if (!context.terminalError) this.#terminate(context, "network-failure");
        return;
      }
      context.fetchPromise = null;
      this.#attachResponseBody(context, response);
      if (context.terminalError) {
        this.#requestCancellation(context);
        return;
      }
      if (!this.#checkBoundary(context)) return;
      const contentLength = this.#readResponseHeaders(context, response);
      if (contentLength === false) {
        this.#terminate(context, "unacceptable-response");
        return;
      }
      context.contentLength = contentLength;
      if (!context.reader) {
        this.#terminate(context, "unacceptable-response");
        return;
      }
      if (!this.#checkBoundary(context)) return;
      context.reading = true;
      this.#reading++;
      let packageBytes: Uint8Array | null;
      try {
        packageBytes = await this.#readBody(context);
      } finally {
        context.reading = false;
        this.#reading--;
      }
      if (
        !packageBytes || context.terminalError || !this.#checkBoundary(context)
      ) return;
      context.verifying = true;
      this.#verifying++;
      let verified: VerifiedFeaturePackage;
      try {
        const verification = verifySignedFeaturePackage(
          packageBytes,
          prepared.policy,
        );
        context.verifyPromise = verification;
        verified = await verification;
      } catch (error: unknown) {
        const failure = this.#verifierFailure(error);
        this.#terminate(context, failure.code, failure.verifierCode);
        return;
      } finally {
        context.verifyPromise = null;
        context.verifying = false;
        this.#verifying--;
      }
      if (!this.#checkBoundary(context)) return;
      if (verified.packageSha256 !== prepared.packageSha256) {
        this.#terminate(context, "expected-package-mismatch");
        return;
      }
      successfulBytes = packageBytes.slice();
    } catch {
      if (!context.terminalError) this.#terminate(context, "network-failure");
    } finally {
      await this.#finish(context, successfulBytes);
    }
  }

  async #awaitCancellations(context: DownloadContext): Promise<void> {
    let awaited = 0;
    while (awaited < context.cancelPromises.length) {
      const pending = context.cancelPromises.slice(awaited);
      awaited = context.cancelPromises.length;
      const outcomes = await Promise.all(pending);
      const closed = context.readerClosed ? await context.readerClosed : null;
      for (const outcome of outcomes) {
        if (outcome.fulfilled) continue;
        const sameClosedError = closed && !closed.fulfilled &&
          Object.is(outcome.reason, closed.reason);
        if (!sameClosedError) context.cleanupFailure = true;
      }
    }
  }

  async #finish(
    context: DownloadContext,
    successfulBytes: Uint8Array | null,
  ): Promise<void> {
    context.cleanupPending = true;
    this.#cleanupPending++;
    try {
      if (context.reader && !context.readerDone && context.cancelRequested) {
        this.#startReaderCancel(context);
      }
      if (context.body && !context.reader && context.cancelRequested) {
        this.#startBodyCancel(context);
      }
      if (context.readPromise) {
        await context.readPromise;
      }
      await this.#awaitCancellations(context);
      if (context.readerClosed) await context.readerClosed;
      if (context.reader) {
        try {
          context.reader.releaseLock();
        } catch {
          context.cleanupFailure = true;
        }
      }
      if (context.cancelPromises.length > 0) {
        await this.#awaitCancellations(context);
      }
    } catch {
      context.cleanupFailure = true;
    } finally {
      context.cleanupPending = false;
      this.#cleanupPending--;
    }

    if (context.cleanupFailure) {
      this.#disposeMonitoring(context);
      this.#block(context);
      if (!context.publicSettled) {
        this.#settleReject(context, downloadError("cleanup-unproven"));
      }
      return;
    }
    if (context.terminalError) {
      this.#release(context);
      return;
    }
    if (!successfulBytes) {
      this.#terminate(context, "network-failure");
      if (context.cleanupFailure) this.#block(context);
      else this.#release(context);
      return;
    }
    if (!this.#checkBoundary(context)) {
      if (context.cleanupFailure) this.#block(context);
      else this.#release(context);
      return;
    }
    this.#disposeMonitoring(context);
    if (context.cleanupFailure) {
      this.#block(context);
      this.#settleReject(context, downloadError("cleanup-unproven"));
      return;
    }
    if (!this.#checkBoundary(context)) {
      this.#release(context);
      return;
    }
    this.#release(context);
    if (!context.publicSettled) {
      context.publicSettled = true;
      context.resolve(successfulBytes.slice());
    }
  }
}
