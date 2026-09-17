// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  WORKSPACE_ICON_REGISTRY,
  WorkspaceIcons,
} from "../utils/workspace-icons.ts";
import { workspaceIconTranslationKeys } from "../utils/icon-translations.ts";
import {
  assert,
  assertEquals,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

const EXPECTED_ALIASES = [
  "article",
  "book",
  "briefcase",
  "cart",
  "chat",
  "chill",
  "circle",
  "compass",
  "code",
  "dollar",
  "fence",
  "fingerprint",
  "food",
  "fruit",
  "game",
  "gear",
  "gift",
  "key",
  "lightning",
  "network",
  "notes",
  "paint",
  "photo",
  "pin",
  "pet",
  "question",
  "smartphone",
  "star",
  "tree",
  "vacation",
  "love",
  "moon",
  "music",
  "user",
] as const;

function testRegistryIsExactAndUnique(): void {
  assertEquals(WORKSPACE_ICON_REGISTRY.length, 34, "registry has 34 entries");
  assertEquals(
    new Set(WORKSPACE_ICON_REGISTRY.map((entry) => entry.slug)).size,
    34,
    "slugs are unique",
  );
  assertEquals(
    new Set(WORKSPACE_ICON_REGISTRY.map((entry) => entry.alias)).size,
    34,
    "aliases are unique",
  );
  assertEquals(
    new Set(WORKSPACE_ICON_REGISTRY.map((entry) => entry.canonicalId)).size,
    34,
    "canonical IDs are unique",
  );
  assertEquals(
    new Set(WORKSPACE_ICON_REGISTRY.map((entry) => entry.assetPath)).size,
    34,
    "bundled assets are one-to-one",
  );
  assertEquals(
    JSON.stringify(WORKSPACE_ICON_REGISTRY.map((entry) => entry.alias)),
    JSON.stringify(EXPECTED_ALIASES),
    "all legacy aliases are preserved exactly",
  );
}

function testRegistryMetadataMatchesSlugs(): void {
  assertEquals(
    Object.keys(workspaceIconTranslationKeys).length,
    34,
    "translation map has 34 keys",
  );
  for (const entry of WORKSPACE_ICON_REGISTRY) {
    assertEquals(entry.alias, entry.slug, `${entry.slug} alias`);
    assertEquals(
      entry.canonicalId,
      `floorp-icon:v1:${entry.slug}`,
      `${entry.slug} canonical ID`,
    );
    assertEquals(
      entry.assetPath,
      `../icons/${entry.slug}.svg`,
      `${entry.slug} bundled asset`,
    );
    assertEquals(
      entry.labelKey,
      workspaceIconTranslationKeys[entry.slug],
      `${entry.slug} translation key`,
    );
    assert(entry.keywords.length > 0, `${entry.slug} has keywords`);
  }
}

function testAliasesAndCanonicalIdsResolveIdentically(): void {
  const icons = new WorkspaceIcons();
  assertEquals(
    icons.workspaceIconsArray.length,
    34,
    "legacy set has 34 aliases",
  );
  for (const entry of WORKSPACE_ICON_REGISTRY) {
    const aliasUrl = icons.getWorkspaceIconUrl(entry.alias);
    const canonicalUrl = icons.getWorkspaceIconUrl(entry.canonicalId);
    assertEquals(
      aliasUrl,
      canonicalUrl,
      `${entry.slug} forms resolve identically`,
    );
    assert(
      canonicalUrl.startsWith("data:image/svg+xml;base64,"),
      `${entry.slug} resolves to a bundled SVG data URL`,
    );
    assertEquals(
      icons.getWorkspaceIconCanonicalId(entry.alias),
      entry.canonicalId,
      `${entry.slug} alias canonicalizes only for display`,
    );
  }
}

function testUnsafeAndNonExactValuesUseFallback(): void {
  const icons = new WorkspaceIcons();
  const fallback = icons.getWorkspaceIconUrl("fingerprint");
  const invalidValues: unknown[] = [
    null,
    undefined,
    "",
    "Article",
    " article",
    "article ",
    "floorp-icon:v1:Article",
    "floorp-icon:v1:article ",
    "floorp-icon:v2:article",
    "floorp-icon:v1:unknown",
    "https://example.invalid/icon.svg",
    "chrome://browser/content/icon.svg",
    "data:text/html;base64,PHNjcmlwdD4=",
    "data:image/svg+xml,<svg onload='fetch(1)'></svg>",
  ];
  for (const value of invalidValues) {
    const resolved = icons.getWorkspaceIconUrl(value);
    assertEquals(
      resolved,
      fallback,
      `${String(value)} uses fingerprint fallback`,
    );
    assert(
      resolved.startsWith("data:image/svg+xml;base64,"),
      "fallback is always a bundled SVG data URL",
    );
  }
  assertEquals(
    icons.getWorkspaceIconCanonicalId("https://example.invalid/icon.svg"),
    "floorp-icon:v1:fingerprint",
    "unsafe raw value maps only to the fallback display ID",
  );
}

export function runAllTests(): void {
  const tests: TestCase[] = [
    { name: "registry is exact and unique", fn: testRegistryIsExactAndUnique },
    {
      name: "registry metadata matches slugs",
      fn: testRegistryMetadataMatchesSlugs,
    },
    {
      name: "aliases and canonical IDs resolve identically",
      fn: testAliasesAndCanonicalIdsResolveIdentically,
    },
    {
      name: "unsafe and non-exact values use fallback",
      fn: testUnsafeAndNonExactValuesUseFallback,
    },
  ];

  const failures: string[] = [];
  for (const test of tests) {
    try {
      test.fn();
    } catch (error) {
      failures.push(
        `${test.name}: ${
          error instanceof Error ? error.message : String(error)
        }`,
      );
    }
  }
  if (failures.length > 0) {
    throw new Error(`workspaceIcons.test.ts failures: ${failures.join(" | ")}`);
  }
}
