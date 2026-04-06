// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { WorkspaceIcons } from "../utils/workspace-icons.ts";
import {
  type TestCase,
  assert,
  assertEquals,
} from "../../../test/utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

let icons: WorkspaceIcons;

function setup(): void {
  icons = new WorkspaceIcons();
}

function testWorkspaceIconsSetNotEmpty(): void {
  setup();
  assert(icons.workspaceIcons.size > 0, "icon set should not be empty");
}

function testWorkspaceIconsArrayLength(): void {
  setup();
  assertEquals(
    icons.workspaceIconsArray.length,
    icons.workspaceIcons.size,
    "array length should match set size",
  );
}

function testKnownIconsExist(): void {
  setup();
  const expected = ["briefcase", "star", "fingerprint", "code", "book"];
  for (const name of expected) {
    assert(icons.workspaceIcons.has(name), `"${name}" should be in icon set`);
  }
}

function testGetWorkspaceIconUrlValid(): void {
  setup();
  const url = icons.getWorkspaceIconUrl("fingerprint");
  assert(
    typeof url === "string" && url.startsWith("data:image/svg+xml;base64,"),
    "valid icon should return data URL",
  );
}

function testGetWorkspaceIconUrlNull(): void {
  setup();
  const url = icons.getWorkspaceIconUrl(null);
  assert(
    typeof url === "string" && url.length > 0,
    "null icon should return fallback (fingerprint)",
  );
}

function testGetWorkspaceIconUrlUndefined(): void {
  setup();
  const url = icons.getWorkspaceIconUrl(undefined);
  assert(
    typeof url === "string" && url.length > 0,
    "undefined icon should return fallback",
  );
}

function testGetWorkspaceIconUrlInvalid(): void {
  setup();
  const url = icons.getWorkspaceIconUrl("nonexistent-icon");
  assert(
    typeof url === "string",
    "invalid icon name should return fallback string",
  );
}

function testAllIconsResolveToDataUrls(): void {
  setup();
  for (const name of icons.workspaceIconsArray) {
    const url = icons.getWorkspaceIconUrl(name);
    assert(
      typeof url === "string" && url.startsWith("data:"),
      `icon "${name}" should resolve to a data URL`,
    );
  }
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "icon set not empty", fn: testWorkspaceIconsSetNotEmpty },
    { name: "array length matches set", fn: testWorkspaceIconsArrayLength },
    { name: "known icons exist", fn: testKnownIconsExist },
    { name: "getWorkspaceIconUrl valid", fn: testGetWorkspaceIconUrlValid },
    { name: "getWorkspaceIconUrl null", fn: testGetWorkspaceIconUrlNull },
    {
      name: "getWorkspaceIconUrl undefined",
      fn: testGetWorkspaceIconUrlUndefined,
    },
    { name: "getWorkspaceIconUrl invalid", fn: testGetWorkspaceIconUrlInvalid },
    {
      name: "all icons resolve to data URLs",
      fn: testAllIconsResolveToDataUrls,
    },
  ];

  const failures: string[] = [];

  for (const test of tests) {
    try {
      test.fn();
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      failures.push(`${test.name}: ${message}`);
    }
  }

  if (failures.length > 0) {
    throw new Error(`workspaceIcons.test.ts failures: ${failures.join(" | ")}`);
  }
}
