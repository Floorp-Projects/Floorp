// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { PrivateContainer } from "../PrivateContainer.ts";
import {
  type TestCase,
  assert,
  assertEquals,
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
// Tests – removePrivateContainerData (extended)
// ---------------------------------------------------------------------------

function testRemovePrivateContainerDataWithZeroUserContextId(): void {
  withContextualIdentitySnapshot(() => {
    const ContextualIdentityService = getContextualIdentityService();
    // Edge case: container with userContextId of 0
    ContextualIdentityService._identities.push({
      userContextId: 0,
      floorpPrivateContainer: true,
    });

    let threw = false;
    try {
      PrivateContainer.removePrivateContainerData();
    } catch {
      threw = true;
    }
    assert(
      !threw,
      "should handle container with userContextId of 0 without throwing",
    );
  });
}

function testRemovePrivateContainerDataWithNegativeUserContextId(): void {
  withContextualIdentitySnapshot(() => {
    const ContextualIdentityService = getContextualIdentityService();
    // Edge case: container with negative userContextId
    ContextualIdentityService._identities.push({
      userContextId: -1,
      floorpPrivateContainer: true,
    });

    let threw = false;
    try {
      PrivateContainer.removePrivateContainerData();
    } catch {
      threw = true;
    }
    assert(
      !threw,
      "should handle container with negative userContextId without throwing",
    );
  });
}

// ---------------------------------------------------------------------------
// Tests – StartupCreatePrivateContainer (extended)
// ---------------------------------------------------------------------------

function testStartupCreatePrivateContainerIconValue(): void {
  withContextualIdentitySnapshot(() => {
    removePrivateContainers();
    PrivateContainer.StartupCreatePrivateContainer();

    const container = PrivateContainer.getPrivateContainer();
    assert(container !== undefined, "container should exist");
    assertEquals(
      container.icon,
      "chill",
      "icon should be set to 'chill'",
    );
  });
}

function testStartupCreatePrivateContainerColorValue(): void {
  withContextualIdentitySnapshot(() => {
    removePrivateContainers();
    PrivateContainer.StartupCreatePrivateContainer();

    const container = PrivateContainer.getPrivateContainer();
    assert(container !== undefined, "container should exist");
    assertEquals(
      container.color,
      "purple",
      "color should be set to 'purple'",
    );
  });
}

function testStartupCreatePrivateContainerWithLargeUserContextId(): void {
  withContextualIdentitySnapshot(() => {
    const ContextualIdentityService = getContextualIdentityService();
    removePrivateContainers();

    // Set _lastUserContextId to a large value
    ContextualIdentityService._lastUserContextId = 99999;
    PrivateContainer.StartupCreatePrivateContainer();

    const container = PrivateContainer.getPrivateContainer();
    assert(container !== undefined, "container should exist");
    assertEquals(
      container.userContextId,
      100000,
      "should increment from large value correctly",
    );
  });
}

function testStartupCreatePrivateContainerCallsEnsureDataReady(): void {
  withContextualIdentitySnapshot(() => {
    removePrivateContainers();
    const ContextualIdentityService = getContextualIdentityService();

    let ensureDataReadyCalled = false;
    const originalEnsureDataReady = ContextualIdentityService.ensureDataReady;
    ContextualIdentityService.ensureDataReady = () => {
      ensureDataReadyCalled = true;
    };

    try {
      PrivateContainer.StartupCreatePrivateContainer();
      assert(
        ensureDataReadyCalled,
        "ensureDataReady should be called during startup",
      );
    } finally {
      ContextualIdentityService.ensureDataReady = originalEnsureDataReady;
    }
  });
}

function testStartupCreatePrivateContainerCallsSaveSoon(): void {
  withContextualIdentitySnapshot(() => {
    removePrivateContainers();
    const ContextualIdentityService = getContextualIdentityService();

    let saveSoonCalled = false;
    const originalSaveSoon = ContextualIdentityService.saveSoon;
    ContextualIdentityService.saveSoon = () => {
      saveSoonCalled = true;
    };

    try {
      PrivateContainer.StartupCreatePrivateContainer();
      assert(
        saveSoonCalled,
        "saveSoon should be called during startup",
      );
    } finally {
      ContextualIdentityService.saveSoon = originalSaveSoon;
    }
  });
}

function testStartupCreatePrivateContainerNotifiesObserver(): void {
  withContextualIdentitySnapshot(() => {
    removePrivateContainers();

    let observerNotified = false;
    let notifiedIdentity: unknown = null;

    const observer = {
      observe: (subject: unknown, topic: string) => {
        if (topic === "contextual-identity-created") {
          observerNotified = true;
          notifiedIdentity = subject;
        }
      },
    };

    Services.obs.addObserver(observer, "contextual-identity-created");

    try {
      PrivateContainer.StartupCreatePrivateContainer();
      assert(
        observerNotified,
        "should notify observer on container creation",
      );
      assert(
        notifiedIdentity !== null,
        "observer should receive identity data",
      );
    } finally {
      Services.obs.removeObserver(observer, "contextual-identity-created");
    }
  });
}

function testStartupCreatePrivateContainerWithEmptyNameFromI18n(): void {
  withContextualIdentitySnapshot(() => {
    removePrivateContainers();

    // Import i18next from the module (it's imported at module level in PrivateContainer.ts)
    // We need to mock the i18next.t function
    // The source imports i18next at the top of PrivateContainer.ts
    // In test env, i18next may or may not be available on globalThis
    // We try to access it via the imported module
    let mockApplied = false;
    try {
      const i18nextModule = (globalThis as Record<string, unknown>)["i18next"];
      if (i18nextModule && typeof (i18nextModule as { t: unknown }).t === "function") {
        const originalT = (i18nextModule as { t: (key: string) => string }).t;
        (i18nextModule as { t: (key: string) => string }).t = () => "";
        mockApplied = true;

        try {
          PrivateContainer.StartupCreatePrivateContainer();
          const container = PrivateContainer.getPrivateContainer();
          assert(container !== undefined, "container should exist");
          assertEquals(
            container.name,
            "",
            "should use empty string when i18next returns empty",
          );
        } finally {
          (i18nextModule as { t: (key: string) => string }).t = originalT;
        }
      }
    } catch {
      // i18next not available in test environment
    }

    if (!mockApplied) {
      // If i18next is not available, the startup should still work with whatever name is provided
      PrivateContainer.StartupCreatePrivateContainer();
      const container = PrivateContainer.getPrivateContainer();
      assert(container !== undefined, "container should exist even without i18next mock");
      assert(
        typeof container.name === "string",
        "name should be a string",
      );
    }
  });
}

// ---------------------------------------------------------------------------
// Tests – getPrivateContainer (extended)
// ---------------------------------------------------------------------------

function testGetPrivateContainerWithFalsyUserContextId(): void {
  withContextualIdentitySnapshot(() => {
    const ContextualIdentityService = getContextualIdentityService();
    ContextualIdentityService._identities = [
      { userContextId: 0, floorpPrivateContainer: true },
    ];

    const container = PrivateContainer.getPrivateContainer();
    assert(container !== undefined, "should find container with userContextId 0");
    assertEquals(
      container.userContextId,
      0,
      "should return container with userContextId of 0",
    );
  });
}

function testGetPrivateContainerWithUndefinedUserContextId(): void {
  withContextualIdentitySnapshot(() => {
    const ContextualIdentityService = getContextualIdentityService();
    ContextualIdentityService._identities = [
      { floorpPrivateContainer: true },
    ];

    const container = PrivateContainer.getPrivateContainer();
    assert(container !== undefined, "should find container even without userContextId");
    assertEquals(
      container.userContextId,
      undefined,
      "should return container with undefined userContextId",
    );
  });
}

// ---------------------------------------------------------------------------
// Tests – getPrivateContainerUserContextId (extended)
// ---------------------------------------------------------------------------

function testGetPrivateContainerUserContextIdWithZeroId(): void {
  withContextualIdentitySnapshot(() => {
    const ContextualIdentityService = getContextualIdentityService();
    ContextualIdentityService._identities = [
      { userContextId: 0, floorpPrivateContainer: true },
    ];

    const id = PrivateContainer.getPrivateContainerUserContextId();
    assertEquals(
      id,
      0,
      "should return 0 when private container has userContextId of 0",
    );
  });
}

function testGetPrivateContainerUserContextIdWithUndefinedId(): void {
  withContextualIdentitySnapshot(() => {
    const ContextualIdentityService = getContextualIdentityService();
    ContextualIdentityService._identities = [
      { floorpPrivateContainer: true },
    ];

    const id = PrivateContainer.getPrivateContainerUserContextId();
    // The source returns: privateContainer ? privateContainer.userContextId : null
    // When the container exists but userContextId is undefined, it returns undefined (not null)
    assertEquals(
      id,
      undefined,
      "should return undefined when private container has undefined userContextId",
    );
  });
}

// ---------------------------------------------------------------------------
// Tests – Integration scenarios
// ---------------------------------------------------------------------------

function testCreateThenRemoveThenCreate(): void {
  withContextualIdentitySnapshot(() => {
    removePrivateContainers();

    // First creation
    PrivateContainer.StartupCreatePrivateContainer();
    const firstContainer = PrivateContainer.getPrivateContainer();
    assert(firstContainer !== undefined, "first container should exist");

    // Remove data
    PrivateContainer.removePrivateContainerData();

    // Second creation should still work
    PrivateContainer.StartupCreatePrivateContainer();
    const secondContainer = PrivateContainer.getPrivateContainer();
    assert(secondContainer !== undefined, "second container should exist");

    // Should be the same container (idempotent)
    assertEquals(
      firstContainer.userContextId,
      secondContainer.userContextId,
      "should reuse same container",
    );
  });
}

function testMultipleContainersWithSameFlag(): void {
  withContextualIdentitySnapshot(() => {
    const ContextualIdentityService = getContextualIdentityService();
    removePrivateContainers();

    // Manually add multiple flagged containers (edge case)
    ContextualIdentityService._identities.push(
      { userContextId: 100, floorpPrivateContainer: true },
      { userContextId: 200, floorpPrivateContainer: true },
    );

    const container = PrivateContainer.getPrivateContainer();
    assert(container !== undefined, "should find a container");
    assertEquals(
      container.userContextId,
      100,
      "should return first flagged container",
    );
  });
}

function testGetPrivateContainerDoesNotModifyIdentities(): void {
  withContextualIdentitySnapshot(() => {
    const ContextualIdentityService = getContextualIdentityService();
    const originalLength = ContextualIdentityService._identities.length;

    // Call getPrivateContainer multiple times
    PrivateContainer.getPrivateContainer();
    PrivateContainer.getPrivateContainer();
    PrivateContainer.getPrivateContainer();

    assertEquals(
      ContextualIdentityService._identities.length,
      originalLength,
      "should not modify identities array",
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
    {
      name: "getPrivateContainer with falsy userContextId",
      fn: testGetPrivateContainerWithFalsyUserContextId,
    },
    {
      name: "getPrivateContainer with undefined userContextId",
      fn: testGetPrivateContainerWithUndefinedUserContextId,
    },
    {
      name: "getPrivateContainer does not modify identities",
      fn: testGetPrivateContainerDoesNotModifyIdentities,
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
    {
      name: "getPrivateContainerUserContextId with zero id",
      fn: testGetPrivateContainerUserContextIdWithZeroId,
    },
    {
      name: "getPrivateContainerUserContextId with undefined id",
      fn: testGetPrivateContainerUserContextIdWithUndefinedId,
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
    {
      name: "startup creates container with correct icon",
      fn: testStartupCreatePrivateContainerIconValue,
    },
    {
      name: "startup creates container with correct color",
      fn: testStartupCreatePrivateContainerColorValue,
    },
    {
      name: "startup with large userContextId",
      fn: testStartupCreatePrivateContainerWithLargeUserContextId,
    },
    {
      name: "startup calls ensureDataReady",
      fn: testStartupCreatePrivateContainerCallsEnsureDataReady,
    },
    {
      name: "startup calls saveSoon",
      fn: testStartupCreatePrivateContainerCallsSaveSoon,
    },
    {
      name: "startup notifies observer",
      fn: testStartupCreatePrivateContainerNotifiesObserver,
    },
    {
      name: "startup with empty name from i18n",
      fn: testStartupCreatePrivateContainerWithEmptyNameFromI18n,
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
    {
      name: "remove data with zero userContextId",
      fn: testRemovePrivateContainerDataWithZeroUserContextId,
    },
    {
      name: "remove data with negative userContextId",
      fn: testRemovePrivateContainerDataWithNegativeUserContextId,
    },

    // Integration scenarios
    {
      name: "create then remove then create",
      fn: testCreateThenRemoveThenCreate,
    },
    {
      name: "multiple containers with same flag",
      fn: testMultipleContainersWithSameFlag,
    },
  ];

  await runTests("privateContainer.test.ts", tests);
}
