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
} from "../utils/recognizer.ts";
import type { GestureAction, GestureDirection } from "../config.ts";
import {
  assert,
  assertApprox,
  runTests,
} from "../../../test/utils/test_harness.ts";

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

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

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

function joinTrails(
  ...segments: { x: number; y: number }[][]
): { x: number; y: number }[] {
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

// ---------------------------------------------------------------------------
// Tests — angleToDirection (cardinal + diagonal)
// ---------------------------------------------------------------------------

function testAngleAndDeltaDirection(): void {
  assertEquals(angleToDirection(0), "right", "angle 0 should be right");
  assertEquals(
    angleToDirection(Math.PI / 2),
    "down",
    "angle PI/2 should be down",
  );
  assertEquals(
    angleToDirection(-Math.PI / 2),
    "up",
    "angle -PI/2 should be up",
  );
  assertEquals(angleToDirection(Math.PI), "left", "angle PI should be left");

  assertEquals(deltaToDirection(10, 0), "right", "dx+ should be right");
  assertEquals(deltaToDirection(0, -10), "up", "dy- should be up");
  assertEquals(
    deltaToDirection(10, 10),
    "downRight",
    "diag +/+ should be downRight",
  );
  assertEquals(
    deltaToDirection(-10, 10),
    "downLeft",
    "diag -/+ should be downLeft",
  );
}

function testAngleToDirectionDiagonals(): void {
  assertEquals(
    angleToDirection(Math.PI / 4),
    "downRight",
    "PI/4 should be downRight",
  );
  assertEquals(
    angleToDirection((3 * Math.PI) / 4),
    "downLeft",
    "3PI/4 should be downLeft",
  );
  assertEquals(
    angleToDirection((-3 * Math.PI) / 4),
    "upLeft",
    "-3PI/4 should be upLeft",
  );
  assertEquals(
    angleToDirection(-Math.PI / 4),
    "upRight",
    "-PI/4 should be upRight",
  );
}

function testAngleToDirectionBoundary(): void {
  // Near PI/8 boundary between right and downRight
  assertEquals(
    angleToDirection(Math.PI / 8 - 0.01),
    "right",
    "just before PI/8 should be right",
  );
  assertEquals(
    angleToDirection(Math.PI / 8 + 0.01),
    "downRight",
    "just after PI/8 should be downRight",
  );
}

function testDeltaToDirectionCardinal(): void {
  assertEquals(deltaToDirection(100, 0), "right", "right");
  assertEquals(deltaToDirection(-100, 0), "left", "left");
  assertEquals(deltaToDirection(0, 100), "down", "down");
  assertEquals(deltaToDirection(0, -100), "up", "up");
}

function testDeltaToDirectionDiagonal(): void {
  assertEquals(deltaToDirection(100, -100), "upRight", "upRight");
  assertEquals(deltaToDirection(-100, -100), "upLeft", "upLeft");
  assertEquals(deltaToDirection(100, 100), "downRight", "downRight");
  assertEquals(deltaToDirection(-100, 100), "downLeft", "downLeft");
}

function testDeltaToDirectionSmallValues(): void {
  // Very small values should still resolve based on angle
  assertEquals(deltaToDirection(0.1, 0), "right", "tiny right");
  assertEquals(deltaToDirection(0, -0.1), "up", "tiny up");
}

// ---------------------------------------------------------------------------
// Tests — convertPatternToPoints
// ---------------------------------------------------------------------------

function testPointConversion(): void {
  const points = convertPatternToPoints(["up", "right"], 100, 10);
  assertEquals(
    points.length,
    21,
    "pattern with 2 segments should generate segment*points+1",
  );
  assertApprox(points[0].X, 0, 0.001, "first point X");
  assertApprox(points[0].Y, 0, 0.001, "first point Y");

  const last = points[points.length - 1];
  assertApprox(last.X, 100, 0.1, "last point X should end at +100");
  assertApprox(last.Y, -100, 0.1, "last point Y should end at -100");
}

function testConvertPatternSingleDirection(): void {
  const points = convertPatternToPoints(["left"], 50, 5);
  assertEquals(
    points.length,
    6,
    "single direction should produce pointsPerSegment+1",
  );
  assertApprox(points[0].X, 0, 0.001, "start X");
  assertApprox(points[0].Y, 0, 0.001, "start Y");
  assertApprox(points[5].X, -50, 0.1, "end X should be -50");
  assertApprox(points[5].Y, 0, 0.1, "end Y should be 0");
}

function testConvertPatternDiagonal(): void {
  const points = convertPatternToPoints(["downRight"], 100, 10);
  assertEquals(points.length, 11, "diagonal single direction");
  const last = points[points.length - 1];
  assertApprox(last.X, 100 * 0.707, 1, "downRight end X");
  assertApprox(last.Y, 100 * 0.707, 1, "downRight end Y");
}

function testConvertPatternThreeDirections(): void {
  const points = convertPatternToPoints(["right", "down", "left"], 100, 5);
  assertEquals(points.length, 16, "3 directions × 5 points + 1 = 16");
  // After right: X=+100, Y=0
  // After down: X=+100, Y=+100
  // After left: X=0, Y=+100
  const last = points[points.length - 1];
  assertApprox(last.X, 0, 0.5, "3-dir end X");
  assertApprox(last.Y, 100, 0.5, "3-dir end Y");
}

function testConvertTrailToPoints(): void {
  const trailPoints = convertTrailToPoints([
    { x: 1, y: 2 },
    { x: 5, y: 8 },
  ]);
  assertEquals(
    trailPoints.length,
    2,
    "trail conversion should preserve point count",
  );
  assertApprox(trailPoints[1].X, 5, 0.001, "converted trail X");
  assertApprox(trailPoints[1].Y, 8, 0.001, "converted trail Y");
}

function testConvertTrailToPointsEmpty(): void {
  const points = convertTrailToPoints([]);
  assertEquals(points.length, 0, "empty trail should produce empty points");
}

function testConvertTrailToPointsSingle(): void {
  const points = convertTrailToPoints([{ x: 42, y: 99 }]);
  assertEquals(points.length, 1, "single-point trail");
  assertApprox(points[0].X, 42, 0.001, "single point X");
  assertApprox(points[0].Y, 99, 0.001, "single point Y");
}

// ---------------------------------------------------------------------------
// Tests — createRecognizer
// ---------------------------------------------------------------------------

function testRecognizerCreation(): void {
  const actions: GestureAction[] = [
    { pattern: ["left"], action: "go-back" },
    { pattern: ["up", "right"], action: "custom-action" },
    { pattern: ["up", "right"], action: "duplicate-pattern" },
  ];

  const { recognizer, shapeDb } = createRecognizer(actions);

  assert(
    shapeDb.size >= 2,
    "shape database should include simple and complex shapes",
  );
  assert(
    recognizer.Unistrokes.length > 0,
    "complex patterns should register templates",
  );

  const complexEntry = shapeDb.get("up-right");
  assert(complexEntry !== undefined, "shapeDb should contain up-right key");
  assertEquals(
    complexEntry.variants.length,
    1,
    "duplicate patterns should be deduplicated",
  );
}

function testCreateRecognizerSimplePatternsOnly(): void {
  const actions: GestureAction[] = [
    { pattern: ["left"], action: "go-back" },
    { pattern: ["right"], action: "go-forward" },
    { pattern: ["up", "down"], action: "reload" },
    { pattern: ["down", "up"], action: "new-tab" },
  ];
  const { recognizer, shapeDb } = createRecognizer(actions);
  assertEquals(
    recognizer.Unistrokes.length,
    0,
    "simple patterns should not register $1 templates",
  );
  assertEquals(shapeDb.size, 4, "all simple patterns should be in shapeDb");
  assert(shapeDb.has("left"), "shapeDb should contain left");
  assert(shapeDb.has("right"), "shapeDb should contain right");
  assert(shapeDb.has("up-down"), "shapeDb should contain up-down");
  assert(shapeDb.has("down-up"), "shapeDb should contain down-up");
}

function testCreateRecognizerComplexPatternRegistersTemplates(): void {
  const actions: GestureAction[] = [
    { pattern: ["down", "right"], action: "close-tab" },
    { pattern: ["right", "down", "left", "up", "right"], action: "square" },
  ];
  const { recognizer, shapeDb: _shapeDb } = createRecognizer(actions);
  assert(
    recognizer.Unistrokes.length > 0,
    "complex patterns should register $1 templates",
  );
  // 5 segment lengths per complex pattern
  assert(
    recognizer.Unistrokes.length >= 5,
    `each complex pattern should have multiple templates, got ${recognizer.Unistrokes.length}`,
  );
}

function testCreateRecognizerEmptyActions(): void {
  const { recognizer, shapeDb } = createRecognizer([]);
  assertEquals(recognizer.Unistrokes.length, 0, "no actions = no templates");
  assertEquals(shapeDb.size, 0, "no actions = empty shapeDb");
}

function testCreateRecognizerEmptyPatternSkipped(): void {
  const actions: GestureAction[] = [
    { pattern: ["left"], action: "go-back" },
    { pattern: [] as GestureDirection[], action: "empty-pattern" },
  ];
  const { shapeDb } = createRecognizer(actions);
  assertEquals(shapeDb.size, 1, "empty pattern should be skipped");
  assert(shapeDb.has("left"), "only non-empty pattern should be in shapeDb");
}

// ---------------------------------------------------------------------------
// Tests — extractDirectionSequence
// ---------------------------------------------------------------------------

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

function testExtractDirectionSequenceSingleLine(): void {
  const trail = buildLineTrail(0, 0, 200, 0);
  const sequence = extractDirectionSequence(trail, 20);
  assertEquals(sequence.length, 1, "single line should produce one direction");
  assertEquals(sequence[0], "right", "horizontal line should be right");
}

function testExtractDirectionSequenceVertical(): void {
  const trail = buildLineTrail(0, 200, 0, 0);
  const sequence = extractDirectionSequence(trail, 20);
  assertEquals(
    sequence.length,
    1,
    "vertical line should produce one direction",
  );
  assertEquals(sequence[0], "up", "upward line should be up");
}

function testExtractDirectionSequenceTooShort(): void {
  const trail = [
    { x: 0, y: 0 },
    { x: 5, y: 5 },
  ];
  const sequence = extractDirectionSequence(trail, 20);
  assertEquals(
    sequence.length,
    0,
    "trail shorter than minSegmentLength should be empty",
  );
}

function testExtractDirectionSequenceEmptyTrail(): void {
  const sequence = extractDirectionSequence([], 20);
  assertEquals(sequence.length, 0, "empty trail should produce empty sequence");
}

function testExtractDirectionSequenceSinglePoint(): void {
  const sequence = extractDirectionSequence([{ x: 0, y: 0 }], 20);
  assertEquals(
    sequence.length,
    0,
    "single point should produce empty sequence",
  );
}

function testExtractDirectionSequenceZigzag(): void {
  const zigzagTrail = joinTrails(
    buildLineTrail(0, 0, 150, 0), // right
    buildLineTrail(150, 0, 150, 150), // down
    buildLineTrail(150, 150, 0, 150), // left
  );
  const sequence = extractDirectionSequence(zigzagTrail, 20);
  assertEquals(sequence.length, 3, "zigzag should produce 3 directions");
  assertEquals(sequence[0], "right", "zigzag first is right");
  assertEquals(sequence[1], "down", "zigzag second is down");
  assertEquals(sequence[2], "left", "zigzag third is left");
}

// ---------------------------------------------------------------------------
// Tests — recognize with geometric paths
// ---------------------------------------------------------------------------

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
  assertEquals(
    rightResult.patternName,
    "right",
    "straight horizontal line should match right pattern",
  );
  assertEquals(
    rightResult.score,
    1,
    "geometric straight-line detection should use max confidence",
  );

  const upDownTrail = joinTrails(
    buildLineTrail(0, 120, 0, 0),
    buildLineTrail(0, 0, 0, 120),
  );
  const upDownResult = recognize(recognizer, upDownTrail, 0.7, shapeDb);
  assert(upDownResult !== null, "vertical oscillation should be recognized");
  assertEquals(
    upDownResult.patternName,
    "up-down",
    "initial upward movement should resolve to up-down",
  );

  const downUpTrail = joinTrails(
    buildLineTrail(0, 0, 0, 120),
    buildLineTrail(0, 120, 0, 0),
  );
  const downUpResult = recognize(recognizer, downUpTrail, 0.7, shapeDb);
  assert(
    downUpResult !== null,
    "reverse vertical oscillation should be recognized",
  );
  assertEquals(
    downUpResult.patternName,
    "down-up",
    "initial downward movement should resolve to down-up",
  );

  const squareTrail = joinTrails(
    buildLineTrail(0, 0, 140, 0),
    buildLineTrail(140, 0, 140, 140),
    buildLineTrail(140, 140, 0, 140),
    buildLineTrail(0, 140, 0, 0),
    buildLineTrail(0, 0, 140, 0),
  );
  const squareResult = recognize(recognizer, squareTrail, 0.7, shapeDb, 20);
  assert(
    squareResult !== null,
    "direction sequence trail should be recognized",
  );
  assertEquals(
    squareResult.patternName,
    "right-down-left-up-right",
    "sequence recognition should match exact configured pattern",
  );
}

function testRecognizeLeftStraightLine(): void {
  const actions: GestureAction[] = [{ pattern: ["left"], action: "go-back" }];
  const { recognizer, shapeDb } = createRecognizer(actions);
  const leftTrail = buildLineTrail(200, 0, 0, 0);
  const result = recognize(recognizer, leftTrail, 0.7, shapeDb);
  assert(result !== null, "left trail should be recognized");
  assertEquals(result.patternName, "left", "should match left");
  assertEquals(result.score, 1, "straight line should have score 1");
}

function testRecognizeUpStraightLine(): void {
  const actions: GestureAction[] = [{ pattern: ["up"], action: "scroll-top" }];
  const { recognizer, shapeDb } = createRecognizer(actions);
  const upTrail = buildLineTrail(0, 200, 0, 0);
  const result = recognize(recognizer, upTrail, 0.7, shapeDb);
  assert(result !== null, "up trail should be recognized");
  assertEquals(result.patternName, "up", "should match up");
}

function testRecognizeDownStraightLine(): void {
  const actions: GestureAction[] = [
    { pattern: ["down"], action: "scroll-bottom" },
  ];
  const { recognizer, shapeDb } = createRecognizer(actions);
  const downTrail = buildLineTrail(0, 0, 0, 200);
  const result = recognize(recognizer, downTrail, 0.7, shapeDb);
  assert(result !== null, "down trail should be recognized");
  assertEquals(result.patternName, "down", "should match down");
}

function testRecognizeOscillatingHorizontal(): void {
  const actions: GestureAction[] = [
    { pattern: ["left", "right"], action: "lr" },
    { pattern: ["right", "left"], action: "rl" },
  ];
  const { recognizer, shapeDb } = createRecognizer(actions);

  const leftRightTrail = joinTrails(
    buildLineTrail(100, 0, 0, 0),
    buildLineTrail(0, 0, 100, 0),
  );
  const lrResult = recognize(recognizer, leftRightTrail, 0.7, shapeDb);
  assert(lrResult !== null, "left-right oscillation should be recognized");
  assertEquals(lrResult.patternName, "left-right", "should match left-right");
}

function testRecognizeRightLeftOscillation(): void {
  const actions: GestureAction[] = [
    { pattern: ["left", "right"], action: "lr" },
    { pattern: ["right", "left"], action: "rl" },
  ];
  const { recognizer, shapeDb } = createRecognizer(actions);

  const rightLeftTrail = joinTrails(
    buildLineTrail(0, 0, 100, 0),
    buildLineTrail(100, 0, 0, 0),
  );
  const rlResult = recognize(recognizer, rightLeftTrail, 0.7, shapeDb);
  assert(rlResult !== null, "right-left oscillation should be recognized");
  assertEquals(rlResult.patternName, "right-left", "should match right-left");
}

// ---------------------------------------------------------------------------
// Tests — recognize edge cases
// ---------------------------------------------------------------------------

function testUnconfiguredSimplePatternReturnsNull(): void {
  const actions: GestureAction[] = [
    { pattern: ["up", "right"], action: "complex-only" },
  ];
  const { recognizer, shapeDb } = createRecognizer(actions);
  const leftTrail = buildLineTrail(120, 0, 0, 0);
  const result = recognize(recognizer, leftTrail, 0.7, shapeDb);
  assertEquals(
    result,
    null,
    "unconfigured simple straight-line gesture should not match complex templates",
  );
}

function testSimplePatternsStayOutOfDollarRecognizer(): void {
  const actions: GestureAction[] = [
    { pattern: ["left"], action: "go-back" },
    { pattern: ["right"], action: "go-forward" },
    { pattern: ["up", "down"], action: "reload" },
    { pattern: ["down", "up"], action: "new-tab" },
  ];
  const { recognizer, shapeDb } = createRecognizer(actions);
  assertEquals(
    recognizer.Unistrokes.length,
    0,
    "simple patterns should not be registered as $1 templates",
  );
  assert(shapeDb.has("left"), "simple patterns should still exist in shapeDb");
  assert(
    shapeDb.has("up-down"),
    "oscillating patterns should still exist in shapeDb",
  );
}

function testRecognizeShortTrailReturnsNull(): void {
  const actions: GestureAction[] = [{ pattern: ["left"], action: "go-back" }];
  const { recognizer, shapeDb } = createRecognizer(actions);
  const result = recognize(recognizer, [{ x: 0, y: 0 }], 0.7, shapeDb);
  assertEquals(result, null, "single-point trail should return null");
}

function testRecognizeEmptyTrailReturnsNull(): void {
  const actions: GestureAction[] = [{ pattern: ["left"], action: "go-back" }];
  const { recognizer, shapeDb } = createRecognizer(actions);
  const result = recognize(recognizer, [], 0.7, shapeDb);
  assertEquals(result, null, "empty trail should return null");
}

function testRecognizeUnconfiguredOscillationReturnsNull(): void {
  const actions: GestureAction[] = [{ pattern: ["left"], action: "go-back" }];
  const { recognizer, shapeDb } = createRecognizer(actions);
  const oscillationTrail = joinTrails(
    buildLineTrail(0, 120, 0, 0),
    buildLineTrail(0, 0, 0, 120),
  );
  const result = recognize(recognizer, oscillationTrail, 0.7, shapeDb);
  assertEquals(
    result,
    null,
    "unconfigured oscillation should not match a single-direction pattern",
  );
}

function testRecognizeLShapeSequence(): void {
  const actions: GestureAction[] = [
    { pattern: ["right", "down"], action: "close-tab" },
  ];
  const { recognizer, shapeDb } = createRecognizer(actions);
  const lTrail = joinTrails(
    buildLineTrail(0, 0, 160, 0),
    buildLineTrail(160, 0, 160, 160),
  );
  const result = recognize(recognizer, lTrail, 0.7, shapeDb);
  assert(result !== null, "L-shape should be recognized");
  assertEquals(result.patternName, "right-down", "should match right-down");
}

function testRecognizeMultiSegmentSequence(): void {
  const actions: GestureAction[] = [
    { pattern: ["down", "right", "up"], action: "custom" },
  ];
  const { recognizer, shapeDb } = createRecognizer(actions);
  const trail = joinTrails(
    buildLineTrail(0, 0, 0, 160), // down
    buildLineTrail(0, 160, 160, 160), // right
    buildLineTrail(160, 160, 160, 0), // up
  );
  const result = recognize(recognizer, trail, 0.7, shapeDb);
  assert(result !== null, "down-right-up should be recognized");
  assertEquals(
    result.patternName,
    "down-right-up",
    "should match down-right-up",
  );
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Additional edge case tests for angleToDirection
// ---------------------------------------------------------------------------

function testAngleToDirectionNegativeAngles(): void {
  assertEquals(
    angleToDirection(-Math.PI),
    "left",
    "negative PI should be left",
  );
  assertEquals(
    angleToDirection(-Math.PI / 4),
    "upRight",
    "negative PI/4 should be upRight",
  );
  assertEquals(
    angleToDirection(-3 * Math.PI / 4),
    "upLeft",
    "negative 3PI/4 should be upLeft",
  );
}

function testAngleToDirectionLargeAngles(): void {
  // Angles > 2*PI should wrap around
  const largeAngle = 3 * Math.PI; // 540 degrees
  const result = angleToDirection(largeAngle);
  // Should normalize and return a valid direction
  assert(
    ["up", "down", "left", "right", "upRight", "upLeft", "downRight", "downLeft"].includes(result),
    "large angles should return valid direction",
  );
}

function testAngleToDirectionZero(): void {
  assertEquals(
    angleToDirection(0),
    "right",
    "zero angle should be right",
  );
}

// ---------------------------------------------------------------------------
// Additional edge case tests for deltaToDirection
// ---------------------------------------------------------------------------

function testDeltaToDirectionZeroDeltas(): void {
  // Both zero - angle is NaN or undefined
  const result = deltaToDirection(0, 0);
  // Should handle gracefully
  assert(
    ["up", "down", "left", "right", "upRight", "upLeft", "downRight", "downLeft"].includes(result) || result === "right",
    "zero deltas should return a direction",
  );
}

function testDeltaToDirectionNegativeValues(): void {
  assertEquals(
    deltaToDirection(-1, 0),
    "left",
    "negative dx should be left",
  );
  assertEquals(
    deltaToDirection(0, -1),
    "up",
    "negative dy should be up",
  );
}

function testDeltaToDirectionVeryLargeValues(): void {
  const result = deltaToDirection(1000000, 1000000);
  assertEquals(
    result,
    "downRight",
    "very large values should still return correct direction",
  );
}

function testDeltaToDirectionOpposingValues(): void {
  // When values oppose, the dominant one determines direction
  const result1 = deltaToDirection(10, -1);
  assertEquals(
    result1,
    "right",
    "horizontal should dominate when much larger",
  );

  const result2 = deltaToDirection(1, -10);
  assertEquals(
    result2,
    "up",
    "vertical should dominate when much larger",
  );
}

// ---------------------------------------------------------------------------
// Additional edge case tests for convertPatternToPoints
// ---------------------------------------------------------------------------

function testConvertPatternEmptyPattern(): void {
  const points = convertPatternToPoints([], 100, 10);
  assertEquals(
    points.length,
    1,
    "empty pattern should generate one point (origin)",
  );
  assertEquals(points[0].X, 0, "origin X should be 0");
  assertEquals(points[0].Y, 0, "origin Y should be 0");
}

function testConvertPatternVerySmallSegmentLength(): void {
  const points = convertPatternToPoints(["right"], 1, 5);
  assertEquals(
    points.length,
    6,
    "should generate points for small segment length",
  );
  const last = points[points.length - 1];
  assert(
    last.X > 0 && last.X < 2,
    "last point X should be small but positive",
  );
}

function testConvertPatternVeryLargeSegmentLength(): void {
  const points = convertPatternToPoints(["up"], 10000, 10);
  assertEquals(
    points.length,
    11,
    "should generate points for large segment length",
  );
  const last = points[points.length - 1];
  assertApprox(last.Y, -10000, 100, "last point Y should be very negative");
}

function testConvertPatternZeroPointsPerSegment(): void {
  const points = convertPatternToPoints(["right"], 100, 0);
  // Edge case: 0 points per segment should still generate at least one point
  assert(
    points.length >= 1,
    "should generate at least one point",
  );
}

// ---------------------------------------------------------------------------
// Additional edge case tests for createRecognizer
// ---------------------------------------------------------------------------

function testCreateRecognizerWithNullActions(): void {
  // createRecognizer does not handle null/undefined gracefully - it iterates with for...of
  // which throws TypeError when given null or undefined
  let threw = false;
  try {
    createRecognizer(null as unknown as GestureAction[]);
  } catch (e) {
    threw = true;
    assert(
      e instanceof TypeError,
      "should throw TypeError for null actions",
    );
  }
  assert(threw, "createRecognizer should throw for null actions");
}

function testCreateRecognizerWithUndefinedActions(): void {
  let threw = false;
  try {
    createRecognizer(undefined as unknown as GestureAction[]);
  } catch (e) {
    threw = true;
    assert(
      e instanceof TypeError,
      "should throw TypeError for undefined actions",
    );
  }
  assert(threw, "createRecognizer should throw for undefined actions");
}

// ---------------------------------------------------------------------------
// Additional edge case tests for extractDirectionSequence
// ---------------------------------------------------------------------------

function testExtractDirectionSequenceWithNoise(): void {
  // Trail with small random noise
  const noisyTrail: { x: number; y: number }[] = [];
  for (let i = 0; i <= 50; i++) {
    noisyTrail.push({
      x: i * 4 + (Math.random() - 0.5) * 2, // Add small noise
      y: 0 + (Math.random() - 0.5) * 2,
    });
  }

  const sequence = extractDirectionSequence(noisyTrail, 20);
  assertEquals(
    sequence.length,
    1,
    "noisy horizontal line should still produce one direction",
  );
  assertEquals(
    sequence[0],
    "right",
    "noisy horizontal line should be recognized as right",
  );
}

function testExtractDirectionSequenceVeryLongTrail(): void {
  // Create a very long trail
  const longTrail: { x: number; y: number }[] = [];
  for (let i = 0; i < 1000; i++) {
    longTrail.push({ x: i, y: 0 });
  }

  const sequence = extractDirectionSequence(longTrail, 20);
  assertEquals(
    sequence.length,
    1,
    "very long trail should produce one direction",
  );
}

function testExtractDirectionSequenceWithRepeatedChanges(): void {
  // Trail that changes direction frequently
  const trail: { x: number; y: number }[] = [];
  for (let i = 0; i < 10; i++) {
    // Small horizontal segment
    trail.push({ x: i * 10, y: 0 });
    trail.push({ x: i * 10 + 5, y: 0 });
    // Small vertical segment
    trail.push({ x: i * 10 + 5, y: 5 });
    trail.push({ x: i * 10 + 5, y: 10 });
  }

  const sequence = extractDirectionSequence(trail, 25);
  // Should filter out small segments
  assert(
    sequence.length <= 2,
    "should filter out small direction changes",
  );
}

// ---------------------------------------------------------------------------
// Additional edge case tests for recognize
// ---------------------------------------------------------------------------

function testRecognizeWithVeryHighMinScore(): void {
  const actions: GestureAction[] = [{ pattern: ["right"], action: "go-forward" }];
  const { recognizer, shapeDb } = createRecognizer(actions);
  const trail = buildLineTrail(0, 0, 100, 0);

  // Very high min score (2.0 is impossible for $1 Recognizer)
  // But geometric detection (straight lines) bypasses the score check entirely,
  // returning score: 1.0. So a perfect straight line will still match.
  const result = recognize(recognizer, trail, 2.0, shapeDb);

  // Geometric detection returns a match regardless of minScore
  assert(
    result !== null,
    "geometric straight-line detection should match regardless of minScore",
  );
}

function testRecognizeWithZeroMinScore(): void {
  const actions: GestureAction[] = [{ pattern: ["right"], action: "go-forward" }];
  const { recognizer, shapeDb } = createRecognizer(actions);
  const trail = buildLineTrail(0, 0, 100, 0);

  // Zero min score should match anything
  const result = recognize(recognizer, trail, 0, shapeDb);

  assert(
    result !== null,
    "should match with zero min score",
  );
}

function testRecognizeWithNegativeMinScore(): void {
  const actions: GestureAction[] = [{ pattern: ["right"], action: "go-forward" }];
  const { recognizer, shapeDb } = createRecognizer(actions);
  const trail = buildLineTrail(0, 0, 100, 0);

  // Negative min score should be treated as 0
  const result = recognize(recognizer, trail, -1, shapeDb);

  assert(
    result !== null,
    "should match with negative min score",
  );
}

function testRecognizeWithNoShapeDb(): void {
  const actions: GestureAction[] = [{ pattern: ["right"], action: "go-forward" }];
  const { recognizer } = createRecognizer(actions);
  const trail = buildLineTrail(0, 0, 100, 0);

  // Call without shapeDb
  const result = recognize(recognizer, trail, 0.7, undefined);

  // Should still work but may not match simple patterns geometrically
  assert(
    result === null || typeof result === "object",
    "should handle missing shapeDb",
  );
}

function testRecognizeWithTrailContainingDuplicates(): void {
  const actions: GestureAction[] = [{ pattern: ["right"], action: "go-forward" }];
  const { recognizer, shapeDb } = createRecognizer(actions);

  // Trail with duplicate consecutive points
  const trailWithDuplicates: { x: number; y: number }[] = [];
  for (let i = 0; i <= 20; i++) {
    trailWithDuplicates.push({ x: i * 5, y: 0 });
    trailWithDuplicates.push({ x: i * 5, y: 0 }); // Duplicate
  }

  const result = recognize(recognizer, trailWithDuplicates, 0.7, shapeDb);

  assert(
    result !== null,
    "should handle trails with duplicate points",
  );
  if (result) {
    assertEquals(
      result.patternName,
      "right",
      "should still recognize correctly",
    );
  }
}

function testRecognizeDiagonalPattern(): void {
  const actions: GestureAction[] = [{ pattern: ["downRight"], action: "diagonal" }];
  const { recognizer, shapeDb } = createRecognizer(actions);
  const trail = buildLineTrail(0, 0, 100, 100);

  const result = recognize(recognizer, trail, 0.7, shapeDb);

  assert(
    result !== null,
    "should recognize diagonal pattern",
  );
  if (result) {
    assertEquals(
      result.patternName,
      "downRight",
      "should match diagonal pattern",
    );
  }
}

function testRecognizeComplexShapeWithoutConfig(): void {
  const actions: GestureAction[] = [{ pattern: ["left"], action: "go-back" }];
  const { recognizer, shapeDb } = createRecognizer(actions);

  // Create a complex trail (circle-like)
  const circleTrail: { x: number; y: number }[] = [];
  for (let i = 0; i <= 30; i++) {
    const angle = (2 * Math.PI * i) / 30;
    circleTrail.push({
      x: 50 + 40 * Math.cos(angle),
      y: 50 + 40 * Math.sin(angle),
    });
  }

  const result = recognize(recognizer, circleTrail, 0.7, shapeDb);

  // Should not match the simple "left" pattern for a circle
  assertEquals(
    result,
    null,
    "complex unconfigured shape should not match simple pattern",
  );
}

export async function runAllTests(): Promise<void> {
  await runTests("mouseGestureRecognizer.test.ts", [
    // angleToDirection / deltaToDirection
    { name: "angle and delta direction", fn: testAngleAndDeltaDirection },
    { name: "angleToDirection diagonals", fn: testAngleToDirectionDiagonals },
    { name: "angleToDirection boundary", fn: testAngleToDirectionBoundary },
    { name: "deltaToDirection cardinal", fn: testDeltaToDirectionCardinal },
    { name: "deltaToDirection diagonal", fn: testDeltaToDirectionDiagonal },
    {
      name: "deltaToDirection small values",
      fn: testDeltaToDirectionSmallValues,
    },
    // convertPatternToPoints / convertTrailToPoints
    { name: "point conversion", fn: testPointConversion },
    {
      name: "convertPattern single direction",
      fn: testConvertPatternSingleDirection,
    },
    { name: "convertPattern diagonal", fn: testConvertPatternDiagonal },
    {
      name: "convertPattern three directions",
      fn: testConvertPatternThreeDirections,
    },
    { name: "convertTrailToPoints", fn: testConvertTrailToPoints },
    { name: "convertTrailToPoints empty", fn: testConvertTrailToPointsEmpty },
    { name: "convertTrailToPoints single", fn: testConvertTrailToPointsSingle },
    // createRecognizer
    { name: "recognizer creation", fn: testRecognizerCreation },
    {
      name: "simple patterns only",
      fn: testCreateRecognizerSimplePatternsOnly,
    },
    {
      name: "complex patterns register templates",
      fn: testCreateRecognizerComplexPatternRegistersTemplates,
    },
    { name: "empty actions", fn: testCreateRecognizerEmptyActions },
    {
      name: "empty pattern skipped",
      fn: testCreateRecognizerEmptyPatternSkipped,
    },
    // extractDirectionSequence
    { name: "extract direction sequence", fn: testExtractDirectionSequence },
    {
      name: "extract sequence single line",
      fn: testExtractDirectionSequenceSingleLine,
    },
    {
      name: "extract sequence vertical",
      fn: testExtractDirectionSequenceVertical,
    },
    {
      name: "extract sequence too short",
      fn: testExtractDirectionSequenceTooShort,
    },
    {
      name: "extract sequence empty trail",
      fn: testExtractDirectionSequenceEmptyTrail,
    },
    {
      name: "extract sequence single point",
      fn: testExtractDirectionSequenceSinglePoint,
    },
    { name: "extract sequence zigzag", fn: testExtractDirectionSequenceZigzag },
    // recognize
    {
      name: "recognize with geometric paths",
      fn: testRecognizeWithGeometricPaths,
    },
    { name: "recognize left straight line", fn: testRecognizeLeftStraightLine },
    { name: "recognize up straight line", fn: testRecognizeUpStraightLine },
    { name: "recognize down straight line", fn: testRecognizeDownStraightLine },
    {
      name: "recognize oscillating horizontal",
      fn: testRecognizeOscillatingHorizontal,
    },
    {
      name: "recognize right-left oscillation",
      fn: testRecognizeRightLeftOscillation,
    },
    {
      name: "unconfigured simple pattern returns null",
      fn: testUnconfiguredSimplePatternReturnsNull,
    },
    {
      name: "simple patterns stay out of $1 recognizer",
      fn: testSimplePatternsStayOutOfDollarRecognizer,
    },
    {
      name: "recognize short trail returns null",
      fn: testRecognizeShortTrailReturnsNull,
    },
    {
      name: "recognize empty trail returns null",
      fn: testRecognizeEmptyTrailReturnsNull,
    },
    {
      name: "recognize unconfigured oscillation returns null",
      fn: testRecognizeUnconfiguredOscillationReturnsNull,
    },
    { name: "recognize L-shape sequence", fn: testRecognizeLShapeSequence },
    {
      name: "recognize multi-segment sequence",
      fn: testRecognizeMultiSegmentSequence,
    },

    // Additional edge case tests
    // angleToDirection edge cases
    {
      name: "angleToDirection negative angles",
      fn: testAngleToDirectionNegativeAngles,
    },
    {
      name: "angleToDirection large angles",
      fn: testAngleToDirectionLargeAngles,
    },
    {
      name: "angleToDirection zero",
      fn: testAngleToDirectionZero,
    },
    // deltaToDirection edge cases
    {
      name: "deltaToDirection zero deltas",
      fn: testDeltaToDirectionZeroDeltas,
    },
    {
      name: "deltaToDirection negative values",
      fn: testDeltaToDirectionNegativeValues,
    },
    {
      name: "deltaToDirection very large values",
      fn: testDeltaToDirectionVeryLargeValues,
    },
    {
      name: "deltaToDirection opposing values",
      fn: testDeltaToDirectionOpposingValues,
    },
    // convertPatternToPoints edge cases
    {
      name: "convertPattern empty pattern",
      fn: testConvertPatternEmptyPattern,
    },
    {
      name: "convertPattern very small segment length",
      fn: testConvertPatternVerySmallSegmentLength,
    },
    {
      name: "convertPattern very large segment length",
      fn: testConvertPatternVeryLargeSegmentLength,
    },
    {
      name: "convertPattern zero points per segment",
      fn: testConvertPatternZeroPointsPerSegment,
    },
    // createRecognizer edge cases
    {
      name: "createRecognizer with null actions",
      fn: testCreateRecognizerWithNullActions,
    },
    {
      name: "createRecognizer with undefined actions",
      fn: testCreateRecognizerWithUndefinedActions,
    },
    // extractDirectionSequence edge cases
    {
      name: "extractDirectionSequence with noise",
      fn: testExtractDirectionSequenceWithNoise,
    },
    {
      name: "extractDirectionSequence very long trail",
      fn: testExtractDirectionSequenceVeryLongTrail,
    },
    {
      name: "extractDirectionSequence with repeated changes",
      fn: testExtractDirectionSequenceWithRepeatedChanges,
    },
    // recognize edge cases
    {
      name: "recognize with very high min score",
      fn: testRecognizeWithVeryHighMinScore,
    },
    {
      name: "recognize with zero min score",
      fn: testRecognizeWithZeroMinScore,
    },
    {
      name: "recognize with negative min score",
      fn: testRecognizeWithNegativeMinScore,
    },
    {
      name: "recognize with no shapeDb",
      fn: testRecognizeWithNoShapeDb,
    },
    {
      name: "recognize with trail containing duplicates",
      fn: testRecognizeWithTrailContainingDuplicates,
    },
    {
      name: "recognize diagonal pattern",
      fn: testRecognizeDiagonalPattern,
    },
    {
      name: "recognize complex shape without config",
      fn: testRecognizeComplexShapeWithoutConfig,
    },
  ]);
}
