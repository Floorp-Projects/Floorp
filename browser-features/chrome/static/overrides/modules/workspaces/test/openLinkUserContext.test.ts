// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { resolveWorkspaceOpenLinkUserContext } from "../open-link-user-context.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../../test/utils/test_harness.ts";

function assertSameOwnProperties(
  actual: Record<string, unknown>,
  expected: Record<string, unknown>,
  message: string,
): void {
  const actualKeys = Object.keys(actual).sort();
  const expectedKeys = Object.keys(expected).sort();

  assertEquals(
    actualKeys.join("\n"),
    expectedKeys.join("\n"),
    `${message}: own keys should match`,
  );

  for (const key of expectedKeys) {
    assertEquals(
      actual[key],
      expected[key],
      `${message}: ${key} should match`,
    );
  }
}

function testCurrentWithoutOptionsOmitsUserContextId(): void {
  const resolution = resolveWorkspaceOpenLinkUserContext(
    undefined,
    "current",
    7,
  );

  assert(
    !Object.hasOwn(resolution.options, "userContextId"),
    "current navigation without options should omit userContextId",
  );
  assertEquals(
    resolution.shouldApplyWorkspaceContainer,
    false,
    "current navigation should not apply the workspace container",
  );
}

function testCurrentWithTargetBrowserOmitsUserContextId(): void {
  const targetBrowser = { userContextId: 4 };
  const resolution = resolveWorkspaceOpenLinkUserContext(
    { targetBrowser },
    "current",
    7,
  );

  assert(
    !Object.hasOwn(resolution.options, "userContextId"),
    "current target-browser navigation should omit top-level userContextId",
  );
  assertEquals(
    resolution.options.targetBrowser,
    targetBrowser,
    "targetBrowser should be preserved",
  );
}

function testCurrentWithUndefinedUserContextIdOmitsKey(): void {
  const input = { userContextId: undefined, marker: "preserved" };
  const resolution = resolveWorkspaceOpenLinkUserContext(
    input,
    "current",
    7,
  );

  assert(
    !Object.hasOwn(resolution.options, "userContextId"),
    "current navigation should remove an undefined userContextId",
  );
  assert(
    Object.hasOwn(input, "userContextId"),
    "resolving should not mutate the input object",
  );
  assertEquals(
    resolution.options.marker,
    "preserved",
    "unrelated options should be preserved",
  );
}

function testExplicitNumericUserContextIdsArePreserved(): void {
  for (const where of ["current", "tab"]) {
    for (const userContextId of [0, 5]) {
      const resolution = resolveWorkspaceOpenLinkUserContext(
        {
          userContextId,
          targetBrowser: { userContextId: 9 },
        },
        where,
        7,
      );

      assert(
        Object.hasOwn(resolution.options, "userContextId"),
        `${where} should retain explicit userContextId ${userContextId}`,
      );
      assertEquals(
        resolution.options.userContextId,
        userContextId,
        `${where} should preserve explicit userContextId ${userContextId}`,
      );
    }
  }
}

function testNonCurrentWorkspaceInjection(): void {
  const withoutTarget = resolveWorkspaceOpenLinkUserContext({}, "tab", 7);
  const targetBrowser = { currentURI: "https://example.com/" };
  const withTargetWithoutDirectId = resolveWorkspaceOpenLinkUserContext(
    { targetBrowser },
    "tabshifted",
    7,
  );

  assertEquals(
    withoutTarget.options.userContextId,
    7,
    "non-current navigation should apply the workspace container",
  );
  assertEquals(
    withTargetWithoutDirectId.options.userContextId,
    7,
    "a targetBrowser without a direct ID should not block workspace injection",
  );
  assertEquals(
    withTargetWithoutDirectId.options.targetBrowser,
    targetBrowser,
    "targetBrowser without a direct ID should be preserved",
  );
  assertEquals(
    withTargetWithoutDirectId.shouldApplyWorkspaceContainer,
    true,
    "workspace injection should be reported for a target without a direct ID",
  );
}

function testNonCurrentTargetBrowserUserContextIdTakesPrecedence(): void {
  const resolution = resolveWorkspaceOpenLinkUserContext(
    { targetBrowser: { userContextId: 3 } },
    "tab",
    7,
  );

  assertEquals(
    resolution.options.userContextId,
    3,
    "targetBrowser userContextId should take precedence over fallback",
  );
  assertEquals(
    resolution.shouldApplyWorkspaceContainer,
    false,
    "a targetBrowser userContextId should block workspace injection",
  );
}

function testNonCurrentUndefinedUserContextIdRetainsFallback(): void {
  const input = { userContextId: undefined };
  const resolution = resolveWorkspaceOpenLinkUserContext(input, "tab", 7);

  assertEquals(
    resolution.options.userContextId,
    0,
    "an own undefined userContextId should retain the existing fallback",
  );
  assertEquals(
    resolution.shouldApplyWorkspaceContainer,
    false,
    "an own undefined userContextId should block workspace injection",
  );
  assertEquals(
    input.userContextId,
    undefined,
    "resolving should not mutate the undefined input value",
  );
}

function testWorkspaceZeroUsesFallback(): void {
  const resolution = resolveWorkspaceOpenLinkUserContext({}, "tab", 0);

  assertEquals(
    resolution.options.userContextId,
    0,
    "workspace ID 0 should retain the existing fallback",
  );
  assertEquals(
    resolution.shouldApplyWorkspaceContainer,
    false,
    "workspace ID 0 should not be treated as an injected container",
  );
}

function testResolutionIsIdempotent(): void {
  const currentOnce = resolveWorkspaceOpenLinkUserContext(
    { targetBrowser: { userContextId: 3 }, marker: "current" },
    "current",
    7,
  ).options;
  const currentTwice = resolveWorkspaceOpenLinkUserContext(
    currentOnce,
    "current",
    7,
  ).options;

  assertSameOwnProperties(
    currentTwice,
    currentOnce,
    "applying the current resolution twice",
  );
  assert(
    currentTwice !== currentOnce,
    "current resolution should return a fresh object",
  );

  const tabOnce = resolveWorkspaceOpenLinkUserContext(
    { marker: "tab" },
    "tab",
    7,
  ).options;
  const tabTwice = resolveWorkspaceOpenLinkUserContext(
    tabOnce,
    "tab",
    7,
  ).options;

  assertSameOwnProperties(
    tabTwice,
    tabOnce,
    "applying the non-current resolution twice",
  );
  assert(tabTwice !== tabOnce, "non-current resolution should be fresh");
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "current without options omits userContextId",
      fn: testCurrentWithoutOptionsOmitsUserContextId,
    },
    {
      name: "current targetBrowser omits userContextId",
      fn: testCurrentWithTargetBrowserOmitsUserContextId,
    },
    {
      name: "current undefined userContextId omits key",
      fn: testCurrentWithUndefinedUserContextIdOmitsKey,
    },
    {
      name: "explicit numeric userContextIds are preserved",
      fn: testExplicitNumericUserContextIdsArePreserved,
    },
    {
      name: "non-current workspace injection",
      fn: testNonCurrentWorkspaceInjection,
    },
    {
      name: "non-current targetBrowser userContextId",
      fn: testNonCurrentTargetBrowserUserContextIdTakesPrecedence,
    },
    {
      name: "non-current undefined userContextId fallback",
      fn: testNonCurrentUndefinedUserContextIdRetainsFallback,
    },
    {
      name: "workspace ID 0 fallback",
      fn: testWorkspaceZeroUsesFallback,
    },
    {
      name: "resolution is idempotent",
      fn: testResolutionIsIdempotent,
    },
  ];

  await runTests("openLinkUserContext.test.ts", tests);
}
