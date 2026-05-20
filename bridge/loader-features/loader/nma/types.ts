// SPDX-License-Identifier: MPL-2.0

/**
 * Noraneko Module Archive (NMA) Types
 *
 * This file contains strictly data definitions (Interfaces and Enums).
 * No logic or implementations should be present here.
 *
 * Ported from noraneko: bridge/loader-features/loader/nma/types.ts
 * TODO(Floorp): Review field names (e.g. noranekoVersion → floorpVersion) when
 *              the NMA format is finalized for Floorp.
 */

// ============================================================================
// Core NMA Types
// ============================================================================

export interface NMAManifest {
  formatVersion: "1.0";
  buildId: string;
  noranekoVersion: string;
  commitSha: string;
  builtAt: string;
  channel: UpdateChannel;
  modules: NMAModule[];
  assets: NMAAsset[];
  sigstoreBundle: SigstoreBundle;
  archiveHash: string;
  isDelta: boolean;
  baseBuildId?: string;
  minVersion: string;
  maxVersion?: string;
}

export interface NMAModule {
  name: string;
  path: string;
  hash: string;
  size: number;
  dependencies: string[];
  essential: boolean;
}

export interface NMAAsset {
  name: string;
  path: string;
  hash: string;
  size: number;
  mimeType: string;
}

// ============================================================================
// Verification Types
// ============================================================================

export enum NMAVerificationStatus {
  VALID = "VALID",
  NOT_FOUND = "NOT_FOUND",
  INVALID_ARCHIVE = "INVALID_ARCHIVE",
  INVALID_MANIFEST = "INVALID_MANIFEST",
  SIGNATURE_INVALID = "SIGNATURE_INVALID",
  UNTRUSTED_SIGNER = "UNTRUSTED_SIGNER",
  HASH_MISMATCH = "HASH_MISMATCH",
  VERSION_MISMATCH = "VERSION_MISMATCH",
  UNKNOWN_ERROR = "UNKNOWN_ERROR",
}

export interface NMAVerificationResult {
  isValid: boolean;
  status: NMAVerificationStatus;
  errorMessage?: string;
  verifiedIdentity?: SignerIdentity;
  manifest?: NMAManifest;
}

export interface NMATrustedConfig {
  allowedIssuers: string[];
  allowedRepositories: string[];
  allowedWorkflows: string[];
  allowUnsignedInDev: boolean;
}

export enum UpdateChannel {
  NIGHTLY = "nightly",
  BETA = "beta",
  RELEASE = "release",
  DEFAULT = "default",
}

// ============================================================================
// Sigstore Support Types
// ============================================================================

export interface SigstoreBundle {
  bundle: string;
  signerIdentity: SignerIdentity;
  rekorLogId: string;
  signedAt: string;
}

export interface SignerIdentity {
  issuer: string;
  subject: string;
  repository: string;
  workflowRef: string;
}

// ============================================================================
// Hashing & Hotswap Types
// ============================================================================

export interface ModuleHashInfo {
  moduleName: string;
  hash: string;
  lastComputed: number;
}

export interface HashState {
  denoLockHash: string;
  moduleHashes: Record<string, ModuleHashInfo>;
  computedAt: number;
}

export interface HashComparisonResult {
  denoLockChanged: boolean;
  changedModules: string[];
  newModules: string[];
  removedModules: string[];
  hasChanges: boolean;
}

export enum HotswapMode {
  NONE = "none",
  FULL = "full",
  SELECTIVE = "selective",
}

export interface HotswapRecommendation {
  mode: HotswapMode;
  modulesToReload: string[];
  reason: string;
}

// ============================================================================
// Loader State
// ============================================================================

export interface NMALoaderState {
  currentNMA: NMAManifest | null;
  nmaPath: string | null;
  isActive: boolean;
  loadedModules: string[];
  lastVerification: NMAVerificationResult | null;
  moduleVersions: Map<string, string>;
}

export interface NMALoaderEvents {
  "nma-loaded": { manifest: NMAManifest };
  "nma-verified": { result: NMAVerificationResult };
  "nma-error": { error: string; status: NMAVerificationStatus };
  "nma-activated": { modules: string[] };
  "nma-hotswap-start": { modules: string[] };
  "nma-hotswap-complete": { swapped: string[]; failed: string[] };
  "nma-module-cleanup": { name: string };
  "nma-module-reload": { name: string };
}
