// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { mapColorToCSSVariable } from "../../common/workspaces/utils/container-color.ts";
import { type TestCase, assert, assertEquals } from "../utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

function testNumberZeroReturnsBlue(): void {
  assertEquals(mapColorToCSSVariable(0), "blue", "0 should map to blue");
}

function testNumberOneReturnsTurquoise(): void {
  assertEquals(
    mapColorToCSSVariable(1),
    "turquoise",
    "1 should map to turquoise",
  );
}

function testNumberTwoReturnsGreen(): void {
  assertEquals(mapColorToCSSVariable(2), "green", "2 should map to green");
}

function testNumberThreeReturnsYellow(): void {
  assertEquals(mapColorToCSSVariable(3), "yellow", "3 should map to yellow");
}

function testNumberFourReturnsOrange(): void {
  assertEquals(mapColorToCSSVariable(4), "orange", "4 should map to orange");
}

function testNumberFiveReturnsRed(): void {
  assertEquals(mapColorToCSSVariable(5), "red", "5 should map to red");
}

function testNumberSixReturnsPink(): void {
  assertEquals(mapColorToCSSVariable(6), "pink", "6 should map to pink");
}

function testNumberSevenReturnsPurple(): void {
  assertEquals(mapColorToCSSVariable(7), "purple", "7 should map to purple");
}

function testNumberEightReturnsToolbar(): void {
  assertEquals(mapColorToCSSVariable(8), "toolbar", "8 should map to toolbar");
}

function testNumberNineReturnsGray(): void {
  assertEquals(mapColorToCSSVariable(9), "gray", "9 should map to gray");
}

function testStringPassthrough(): void {
  const validColors = [
    "blue",
    "turquoise",
    "green",
    "yellow",
    "orange",
    "red",
    "pink",
    "purple",
    "toolbar",
    "gray",
  ];
  for (const color of validColors) {
    assertEquals(
      mapColorToCSSVariable(color),
      color,
      `"${color}" should pass through`,
    );
  }
}

function testWhiteMapsToToolbar(): void {
  assertEquals(
    mapColorToCSSVariable("white"),
    "toolbar",
    '"white" should map to "toolbar"',
  );
}

function testGreyMapsToGray(): void {
  assertEquals(
    mapColorToCSSVariable("grey"),
    "gray",
    '"grey" should map to "gray"',
  );
}

function testNullReturnsNull(): void {
  assertEquals(mapColorToCSSVariable(null), null, "null should return null");
}

function testUnknownNumberReturnsNull(): void {
  assertEquals(
    mapColorToCSSVariable(99),
    null,
    "unknown number should return null",
  );
}

function testUnknownStringReturnsNull(): void {
  assertEquals(
    mapColorToCSSVariable("magenta"),
    null,
    "unknown string should return null",
  );
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "number 0 → blue", fn: testNumberZeroReturnsBlue },
    { name: "number 1 → turquoise", fn: testNumberOneReturnsTurquoise },
    { name: "number 2 → green", fn: testNumberTwoReturnsGreen },
    { name: "number 3 → yellow", fn: testNumberThreeReturnsYellow },
    { name: "number 4 → orange", fn: testNumberFourReturnsOrange },
    { name: "number 5 → red", fn: testNumberFiveReturnsRed },
    { name: "number 6 → pink", fn: testNumberSixReturnsPink },
    { name: "number 7 → purple", fn: testNumberSevenReturnsPurple },
    { name: "number 8 → toolbar", fn: testNumberEightReturnsToolbar },
    { name: "number 9 → gray", fn: testNumberNineReturnsGray },
    { name: "valid string passthrough", fn: testStringPassthrough },
    { name: '"white" → "toolbar"', fn: testWhiteMapsToToolbar },
    { name: '"grey" → "gray"', fn: testGreyMapsToGray },
    { name: "null → null", fn: testNullReturnsNull },
    { name: "unknown number → null", fn: testUnknownNumberReturnsNull },
    { name: "unknown string → null", fn: testUnknownStringReturnsNull },
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
      `containerColor.test.ts failures: ${failures.join(" | ")}`,
    );
  }
}
