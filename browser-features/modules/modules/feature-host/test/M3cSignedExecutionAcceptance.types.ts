// SPDX-License-Identifier: MPL-2.0

import type {
  M3cSignedAuthorization,
  M3cSignedFeatureHost,
  M3cSignedTestHooks,
} from "../M3cSignedExecution.sys.mts";

export interface SigningAuthority {
  readonly keyPair: CryptoKeyPair;
  readonly keyId: string;
  readonly publicKey: string;
}

export interface SignedPackageOptions {
  readonly authority?: SigningAuthority;
  readonly featureId?: string;
  readonly version?: string;
  readonly releaseSequence?: number;
  readonly source?: string;
  readonly minimumReleaseSequence?: number;
}

export interface SignedPackageFixture {
  readonly bytes: Uint8Array;
  readonly authorization: M3cSignedAuthorization;
}

export type SignedPackageAuthorization = M3cSignedAuthorization;

export interface SignedJournalForTests {
  readonly highWater: {
    readonly releaseSequence: number;
    readonly packageSha256: string;
  } | null;
  readonly packages: readonly {
    readonly packageSha256: string;
    readonly sourceSha256: string;
    readonly version: string;
    readonly releaseSequence: number;
  }[];
  readonly lifecycle: Record<string, unknown>;
}

export type Host = M3cSignedFeatureHost;
export type Hooks = M3cSignedTestHooks;
