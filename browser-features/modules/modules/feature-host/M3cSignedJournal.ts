// SPDX-License-Identifier: MPL-2.0

import type { M0IdentityCatalog } from "./M0FeatureHostAdapters.types.ts";
import type {
  M3cHighWater,
  M3cJournalErrorCode,
  M3cPackageMetadata,
  M3cSignedJournal,
} from "./M3cSignedJournal.types.ts";

export const M3C_SIGNED_JOURNAL_MAX_BYTES = 256 * 1024;
export const M3C_SIGNED_PACKAGE_LIMIT = 32;

const hashPattern = /^[a-f0-9]{64}$/;
const versionPattern = /^[A-Za-z0-9][A-Za-z0-9.-]{0,63}$/;

export class M3cJournalError extends Error {
  constructor(readonly code: M3cJournalErrorCode) {
    super(code);
    this.name = "M3cJournalError";
  }
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function hasExactKeys(
  value: Record<string, unknown>,
  expected: readonly string[],
): boolean {
  const actual = Object.keys(value).sort();
  const keys = [...expected].sort();
  return actual.length === keys.length &&
    actual.every((key, index) => key === keys[index]);
}

function isPositiveSafeInteger(value: unknown): value is number {
  return typeof value === "number" && Number.isSafeInteger(value) && value > 0;
}

function snapshotMetadata(value: unknown): M3cPackageMetadata {
  if (
    !isRecord(value) ||
    !hasExactKeys(value, [
      "packageSha256",
      "sourceSha256",
      "version",
      "releaseSequence",
    ]) ||
    typeof value.packageSha256 !== "string" ||
    !hashPattern.test(value.packageSha256) ||
    typeof value.sourceSha256 !== "string" ||
    !hashPattern.test(value.sourceSha256) ||
    typeof value.version !== "string" ||
    !versionPattern.test(value.version) ||
    !isPositiveSafeInteger(value.releaseSequence)
  ) throw new M3cJournalError("invalid-journal");
  return Object.freeze({
    packageSha256: value.packageSha256,
    sourceSha256: value.sourceSha256,
    version: value.version,
    releaseSequence: value.releaseSequence,
  });
}

function snapshotHighWater(value: unknown): M3cHighWater | null {
  if (value === null) return null;
  if (
    !isRecord(value) ||
    !hasExactKeys(value, ["releaseSequence", "packageSha256"]) ||
    !isPositiveSafeInteger(value.releaseSequence) ||
    typeof value.packageSha256 !== "string" ||
    !hashPattern.test(value.packageSha256)
  ) throw new M3cJournalError("invalid-journal");
  return Object.freeze({
    releaseSequence: value.releaseSequence,
    packageSha256: value.packageSha256,
  });
}

function collectHash(value: unknown, output: string[]): void {
  if (value === null) return;
  if (typeof value !== "string" || !hashPattern.test(value)) {
    throw new M3cJournalError("invalid-journal");
  }
  output.push(value);
}

function collectRequest(value: unknown, output: string[]): void {
  if (!isRecord(value)) throw new M3cJournalError("invalid-journal");
  if (value.target !== null) collectHash(value.target, output);
}

function collectSnapshot(value: unknown, output: string[]): void {
  if (!isRecord(value)) throw new M3cJournalError("invalid-journal");
  collectHash(value.selectedVersion, output);
  collectHash(value.selectedDigest, output);
  collectHash(value.rollbackVersion, output);
  collectHash(value.rollbackDigest, output);
}

function collectOutcome(value: unknown, output: string[]): void {
  if (!isRecord(value)) throw new M3cJournalError("invalid-journal");
  collectRequest(value.request, output);
  collectHash(value.selectedVersion, output);
}

function collectLifecycleReferences(
  value: unknown,
  featureId: string,
): string[] {
  if (
    !isRecord(value) || value.schemaVersion !== 2 ||
    value.featureId !== featureId
  ) throw new M3cJournalError("invalid-journal");
  const output: string[] = [];
  collectHash(value.selectedVersion, output);
  collectHash(value.selectedDigest, output);
  collectHash(value.rollbackVersion, output);
  collectHash(value.rollbackDigest, output);
  if (value.pending !== null) {
    if (!isRecord(value.pending)) throw new M3cJournalError("invalid-journal");
    collectRequest(value.pending.request, output);
    collectSnapshot(value.pending.before, output);
    collectHash(value.pending.targetVersion, output);
    collectHash(value.pending.targetDigest, output);
  }
  if (value.lastOutcome !== null) {
    collectOutcome(value.lastOutcome, output);
  }
  if (
    !Array.isArray(value.recentOutcomes) || value.recentOutcomes.length > 32
  ) {
    throw new M3cJournalError("invalid-journal");
  }
  for (const outcome of value.recentOutcomes) {
    collectOutcome(outcome, output);
  }
  if (value.attempt !== null) {
    if (!isRecord(value.attempt)) throw new M3cJournalError("invalid-journal");
    collectRequest(value.attempt.request, output);
    collectHash(value.attempt.targetDigest, output);
    if (
      !Array.isArray(value.attempt.attemptedDigests) ||
      !Array.isArray(value.attempt.rejectedDigests) ||
      value.attempt.attemptedDigests.length > 2 ||
      value.attempt.rejectedDigests.length > 2
    ) throw new M3cJournalError("invalid-journal");
    for (const digest of value.attempt.attemptedDigests) {
      collectHash(digest, output);
    }
    for (const digest of value.attempt.rejectedDigests) {
      collectHash(digest, output);
    }
  }
  return output;
}

export function validateM3cSignedJournal(
  value: unknown,
  featureId: string,
): M3cSignedJournal {
  if (
    !isRecord(value) ||
    !hasExactKeys(value, [
      "schemaVersion",
      "featureId",
      "highWater",
      "packages",
      "lifecycle",
    ]) ||
    value.schemaVersion !== 1 || value.featureId !== featureId ||
    !Array.isArray(value.packages) ||
    value.packages.length > M3C_SIGNED_PACKAGE_LIMIT
  ) throw new M3cJournalError("invalid-journal");

  const packages = value.packages.map(snapshotMetadata);
  const byHash = new Map<string, M3cPackageMetadata>();
  for (const metadata of packages) {
    if (byHash.has(metadata.packageSha256)) {
      throw new M3cJournalError("invalid-journal");
    }
    byHash.set(metadata.packageSha256, metadata);
  }
  const highWater = snapshotHighWater(value.highWater);
  if (highWater) {
    const entry = byHash.get(highWater.packageSha256);
    if (!entry || entry.releaseSequence !== highWater.releaseSequence) {
      throw new M3cJournalError("invalid-journal");
    }
  }
  const references = collectLifecycleReferences(value.lifecycle, featureId);
  const referencedBySequence = new Map<number, string>();
  if (highWater) {
    referencedBySequence.set(
      highWater.releaseSequence,
      highWater.packageSha256,
    );
  }
  for (const hash of references) {
    const entry = byHash.get(hash);
    if (
      !entry || !highWater ||
      entry.releaseSequence > highWater.releaseSequence ||
      (
        entry.releaseSequence === highWater.releaseSequence &&
        entry.packageSha256 !== highWater.packageSha256
      )
    ) throw new M3cJournalError("invalid-journal");
    const sameSequenceHash = referencedBySequence.get(entry.releaseSequence);
    if (sameSequenceHash !== undefined && sameSequenceHash !== hash) {
      throw new M3cJournalError("invalid-journal");
    }
    referencedBySequence.set(entry.releaseSequence, hash);
  }
  return Object.freeze({
    schemaVersion: 1,
    featureId,
    highWater,
    packages: Object.freeze(packages),
    lifecycle: value.lifecycle,
  });
}

export function serializeM3cSignedJournal(
  journal: M3cSignedJournal,
): Uint8Array {
  const bytes = new TextEncoder().encode(JSON.stringify(journal));
  if (bytes.length > M3C_SIGNED_JOURNAL_MAX_BYTES) {
    throw new M3cJournalError("journal-too-large");
  }
  return bytes;
}

export function admitM3cPackage(
  highWater: M3cHighWater | null,
  metadata: M3cPackageMetadata,
): M3cHighWater {
  if (!highWater || metadata.releaseSequence > highWater.releaseSequence) {
    return Object.freeze({
      releaseSequence: metadata.releaseSequence,
      packageSha256: metadata.packageSha256,
    });
  }
  if (metadata.releaseSequence < highWater.releaseSequence) {
    throw new M3cJournalError("sequence-too-low");
  }
  if (metadata.packageSha256 !== highWater.packageSha256) {
    throw new M3cJournalError("sequence-equivocation");
  }
  return highWater;
}

export function mergeM3cHighWater(
  current: M3cHighWater | null,
  candidate: M3cHighWater | null,
): M3cHighWater | null {
  if (!candidate) return current;
  if (!current || candidate.releaseSequence > current.releaseSequence) {
    return candidate;
  }
  return current;
}

export function highWaterAfterM3cCommitFailure(
  current: M3cHighWater | null,
  candidate: M3cHighWater | null,
  uncertain: boolean,
): M3cHighWater | null {
  return uncertain ? mergeM3cHighWater(current, candidate) : current;
}

function metadataEqual(
  left: M3cPackageMetadata,
  right: M3cPackageMetadata,
): boolean {
  return left.packageSha256 === right.packageSha256 &&
    left.sourceSha256 === right.sourceSha256 &&
    left.version === right.version &&
    left.releaseSequence === right.releaseSequence;
}

export class M3cIdentityCatalog implements M0IdentityCatalog {
  readonly featureId: string;
  #entries = new Map<string, M3cPackageMetadata>();

  constructor(featureId: string) {
    this.featureId = featureId;
  }

  hasVersion(version: string): boolean {
    return this.#entries.has(version);
  }

  digestForVersion(version: string): string | null {
    return this.#entries.get(version)?.packageSha256 ?? null;
  }

  hasDigest(digest: string): boolean {
    return this.#entries.has(digest);
  }

  compatibilityForVersion(): null {
    return null;
  }

  get(packageSha256: string): M3cPackageMetadata | null {
    return this.#entries.get(packageSha256) ?? null;
  }

  add(value: M3cPackageMetadata): void {
    const metadata = snapshotMetadata(value);
    const previous = this.#entries.get(metadata.packageSha256);
    if (previous) {
      if (!metadataEqual(previous, metadata)) {
        throw new M3cJournalError("package-conflict");
      }
      return;
    }
    if (this.#entries.size >= M3C_SIGNED_PACKAGE_LIMIT) {
      throw new M3cJournalError("catalog-full");
    }
    this.#entries.set(metadata.packageSha256, metadata);
  }

  replace(values: readonly M3cPackageMetadata[]): void {
    if (values.length > M3C_SIGNED_PACKAGE_LIMIT) {
      throw new M3cJournalError("invalid-journal");
    }
    const entries = new Map<string, M3cPackageMetadata>();
    for (const value of values) {
      const metadata = snapshotMetadata(value);
      if (entries.has(metadata.packageSha256)) {
        throw new M3cJournalError("invalid-journal");
      }
      entries.set(metadata.packageSha256, metadata);
    }
    this.#entries = entries;
  }

  values(): readonly M3cPackageMetadata[] {
    return Object.freeze(
      [...this.#entries.values()].sort((left, right) =>
        left.packageSha256.localeCompare(right.packageSha256)
      ),
    );
  }
}
