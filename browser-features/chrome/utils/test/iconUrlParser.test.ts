// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { iconUrlParser } from "../iconUrlParser.ts";
import {
  assert,
  runTests,
} from "../../test/utils/test_harness.ts";

function assertEquals(
  actual: unknown,
  expected: unknown,
  message: string,
): void {
  if (!Object.is(actual, expected)) {
    throw new Error(
      `${message}: expected=${String(expected)} actual=${String(actual)}`,
    );
  }
}

function testIconUrlParser(): void {
  const dataUrl = "data:text/plain;base64,SGVsbG8=";
  assertEquals(
    iconUrlParser(dataUrl),
    dataUrl,
    "data URL should be passed through unchanged",
  );

  const pathOnly = "/browser-features/chrome/static/test.png";
  const parsed = iconUrlParser(pathOnly);

  if (import.meta.env.DEV) {
    assertEquals(
      parsed,
      `http://localhost:5181${pathOnly}`,
      "DEV mode should prepend local asset host",
    );
  } else {
    assertEquals(parsed, pathOnly, "non-DEV mode should keep original URL");
  }

  assert(
    typeof parsed === "string" && parsed.length > 0,
    "parsed URL should be non-empty string",
  );
}

export async function runAllTests(): Promise<void> {
  await runTests("iconUrlParser.test.ts", [
    { name: "icon URL parser", fn: testIconUrlParser },
  ]);
}
