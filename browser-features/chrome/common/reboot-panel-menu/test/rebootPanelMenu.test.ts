// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { RebootPanelMenu } from "../RebootPanelMenu.tsx";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

function testClassCanBeConstructedWithoutPanelUIButton(): void {
  const originalButton = document?.getElementById(
    "PanelUI-menu-button",
  ) as HTMLElement | null;
  const parent = originalButton?.parentNode ?? null;
  const nextSibling = originalButton?.nextSibling ?? null;

  try {
    originalButton?.remove();

    let threw = false;
    try {
      new RebootPanelMenu();
    } catch {
      threw = true;
    }

    assert(!threw, "constructor should not throw when menu button is missing");
  } finally {
    if (originalButton && parent) {
      parent.insertBefore(originalButton, nextSibling);
    }
  }
}

function testRenderReturnsJsxTree(): void {
  const node = RebootPanelMenu.Render();
  assert(node !== null, "Render should return a JSX tree");
  assertEquals(
    typeof node,
    "object",
    "Render return value should be object-like",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "constructor is safe without panel ui button",
      fn: testClassCanBeConstructedWithoutPanelUIButton,
    },
    { name: "Render returns JSX tree", fn: testRenderReturnsJsxTree },
  ];

  await runTests("rebootPanelMenu.test.ts", tests);
}
