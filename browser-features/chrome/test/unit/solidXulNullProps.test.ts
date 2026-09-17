// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { setProp } from "@nora/solid-xul";
import {
  assert,
  assertEquals,
  assertThrows,
  runTests,
} from "../utils/test_harness.ts";

function testNullRemovesAttribute(): void {
  const element = document.createElement("div");
  element.setAttribute("data-value", "old");

  setProp(element, "data-value", null);

  assert(
    !element.hasAttribute("data-value"),
    "null should remove the attribute",
  );
}

function testUndefinedRemovesAttribute(): void {
  const element = document.createElement("div");
  element.setAttribute("data-value", "old");

  setProp(element, "data-value", undefined);

  assert(
    !element.hasAttribute("data-value"),
    "undefined should remove the attribute",
  );
}

function testStringSetsAttribute(): void {
  const element = document.createElement("div");

  setProp(element, "data-value", "text");

  assertEquals(
    element.getAttribute("data-value"),
    "text",
    "string should set the attribute value",
  );
}

function testNumberSetsAttribute(): void {
  const element = document.createElement("div");

  setProp(element, "data-value", 42);

  assertEquals(
    element.getAttribute("data-value"),
    "42",
    "number should set its string representation",
  );
}

function testBooleanSetsAttribute(): void {
  const element = document.createElement("div");

  setProp(element, "data-value", false);

  assertEquals(
    element.getAttribute("data-value"),
    "false",
    "boolean should set its string representation",
  );
}

function testStyleObjectSetsAttribute(): void {
  const element = document.createElement("div");

  setProp(element, "style", { color: "red", display: "none" });

  assertEquals(
    element.getAttribute("style"),
    "color:red;display:none;",
    "style object should be serialized into the attribute",
  );
}

function testFunctionEventListener(): void {
  const element = document.createElement("div");
  let callCount = 0;

  setProp(element, "onclick", () => callCount++);
  element.dispatchEvent(new Event("click"));

  assertEquals(callCount, 1, "function listener should receive the event");
}

function testObjectEventListener(): void {
  const element = document.createElement("div");
  let callCount = 0;
  const listener = {
    handleEvent: () => callCount++,
  };

  setProp(element, "oncommand", listener);
  element.dispatchEvent(new Event("command"));

  assertEquals(callCount, 1, "object listener should receive the event");
}

function testUnsupportedObjectThrows(): void {
  const element = document.createElement("div");
  const error = assertThrows(
    () => setProp(element, "data-value", { nested: { value: "unsupported" } }),
    "unsupported object should throw",
  );

  assert(
    error?.message.includes(
      "the value is not EventListener, style object, string, number, nor boolean",
    ),
    "unsupported object should preserve the renderer contract error",
  );
}

export async function runAllTests(): Promise<void> {
  await runTests("solidXulNullProps.test.ts", [
    { name: "null removes attribute", fn: testNullRemovesAttribute },
    { name: "undefined removes attribute", fn: testUndefinedRemovesAttribute },
    { name: "string sets attribute", fn: testStringSetsAttribute },
    { name: "number sets attribute", fn: testNumberSetsAttribute },
    { name: "boolean sets attribute", fn: testBooleanSetsAttribute },
    { name: "style object sets attribute", fn: testStyleObjectSetsAttribute },
    { name: "function event listener", fn: testFunctionEventListener },
    { name: "object event listener", fn: testObjectEventListener },
    { name: "unsupported object throws", fn: testUnsupportedObjectThrows },
  ]);
}
