// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { resolveWorkspaceOpenLinkUserContext } from "../open-link-user-context.ts";
import type { OpenLinkUserContextInput } from "../open-link-user-context-types.ts";
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

function testCurrentWithUnspecifiedUserContextIdOmitsKey(): void {
  for (const userContextId of [undefined, null]) {
    const input = Object.freeze({ userContextId, marker: "preserved" });
    const resolution = resolveWorkspaceOpenLinkUserContext(
      input,
      "current",
      7,
    );

    assert(
      !Object.hasOwn(resolution.options, "userContextId"),
      "current navigation should remove an unspecified userContextId",
    );
    assert(
      Object.hasOwn(input, "userContextId"),
      "resolving should not mutate the input object",
    );
    assertEquals(
      input.userContextId,
      userContextId,
      "resolving should preserve the input value",
    );
    assertEquals(
      resolution.options.marker,
      "preserved",
      "unrelated options should be preserved",
    );
  }
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

function testNonCurrentUnspecifiedUserContextIdUsesWorkspace(): void {
  for (const userContextId of [undefined, null]) {
    const input = Object.freeze({ userContextId });
    const resolution = resolveWorkspaceOpenLinkUserContext(input, "tab", 7);

    assertEquals(
      resolution.options.userContextId,
      7,
      "an unspecified userContextId should use the workspace container",
    );
    assertEquals(
      resolution.shouldApplyWorkspaceContainer,
      true,
      "an unspecified userContextId should allow workspace injection",
    );
    assertEquals(
      input.userContextId,
      userContextId,
      "resolving should not mutate the unspecified input value",
    );
  }
}

function testUnspecifiedIdPreservesOriginIncludingStandard(): void {
  for (const userContextId of [undefined, null]) {
    for (const originId of [0, 3]) {
      const resolution = resolveWorkspaceOpenLinkUserContext(
        {
          userContextId,
          targetBrowser: { userContextId: originId },
        },
        "tab",
        7,
      );

      assertEquals(
        resolution.options.userContextId,
        originId,
        "an originating standard or container tab should beat the workspace",
      );
      assertEquals(
        resolution.shouldApplyWorkspaceContainer,
        false,
        "origin inheritance must not be reported as workspace injection",
      );
    }
  }
}

function assertInvalidUserContextId(
  resolve: () => unknown,
  message: string,
): void {
  let error: unknown;
  try {
    resolve();
  } catch (caught) {
    error = caught;
  }
  assert(error instanceof RangeError, message);
}

function testInvalidExplicitIdsAreRejected(): void {
  for (
    const userContextId of [
      -1,
      1.5,
      NaN,
      Infinity,
      -Infinity,
      Number.MAX_SAFE_INTEGER + 1,
      "3",
      false,
    ]
  ) {
    for (const where of ["current", "tab"]) {
      assertInvalidUserContextId(
        () =>
          resolveWorkspaceOpenLinkUserContext(
            { userContextId } as OpenLinkUserContextInput,
            where,
            7,
          ),
        `invalid explicit ID ${
          String(userContextId)
        } should be rejected for ${where}`,
      );
    }
  }
}

function testInvalidOriginAndWorkspaceIdsAreRejected(): void {
  for (const userContextId of [-1, 1.5, NaN, Infinity]) {
    assertInvalidUserContextId(
      () =>
        resolveWorkspaceOpenLinkUserContext(
          {
            targetBrowser: { userContextId },
          },
          "tab",
          7,
        ),
      "an invalid origin context must not fall through to the workspace",
    );
    assertInvalidUserContextId(
      () => resolveWorkspaceOpenLinkUserContext({}, "tab", userContextId),
      "an invalid workspace default must not fall through to standard",
    );
  }
}

function testUnusedDefaultsDoNotOverrideExplicitOrCurrentContext(): void {
  const explicit = resolveWorkspaceOpenLinkUserContext(
    {
      userContextId: 0,
      targetBrowser: { userContextId: -1 },
    },
    "tab",
    -1,
  );
  assertEquals(
    explicit.options.userContextId,
    0,
    "an explicit standard context should not depend on unused defaults",
  );

  const current = resolveWorkspaceOpenLinkUserContext({}, "current", -1);
  assert(
    !Object.hasOwn(current.options, "userContextId"),
    "a current navigation should not depend on the workspace default",
  );
}

function testUnusedDefaultGetterIsNotEvaluated(): void {
  const missingDefault = (): number => {
    throw new Error("Missing workspace session");
  };
  assertEquals(
    resolveWorkspaceOpenLinkUserContext(
      { userContextId: 0 },
      "tab",
      missingDefault,
    ).options.userContextId,
    0,
    "an explicit standard opening does not read a missing default",
  );
  assertEquals(
    resolveWorkspaceOpenLinkUserContext(
      { targetBrowser: { userContextId: 3 } },
      "tab",
      missingDefault,
    ).options.userContextId,
    3,
    "origin inheritance does not read a missing default",
  );
  assert(
    !Object.hasOwn(
      resolveWorkspaceOpenLinkUserContext({}, "current", missingDefault)
        .options,
      "userContextId",
    ),
    "current navigation does not read a missing default",
  );
}

function testOpenerActualContextBeatsWorkspaceDefault(): void {
  const missingDefault = (): number => {
    throw new Error("Missing workspace default");
  };
  for (const userContextId of [0, 3]) {
    for (
      const input of [
        { openerBrowser: { userContextId } },
        {
          openerBrowser: {
            userContextId: 7,
            browsingContext: { originAttributes: { userContextId } },
          },
        },
        {
          openerTab: {
            userContextId: 7,
            linkedBrowser: {
              browsingContext: { originAttributes: { userContextId } },
            },
          },
        },
        { openerTab: { getAttribute: () => String(userContextId) } },
        { openWindowInfo: { originAttributes: { userContextId } } },
      ]
    ) {
      const result = resolveWorkspaceOpenLinkUserContext(
        input,
        "window",
        missingDefault,
      );
      assertEquals(
        result.options.userContextId,
        userContextId,
        "derived window inherits the opener's actual context",
      );
      assertEquals(
        result.shouldApplyWorkspaceContainer,
        false,
        "origin inheritance is not a default injection",
      );
    }
  }
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
      name: "current unspecified userContextId omits key",
      fn: testCurrentWithUnspecifiedUserContextIdOmitsKey,
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
      name: "non-current unspecified userContextId uses workspace",
      fn: testNonCurrentUnspecifiedUserContextIdUsesWorkspace,
    },
    {
      name: "unspecified userContextId preserves origin including standard",
      fn: testUnspecifiedIdPreservesOriginIncludingStandard,
    },
    {
      name: "invalid explicit userContextIds are rejected",
      fn: testInvalidExplicitIdsAreRejected,
    },
    {
      name: "invalid origin and workspace defaults are rejected",
      fn: testInvalidOriginAndWorkspaceIdsAreRejected,
    },
    {
      name: "unused defaults do not override explicit or current context",
      fn: testUnusedDefaultsDoNotOverrideExplicitOrCurrentContext,
    },
    {
      name: "unused default getter is not evaluated",
      fn: testUnusedDefaultGetterIsNotEvaluated,
    },
    {
      name: "opener actual context beats workspace default",
      fn: testOpenerActualContextBeatsWorkspaceDefault,
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
