// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  isChromeWebStoreUrl,
  extractExtensionId,
  generateFirefoxId,
  isPermissionSupported,
  isOptionalPermissionAllowed,
  EXTENSION_ID_SUFFIX,
} from "../Constants.sys.mts";

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Tests — isChromeWebStoreUrl
// ---------------------------------------------------------------------------

function testIsCWSUrlNewDomain(): void {
  assert(
    isChromeWebStoreUrl("https://chromewebstore.google.com/detail/ext/abc"),
    "new CWS domain should be recognized",
  );
}

function testIsCWSUrlOldDomain(): void {
  assert(
    isChromeWebStoreUrl("https://chrome.google.com/webstore/detail/ext/abc"),
    "old CWS domain should be recognized",
  );
}

function testIsCWSUrlUnrelated(): void {
  assertEquals(
    isChromeWebStoreUrl("https://example.com"),
    false,
    "unrelated URL should not match",
  );
}

function testIsCWSUrlEmpty(): void {
  assertEquals(isChromeWebStoreUrl(""), false, "empty string should not match");
}

// ---------------------------------------------------------------------------
// Tests — extractExtensionId
// ---------------------------------------------------------------------------

function testExtractIdNewUrl(): void {
  const id = "abcdefghijklmnopqrstuvwxyzabcdef"; // 32 lowercase letters
  const url = `https://chromewebstore.google.com/detail/some-extension/${id}`;
  assertEquals(extractExtensionId(url), id, "should extract ID from new URL");
}

function testExtractIdOldUrl(): void {
  const id = "abcdefghijklmnopqrstuvwxyzabcdef";
  const url = `https://chrome.google.com/webstore/detail/some-extension/${id}`;
  assertEquals(extractExtensionId(url), id, "should extract ID from old URL");
}

function testExtractIdInvalidUrl(): void {
  assertEquals(
    extractExtensionId("https://example.com/detail/ext/abc"),
    null,
    "non-CWS URL should return null",
  );
}

function testExtractIdTooShort(): void {
  assertEquals(
    extractExtensionId("https://chromewebstore.google.com/detail/ext/abc"),
    null,
    "ID shorter than 32 chars should return null",
  );
}

function testExtractIdWithUppercase(): void {
  // 32 chars but with uppercase — should not match [a-z]{32}
  const mixedId = "ABCDEFGHijklmnopqrstuvwxyzabcdef";
  assertEquals(
    extractExtensionId(
      `https://chromewebstore.google.com/detail/ext/${mixedId}`,
    ),
    null,
    "mixed-case ID should return null",
  );
}

// ---------------------------------------------------------------------------
// Tests — generateFirefoxId
// ---------------------------------------------------------------------------

function testGenerateFirefoxId(): void {
  const chromeId = "abcdefghijklmnopqrstuvwxyzabcdef";
  const expected = `${chromeId}${EXTENSION_ID_SUFFIX}`;
  assertEquals(
    generateFirefoxId(chromeId),
    expected,
    "should append suffix to chrome ID",
  );
}

// ---------------------------------------------------------------------------
// Tests — isPermissionSupported
// ---------------------------------------------------------------------------

function testSupportedPermission(): void {
  assert(isPermissionSupported("tabs"), '"tabs" should be supported');
}

function testUnsupportedPermission(): void {
  assertEquals(
    isPermissionSupported("tabCapture"),
    false,
    '"tabCapture" should not be supported',
  );
}

function testUnsupportedPermissionEnterprise(): void {
  assertEquals(
    isPermissionSupported("enterprise.platformKeys"),
    false,
    '"enterprise.platformKeys" should not be supported',
  );
}

// ---------------------------------------------------------------------------
// Tests — isOptionalPermissionAllowed
// ---------------------------------------------------------------------------

function testOptionalPermissionAllowed(): void {
  assert(
    isOptionalPermissionAllowed("tabs"),
    '"tabs" should be allowed as optional',
  );
}

function testOptionalPermissionContextMenus(): void {
  assertEquals(
    isOptionalPermissionAllowed("contextMenus"),
    false,
    '"contextMenus" should not be allowed as optional in Firefox',
  );
}

function testOptionalPermissionUnsupported(): void {
  assertEquals(
    isOptionalPermissionAllowed("tabCapture"),
    false,
    '"tabCapture" should not be allowed as optional',
  );
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "isCWSUrl new domain", fn: testIsCWSUrlNewDomain },
    { name: "isCWSUrl old domain", fn: testIsCWSUrlOldDomain },
    { name: "isCWSUrl unrelated", fn: testIsCWSUrlUnrelated },
    { name: "isCWSUrl empty", fn: testIsCWSUrlEmpty },
    { name: "extractExtensionId new URL", fn: testExtractIdNewUrl },
    { name: "extractExtensionId old URL", fn: testExtractIdOldUrl },
    { name: "extractExtensionId invalid URL", fn: testExtractIdInvalidUrl },
    { name: "extractExtensionId too short", fn: testExtractIdTooShort },
    { name: "extractExtensionId uppercase", fn: testExtractIdWithUppercase },
    { name: "generateFirefoxId", fn: testGenerateFirefoxId },
    { name: "supported permission", fn: testSupportedPermission },
    { name: "unsupported permission", fn: testUnsupportedPermission },
    {
      name: "unsupported enterprise permission",
      fn: testUnsupportedPermissionEnterprise,
    },
    { name: "optional permission allowed", fn: testOptionalPermissionAllowed },
    {
      name: "optional permission contextMenus",
      fn: testOptionalPermissionContextMenus,
    },
    {
      name: "optional permission unsupported",
      fn: testOptionalPermissionUnsupported,
    },
  ];

  await runTests("Constants.test.mts", tests);
}
