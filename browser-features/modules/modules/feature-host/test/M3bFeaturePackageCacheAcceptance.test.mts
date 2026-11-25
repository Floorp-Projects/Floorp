// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert as harnessAssert,
  assertEquals as harnessAssertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";
import {
  M3A_INERT_PROBE_ENVELOPE,
  M3A_INERT_PROBE_PUBLIC_KEY,
} from "../package/FeaturePackageFixture.ts";
import type {
  FeaturePackageCache,
  FeaturePackagePolicy,
} from "../package/types.ts";

const cacheModule = ChromeUtils.importESModule(
  "resource://noraneko/modules/feature-host/package/FeaturePackageCache.sys.mjs",
) as typeof import("../package/FeaturePackageCache.sys.mts");
const verifier = ChromeUtils.importESModule(
  "resource://noraneko/modules/feature-host/package/FeaturePackageVerifier.sys.mjs",
) as typeof import("../package/FeaturePackageVerifier.sys.mts");
const storageModule = ChromeUtils.importESModule(
  "resource://noraneko/modules/feature-host/package/FeaturePackageCacheStorage.sys.mjs",
) as typeof import("../package/FeaturePackageCacheStorage.sys.mts");
const probePref = "nora.m3a.package_source_execution_probe";
const encoder = new TextEncoder();

function assert(condition: unknown, message: string): asserts condition {
  harnessAssert(condition, message);
}

function assertEquals<T>(actual: T, expected: T, message: string): void {
  harnessAssertEquals(actual, expected, message);
}

function policy(): FeaturePackagePolicy {
  return {
    featureId: "hub-panel-menu",
    hostSchema: 2,
    hostBuildId: "fixture-host",
    runtimeBuildId: "fixture-runtime",
    allowedCapabilities: ["hub-panel"],
    minimumReleaseSequence: 1,
    trustedKeys: [{
      keyId: "inert-probe-key",
      publicKey: M3A_INERT_PROBE_PUBLIC_KEY,
    }],
  };
}

function profileTestTree(testId: string): nsIFile {
  const root = Services.dirsvc.get("ProfD", Components.interfaces.nsIFile);
  root.append("floorp-signed-package-cache-v1");
  root.append("tests");
  root.append(testId);
  return root;
}

function profileTestRoot(testId: string): nsIFile {
  const root = profileTestTree(testId);
  root.append("hub-panel-menu");
  return root;
}

function symlinkCreator():
  | ((targetPath: string, createPath: string) => Promise<void>)
  | undefined {
  const linuxCreate = Services.appinfo.OS !== "Linux"
    ? undefined
    : (targetPath: string, createPath: string): Promise<void> => {
      const executableFactory = Reflect.get(
        Components.classes,
        "@mozilla.org/file/local;1",
      ) as { createInstance(interfaceId: unknown): nsIFile };
      const executable = executableFactory.createInstance(
        Components.interfaces.nsIFile,
      );
      executable.initWithPath("/bin/ln");
      const processFactory = Reflect.get(
        Components.classes,
        "@mozilla.org/process/util;1",
      ) as {
        createInstance(interfaceId: unknown): {
          init(executable: nsIFile): void;
          run(blocking: boolean, args: string[], count: number): void;
          readonly exitValue: number;
        };
      };
      const process = processFactory.createInstance(
        Components.interfaces.nsIProcess,
      );
      process.init(executable);
      const args = ["-s", targetPath, createPath];
      process.run(true, args, args.length);
      if (process.exitValue !== 0) {
        throw new Error("Could not create the controlled test symlink.");
      }
      return Promise.resolve();
    };
  try {
    const module = ChromeUtils.importESModule(
      "resource://gre/modules/osfile.sys.mjs",
    ) as {
      readonly OS?: {
        readonly File?: {
          readonly unixSymLink?: (
            targetPath: string,
            createPath: string,
          ) => Promise<void>;
        };
      };
    };
    const file = module.OS?.File;
    const create = file?.unixSymLink;
    if (typeof create === "function" && file) {
      return (targetPath, createPath) =>
        create.call(file, targetPath, createPath);
    }
  } catch {
    return linuxCreate;
  }
  return linuxCreate;
}

async function expectCode(
  operation: Promise<unknown>,
  code: string,
): Promise<void> {
  let caught: unknown;
  try {
    await operation;
  } catch (error: unknown) {
    caught = error;
  }
  assert(
    caught && typeof caught === "object",
    "operation returns bounded error",
  );
  assertEquals(
    (caught as { readonly code?: unknown }).code,
    code,
    "cache error code matches",
  );
  assert(
    !(caught as { readonly message?: string }).message?.includes("floorp-"),
    "cache error omits filesystem paths",
  );
}

async function withProbe(operation: () => Promise<void>): Promise<void> {
  const hadValue = Services.prefs.prefHasUserValue(probePref);
  if (hadValue) {
    assertEquals(
      Services.prefs.getPrefType(probePref),
      Services.prefs.PREF_BOOL,
      "previous probe preference is boolean",
    );
  }
  const previous = hadValue ? Services.prefs.getBoolPref(probePref) : false;
  try {
    Services.prefs.setBoolPref(probePref, false);
    await operation();
  } finally {
    if (hadValue) Services.prefs.setBoolPref(probePref, previous);
    else Services.prefs.clearUserPref(probePref);
  }
}

async function withCacheTestScope(
  operation: (
    makeTestId: () => string,
    openCache: (testId: string) => Promise<FeaturePackageCache>,
  ) => Promise<void>,
): Promise<void> {
  const testIds: string[] = [];
  const caches: FeaturePackageCache[] = [];
  const makeTestId = (): string => {
    const testId = crypto.randomUUID().toLowerCase();
    assert(
      /^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/
        .test(testId),
      "test ID is a lowercase UUID",
    );
    assert(
      !testIds.includes(testId),
      "test IDs are unique within an invocation",
    );
    testIds.push(testId);
    return testId;
  };
  const openCache = async (
    testId: string,
  ): Promise<FeaturePackageCache> => {
    const cache = await cacheModule.openTestProfileFeaturePackageCache(
      "hub-panel-menu",
      testId,
    );
    caches.push(cache);
    return cache;
  };

  await withProbe(async () => {
    const errors: unknown[] = [];
    try {
      await operation(makeTestId, openCache);
    } catch (error: unknown) {
      errors.push(error);
    }
    const closeResults = await Promise.allSettled(
      caches.map((cache) => cache.close()),
    );
    for (const result of closeResults) {
      if (result.status === "rejected") errors.push(result.reason);
    }
    try {
      for (const testId of testIds) {
        const subtree = profileTestTree(testId);
        if (subtree.exists()) {
          await IOUtils.remove(subtree.path, { recursive: true });
        }
      }
    } catch (error: unknown) {
      errors.push(error);
    }
    if (errors.length === 1) throw errors[0];
    if (errors.length > 1) {
      throw new AggregateError(
        errors,
        "cache test operation or cleanup failed",
      );
    }
  });
}

function assertProbeInert(message: string): void {
  assertEquals(Services.prefs.getBoolPref(probePref), false, message);
}

function assertBytesEqual(
  actual: Uint8Array,
  expected: Uint8Array,
  message: string,
): void {
  if (actual.length !== expected.length) {
    throw new Error(
      `${message} (expected ${expected.length} bytes, got ${actual.length})`,
    );
  }
  for (let index = 0; index < expected.length; index++) {
    if (actual[index] !== expected[index]) {
      throw new Error(`${message} (byte ${index} differs)`);
    }
  }
}

async function testPutReadReopenAndInertSource(): Promise<void> {
  await withCacheTestScope(async (makeTestId, openCache) => {
    const testId = makeTestId();
    const bytes = encoder.encode(M3A_INERT_PROBE_ENVELOPE);
    let cache = await openCache(testId);
    const first = await cache.put(bytes, policy());
    assertProbeInert("successful put does not execute package source");
    assertEquals(
      (await cache.read(first.packageSha256, policy())).source,
      first.source,
      "read returns the verified exact source",
    );
    assertProbeInert("successful read does not execute package source");
    const storedPath = profileTestRoot(testId);
    storedPath.append(first.packageSha256);
    const storedBefore = await IOUtils.read(storedPath.path);
    const duplicate = await cache.put(bytes, policy());
    assertEquals(
      duplicate.packageSha256,
      first.packageSha256,
      "duplicate put is idempotent",
    );
    assertProbeInert("duplicate put does not execute package source");
    assertBytesEqual(
      await IOUtils.read(storedPath.path),
      storedBefore,
      "duplicate put does not replace stored bytes",
    );
    assertProbeInert("verified source remains inert");
    await cache.close();

    cache = await openCache(testId);
    assertEquals(
      (await cache.read(first.packageSha256, policy())).source,
      first.source,
      "reopened cache re-verifies the persisted envelope",
    );
    assertProbeInert("reopened read leaves source inert");
    await cache.close();
  });
}

async function testCreateOnlyStagingAndCleanup(): Promise<void> {
  await withCacheTestScope(async (makeTestId, openCache) => {
    const testId = makeTestId();
    const root = profileTestRoot(testId);
    const { storage } = storageModule.createProfileFeaturePackageCacheStorage(
      "hub-panel-menu",
      testId,
    );
    await storage.initialize();
    const stage = root.clone();
    stage.append("candidate.tmp");
    const original = new Uint8Array([4, 5, 6]);
    await IOUtils.write(stage.path, original, { mode: "create" });
    let createRefused = false;
    try {
      await storage.writeStaging(new Uint8Array([1, 2, 3]));
    } catch {
      createRefused = true;
    }
    assert(createRefused, "adapter staging write refuses an existing file");
    assertBytesEqual(
      await IOUtils.read(stage.path),
      original,
      "create-only refusal preserves existing staging bytes",
    );
    assert(stage.exists(), "refused staging file remains for cache recovery");

    const cache = await openCache(testId);
    await cache.put(encoder.encode(M3A_INERT_PROBE_ENVELOPE), policy());
    assert(!stage.exists(), "stale regular staging file is cleaned");
    assertProbeInert("staging recovery does not execute package source");

    const moveRoot = profileTestRoot(makeTestId());
    await IOUtils.makeDirectory(moveRoot.path, {
      createAncestors: true,
      ignoreExisting: true,
    });
    const source = moveRoot.clone();
    source.append("move-source.tmp");
    const existing = moveRoot.clone();
    existing.append("move-destination");
    const moveOriginal = new Uint8Array([7, 8, 9]);
    await IOUtils.write(source.path, new Uint8Array([1, 2, 3]), {
      mode: "create",
    });
    await IOUtils.write(existing.path, moveOriginal, { mode: "create" });
    let moveRefused = false;
    try {
      await IOUtils.move(source.path, existing.path, { noOverwrite: true });
    } catch {
      moveRefused = true;
    }
    assert(moveRefused, "no-overwrite move refuses an existing target");
    assertBytesEqual(
      await IOUtils.read(existing.path),
      moveOriginal,
      "existing move target is unchanged",
    );
    assert(source.exists(), "refused move preserves its source");
    assertProbeInert("create-only and no-overwrite checks remain inert");
  });
}

async function testBoundedEnumeration(): Promise<void> {
  await withCacheTestScope(async (makeTestId, _openCache) => {
    const testId = makeTestId();
    const root = profileTestRoot(testId);
    const { storage } = storageModule.createProfileFeaturePackageCacheStorage(
      "hub-panel-menu",
      testId,
    );
    await storage.initialize();
    for (let index = 0; index < 40; index++) {
      const entry = root.clone();
      entry.append("entry-" + index);
      await IOUtils.write(entry.path, new Uint8Array([index]), {
        mode: "create",
      });
    }
    const entries = await storage.enumerate(34);
    assertEquals(entries.length, 34, "adapter stops after the requested bound");
    assert(
      entries.every((entry) => entry.kind === "file"),
      "bounded enumeration reports real regular files",
    );
    assertEquals(
      (await storage.inspect("entry-39"))?.kind,
      "file",
      "more than 34 real entries exist below the adapter root",
    );
    assertProbeInert("bounded enumeration leaves package source inert");
  });
}

async function testUnsafeLayoutsAndLinks(): Promise<void> {
  await withCacheTestScope(async (makeTestId, openCache) => {
    const bytes = encoder.encode(M3A_INERT_PROBE_ENVELOPE);
    const verified = await verifier.verifySignedFeaturePackage(
      bytes,
      policy(),
    );

    const destinationId = makeTestId();
    const destinationCache = await openCache(destinationId);
    const destination = profileTestRoot(destinationId);
    destination.append(verified.packageSha256);
    destination.create(Components.interfaces.nsIFile.DIRECTORY_TYPE!, 0o700);
    await expectCode(
      destinationCache.put(bytes, policy()),
      "cache-unsafe-layout",
    );
    assert(destination.isDirectory(), "destination directory is preserved");
    assertProbeInert("directory destination rejection leaves source inert");

    const ancestorId = makeTestId();
    const ancestor = profileTestTree(ancestorId);
    await IOUtils.makeDirectory(ancestor.parent.path, {
      createAncestors: true,
      ignoreExisting: true,
    });
    const ancestorBytes = new Uint8Array([3, 2, 1]);
    await IOUtils.write(ancestor.path, ancestorBytes, { mode: "create" });
    await expectCode(openCache(ancestorId), "cache-unsafe-layout");
    assertBytesEqual(
      await IOUtils.read(ancestor.path),
      ancestorBytes,
      "non-directory controlled ancestor is preserved",
    );
    assertProbeInert("non-directory ancestor rejection leaves source inert");

    const createSymlink = symlinkCreator();
    if (!createSymlink) {
      if (Services.appinfo.OS === "Linux") {
        throw new Error(
          "Linux cache layout tests require OS.File.unixSymLink.",
        );
      }
      console.warn(
        "[M3b] Symlink layout cases skipped: OS.File.unixSymLink is unavailable.",
      );
      return;
    }
    console.log("[M3b] Static-link layout cases executing.");

    const rootLinkId = makeTestId();
    const rootTree = profileTestTree(rootLinkId);
    const rootTarget = rootTree.clone();
    rootTarget.append("owned-target");
    await IOUtils.makeDirectory(rootTarget.path, {
      createAncestors: true,
      ignoreExisting: true,
    });
    const linkedRoot = profileTestRoot(rootLinkId);
    await createSymlink(rootTarget.path, linkedRoot.path);
    await expectCode(openCache(rootLinkId), "cache-unsafe-layout");
    assert(linkedRoot.isSymlink(), "controlled root symlink is preserved");
    assert(rootTarget.isDirectory(), "root symlink target is preserved");
    assertProbeInert("root symlink rejection leaves source inert");

    const stageLinkId = makeTestId();
    const stageCache = await openCache(stageLinkId);
    const stageRoot = profileTestRoot(stageLinkId);
    const stageTarget = profileTestTree(stageLinkId);
    stageTarget.append("owned-target");
    const stageTargetBytes = new Uint8Array([8, 7, 6]);
    await IOUtils.write(stageTarget.path, stageTargetBytes, { mode: "create" });
    const stageLink = stageRoot.clone();
    stageLink.append("candidate.tmp");
    await createSymlink(stageTarget.path, stageLink.path);
    await expectCode(stageCache.put(bytes, policy()), "cache-unsafe-layout");
    assert(stageLink.isSymlink(), "staging link is preserved");
    assertBytesEqual(
      await IOUtils.read(stageTarget.path),
      stageTargetBytes,
      "staging link target is unchanged",
    );
    assertProbeInert("staging link rejection leaves source inert");

    const destinationLinkId = makeTestId();
    const destinationLinkCache = await openCache(destinationLinkId);
    const linkRoot = profileTestRoot(destinationLinkId);
    const linkTarget = linkRoot.clone();
    linkTarget.append("owned-target");
    const linkTargetBytes = new Uint8Array([9, 8, 7]);
    await IOUtils.write(linkTarget.path, linkTargetBytes, { mode: "create" });
    const destinationLink = linkRoot.clone();
    destinationLink.append(verified.packageSha256);
    await createSymlink(linkTarget.path, destinationLink.path);
    await expectCode(
      destinationLinkCache.put(bytes, policy()),
      "cache-unsafe-layout",
    );
    assert(destinationLink.isSymlink(), "destination link is preserved");
    assertBytesEqual(
      await IOUtils.read(linkTarget.path),
      linkTargetBytes,
      "destination link target is unchanged",
    );
    assertProbeInert("destination link rejection leaves source inert");
  });
}

async function testBoundedReadAndInvalidTestId(): Promise<void> {
  await withCacheTestScope(async (makeTestId, openCache) => {
    const testId = makeTestId();
    const cache = await openCache(testId);
    const oversizedName = "a".repeat(64);
    const oversized = profileTestRoot(testId);
    oversized.append(oversizedName);
    await IOUtils.write(oversized.path, new Uint8Array(2_097_152), {
      mode: "create",
    });
    assertEquals(
      (await IOUtils.read(oversized.path, { maxBytes: 1_048_577 })).byteLength,
      1_048_577,
      "selected Runtime read honors the explicit byte bound",
    );
    await expectCode(cache.read(oversizedName, policy()), "size-limit");
    assertProbeInert("oversized read rejection leaves source inert");

    let invalidTestId = false;
    try {
      await cacheModule.openTestProfileFeaturePackageCache(
        "hub-panel-menu",
        "../arbitrary",
      );
    } catch (error: unknown) {
      invalidTestId = error instanceof cacheModule.FeaturePackageCacheError &&
        error.code === "cache-invalid-test-id";
    }
    assert(invalidTestId, "test factory rejects arbitrary paths");
    assertProbeInert("invalid factory rejection leaves source inert");
  });
}

export function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "M3b cache put/read/reopen is inert",
      fn: testPutReadReopenAndInertSource,
    },
    {
      name: "M3b create-only staging and cleanup",
      fn: testCreateOnlyStagingAndCleanup,
    },
    { name: "M3b bounded enumeration", fn: testBoundedEnumeration },
    { name: "M3b unsafe layouts and links", fn: testUnsafeLayoutsAndLinks },
    {
      name: "M3b bounded read and test ID validation",
      fn: testBoundedReadAndInvalidTestId,
    },
  ];
  return runTests("M3bFeaturePackageCacheAcceptance.test.mts", tests);
}
