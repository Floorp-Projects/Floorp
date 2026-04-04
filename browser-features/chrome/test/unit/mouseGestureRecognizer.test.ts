// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  angleToDirection,
  convertPatternToPoints,
  convertTrailToPoints,
  createRecognizer,
  deltaToDirection,
  extractDirectionSequence,
  recognize,
} from "../../common/mouse-gesture/utils/recognizer.ts";
import type { GestureAction } from "../../common/mouse-gesture/config.ts";

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) {
    throw new Error(message);
  }
}

function assertEquals(actual: unknown, expected: unknown, message: string): void {
  if (!Object.is(actual, expected)) {
    throw new Error(`${message}: expected=${String(expected)} actual=${String(actual)}`);
  }
}

function assertApprox(actual: number, expected: number, epsilon: number, message: string): void {
  if (Math.abs(actual - expected) > epsilon) {
    throw new Error(`${message}: expected=${expected} actual=${actual}`);
  }
}

function testAngleAndDeltaDirection(): void {
  assertEquals(angleToDirection(0), "right", "angle 0 should be right");
  assertEquals(angleToDirection(Math.PI / 2), "down", "angle PI/2 should be down");
  assertEquals(angleToDirection(-Math.PI / 2), "up", "angle -PI/2 should be up");
  assertEquals(angleToDirection(Math.PI), "left", "angle PI should be left");

  assertEquals(deltaToDirection(10, 0), "right", "dx+ should be right");
  assertEquals(deltaToDirection(0, -10), "up", "dy- should be up");
  assertEquals(deltaToDirection(10, 10), "downRight", "diag +/+ should be downRight");
  assertEquals(deltaToDirection(-10, 10), "downLeft", "diag -/+ should be downLeft");
}

function testPointConversion(): void {
  const points = convertPatternToPoints(["up", "right"], 100, 10);
  assertEquals(points.length, 21, "pattern with 2 segments should generate segment*points+1");
  assertApprox(points[0].X, 0, 0.001, "first point X");
  assertApprox(points[0].Y, 0, 0.001, "first point Y");

  const last = points[points.length - 1];
  assertApprox(last.X, 100, 0.1, "last point X should end at +100");
  assertApprox(last.Y, -100, 0.1, "last point Y should end at -100");

  const trailPoints = convertTrailToPoints([
    { x: 1, y: 2 },
    { x: 5, y: 8 },
  ]);
  assertEquals(trailPoints.length, 2, "trail conversion should preserve point count");
  assertApprox(trailPoints[1].X, 5, 0.001, "converted trail X");
  assertApprox(trailPoints[1].Y, 8, 0.001, "converted trail Y");
}

function testRecognizerCreation(): void {
  const actions: GestureAction[] = [
    { pattern: ["left"], action: "go-back" },
    { pattern: ["up", "right"], action: "custom-action" },
    { pattern: ["up", "right"], action: "duplicate-pattern" },
  ];

  const { recognizer, shapeDb } = createRecognizer(actions);

  assert(shapeDb.size >= 2, "shape database should include simple and complex shapes");
  assert(recognizer.Unistrokes.length > 0, "complex patterns should register templates");

  const complexEntry = shapeDb.get("up-right");
  assert(complexEntry !== undefined, "shapeDb should contain up-right key");
  assertEquals(complexEntry.variants.length, 1, "duplicate patterns should be deduplicated");
}

function buildLineTrail(
  startX: number,
  startY: number,
  endX: number,
  endY: number,
  steps = 16,
): { x: number; y: number }[] {
  const points: { x: number; y: number }[] = [];
  for (let i = 0; i <= steps; i++) {
    const t = i / steps;
    points.push({
      x: startX + (endX - startX) * t,
      y: startY + (endY - startY) * t,
    });
  }
  return points;
}

function joinTrails(...segments: { x: number; y: number }[][]): { x: number; y: number }[] {
  const merged: { x: number; y: number }[] = [];
  for (const segment of segments) {
    if (segment.length === 0) {
      continue;
    }
    if (merged.length === 0) {
      merged.push(...segment);
    } else {
      merged.push(...segment.slice(1));
    }
  }
  return merged;
}

function testExtractDirectionSequence(): void {
  const lShapeTrail = joinTrails(
    buildLineTrail(0, 0, 140, 0),
    buildLineTrail(140, 0, 140, 140),
  );

  const sequence = extractDirectionSequence(lShapeTrail, 20);
  assertEquals(sequence.length, 2, "L-shape should produce two directions");
  assertEquals(sequence[0], "right", "first segment should be right");
  assertEquals(sequence[1], "down", "second segment should be down");
}

function testRecognizeWithGeometricPaths(): void {
  const actions: GestureAction[] = [
    { pattern: ["left"], action: "go-back" },
    { pattern: ["right"], action: "go-forward" },
    { pattern: ["up", "down"], action: "reload" },
    { pattern: ["down", "up"], action: "new-tab" },
    { pattern: ["right", "down", "left", "up", "right"], action: "square" },
    { pattern: ["up", "right"], action: "diagonal-shape" },
  ];

  const { recognizer, shapeDb } = createRecognizer(actions);

  const rightTrail = buildLineTrail(0, 0, 160, 0);
  const rightResult = recognize(recognizer, rightTrail, 0.7, shapeDb);
  assert(rightResult !== null, "right line should be recognized");
  assertEquals(rightResult.patternName, "right", "straight horizontal line should match right pattern");
  assertEquals(rightResult.score, 1, "geometric straight-line detection should use max confidence");

  const upDownTrail = joinTrails(
    buildLineTrail(0, 120, 0, 0),
    buildLineTrail(0, 0, 0, 120),
  );
  const upDownResult = recognize(recognizer, upDownTrail, 0.7, shapeDb);
  assert(upDownResult !== null, "vertical oscillation should be recognized");
  assertEquals(upDownResult.patternName, "up-down", "initial upward movement should resolve to up-down");

  const downUpTrail = joinTrails(
    buildLineTrail(0, 0, 0, 120),
    buildLineTrail(0, 120, 0, 0),
  );
  const downUpResult = recognize(recognizer, downUpTrail, 0.7, shapeDb);
  assert(downUpResult !== null, "reverse vertical oscillation should be recognized");
  assertEquals(downUpResult.patternName, "down-up", "initial downward movement should resolve to down-up");

  const squareTrail = joinTrails(
    buildLineTrail(0, 0, 140, 0),
    buildLineTrail(140, 0, 140, 140),
    buildLineTrail(140, 140, 0, 140),
    buildLineTrail(0, 140, 0, 0),
    buildLineTrail(0, 0, 140, 0),
  );
  const squareResult = recognize(recognizer, squareTrail, 0.7, shapeDb, 20);
  assert(squareResult !== null, "direction sequence trail should be recognized");
  assertEquals(
    squareResult.patternName,
    "right-down-left-up-right",
    "sequence recognition should match exact configured pattern",
  );
}

function testUnconfiguredSimplePatternReturnsNull(): void {
  const actions: GestureAction[] = [
    { pattern: ["up", "right"], action: "complex-only" },
  ];
  const { recognizer, shapeDb } = createRecognizer(actions);
  const leftTrail = buildLineTrail(120, 0, 0, 0);
  const result = recognize(recognizer, leftTrail, 0.7, shapeDb);
  assertEquals(result, null, "unconfigured simple straight-line gesture should not match complex templates");
}

function testSimplePatternsStayOutOfDollarRecognizer(): void {
  const actions: GestureAction[] = [
    { pattern: ["left"], action: "go-back" },
    { pattern: ["right"], action: "go-forward" },
    { pattern: ["up", "down"], action: "reload" },
    { pattern: ["down", "up"], action: "new-tab" },
  ];
  const { recognizer, shapeDb } = createRecognizer(actions);
  assertEquals(recognizer.Unistrokes.length, 0, "simple patterns should not be registered as $1 templates");
  assert(shapeDb.has("left"), "simple patterns should still exist in shapeDb");
  assert(shapeDb.has("up-down"), "oscillating patterns should still exist in shapeDb");
}

export function runAllTests(): void {
  testAngleAndDeltaDirection();
  testPointConversion();
  testRecognizerCreation();
  testExtractDirectionSequence();
  testRecognizeWithGeometricPaths();
  testUnconfiguredSimplePatternReturnsNull();
  testSimplePatternsStayOutOfDollarRecognizer();
}
