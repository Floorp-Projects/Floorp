// SPDX-License-Identifier: MPL-2.0

import {
  FeaturePackageCacheError,
  openFeaturePackageCache,
  validateFeaturePackageCacheTestId,
} from "./FeaturePackageCache.ts";
import { isValidFeaturePackageId } from "./FeaturePackageVerifier.ts";
import { createProfileFeaturePackageCacheStorage } from "./FeaturePackageCacheStorage.sys.mts";
import type {
  FeaturePackageCache,
  FeaturePackageCacheErrorCode,
  FeaturePackagePolicy,
  VerifiedFeaturePackage,
} from "./types.ts";

function createCache(
  featureId: string,
  testId?: string,
): Promise<FeaturePackageCache> {
  if (!isValidFeaturePackageId(featureId)) {
    throw new FeaturePackageCacheError("cache-invalid-feature-id");
  }
  try {
    const { storage, ownerKey } = createProfileFeaturePackageCacheStorage(
      featureId,
      testId,
    );
    return openFeaturePackageCache(featureId, ownerKey, storage);
  } catch (error: unknown) {
    if (error instanceof FeaturePackageCacheError) throw error;
    throw new FeaturePackageCacheError("cache-storage");
  }
}

export function openProfileFeaturePackageCache(
  featureId: string,
): Promise<FeaturePackageCache> {
  return createCache(featureId);
}

export function openTestProfileFeaturePackageCache(
  featureId: string,
  testId: string,
): Promise<FeaturePackageCache> {
  if (!isValidFeaturePackageId(featureId)) {
    throw new FeaturePackageCacheError("cache-invalid-feature-id");
  }
  const validTestId = validateFeaturePackageCacheTestId(testId);
  return createCache(featureId, validTestId);
}

export { FeaturePackageCacheError };
export type {
  FeaturePackageCache,
  FeaturePackageCacheErrorCode,
  FeaturePackagePolicy,
  VerifiedFeaturePackage,
};
