// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { Point, DollarRecognizer } from "../utils/dollar.ts";
import type { DollarPoint } from "../utils/dollar.ts";
import {
  type TestCase,
  assert,
  assertEquals,
} from "../../../test/utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Helpers — generate point sequences for gestures
// ---------------------------------------------------------------------------

function generateCirclePoints(
  cx: number,
  cy: number,
  r: number,
  n: number,
): DollarPoint[] {
  const points: DollarPoint[] = [];
  for (let i = 0; i < n; i++) {
    const angle = (2 * Math.PI * i) / n;
    // @ts-expect-error - Point is a legacy JS function constructor
    points.push(new Point(cx + r * Math.cos(angle), cy + r * Math.sin(angle)));
  }
  return points;
}

function generateLinePoints(
  x1: number,
  y1: number,
  x2: number,
  y2: number,
  n: number,
): DollarPoint[] {
  const points: DollarPoint[] = [];
  for (let i = 0; i < n; i++) {
    const t = i / (n - 1);
    // @ts-expect-error - Point is a legacy JS function constructor
    points.push(new Point(x1 + t * (x2 - x1), y1 + t * (y2 - y1)));
  }
  return points;
}

function generateVPoints(): DollarPoint[] {
  // Down-left stroke then up-right stroke
  const left = generateLinePoints(100, 100, 150, 250, 20);
  const right = generateLinePoints(150, 250, 200, 100, 20);
  return [...left, ...right];
}

// ---------------------------------------------------------------------------
// Tests — Point
// ---------------------------------------------------------------------------

function testPointConstructor(): void {
  // @ts-expect-error - Point is a legacy JS function constructor
  const p = new Point(3, 7);
  assertEquals(p.X, 3, "X should be 3");
  assertEquals(p.Y, 7, "Y should be 7");
}

// ---------------------------------------------------------------------------
// Tests — DollarRecognizer initialization
// ---------------------------------------------------------------------------

function testRecognizerHas16Unistrokes(): void {
  // @ts-expect-error - DollarRecognizer is a legacy JS function constructor
  const r = new DollarRecognizer();
  assertEquals(r.Unistrokes.length, 16, "should have 16 built-in unistrokes");
}

function testRecognizerUnistrokeNames(): void {
  // @ts-expect-error - DollarRecognizer is a legacy JS function constructor
  const r = new DollarRecognizer();
  const names = r.Unistrokes.map((u: { Name: string }) => u.Name);
  assert(names.includes("triangle"), "should include triangle");
  assert(names.includes("circle"), "should include circle");
  assert(names.includes("x"), "should include x");
  assert(names.includes("rectangle"), "should include rectangle");
  assert(names.includes("check"), "should include check");
  assert(names.includes("star"), "should include star");
  assert(names.includes("arrow"), "should include arrow");
  assert(names.includes("v"), "should include v");
}

// ---------------------------------------------------------------------------
// Tests — Recognize
// ---------------------------------------------------------------------------

function testRecognizeCircle(): void {
  // @ts-expect-error - DollarRecognizer is a legacy JS function constructor
  const r = new DollarRecognizer();
  const points = generateCirclePoints(150, 150, 80, 40);
  const result = r.Recognize(points, false);
  assert(
    result.Name.length > 0 && result.Name !== "No match.",
    `should recognize a gesture, got: ${result.Name}`,
  );
  assert(result.Score > 0.3, `score should be > 0.3, got ${result.Score}`);
}

function testRecognizeV(): void {
  // @ts-expect-error - DollarRecognizer is a legacy JS function constructor
  const r = new DollarRecognizer();
  const points = generateVPoints();
  const result = r.Recognize(points, false);
  assertEquals(result.Name, "v", "V points should match v or check");
}

function testRecognizeProtractorMode(): void {
  // @ts-expect-error - DollarRecognizer is a legacy JS function constructor
  const r = new DollarRecognizer();
  const points = generateCirclePoints(150, 150, 80, 40);
  const result = r.Recognize(points, true);
  assert(
    result.Name.length > 0 && result.Name !== "No match.",
    `Protractor mode should recognize a gesture, got: ${result.Name}`,
  );
  assert(result.Score > 0, "Protractor score should be positive");
}

function testRecognizeTimeMeasured(): void {
  // @ts-expect-error - DollarRecognizer is a legacy JS function constructor
  const r = new DollarRecognizer();
  const points = generateCirclePoints(150, 150, 80, 40);
  const result = r.Recognize(points, false);
  assert(
    typeof result.Time === "number" && result.Time >= 0,
    "Time should be a non-negative number",
  );
}

// ---------------------------------------------------------------------------
// Tests — AddGesture / DeleteUserGestures
// ---------------------------------------------------------------------------

function testAddGesture(): void {
  // @ts-expect-error - DollarRecognizer is a legacy JS function constructor
  const r = new DollarRecognizer();
  const points = generateLinePoints(0, 0, 200, 200, 30);
  const count = r.AddGesture("diagonal", points);
  assertEquals(count, 1, "first diagonal should return count 1");
  assertEquals(
    r.Unistrokes.length,
    17,
    "should have 17 unistrokes after adding",
  );
}

function testAddGestureDuplicate(): void {
  // @ts-expect-error - DollarRecognizer is a legacy JS function constructor
  const r = new DollarRecognizer();
  const points = generateLinePoints(0, 0, 200, 200, 30);
  r.AddGesture("custom", points);
  const count = r.AddGesture("custom", points);
  assertEquals(count, 2, "second custom should return count 2");
}

function testDeleteUserGestures(): void {
  // @ts-expect-error - DollarRecognizer is a legacy JS function constructor
  const r = new DollarRecognizer();
  r.AddGesture("custom1", generateLinePoints(0, 0, 100, 100, 20));
  r.AddGesture("custom2", generateLinePoints(0, 100, 100, 0, 20));
  assertEquals(r.Unistrokes.length, 18, "should have 18 before delete");
  const remaining = r.DeleteUserGestures();
  assertEquals(remaining, 16, "should return 16");
  assertEquals(r.Unistrokes.length, 16, "should be back to 16");
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export function runAllTests(): void {
  const tests: TestCase[] = [
    { name: "Point constructor", fn: testPointConstructor },
    { name: "recognizer has 16 unistrokes", fn: testRecognizerHas16Unistrokes },
    { name: "recognizer unistroke names", fn: testRecognizerUnistrokeNames },
    { name: "recognize circle", fn: testRecognizeCircle },
    { name: "recognize V", fn: testRecognizeV },
    { name: "recognize Protractor mode", fn: testRecognizeProtractorMode },
    { name: "recognize time measured", fn: testRecognizeTimeMeasured },
    { name: "add gesture", fn: testAddGesture },
    { name: "add gesture duplicate", fn: testAddGestureDuplicate },
    { name: "delete user gestures", fn: testDeleteUserGestures },
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
      `dollarRecognizer.test.ts failures: ${failures.join(" | ")}`,
    );
  }
}
