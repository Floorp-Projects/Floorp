// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  zWorkspaceDetail,
  zWorkspacePreference,
  zWindowWorkspaces,
  zFloorp11WorkspacesSchema,
} from "../data/migrate/old_type.ts";
import {
  type TestCase,
  type assert,
  assertEquals,
} from "../../../test/utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Tests — zWorkspaceDetail
// ---------------------------------------------------------------------------

function testWorkspaceDetailValid(): void {
  const input = {
    name: "Work",
    tabs: [],
    defaultWorkspace: true,
    id: "ws-1",
    icon: "briefcase",
  };
  const result = zWorkspaceDetail.decode(input);
  assertEquals(result._tag, "Right", "valid workspace detail should decode");
}

function testWorkspaceDetailWithOptional(): void {
  const input = {
    name: "Private",
    tabs: [],
    defaultWorkspace: false,
    id: "ws-2",
    icon: null,
    userContextId: 3,
    isPrivateContainerWorkspace: true,
  };
  const result = zWorkspaceDetail.decode(input);
  assertEquals(
    result._tag,
    "Right",
    "workspace detail with optional fields should decode",
  );
}

function testWorkspaceDetailMissingName(): void {
  const input = {
    tabs: [],
    defaultWorkspace: true,
    id: "ws-1",
    icon: null,
  };
  const result = zWorkspaceDetail.decode(input);
  assertEquals(result._tag, "Left", "missing name should fail");
}

function testWorkspaceDetailWrongType(): void {
  const input = {
    name: 123, // should be string
    tabs: [],
    defaultWorkspace: true,
    id: "ws-1",
    icon: null,
  };
  const result = zWorkspaceDetail.decode(input);
  assertEquals(result._tag, "Left", "wrong type should fail");
}

// ---------------------------------------------------------------------------
// Tests — zWorkspacePreference
// ---------------------------------------------------------------------------

function testWorkspacePreferenceValid(): void {
  const input = {
    selectedWorkspaceId: "ws-1",
    defaultWorkspace: "ws-1",
  };
  const result = zWorkspacePreference.decode(input);
  assertEquals(result._tag, "Right", "valid preference should decode");
}

function testWorkspacePreferenceEmpty(): void {
  const result = zWorkspacePreference.decode({});
  assertEquals(
    result._tag,
    "Right",
    "empty object should decode (all optional)",
  );
}

// ---------------------------------------------------------------------------
// Tests — zWindowWorkspaces (recordFromString)
// ---------------------------------------------------------------------------

function testWindowWorkspacesValid(): void {
  const input = {
    "ws-1": {
      name: "Work",
      tabs: [],
      defaultWorkspace: true,
      id: "ws-1",
      icon: null,
    },
    preference: {
      selectedWorkspaceId: "ws-1",
    },
  };
  const result = zWindowWorkspaces.decode(input);
  assertEquals(result._tag, "Right", "valid window workspaces should decode");
}

function testWindowWorkspacesNonObject(): void {
  const result = zWindowWorkspaces.decode("not-an-object");
  assertEquals(result._tag, "Left", "non-object should fail");
}

// ---------------------------------------------------------------------------
// Tests — zFloorp11WorkspacesSchema
// ---------------------------------------------------------------------------

function testFullSchemaValid(): void {
  const input = {
    windows: {
      "window-1": {
        "ws-1": {
          name: "Default",
          tabs: [],
          defaultWorkspace: true,
          id: "ws-1",
          icon: null,
        },
      },
    },
  };
  const result = zFloorp11WorkspacesSchema.decode(input);
  assertEquals(result._tag, "Right", "full schema should decode");
}

function testFullSchemaMissingWindows(): void {
  const result = zFloorp11WorkspacesSchema.decode({});
  assertEquals(result._tag, "Left", "missing windows should fail");
}

function testFullSchemaEmptyWindows(): void {
  const result = zFloorp11WorkspacesSchema.decode({ windows: {} });
  assertEquals(result._tag, "Right", "empty windows should decode");
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "workspace detail valid", fn: testWorkspaceDetailValid },
    {
      name: "workspace detail with optional",
      fn: testWorkspaceDetailWithOptional,
    },
    {
      name: "workspace detail missing name",
      fn: testWorkspaceDetailMissingName,
    },
    { name: "workspace detail wrong type", fn: testWorkspaceDetailWrongType },
    { name: "workspace preference valid", fn: testWorkspacePreferenceValid },
    { name: "workspace preference empty", fn: testWorkspacePreferenceEmpty },
    { name: "window workspaces valid", fn: testWindowWorkspacesValid },
    { name: "window workspaces non-object", fn: testWindowWorkspacesNonObject },
    { name: "full schema valid", fn: testFullSchemaValid },
    { name: "full schema missing windows", fn: testFullSchemaMissingWindows },
    { name: "full schema empty windows", fn: testFullSchemaEmptyWindows },
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
    throw new Error(
      `workspacesOldType.test.ts failures: ${failures.join(" | ")}`,
    );
  }
}
