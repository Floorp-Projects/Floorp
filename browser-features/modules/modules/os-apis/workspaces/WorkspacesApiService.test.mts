// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  buildWorkspaceInfo,
  type StoredWorkspaceInfo,
} from "./WorkspacesApiService.sys.mts";
import {
  assert,
  assertEquals,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";

const makeWorkspace = (): Omit<StoredWorkspaceInfo, "icon"> => ({
  name: "API workspace",
  userContextId: 3,
});

function testApiPreservesRawIconCategories(): void {
  const cases: Array<[string, StoredWorkspaceInfo]> = [
    ["absent", makeWorkspace()],
    ["own undefined", { ...makeWorkspace(), icon: undefined }],
    ["null", { ...makeWorkspace(), icon: null }],
    ["alias", { ...makeWorkspace(), icon: "article" }],
    ["canonical", { ...makeWorkspace(), icon: "floorp-icon:v1:article" }],
    ["opaque", { ...makeWorkspace(), icon: "future:value" }],
    ["URI", { ...makeWorkspace(), icon: "https://example.invalid/icon.svg" }],
  ];
  for (const [label, workspace] of cases) {
    const result = buildWorkspaceInfo("id", workspace, "id", "id");
    assertEquals(
      Object.hasOwn(result, "icon"),
      Object.hasOwn(workspace, "icon"),
      `${label} presence`,
    );
    if (Object.hasOwn(workspace, "icon")) {
      assertEquals(result.icon, workspace.icon, `${label} value`);
    }
    assertEquals(result.isDefault, true, `${label} default flag`);
    assertEquals(result.isSelected, true, `${label} selected flag`);
  }
}

function testApiJsonOmitsUndefinedWithoutCreatingNull(): void {
  const absent = JSON.parse(
    JSON.stringify(buildWorkspaceInfo("id", makeWorkspace(), "other", null)),
  ) as Record<string, unknown>;
  const ownUndefined = JSON.parse(
    JSON.stringify(
      buildWorkspaceInfo(
        "id",
        { ...makeWorkspace(), icon: undefined },
        "other",
        null,
      ),
    ),
  ) as Record<string, unknown>;
  const explicitNull = JSON.parse(
    JSON.stringify(
      buildWorkspaceInfo(
        "id",
        { ...makeWorkspace(), icon: null },
        "other",
        null,
      ),
    ),
  ) as Record<string, unknown>;
  assert(!Object.hasOwn(absent, "icon"), "absent API icon is omitted");
  assert(
    !Object.hasOwn(ownUndefined, "icon"),
    "undefined API icon is omitted, never converted to null",
  );
  assertEquals(explicitNull.icon, null, "explicit API null remains null");
}

export function runAllTests(): void {
  const tests: TestCase[] = [
    {
      name: "API preserves raw icon categories",
      fn: testApiPreservesRawIconCategories,
    },
    {
      name: "API JSON omits undefined without creating null",
      fn: testApiJsonOmitsUndefinedWithoutCreatingNull,
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
    throw new Error(
      `WorkspacesApiService.test.mts failures: ${failures.join(" | ")}`,
    );
  }
}
