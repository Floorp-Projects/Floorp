// SPDX-License-Identifier: MPL-2.0

export interface FeaturePackageCompatibility {
  readonly hostSchema: number;
  readonly hostBuildId: string;
  readonly runtimeBuildIds: readonly string[];
  readonly capabilities: readonly string[];
}

export interface FeaturePackageManifest {
  readonly featureId: string;
  readonly version: string;
  readonly releaseSequence: number;
  readonly sourceBytes: number;
  readonly sourceSha256: string;
  readonly compatibility: FeaturePackageCompatibility;
}

export interface SignedFeaturePackageEnvelope {
  readonly formatVersion: 1;
  readonly keyId: string;
  readonly manifest: FeaturePackageManifest;
  readonly source: string;
  readonly signature: string;
}

export interface FeaturePackageTrustedKey {
  readonly keyId: string;
  readonly publicKey: string;
}

export interface FeaturePackagePolicy {
  readonly featureId: string;
  readonly hostSchema: number;
  readonly hostBuildId: string;
  readonly runtimeBuildId: string;
  readonly allowedCapabilities: readonly string[];
  readonly minimumReleaseSequence: number;
  readonly trustedKeys: readonly FeaturePackageTrustedKey[];
}

export interface VerifiedFeaturePackage {
  readonly formatVersion: 1;
  readonly keyId: string;
  readonly manifest: FeaturePackageManifest;
  readonly source: string;
  readonly packageSha256: string;
}

export interface FeaturePackageAuthoringMetadata {
  readonly featureId: string;
  readonly version: string;
  readonly releaseSequence: number;
  readonly compatibility: FeaturePackageCompatibility;
}

export interface FeaturePackageSnapshotTrustedKey {
  readonly keyId: string;
  readonly publicKey: Uint8Array;
}

export interface FeaturePackagePolicySnapshot {
  readonly featureId: string;
  readonly hostSchema: number;
  readonly hostBuildId: string;
  readonly runtimeBuildId: string;
  readonly allowedCapabilities: readonly string[];
  readonly minimumReleaseSequence: number;
  readonly trustedKeys: readonly FeaturePackageSnapshotTrustedKey[];
}

export interface ParsedFeaturePackageEnvelope {
  readonly envelope: SignedFeaturePackageEnvelope;
  readonly sourceBytes: Uint8Array;
  readonly signatureBytes: Uint8Array;
  readonly canonicalBytes: Uint8Array;
  readonly signingBytes: Uint8Array;
  readonly manifest: FeaturePackageManifest;
}

export type FeaturePackageCacheErrorCode =
  | "cache-invalid-feature-id"
  | "cache-invalid-test-id"
  | "cache-invalid-package-hash"
  | "cache-policy-mismatch"
  | "cache-owner-active"
  | "cache-closed"
  | "cache-storage"
  | "cache-corrupt"
  | "cache-missing"
  | "cache-capacity"
  | "cache-unsafe-layout";

export type FeaturePackageCacheEntryKind =
  | "file"
  | "directory"
  | "link"
  | "other";

export interface FeaturePackageCacheEntry {
  readonly name: string;
  readonly kind: FeaturePackageCacheEntryKind;
}

export interface FeaturePackageCacheStorage {
  initialize(): Promise<void>;
  inspect(name: string): Promise<FeaturePackageCacheEntry | null>;
  read(name: string, maxBytes: number): Promise<Uint8Array>;
  enumerate(maxEntries: number): Promise<readonly FeaturePackageCacheEntry[]>;
  writeStaging(bytes: Uint8Array): Promise<void>;
  moveStaging(name: string): Promise<void>;
  removeStaging(): Promise<void>;
}

export interface FeaturePackageCache {
  put(
    bytes: Uint8Array,
    policy: FeaturePackagePolicy,
  ): Promise<VerifiedFeaturePackage>;
  read(
    packageSha256: string,
    policy: FeaturePackagePolicy,
  ): Promise<VerifiedFeaturePackage>;
  close(): Promise<void>;
}

export type FeaturePackageErrorCode =
  | "invalid-envelope"
  | "invalid-policy"
  | "invalid-metadata"
  | "invalid-source"
  | "invalid-signing-key"
  | "unsupported-crypto"
  | "unsupported-version"
  | "untrusted-key"
  | "invalid-signature"
  | "source-digest-mismatch"
  | "feature-mismatch"
  | "host-incompatible"
  | "runtime-incompatible"
  | "capability-denied"
  | "sequence-too-low"
  | "size-limit";
