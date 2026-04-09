// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { PrivateContainer } from "../PrivateContainer.ts";
import {
  type TestCase,
  assert,
  assertEquals,
  assertThrows,
  runTests,
} from "../../../test/utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Types & Helpers
// ---------------------------------------------------------------------------

type ContextualIdentityLike = {
  userContextId?: number;
  floorpPrivateContainer?: boolean;
  public?: boolean;
  icon?: string;
  color?: string;
  name?: string;
};

type ContextualIdentityServiceLike = {
  _identities: ContextualIdentityLike[];
  _lastUserContextId: number;
  ensureDataReady: () => void;
  saveSoon: () => void;
  getIdentityObserverOutput: (identity: ContextualIdentityLike) => unknown;
};

function getContextualIdentityService(): ContextualIdentityServiceLike {
  const mod = ChromeUtils.importESModule(
    "resource://gre/modules/ContextualIdentityService.sys.mjs",
  ) as { ContextualIdentityService: ContextualIdentityServiceLike };
  return mod.ContextualIdentityService;
}

function withContextualIdentitySnapshot(run: () => void): void {
  const ContextualIdentityService = getContextualIdentityService();

  const originalIdentities = [...ContextualIdentityService._identities];
  const originalLastUserContextId =
    ContextualIdentityService._lastUserContextId;

  try {
    run();
  } finally {
    ContextualIdentityService._identities = originalIdentities;
    ContextualIdentityService._lastUserContextId = originalLastUserContextId;
  }
}

/** Remove all private containers from the identities array within a snapshot. */
function removePrivateContainers(): void {
  const ContextualIdentityService = getContextualIdentityService();
  ContextualIdentityService._identities =
    ContextualIdentityService._identities.filter(
      (c) => c.floorpPrivateContainer !== true,
    );
}

// ---------------------------------------------------------------------------
// Tests – Constants
// ---------------------------------------------------------------------------

function testConstantsExist(): void {
  assertEquals(
    typeof PrivateContainer.ENABLE_PRIVATE_CONTAINER_PREF,
    "string",
    "ENABLE_PRIVATE_CONTAINER_PREF should be a string",
  );
  assert(
    PrivateContainer.ENABLE_PRIVATE_CONTAINER_PREF.length > 0,
    "pref name should not be empty",
  );
  assertEquals(
    typeof PrivateContainer.PRIVATE_CONTAINER_L10N_ID,
    "string",
    "PRIVATE_CONTAINER_L10N_ID should be a string",
  );
}

function testConstantValuesAreValid(): void {
  assertEquals(
    PrivateContainer.ENABLE_PRIVATE_CONTAINER_PREF,
    "floorp.privateContainer.enabled",
    "ENABLE_PRIVATE_CONTAINER_PREF should be the expected pref name",
  );
  assertEquals(
    PrivateContainer.PRIVATE_CONTAINER_L10N_ID,
    "floorp-private-container-name",
    "PRIVATE_CONTAINER_L10N_ID should be the expected l10n id",
  );
}

// ---------------------------------------------------------------------------
// Tests – getPrivateContainer
// ---------------------------------------------------------------------------

function testGetPrivateContainerType(): void {
  const container = PrivateContainer.getPrivateContainer();
  assert(
    container === undefined || typeof container === "object",
    "should return object or undefined",
  );
}

function testGetPrivateContainerReturnsUndefinedWithNoFlagged(): void {
  withContextualIdentitySnapshot(() => {
    removePrivateContainers();

    const container = PrivateContainer.getPrivateContainer();
    assertEquals(
      container,
      undefined,
      "should return undefined when no private container exists",
    );
  });
}

function testGetPrivateContainerFindsFlaggedIdentity(): void {
  withContextualIdentitySnapshot(() => {
    const ContextualIdentityService = getContextualIdentityService();

    ContextualIdentityService._identities = [
      { userContextId: 1, floorpPrivateContainer: false },
      { userContextId: 42, floorpPrivateContainer: true },
      { userContextId: 3, floorpPrivateContainer: false },
    ];

    const container = PrivateContainer.getPrivateContainer();
    assert(container !== undefined, "should find flagged private container");
    assertEquals(
      container.userContextId,
      42,
      "should return the identity with floorpPrivateContainer=true",
    );
    assertEquals(
      PrivateContainer.getPrivateContainerUserContextId(),
      42,
      "userContextId helper should match selected private container",
    );
  });
}

function testGetPrivateContainerFindsFirstFlagged(): void {
  withContextualIdentitySnapshot(() => {
    const ContextualIdentityService = getContextualIdentityService();

    // Edge case: multiple flagged containers — should find the first
    ContextualIdentityService._identities = [
      { userContextId: 10, floorpPrivateContainer: true },
      { userContextId: 20, floorpPrivateContainer: true },
    ];

    const container = PrivateContainer.getPrivateContainer();
    assert(container !== undefined, "should find a flagged container");
    assertEquals(
      container.userContextId,
      10,
      "should return the first flagged identity",
    );
  });
}

function testGetPrivateContainerWithEmptyIdentities(): void {
  withContextualIdentitySnapshot(() => {
    const ContextualIdentityService = getContextualIdentityService();
    ContextualIdentityService._identities = [];

    const container = PrivateContainer.getPrivateContainer();
    assertEquals(
      container,
      undefined,
      "should return undefined with empty identities array",
    );
  });
}

// ---------------------------------------------------------------------------
// Tests – getPrivateContainerUserContextId
// ---------------------------------------------------------------------------

function testGetPrivateContainerUserContextId(): void {
  const id = PrivateContainer.getPrivateContainerUserContextId();
  assert(
    id === null || typeof id === "number",
    `should be null or number, got ${typeof id}`,
  );
}

function testGetPrivateContainerUserContextIdReturnsNullWithNoFlagged(): void {
  withContextualIdentitySnapshot(() => {
    removePrivateContainers();

    assertEquals(
      PrivateContainer.getPrivateContainerUserContextId(),
      null,
      "should return null when no private container exists",
    );
  });
}

function testPrivateContainerConsistency(): void {
  const container = PrivateContainer.getPrivateContainer();
  const id = PrivateContainer.getPrivateContainerUserContextId();

  if (container) {
    assert(
      typeof id === "number" && id > 0,
      "if container exists, userContextId should be a positive number",
    );
    assert(
      container.floorpPrivateContainer === true,
      "container should have floorpPrivateContainer=true",
    );
  } else {
    assertEquals(id, null, "if no container, userContextId should be null");
  }
}

// ---------------------------------------------------------------------------
// Tests – StartupCreatePrivateContainer
// ---------------------------------------------------------------------------

function testStartupCreatePrivateContainer(): void {
  withContextualIdentitySnapshot(() => {
    PrivateContainer.StartupCreatePrivateContainer();
    const container = PrivateContainer.getPrivateContainer();
    assert(
      container !== undefined,
      "private container should exist after startup",
    );
    assert(
      container.floorpPrivateContainer === true,
      "should have floorpPrivateContainer flag",
    );
  });
}

function testStartupCreatePrivateContainerProperties(): void {
  withContextualIdentitySnapshot(() => {
    removePrivateContainers();
    PrivateContainer.StartupCreatePrivateContainer();

    const container = PrivateContainer.getPrivateContainer();
    assert(container !== undefined, "container should exist");

    assertEquals(container.floorpPrivateContainer, true, "flag should be true");
    assertEquals(container.public, true, "should be public");
    assertEquals(
      typeof container.userContextId,
      "number",
      "userContextId should be a number",
    );
    assert(
      (container.userContextId as number) > 0,
      "userContextId should be positive",
    );
    assertEquals(typeof container.icon, "string", "icon should be a string");
    assertEquals(typeof container.color, "string", "color should be a string");
    assertEquals(typeof container.name, "string", "name should be a string");
  });
}

function testStartupCreatePrivateContainerIncrementsUserContextId(): void {
  withContextualIdentitySnapshot(() => {
    const ContextualIdentityService = getContextualIdentityService();
    removePrivateContainers();

    const before = ContextualIdentityService._lastUserContextId;
    PrivateContainer.StartupCreatePrivateContainer();

    const after = ContextualIdentityService._lastUserContextId;
    assertEquals(
      after,
      before + 1,
      "_lastUserContextId should be incremented by 1",
    );
  });
}

function testStartupIdempotent(): void {
  withContextualIdentitySnapshot(() => {
    PrivateContainer.StartupCreatePrivateContainer();
    PrivateContainer.StartupCreatePrivateContainer();

    const ContextualIdentityService = getContextualIdentityService();
    const privateContainers = ContextualIdentityService._identities.filter(
      (c) => c.floorpPrivateContainer === true,
    );
    assertEquals(
      privateContainers.length,
      1,
      "should only have one private container",
    );
  });
}

function testStartupIdempotentDoesNotIncrementUserContextId(): void {
  withContextualIdentitySnapshot(() => {
    PrivateContainer.StartupCreatePrivateContainer();

    const ContextualIdentityService = getContextualIdentityService();
    const afterFirst = ContextualIdentityService._lastUserContextId;

    // Second call should be a no-op
    PrivateContainer.StartupCreatePrivateContainer();
    const afterSecond = ContextualIdentityService._lastUserContextId;

    assertEquals(
      afterSecond,
      afterFirst,
      "second startup call should not increment _lastUserContextId",
    );
  });
}

// ---------------------------------------------------------------------------
// Tests – removePrivateContainerData
// ---------------------------------------------------------------------------

function testRemovePrivateContainerDataNoContainerDoesNotThrow(): void {
  withContextualIdentitySnapshot(() => {
    removePrivateContainers();

    let threw = false;
    try {
      PrivateContainer.removePrivateContainerData();
    } catch {
      threw = true;
    }
    assert(
      !threw,
      "removePrivateContainerData should not throw without container",
    );
  });
}

function testRemovePrivateContainerDataWithContainerDoesNotThrow(): void {
  withContextualIdentitySnapshot(() => {
    PrivateContainer.StartupCreatePrivateContainer();
    const privateContainer = PrivateContainer.getPrivateContainer();
    assert(privateContainer !== undefined, "private container should exist");

    let threw = false;
    try {
      PrivateContainer.removePrivateContainerData();
    } catch {
      threw = true;
    }

    assert(
      !threw,
      "removePrivateContainerData should not throw when private container exists",
    );
  });
}

function testRemovePrivateContainerDataReturnsEarlyWhenNoUserContextId(): void {
  withContextualIdentitySnapshot(() => {
    const ContextualIdentityService = getContextualIdentityService();
    // Add a container with the flag but no userContextId
    ContextualIdentityService._identities.push({
      floorpPrivateContainer: true,
      // userContextId intentionally omitted → undefined
    });

    // Should not throw — returns early because userContextId is falsy
    let threw = false;
    try {
      PrivateContainer.removePrivateContainerData();
    } catch {
      threw = true;
    }
    assert(
      !threw,
      "should not throw when private container has no userContextId",
    );
  });
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    // Constants
    { name: "constants exist", fn: testConstantsExist },
    { name: "constant values are valid", fn: testConstantValuesAreValid },

    // getPrivateContainer
    { name: "getPrivateContainer type", fn: testGetPrivateContainerType },
    {
      name: "getPrivateContainer returns undefined with no flagged",
      fn: testGetPrivateContainerReturnsUndefinedWithNoFlagged,
    },
    {
      name: "getPrivateContainer finds flagged identity",
      fn: testGetPrivateContainerFindsFlaggedIdentity,
    },
    {
      name: "getPrivateContainer finds first flagged",
      fn: testGetPrivateContainerFindsFirstFlagged,
    },
    {
      name: "getPrivateContainer with empty identities",
      fn: testGetPrivateContainerWithEmptyIdentities,
    },

    // getPrivateContainerUserContextId
    {
      name: "getPrivateContainerUserContextId type",
      fn: testGetPrivateContainerUserContextId,
    },
    {
      name: "getPrivateContainerUserContextId returns null with no flagged",
      fn: testGetPrivateContainerUserContextIdReturnsNullWithNoFlagged,
    },
    { name: "consistency check", fn: testPrivateContainerConsistency },

    // StartupCreatePrivateContainer
    {
      name: "startup creates container",
      fn: testStartupCreatePrivateContainer,
    },
    {
      name: "startup creates container with correct properties",
      fn: testStartupCreatePrivateContainerProperties,
    },
    {
      name: "startup increments _lastUserContextId",
      fn: testStartupCreatePrivateContainerIncrementsUserContextId,
    },
    { name: "startup idempotent", fn: testStartupIdempotent },
    {
      name: "startup idempotent does not increment _lastUserContextId",
      fn: testStartupIdempotentDoesNotIncrementUserContextId,
    },

    // removePrivateContainerData
    {
      name: "remove data without container does not throw",
      fn: testRemovePrivateContainerDataNoContainerDoesNotThrow,
    },
    {
      name: "remove data with container does not throw",
      fn: testRemovePrivateContainerDataWithContainerDoesNotThrow,
    },
    {
      name: "remove data returns early when no userContextId",
      fn: testRemovePrivateContainerDataReturnsEarlyWhenNoUserContextId,
    },
  ];

  await runTests("privateContainer.test.ts", tests);
}
