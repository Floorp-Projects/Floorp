// SPDX-License-Identifier: MPL-2.0

export interface M3cPackageMetadata {
  readonly packageSha256: string;
  readonly sourceSha256: string;
  readonly version: string;
  readonly releaseSequence: number;
}

export interface M3cHighWater {
  readonly releaseSequence: number;
  readonly packageSha256: string;
}

export interface M3cSignedJournal {
  readonly schemaVersion: 1;
  readonly featureId: string;
  readonly highWater: M3cHighWater | null;
  readonly packages: readonly M3cPackageMetadata[];
  readonly lifecycle: unknown;
}

export type M3cJournalErrorCode =
  | "invalid-journal"
  | "journal-too-large"
  | "catalog-full"
  | "package-conflict"
  | "sequence-too-low"
  | "sequence-equivocation";
