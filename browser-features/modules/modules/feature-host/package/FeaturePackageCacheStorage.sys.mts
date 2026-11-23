// SPDX-License-Identifier: MPL-2.0

import { FeaturePackageCacheError } from "./FeaturePackageCache.ts";
import type {
  FeaturePackageCacheEntry,
  FeaturePackageCacheStorage,
} from "./types.ts";

const ROOT_NAME = "floorp-signed-package-cache-v1";
const STAGING_NAME = "candidate.tmp";

function failUnsafe(): never {
  throw new FeaturePackageCacheError("cache-unsafe-layout");
}

function fileKind(file: nsIFile): FeaturePackageCacheEntry["kind"] {
  if (file.isSymlink()) return "link";
  if (file.isFile()) return "file";
  if (file.isDirectory()) return "directory";
  return "other";
}

function ensureDirectory(
  file: nsIFile,
  onCreated?: (directory: nsIFile) => void,
): void {
  if (file.exists()) {
    if (file.isSymlink() || !file.isDirectory()) failUnsafe();
    return;
  }
  const createdDirectory = file.clone();
  file.create(Components.interfaces.nsIFile.DIRECTORY_TYPE!, 0o700);
  onCreated?.(createdDirectory);
  if (file.isSymlink() || !file.isDirectory()) failUnsafe();
}

function child(parent: nsIFile, name: string): nsIFile {
  const result = parent.clone();
  result.append(name);
  return result;
}

export class GeckoFeaturePackageCacheStorage
  implements FeaturePackageCacheStorage {
  constructor(
    private readonly root: nsIFile,
    private readonly directories: readonly nsIFile[],
    private readonly onDirectoryCreated?: (directory: nsIFile) => void,
  ) {}

  initialize(): Promise<void> {
    const profile = Services.dirsvc.get(
      "ProfD",
      Components.interfaces.nsIFile,
    );
    if (profile.isSymlink() || !profile.isDirectory()) failUnsafe();
    for (const directory of this.directories) {
      ensureDirectory(directory, this.onDirectoryCreated);
    }
    return Promise.resolve();
  }

  inspect(name: string): Promise<FeaturePackageCacheEntry | null> {
    const file = child(this.root, name);
    if (!file.exists()) return Promise.resolve(null);
    return Promise.resolve({ name: file.leafName, kind: fileKind(file) });
  }

  async read(name: string, maxBytes: number): Promise<Uint8Array> {
    return await IOUtils.read(child(this.root, name).path, { maxBytes });
  }

  enumerate(
    maxEntries: number,
  ): Promise<readonly FeaturePackageCacheEntry[]> {
    const enumerator = this.root.directoryEntries;
    const entries: FeaturePackageCacheEntry[] = [];
    try {
      for (let index = 0; index < maxEntries; index++) {
        const file = enumerator.nextFile;
        if (file === null) break;
        entries.push({ name: file.leafName, kind: fileKind(file) });
      }
      return Promise.resolve(entries);
    } finally {
      enumerator.close();
    }
  }

  async writeStaging(bytes: Uint8Array): Promise<void> {
    await IOUtils.write(
      child(this.root, STAGING_NAME).path,
      bytes,
      { mode: "create", flush: true },
    );
  }

  async moveStaging(name: string): Promise<void> {
    await IOUtils.move(
      child(this.root, STAGING_NAME).path,
      child(this.root, name).path,
      { noOverwrite: true },
    );
  }

  async removeStaging(): Promise<void> {
    const staging = child(this.root, STAGING_NAME);
    if (!staging.exists()) return;
    if (fileKind(staging) !== "file") failUnsafe();
    await IOUtils.remove(staging.path);
  }
}

export function createProfileFeaturePackageCacheStorage(
  featureId: string,
  testId?: string,
  onDirectoryCreated?: (directory: nsIFile) => void,
): {
  readonly storage: GeckoFeaturePackageCacheStorage;
  readonly ownerKey: string;
} {
  const profile = Services.dirsvc.get(
    "ProfD",
    Components.interfaces.nsIFile,
  );
  const namespace = child(profile, ROOT_NAME);
  const category = child(namespace, testId ? "tests" : "features");
  const testRoot = testId ? child(category, testId) : undefined;
  const root = child(testRoot ?? category, featureId);
  const directories = [
    namespace,
    category,
    ...(testRoot ? [testRoot] : []),
    root,
  ];
  const normalized = [
    profile.path.replaceAll("\\", "/"),
    ROOT_NAME,
    testId ? "tests" : "features",
    ...(testId ? [testId] : []),
    featureId,
  ].join("/");
  const ownerKey = Services.appinfo.OS === "WINNT"
    ? normalized.toLowerCase()
    : normalized;
  return {
    storage: new GeckoFeaturePackageCacheStorage(
      root,
      directories,
      onDirectoryCreated,
    ),
    ownerKey,
  };
}
