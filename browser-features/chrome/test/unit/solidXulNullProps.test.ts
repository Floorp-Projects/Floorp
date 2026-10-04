// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { signal } from "@preact/signals";
import { h, safeRender } from "@nora/preact-xul";
import { act } from "preact/test-utils";
import { assert, assertEquals, runTests } from "../utils/test_harness.ts";

async function testNullishAttributeUpdates(): Promise<void> {
  const container = document.createElement("div");
  const value = signal<string | number | boolean | null | undefined>("text");
  const dispose = safeRender(
    () => h("xul:label", { "data-value": value.value }),
    container,
  );
  try {
    const element = container.firstElementChild;
    assert(element, "renderer creates a native label");
    assertEquals(
      element.getAttribute("data-value"),
      "text",
      "string attribute",
    );
    for (const next of [42, false, null, "restored", undefined]) {
      await act(() => {
        value.value = next;
      });
      assertEquals(
        container.firstElementChild,
        element,
        "updates preserve the native element",
      );
      assertEquals(
        element.getAttribute("data-value"),
        next == null ? null : String(next),
        "null/undefined remove attributes while data attributes retain false",
      );
    }
  } finally {
    dispose();
  }
}

async function testStyleUpdates(): Promise<void> {
  const container = document.createElement("div");
  const visible = signal(false);
  const dispose = safeRender(
    () =>
      h("xul:hbox", {
        style: visible.value
          ? { color: "blue" }
          : { color: "red", display: "none" },
      }),
    container,
  );
  try {
    const element = container.firstElementChild as HTMLElement;
    assertEquals(element.style.color, "red", "style object sets color");
    assertEquals(element.style.display, "none", "style object sets display");
    await act(() => {
      visible.value = true;
    });
    assertEquals(element.style.color, "blue", "style object updates color");
    assertEquals(
      element.style.display,
      "",
      "omitted style property is removed",
    );
  } finally {
    dispose();
  }
}

async function testListenerReplacementAndRemoval(): Promise<void> {
  const container = document.createElement("div");
  let first = 0;
  let second = 0;
  const listener = signal<(() => void) | null>(() => first++);
  const dispose = safeRender(
    () => h("xul:menuitem", { onCommand: listener.value }),
    container,
  );
  try {
    const element = container.firstElementChild;
    assert(element, "renderer creates a native menu item");
    element.dispatchEvent(new Event("command"));
    await act(() => {
      listener.value = () => second++;
    });
    element.dispatchEvent(new Event("command"));
    await act(() => {
      listener.value = null;
    });
    element.dispatchEvent(new Event("command"));
    assertEquals(first, 1, "old event listener is replaced");
    assertEquals(second, 1, "null event listener is removed");
  } finally {
    dispose();
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("preactXulNullProps.test.ts", [
    {
      name: "nullish values remove real XUL attributes on update",
      fn: testNullishAttributeUpdates,
    },
    { name: "style updates remove stale CSS properties", fn: testStyleUpdates },
    {
      name: "native command listeners replace and remove",
      fn: testListenerReplacementAndRemoval,
    },
  ]);
}
