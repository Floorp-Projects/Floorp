// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { ProfileManagerService } from "../service.tsx";
import { MenuPopup } from "../components/popup.tsx";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

function testGetInstanceReturnsSingleton(): void {
  const first = ProfileManagerService.getInstance();
  const second = ProfileManagerService.getInstance();
  assert(first === second, "getInstance should return singleton instance");
}

function testInitDoesNotThrow(): void {
  const service = ProfileManagerService.getInstance();
  let threw = false;

  try {
    service.init();
  } catch {
    threw = true;
  }

  assert(!threw, "ProfileManagerService.init should not throw");
}

function testInitIsIdempotent(): void {
  const service = ProfileManagerService.getInstance();

  let threw = false;
  try {
    service.init();
    service.init();
  } catch {
    threw = true;
  }

  assert(!threw, "calling init() repeatedly should remain safe");
}

function testMenuPopupFactoryReturnsElement(): void {
  const element = MenuPopup();
  assert(element !== null, "MenuPopup should return an element");
  assertEquals(
    typeof element,
    "object",
    "MenuPopup result should be JSX object",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "getInstance returns singleton",
      fn: testGetInstanceReturnsSingleton,
    },
    { name: "init does not throw", fn: testInitDoesNotThrow },
    {
      name: "init is idempotent",
      fn: testInitIsIdempotent,
    },
    {
      name: "MenuPopup returns element",
      fn: testMenuPopupFactoryReturnsElement,
    },
  ];

  await runTests("profileManagerService.test.ts", tests);
}
