// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";
import type {
  UserScriptsPermissionAddon,
  UserScriptsPermissionAddonManager,
  UserScriptsPermissionCase,
  UserScriptsPermissionExtensionParent,
} from "./helpers/user-scripts-permission-test-types.ts";

async function checkPermission(test: UserScriptsPermissionCase): Promise<void> {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  ) as unknown as { AddonManager: UserScriptsPermissionAddonManager };
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  ) as unknown as { ExtensionParent: UserScriptsPermissionExtensionParent };
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const originalId = test.letter.repeat(32);
  const id = test.metadata === "mismatched-id"
    ? `${"p".repeat(32)}@chromium-extension`
    : test.metadata === "missing"
    ? `ordinary-user-scripts-${test.letter}@floorp.test`
    : `${originalId}@chromium-extension`;
  const gecko = {
    id,
    ...(test.metadata === "missing" ? {} : {
      floorp_chrome_extension: { schema_version: 1, original_id: originalId },
    }),
  };
  const path = PathUtils.join(
    PathUtils.tempDir,
    `user-scripts-permission-${Services.uuid.generateUUID()}.xpi`,
  );
  const manifest = {
    manifest_version: test.manifestVersion,
    name: "Authored required and optional userScripts permission boundary",
    version: "1.0",
    permissions: test.required ? ["userScripts"] : [],
    optional_permissions: test.required ? [] : ["userScripts"],
    browser_specific_settings: { gecko },
  };
  let addon: UserScriptsPermissionAddon | undefined;
  try {
    await IOUtils.write(
      path,
      zipSync({
        "manifest.json": new TextEncoder().encode(JSON.stringify(manifest)),
      }),
    );
    addon = await AddonManager.installTemporaryAddon(new FileUtils.File(path));
    const extension = ExtensionParent.GlobalManager.extensionMap.get(id);
    assert(extension, "the actual installed extension has a native parent");
    assertEquals(
      extension.manifest.permissions.includes("userScripts"),
      test.expectedGranted,
      "normalization retains only explicitly supported required declarations",
    );
    assertEquals(
      extension.permissions.has("userScripts"),
      test.expectedGranted,
      "native active permission set follows the normalized declaration",
    );
    assertEquals(
      extension.hasPermission("userScripts"),
      test.expectedGranted,
      "ordinary native permission authorization remains authoritative",
    );
    assertEquals(
      extension.getRequiredPermissions()?.permissions.includes("userScripts"),
      test.expectedGranted,
      "normal required-permission consent description includes the permission",
    );
    assertEquals(
      extension.manifest.optional_permissions.includes("userScripts"),
      !test.required,
      "optional declarations are retained without being silently granted",
    );
  } finally {
    await addon?.uninstall();
    await IOUtils.remove(path, { ignoreAbsent: true });
  }
}

export function runAllTests(): Promise<void> {
  const cases: UserScriptsPermissionCase[] = [
    {
      name:
        "exact marked MV3 required userScripts survives native normalization",
      letter: "a",
      metadata: "valid",
      required: true,
      expectedGranted: true,
      manifestVersion: 3,
    },
    {
      name: "ordinary Firefox required userScripts remains optional-only",
      letter: "b",
      metadata: "missing",
      required: true,
      expectedGranted: false,
      manifestVersion: 3,
    },
    {
      name: "metadata and installed ID mismatch cannot enable the permission",
      letter: "c",
      metadata: "mismatched-id",
      required: true,
      expectedGranted: false,
      manifestVersion: 3,
    },
    {
      name: "marked optional userScripts remains ungranted",
      letter: "d",
      metadata: "valid",
      required: false,
      expectedGranted: false,
      manifestVersion: 3,
    },
    {
      name: "ordinary optional userScripts remains ungranted",
      letter: "e",
      metadata: "missing",
      required: false,
      expectedGranted: false,
      manifestVersion: 3,
    },
    {
      name: "MV2 keeps its existing legacy user_scripts permission contract",
      letter: "f",
      metadata: "valid",
      required: true,
      expectedGranted: false,
      manifestVersion: 2,
    },
  ];
  return runTests(
    "ChromeUserScriptsPermissionsNative.test.mts",
    cases.map((test) => ({ name: test.name, fn: () => checkPermission(test) })),
  );
}
