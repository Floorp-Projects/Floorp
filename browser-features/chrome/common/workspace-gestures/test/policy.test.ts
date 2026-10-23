// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { sidebarGestureTarget } from "../policy.ts";
import { assertEquals, runTests } from "../../../test/utils/test_harness.ts";

function testEligibleChromeAreaOnly(): void {
  const area = document.createElementNS("http://www.w3.org/1999/xhtml", "div");
  area.id = "workspaces-panel-sidebar-section";
  const button = document.createElementNS(
    "http://www.w3.org/1999/xhtml",
    "button",
  );
  area.append(button);
  document.documentElement.append(area);
  try {
    assertEquals(
      sidebarGestureTarget(button, document),
      area,
      "workspace chrome is eligible",
    );
    for (const tag of ["input", "textarea", "select", "iframe", "browser"]) {
      const excluded = document.createElementNS(
        "http://www.w3.org/1999/xhtml",
        tag,
      );
      area.append(excluded);
      assertEquals(
        sidebarGestureTarget(excluded, document),
        null,
        `${tag} keeps its own input`,
      );
    }
    const editable = document.createElementNS(
      "http://www.w3.org/1999/xhtml",
      "div",
    );
    editable.setAttribute("contenteditable", "");
    area.append(editable);
    assertEquals(
      sidebarGestureTarget(editable, document),
      null,
      "editable chrome is excluded",
    );
    assertEquals(
      sidebarGestureTarget(document.documentElement, document),
      null,
      "page/chrome outside workspace area is excluded",
    );
  } finally {
    area.remove();
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("workspaceGesturePolicy.test.ts", [
    {
      name: "eligible chrome area excludes content and editing",
      fn: testEligibleChromeAreaOnly,
    },
  ]);
}
