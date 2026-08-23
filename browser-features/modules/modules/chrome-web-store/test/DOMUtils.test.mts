// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { findPrimaryVisibleActionButton } from "../DOMUtils.sys.mts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";

function createHeader(): HTMLElement {
  return document.createElement("section");
}

function createButton(id = ""): HTMLButtonElement {
  const button = document.createElement("button");
  button.id = id;
  return button;
}

function testSelectsLastVisibleButton(): void {
  const header = createHeader();
  const hiddenUtility = createButton();
  const share = createButton();
  const install = createButton();
  header.append(hiddenUtility, share, install);

  const result = findPrimaryVisibleActionButton(
    header,
    (button) => button !== hiddenUtility,
  );

  assert(
    result === install,
    "the last visible header action should be selected",
  );
}

function testSkipsInjectedFloorpButton(): void {
  const header = createHeader();
  const install = createButton();
  const floorp = createButton("floorp-add-extension-btn");
  header.append(install, floorp);

  const result = findPrimaryVisibleActionButton(header, () => true);

  assert(result === install, "the injected Floorp button should be ignored");
}

function testReturnsNullWithoutVisibleButton(): void {
  const header = createHeader();
  header.append(createButton(), createButton("floorp-add-extension-btn"));

  const result = findPrimaryVisibleActionButton(header, () => false);

  assertEquals(result, null, "no visible store action should return null");
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "selects last visible button", fn: testSelectsLastVisibleButton },
    { name: "skips injected Floorp button", fn: testSkipsInjectedFloorpButton },
    {
      name: "returns null without visible button",
      fn: testReturnsNullWithoutVisibleButton,
    },
  ];

  await runTests("DOMUtils.test.mts", tests);
}
