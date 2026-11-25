// SPDX-License-Identifier: MPL-2.0

import type { PinnedFixture } from "./FeatureHostTypes.ts";

export interface M0IdentityCatalog {
  readonly featureId: string;
  hasVersion(version: string): boolean;
  digestForVersion(version: string): string | null;
  hasDigest(digest: string): boolean;
  compatibilityForVersion(
    version: string,
  ): PinnedFixture<string>["compatibility"] | null;
}

export interface M0PackageProvider {
  ensure(version: string): Promise<PinnedFixture<string>>;
  load(digest: string, version: string): Promise<PinnedFixture<string>>;
}

export interface M0PreparedRecordWrite {
  commit(): Promise<void>;
}

export interface M0RecordStoreAdapter {
  read(): Promise<unknown | null>;
  prepare(next: unknown, previous: unknown): M0PreparedRecordWrite;
}
