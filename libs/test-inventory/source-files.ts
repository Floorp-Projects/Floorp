// SPDX-License-Identifier: MPL-2.0

import { constants } from "node:fs";
import { lstat, open } from "node:fs/promises";
import { createHash } from "node:crypto";
import type {
  SourceBuffer,
  SourceFileOperations,
  SourceFileStat,
} from "./source-types.ts";

export const MAX_SOURCE_BYTES = 2 * 1024 * 1024;
export const MAX_SOURCE_TOTAL_BYTES = 32 * 1024 * 1024;

export function sourceDigest(bytes: Uint8Array): string {
  return createHash("sha256").update(bytes).digest("hex");
}

function requireFileIdentity(stat: SourceFileStat): void {
  if (
    !Number.isSafeInteger(stat.dev) || stat.dev <= 0 ||
    !Number.isSafeInteger(stat.ino) || stat.ino <= 0
  ) {
    throw new Error("Source filesystem does not provide a safe file identity");
  }
}

/** The digest and caller's code come from this single bounded descriptor read. */
export async function readSourceBuffer(
  filename: string,
  limit = MAX_SOURCE_BYTES,
  operations: SourceFileOperations = { lstat, open },
): Promise<SourceBuffer> {
  if (
    !Number.isSafeInteger(limit) || limit < 1 || limit > MAX_SOURCE_TOTAL_BYTES
  ) {
    throw new Error("Invalid source read bound");
  }
  if (
    !Number.isSafeInteger(constants.O_NONBLOCK) || constants.O_NONBLOCK <= 0
  ) {
    throw new Error("Nonblocking source open is unsupported on this platform");
  }
  const beforePath = await operations.lstat(filename);
  if (!beforePath.isFile() || beforePath.isSymbolicLink()) {
    throw new Error("Test source must be an ordinary file");
  }
  requireFileIdentity(beforePath);
  // NONBLOCK prevents a POSIX FIFO substituted between lstat/open from hanging.
  // NOFOLLOW is requested as defense in depth, not trusted: Deno 2.9.6 ignores it.
  // The pre-read FD identity check is mandatory even when this flag is present.
  const flags = constants.O_RDONLY | constants.O_NONBLOCK |
    (constants.O_NOFOLLOW ?? 0);
  const file = await operations.open(filename, flags);
  let failed = false;
  let failure: unknown;
  let result: SourceBuffer | undefined;
  try {
    const before = await file.stat();
    requireFileIdentity(before);
    if (!before.isFile() || before.size > limit) {
      throw new Error("Invalid or oversized test source");
    }
    if (before.dev !== beforePath.dev || before.ino !== beforePath.ino) {
      throw new Error("Test source changed while opening");
    }
    const chunks: Uint8Array[] = [];
    let size = 0;
    while (true) {
      const buffer = new Uint8Array(Math.min(64 * 1024, limit + 1 - size));
      const { bytesRead } = await file.read(buffer);
      if (
        !Number.isSafeInteger(bytesRead) || bytesRead < 0 ||
        bytesRead > buffer.length
      ) {
        throw new Error("Invalid source read count");
      }
      if (bytesRead === 0) break;
      size += bytesRead;
      if (size > limit) throw new Error("Test source read bound exceeded");
      chunks.push(buffer.subarray(0, bytesRead));
    }
    const after = await file.stat();
    const afterPath = await operations.lstat(filename);
    if (
      !afterPath.isFile() || afterPath.isSymbolicLink() ||
      before.dev !== after.dev || before.ino !== after.ino ||
      after.dev !== afterPath.dev || after.ino !== afterPath.ino ||
      before.size !== after.size || after.size !== size ||
      before.mtimeMs !== after.mtimeMs || before.ctimeMs !== after.ctimeMs
    ) throw new Error("Test source changed during the bounded read");
    const bytes = new Uint8Array(size);
    let offset = 0;
    for (const chunk of chunks) {
      bytes.set(chunk, offset);
      offset += chunk.length;
    }
    result = { bytes, sha256: sourceDigest(bytes) };
  } catch (error) {
    failed = true;
    failure = error;
  } finally {
    try {
      await file.close();
    } catch (error) {
      if (!failed) {
        failed = true;
        failure = error;
      }
    }
  }
  if (failed) throw failure;
  if (!result) throw new Error("Source read did not produce a buffer");
  return result;
}
