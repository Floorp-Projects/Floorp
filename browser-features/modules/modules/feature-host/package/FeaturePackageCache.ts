// SPDX-License-Identifier: MPL-2.0

import {
  FEATURE_PACKAGE_MAX_BYTES,
  FeaturePackageError,
  isValidFeaturePackageId,
  snapshotFeaturePackageBytes,
  snapshotFeaturePackagePolicy,
  verifySignedFeaturePackage,
} from "./FeaturePackageVerifier.ts";
import type {
  FeaturePackageCache,
  FeaturePackageCacheEntry,
  FeaturePackageCacheErrorCode,
  FeaturePackageCacheStorage,
  FeaturePackagePolicy,
  VerifiedFeaturePackage,
} from "./types.ts";

const MAX_OPERATIONS = 16;
const MAX_PACKAGES = 32;
const MAX_DIRECTORY_ENTRIES = 34;
const STAGING_NAME = "candidate.tmp";
const packageHashPattern = /^[a-f0-9]{64}$/;
const testIdPattern =
  /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/;
const owners = new Map<string, symbol>();

const errorMessages: Record<FeaturePackageCacheErrorCode, string> = {
  "cache-invalid-feature-id": "Package cache feature ID is invalid.",
  "cache-invalid-test-id": "Package cache test ID is invalid.",
  "cache-invalid-package-hash": "Package cache hash is invalid.",
  "cache-policy-mismatch": "Package cache policy does not match feature.",
  "cache-owner-active": "Package cache is already open.",
  "cache-closed": "Package cache is closed.",
  "cache-storage": "Package cache storage operation failed.",
  "cache-corrupt": "Cached package is invalid.",
  "cache-missing": "Cached package is missing.",
  "cache-capacity": "Package cache capacity is exhausted.",
  "cache-unsafe-layout": "Package cache layout is invalid.",
};

export class FeaturePackageCacheError extends Error {
  constructor(readonly code: FeaturePackageCacheErrorCode) {
    super(errorMessages[code]);
    this.name = "FeaturePackageCacheError";
  }
}

function fail(code: FeaturePackageCacheErrorCode): never {
  throw new FeaturePackageCacheError(code);
}

function boundedError(error: unknown): unknown {
  try {
    if (
      error instanceof FeaturePackageError ||
      error instanceof FeaturePackageCacheError
    ) return error;
  } catch {
    return new FeaturePackageCacheError("cache-storage");
  }
  return new FeaturePackageCacheError("cache-storage");
}

function sameBytes(left: Uint8Array, right: Uint8Array): boolean {
  return left.length === right.length &&
    left.every((byte, index) => byte === right[index]);
}

function assertRegularEntry(
  entry: FeaturePackageCacheEntry | null,
): asserts entry is FeaturePackageCacheEntry {
  if (!entry) fail("cache-missing");
  if (entry.kind !== "file") fail("cache-unsafe-layout");
}

function validateDirectoryEntries(
  entries: readonly FeaturePackageCacheEntry[],
): { readonly packageCount: number; readonly hasStaging: boolean } {
  if (entries.length >= MAX_DIRECTORY_ENTRIES) fail("cache-capacity");
  let packageCount = 0;
  let hasStaging = false;
  for (const entry of entries) {
    if (entry.kind !== "file") fail("cache-unsafe-layout");
    if (entry.name === STAGING_NAME) {
      if (hasStaging) fail("cache-unsafe-layout");
      hasStaging = true;
    } else if (packageHashPattern.test(entry.name)) {
      packageCount++;
    } else {
      fail("cache-unsafe-layout");
    }
  }
  if (packageCount > MAX_PACKAGES) fail("cache-capacity");
  return { packageCount, hasStaging };
}

export function openFeaturePackageCache(
  featureId: string,
  ownerKey: string,
  storage: FeaturePackageCacheStorage,
): Promise<FeaturePackageCache> {
  if (!isValidFeaturePackageId(featureId)) {
    fail("cache-invalid-feature-id");
  }
  if (owners.has(ownerKey)) fail("cache-owner-active");
  const owner = Symbol("feature-package-cache-owner");
  owners.set(ownerKey, owner);
  return (async () => {
    try {
      await storage.initialize();
    } catch (error: unknown) {
      if (owners.get(ownerKey) === owner) owners.delete(ownerKey);
      if (error instanceof FeaturePackageCacheError) throw error;
      fail("cache-storage");
    }

    let accepting = true;
    let operationCount = 0;
    let queue: Promise<void> = Promise.resolve();
    let closePromise: Promise<void> | undefined;

    function schedule<T>(
      prepare: () => () => Promise<T>,
    ): Promise<T> {
      if (!accepting) {
        return Promise.reject(
          new FeaturePackageCacheError(
            "cache-closed",
          ),
        );
      }
      if (operationCount >= MAX_OPERATIONS) {
        return Promise.reject(new FeaturePackageCacheError("cache-capacity"));
      }
      operationCount++;

      let resolvePreparation!: (
        preparation:
          | { readonly operation: () => Promise<T> }
          | { readonly error: unknown },
      ) => void;
      const preparation = new Promise<
        { readonly operation: () => Promise<T> } | { readonly error: unknown }
      >((resolve) => resolvePreparation = resolve);
      let slotReserved = true;
      const releaseSlot = (): void => {
        if (!slotReserved) return;
        slotReserved = false;
        operationCount--;
      };

      const previous = queue;
      const result = previous.then(async () => {
        const prepared = await preparation;
        if ("error" in prepared) throw prepared.error;
        return await prepared.operation();
      });
      queue = result.then(
        () => undefined,
        () => undefined,
      ).then(releaseSlot);

      try {
        resolvePreparation({ operation: prepare() });
      } catch (error: unknown) {
        const safeError = boundedError(error);
        resolvePreparation({ error: safeError });
        return Promise.reject(safeError);
      }
      return result.catch((error: unknown) => {
        throw boundedError(error);
      });
    }

    async function readAndVerify(
      hash: string,
      policy: FeaturePackagePolicy,
    ): Promise<
      { readonly bytes: Uint8Array; readonly result: VerifiedFeaturePackage }
    > {
      let entry: FeaturePackageCacheEntry | null;
      try {
        entry = await storage.inspect(hash);
      } catch {
        return fail("cache-storage");
      }
      assertRegularEntry(entry);
      let bytes: Uint8Array;
      try {
        bytes = snapshotFeaturePackageBytes(
          await storage.read(hash, FEATURE_PACKAGE_MAX_BYTES + 1),
        );
      } catch (error: unknown) {
        if (error instanceof FeaturePackageError) throw error;
        return fail("cache-storage");
      }
      let result: VerifiedFeaturePackage;
      try {
        result = await verifySignedFeaturePackage(bytes, policy);
      } catch (error: unknown) {
        if (error instanceof FeaturePackageError) throw error;
        return fail("cache-storage");
      }
      if (result.packageSha256 !== hash) fail("cache-corrupt");
      return { bytes, result };
    }

    const cache: FeaturePackageCache = {
      put(bytesInput, policyInput) {
        return schedule(() => {
          const bytes = snapshotFeaturePackageBytes(bytesInput);
          const policy = snapshotFeaturePackagePolicy(policyInput);
          if (policy.featureId !== featureId) fail("cache-policy-mismatch");
          return async () => {
            const verified = await verifySignedFeaturePackage(bytes, policy);
            const hash = verified.packageSha256;
            let existing: FeaturePackageCacheEntry | null;
            try {
              existing = await storage.inspect(hash);
            } catch {
              return fail("cache-storage");
            }
            if (existing) {
              const stored = await readAndVerify(hash, policy);
              if (!sameBytes(stored.bytes, bytes)) fail("cache-corrupt");
              return stored.result;
            }

            let entries: readonly FeaturePackageCacheEntry[];
            try {
              entries = await storage.enumerate(MAX_DIRECTORY_ENTRIES);
            } catch {
              return fail("cache-storage");
            }
            const layout = validateDirectoryEntries(entries);
            if (layout.packageCount >= MAX_PACKAGES) fail("cache-capacity");
            if (layout.hasStaging) {
              try {
                await storage.removeStaging();
              } catch {
                return fail("cache-storage");
              }
            }
            try {
              await storage.writeStaging(bytes);
              const destination = await storage.inspect(hash);
              if (destination) fail("cache-unsafe-layout");
              await storage.moveStaging(hash);
            } catch (error: unknown) {
              throw boundedError(error);
            }
            const stored = await readAndVerify(hash, policy);
            if (!sameBytes(stored.bytes, bytes)) fail("cache-corrupt");
            return stored.result;
          };
        });
      },
      read(hashInput, policyInput) {
        return schedule(() => {
          if (
            typeof hashInput !== "string" ||
            !packageHashPattern.test(hashInput)
          ) fail("cache-invalid-package-hash");
          const policy = snapshotFeaturePackagePolicy(policyInput);
          if (policy.featureId !== featureId) fail("cache-policy-mismatch");
          const hash = hashInput;
          return async () => (await readAndVerify(hash, policy)).result;
        });
      },
      close() {
        if (closePromise) return closePromise;
        accepting = false;
        closePromise = queue.then(() => {
          if (owners.get(ownerKey) === owner) owners.delete(ownerKey);
        });
        return closePromise;
      },
    };
    return cache;
  })();
}

export function validateFeaturePackageCacheTestId(testId: unknown): string {
  if (typeof testId !== "string" || !testIdPattern.test(testId)) {
    fail("cache-invalid-test-id");
  }
  return testId;
}
