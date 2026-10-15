// SPDX-License-Identifier: MPL-2.0

import catalogJson from "./browser-catalog.json" with { type: "json" };
import type {
  BrowserInventoryCatalog,
  TestInventoryMetadata,
} from "./types.ts";

/** Exact IDs, shared by the existing host discovery and development loader. */
export function canonicalTestId(file: string): string {
  let value = file.replaceAll("\\", "/");
  for (
    const [alias, root] of [
      ["#features-chrome/", "browser-features/chrome/"],
      ["#features-modules/", "browser-features/modules/"],
      ["#features-pages/", "browser-features/"],
      ["/link-features-chrome/", "browser-features/chrome/"],
      ["link-features-chrome/", "browser-features/chrome/"],
    ]
  ) {
    if (value.startsWith(alias)) {
      value = root + value.slice(alias.length);
      break;
    }
  }
  if (
    value.length > 512 ||
    !/^[a-zA-Z0-9_.@/-]+$/.test(value) ||
    value.split("/").some((part) =>
      part === "" || part === "." || part === ".."
    )
  ) {
    throw new Error("Invalid test inventory identity");
  }
  return value;
}

export function browserDiscoverable(file: string): boolean {
  return (
    /^browser-features\/chrome\/(?:.*\/)?test\/.*\.test\.(?:ts|mts|tsx|js|mjs|jsx)$/
      .test(file) ||
    /^browser-features\/modules\/.*\.test\.(?:ts|mts|tsx|js|mjs|jsx)$/.test(
      file,
    ) ||
    /^browser-features\/pages-[^/]+\/.*\.test\.(?:ts|mts|tsx|js|mjs|jsx)$/.test(
      file,
    )
  );
}

export function makeBrowserCatalog(
  value: BrowserInventoryCatalog,
): ReadonlyMap<string, TestInventoryMetadata> {
  if (value.schemaVersion !== 1 || !Array.isArray(value.groups)) {
    throw new Error("Invalid browser inventory catalog version");
  }
  const entries = new Map<string, TestInventoryMetadata>();
  const pairs = new Set([
    "floorp/development-browser",
    "floorp/development-harness-compat",
    "mozilla-converted/development-mozilla-wrapper",
    "mozilla-adapted/development-mozilla-adaptation",
  ]);
  for (const group of value.groups) {
    if (
      !pairs.has(group.origin + "/" + group.executionKind) ||
      !Array.isArray(group.files) || group.files.length < 1
    ) {
      throw new Error("Invalid browser inventory classification");
    }
    for (const raw of group.files) {
      const id = canonicalTestId(raw);
      if (id !== raw || !browserDiscoverable(id) || entries.has(id)) {
        throw new Error("Duplicate or noncanonical browser inventory entry");
      }
      const expectedPair = id.includes("/firefox-downloaded/")
        ? "mozilla-converted/development-mozilla-wrapper"
        : id.includes("/firefox-imported/")
        ? "mozilla-adapted/development-mozilla-adaptation"
        : id.includes("/mochitest-compat/")
        ? "floorp/development-harness-compat"
        : "floorp/development-browser";
      if (group.origin + "/" + group.executionKind !== expectedPair) {
        throw new Error(
          "Catalog classification differs from source provenance",
        );
      }
      entries.set(
        id,
        Object.freeze({
          inventoryVersion: 1,
          origin: group.origin,
          executionKind: group.executionKind,
          packagedStatus: "not-onboarded",
        }),
      );
      if (entries.size > 4096) {
        throw new Error("Browser catalog bound exceeded");
      }
    }
  }
  return entries;
}

export const BROWSER_CATALOG = makeBrowserCatalog(
  catalogJson as BrowserInventoryCatalog,
);

export function testInventoryMetadata(
  file: string,
  catalog: ReadonlyMap<string, TestInventoryMetadata> = BROWSER_CATALOG,
): TestInventoryMetadata {
  const id = canonicalTestId(file);
  const browser = catalog.get(id);
  if (browser) return browser;
  if (/^(?:tools|libs|bridge)\//.test(id)) {
    return {
      inventoryVersion: 1,
      origin: "tooling",
      executionKind: "host-tooling",
      packagedStatus: "unsupported",
    };
  }
  return {
    inventoryVersion: 1,
    origin: "unknown",
    executionKind: "unsupported",
    packagedStatus: "unsupported",
  };
}
