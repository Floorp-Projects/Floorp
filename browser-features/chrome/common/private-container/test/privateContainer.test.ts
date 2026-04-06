// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { PrivateContainer } from "../PrivateContainer.ts";
import {
  type TestCase,
  assert,
  assertEquals,
} from "../../../test/utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Tests
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

function testGetPrivateContainerType(): void {
  // getPrivateContainer should return either an object or undefined
  const container = PrivateContainer.getPrivateContainer();
  assert(
    container === undefined || typeof container === "object",
    "should return object or undefined",
  );
}

function testGetPrivateContainerUserContextId(): void {
  const id = PrivateContainer.getPrivateContainerUserContextId();
  // Should be a number if private container exists, or null if not
  assert(
    id === null || typeof id === "number",
    `should be null or number, got ${typeof id}`,
  );
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

function testStartupCreatePrivateContainer(): void {
  // Ensure data is ready, then create if not exists
  PrivateContainer.StartupCreatePrivateContainer();
  // After startup, private container should exist
  const container = PrivateContainer.getPrivateContainer();
  assert(
    container !== undefined,
    "private container should exist after startup",
  );
  assert(
    container.floorpPrivateContainer === true,
    "should have floorpPrivateContainer flag",
  );
}

function testStartupIdempotent(): void {
  // Calling startup twice should not create duplicates
  PrivateContainer.StartupCreatePrivateContainer();
  PrivateContainer.StartupCreatePrivateContainer();

  const { ContextualIdentityService } = ChromeUtils.importESModule(
    "resource://gre/modules/ContextualIdentityService.sys.mjs",
  );
  const privateContainers = ContextualIdentityService._identities.filter(
    (c: { floorpPrivateContainer?: boolean }) =>
      c.floorpPrivateContainer === true,
  );
  assertEquals(
    privateContainers.length,
    1,
    "should only have one private container",
  );
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "constants exist", fn: testConstantsExist },
    { name: "getPrivateContainer type", fn: testGetPrivateContainerType },
    {
      name: "getPrivateContainerUserContextId",
      fn: testGetPrivateContainerUserContextId,
    },
    { name: "consistency check", fn: testPrivateContainerConsistency },
    {
      name: "startup creates container",
      fn: testStartupCreatePrivateContainer,
    },
    { name: "startup idempotent", fn: testStartupIdempotent },
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
      `privateContainer.test.ts failures: ${failures.join(" | ")}`,
    );
  }
}
