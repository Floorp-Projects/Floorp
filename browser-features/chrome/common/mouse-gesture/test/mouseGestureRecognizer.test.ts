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
import type { GestureAction } from "../config.ts";
import {
  assert,
  assertApprox,
  type TestCase,
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
  const { recognizer, shapeDb } = createRecognizer(actions);
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
    { pattern: [] as string[], action: "empty-pattern" },
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
  ]);
}
