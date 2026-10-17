// SPDX-License-Identifier: MPL-2.0

import { browserDiscoverable, canonicalTestId } from "./contract.ts";
import type {
  TestSourceContext,
  TestSourceManifest,
  TestSourceProof,
} from "./source-types.ts";

export function readTestSourceContext(raw: unknown): TestSourceContext {
  if (typeof raw !== "object" || raw === null) {
    throw new Error("Missing or invalid test source driver/context");
  }
  const value = raw as Partial<TestSourceContext> | null;
  if (
    !value || value.schemaVersion !== 1 || typeof value.runId !== "string" ||
    !/^[a-zA-Z0-9_-]{1,128}$/.test(value.runId) ||
    typeof value.manifestSha256 !== "string" ||
    !/^[a-f0-9]{64}$/.test(value.manifestSha256)
  ) {
    throw new Error("Missing or invalid test source driver/context");
  }
  return {
    schemaVersion: 1,
    runId: value.runId,
    manifestSha256: value.manifestSha256,
  };
}

export function readTestSourceProof(
  raw: unknown,
  id: string,
  expected: TestSourceContext,
): TestSourceProof {
  const context = readTestSourceContext(raw);
  const value = raw as Partial<TestSourceProof>;
  if (
    context.runId !== expected.runId ||
    context.manifestSha256 !== expected.manifestSha256 ||
    value.id !== id || typeof value.sourceSha256 !== "string" ||
    !/^[a-f0-9]{64}$/.test(value.sourceSha256)
  ) {
    throw new Error("Test source proof differs from the original run");
  }
  return { ...context, id, sourceSha256: value.sourceSha256 };
}

export const TEST_SOURCE_EXPORT = "__NORA_TEST_SOURCE_PROOF__";
export const TEST_SOURCE_DRIVER = "__NORA_TEST_SOURCE_DRIVER__";

export function testSourceManifestFilename(runId: string): string {
  if (!/^[a-zA-Z0-9_-]{1,128}$/.test(runId)) {
    throw new Error("Invalid test source run ID");
  }
  return `nora-tests-source-${runId}.json`;
}

export function parseTestSourceManifest(
  raw: unknown,
  now = Date.now(),
): TestSourceManifest {
  if (typeof raw !== "object" || raw === null) {
    throw new Error("Missing test source manifest");
  }
  const value = raw as Record<string, unknown>;
  if (
    value.schemaVersion !== 1 || value.scope !== "vite-test-source-input" ||
    typeof value.runId !== "string" ||
    !/^[a-zA-Z0-9_-]{1,128}$/.test(value.runId) ||
    typeof value.expiresAtMs !== "number" ||
    !Number.isSafeInteger(value.expiresAtMs) || value.expiresAtMs <= now ||
    value.expiresAtMs > now + 1_800_000 ||
    !Array.isArray(value.sources) || value.sources.length < 1 ||
    value.sources.length > 4096
  ) throw new Error("Invalid or expired test source manifest");
  const seen = new Set<string>();
  const sources = value.sources.map((row) => {
    if (typeof row !== "object" || row === null) {
      throw new Error("Invalid test source manifest entry");
    }
    const entry = row as Record<string, unknown>;
    if (
      typeof entry.id !== "string" || canonicalTestId(entry.id) !== entry.id ||
      !browserDiscoverable(entry.id) || seen.has(entry.id) ||
      typeof entry.sha256 !== "string" || !/^[a-f0-9]{64}$/.test(entry.sha256)
    ) throw new Error("Duplicate or invalid test source manifest entry");
    seen.add(entry.id);
    return { id: entry.id, sha256: entry.sha256 };
  });
  return {
    schemaVersion: 1,
    scope: "vite-test-source-input",
    runId: value.runId,
    expiresAtMs: value.expiresAtMs,
    sources,
  };
}
