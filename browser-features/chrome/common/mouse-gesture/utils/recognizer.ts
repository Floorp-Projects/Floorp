/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Mouse gesture recognizer using the $1 Unistroke Recognizer algorithm.
 *
 * This module provides the core gesture recognition functionality:
 * - Converts direction patterns from preferences into point arrays
 * - Uses the $1 Recognizer to match mouse trails against patterns
 *
 * The $1 Unistroke Recognizer is a simple, fast algorithm for recognizing
 * single-stroke gestures by comparing them against a set of templates.
 */

import {
  DollarRecognizer,
  Point,
  type DollarPoint,
  type IDollarRecognizer,
} from "./dollar.ts";
import type { GestureDirection, GestureAction } from "../config.ts";

/**
 * Direction vectors mapping each gesture direction to X/Y deltas.
 * Used to convert direction patterns to point sequences.
 */
const DIRECTION_VECTORS: Record<GestureDirection, { dx: number; dy: number }> = {
  right: { dx: 1, dy: 0 },
  downRight: { dx: 0.707, dy: 0.707 },
  down: { dx: 0, dy: 1 },
  downLeft: { dx: -0.707, dy: 0.707 },
  left: { dx: -1, dy: 0 },
  upLeft: { dx: -0.707, dy: -0.707 },
  up: { dx: 0, dy: -1 },
  upRight: { dx: 0.707, dy: -0.707 },
};

/**
 * Shape variant entry containing pattern name and its first direction.
 * Used to store multiple patterns that share the same shape signature.
 */
interface ShapeVariant {
  patternName: string;
  pattern: GestureDirection[];
  firstDirection: GestureDirection | null;
}

/**
 * Shape database entry containing the representative shape key and its variants.
 * The shape key is the pattern name used to register with $1 recognizer.
 * Variants are all patterns that share this shape signature.
 */
export interface ShapeEntry {
  shapeKey: string;
  variants: ShapeVariant[];
}

/**
 * Shape database that maps shape keys to their entries.
 * This allows registering only unique shapes to $1 while tracking
 * all direction variants for each shape.
 */
export type ShapeDatabase = Map<string, ShapeEntry>;

/**
 * Create a $1 Recognizer Point object.
 */
function createPoint(x: number, y: number): DollarPoint {
  return new (Point as unknown as new (x: number, y: number) => DollarPoint)(x, y);
}

/**
 * Convert a direction pattern from preferences to an array of points.
 *
 * This is the key conversion function that bridges the preference format
 * (array of direction names like ["up", "right"]) with the $1 Recognizer
 * format (array of {X, Y} points).
 *
 * Example: ["up", "right"] becomes a series of points forming an L shape.
 */
export function convertPatternToPoints(
  pattern: GestureDirection[],
  segmentLength = 100,
  pointsPerSegment = 20,
): DollarPoint[] {
  const points: DollarPoint[] = [];
  let currentX = 0;
  let currentY = 0;

  // For each direction in the pattern, generate points along that direction
  for (const direction of pattern) {
    const vector = DIRECTION_VECTORS[direction];
    const stepX = (vector.dx * segmentLength) / pointsPerSegment;
    const stepY = (vector.dy * segmentLength) / pointsPerSegment;

    for (let i = 0; i < pointsPerSegment; i++) {
      points.push(createPoint(currentX, currentY));
      currentX += stepX;
      currentY += stepY;
    }
  }

  // Add the final point
  points.push(createPoint(currentX, currentY));

  return points;
}

/**
 * Convert mouse trail coordinates to $1 Recognizer point format.
 */
export function convertTrailToPoints(
  trail: { x: number; y: number }[],
): DollarPoint[] {
  return trail.map((point) => createPoint(point.x, point.y));
}

/**
 * Segment lengths for generating multiple-size templates.
 * The $1 Unistroke Recognizer works better with multiple templates
 * at different scales to handle varying gesture sizes.
 *
 * These values represent small, medium, and large gesture sizes
 * to accommodate different screen sizes and user gesture styles.
 */
const SEGMENT_LENGTHS = [50, 100, 200, 400, 800];

/**
 * Get the first direction in a pattern.
 * Returns the first direction directly since patterns are already stored as GestureDirection arrays.
 * This is used to distinguish opposite gestures like "up-down" vs "down-up".
 */
function getPatternFirstDirection(pattern: GestureDirection[]): GestureDirection | null {
  if (pattern.length === 0) {
    return null;
  }
  return pattern[0];
}

/**
 * Check if a pattern is a simple geometric pattern that should not be registered in $1 Recognizer.
 * Simple patterns include:
 * - Single-direction gestures (length === 1): e.g., ["left"], ["up"], ["right"]
 * - Two-direction oscillating gestures (length === 2): e.g., ["up", "down"], ["left", "right"]
 * 
 * Note: Diagonal oscillations like ["upLeft", "downRight"] are NOT considered simple
 * because they don't oscillate along a single axis. They're more like diagonal lines
 * and should be handled by $1 Recognizer.
 * 
 * These patterns are detected reliably by geometric analysis and should not be
 * matched by the $1 Recognizer to prevent complex gestures from incorrectly
 * matching simple patterns.
 */
function isSimplePattern(pattern: GestureDirection[]): boolean {
  // Single direction gestures
  if (pattern.length === 1) {
    return true;
  }
  
  // Two-direction oscillating gestures (back-and-forth)
  if (pattern.length === 2) {
    const first = pattern[0];
    const second = pattern[1];
    
    // Check for vertical oscillation (up-down or down-up)
    if ((first === "up" && second === "down") || (first === "down" && second === "up")) {
      return true;
    }
    
    // Check for horizontal oscillation (left-right or right-left)
    if ((first === "left" && second === "right") || (first === "right" && second === "left")) {
      return true;
    }
  }
  
  return false;
}

/**
 * Result of creating a recognizer, including the shape database.
 */
export interface RecognizerWithShapeDb {
  recognizer: IDollarRecognizer;
  shapeDb: ShapeDatabase;
}

/**
 * Create a $1 Recognizer instance configured with gesture patterns.
 *
 * Takes the gesture actions from configuration and adds them as templates
 * to the recognizer. Only complex patterns are registered in $1; simple patterns
 * (single direction like ["left"] or oscillating like ["up", "down"]) are stored
 * in the shape database but NOT registered in $1 Recognizer to prevent complex
 * gestures from incorrectly matching simple patterns.
 *
 * The first pattern encountered for each unique shape becomes the representative
 * template. Subsequent patterns with the same shape are added as variants,
 * distinguished by their first direction angle.
 */
export function createRecognizer(actions: GestureAction[]): RecognizerWithShapeDb {
  // Create a new $1 Recognizer instance
  const recognizer = new (DollarRecognizer as unknown as new () => IDollarRecognizer)();

  // Clear the built-in gesture templates
  recognizer.DeleteUserGestures();
  recognizer.Unistrokes.length = 0;

  // Build shape database - maps shape keys to their variants
  const shapeDb: ShapeDatabase = new Map();

  // Track processed pattern names to avoid duplicate variants
  const processedPatterns = new Set<string>();

  // Add each configured gesture as a template
  for (const action of actions) {
    if (action.pattern.length > 0) {
      // Use the pattern as a hyphen-joined string for the template name
      const patternName = action.pattern.join("-");

      // Skip if this exact pattern has already been processed
      if (processedPatterns.has(patternName)) {
        continue;
      }
      processedPatterns.add(patternName);

      const firstDirection = getPatternFirstDirection(action.pattern);

      // Create the shape variant entry
      const variant: ShapeVariant = {
        patternName,
        pattern: action.pattern,
        firstDirection,
      };

      // Check if this pattern's shape matches any existing shape in the database
      // by using the $1 recognizer to detect shape similarity
      let foundExistingShape = false;
      if (shapeDb.size > 0) {
        // Generate test points once for this pattern
        const testPoints = convertPatternToPoints(action.pattern, 100);
        const testResult = recognizer.Recognize(testPoints, true);

        // If the recognizer found a match, add as a variant of that shape
        if (testResult.Name !== "No match." && testResult.Score > 0.95) {
          const shapeEntry = shapeDb.get(testResult.Name);
          if (shapeEntry) {
            shapeEntry.variants.push(variant);
            foundExistingShape = true;
          }
        }
      }

      if (!foundExistingShape) {
        // New unique shape - register to $1 and create new entry
        // Note: Simple patterns are added to shape database for configuration lookup
        // but are intentionally excluded from $1 Recognizer templates (see below)
        shapeDb.set(patternName, {
          shapeKey: patternName,
          variants: [variant],
        });

        // Only register complex patterns in $1 Recognizer
        // Simple patterns (single direction or oscillating) should not be in $1
        // to prevent complex gestures from incorrectly matching them
        if (!isSimplePattern(action.pattern)) {
          // Add templates at multiple sizes for better recognition
          for (const segmentLength of SEGMENT_LENGTHS) {
            const points = convertPatternToPoints(action.pattern, segmentLength);
            recognizer.AddGesture(patternName, points);
          }
        }
      }
    }
  }

  return { recognizer, shapeDb };
}

/**
 * Result of a gesture recognition attempt.
 */
export interface RecognitionResult {
  patternName: string;
  score: number;
}

/**
 * Minimum distance (in pixels) for a movement to be considered significant.
 * This threshold helps filter out noise in the mouse trail.
 */
const MIN_MOVEMENT_DISTANCE = 15;

/**
 * Minimum distance (in pixels) to start calculating mean direction.
 * Direction calculation begins once we've moved this far from the start.
 */
const DIRECTION_SAMPLE_START = 10;

/**
 * Maximum distance (in pixels) for calculating mean direction.
 * We calculate the mean direction of points between DIRECTION_SAMPLE_START
 * and DIRECTION_SAMPLE_END to get a more stable initial direction.
 */
const DIRECTION_SAMPLE_END = 50;

/**
 * Calculate the distance between two points.
 */
function distanceBetween(
  p1: { x: number; y: number },
  p2: { x: number; y: number },
): number {
  const dx = p2.x - p1.x;
  const dy = p2.y - p1.y;
  return Math.sqrt(dx * dx + dy * dy);
}

/**
 * Calculate mean direction from trail points within the sampling range.
 * Collects points between DIRECTION_SAMPLE_START and DIRECTION_SAMPLE_END
 * and returns the mean displacement direction.
 * 
 * @param trail - Array of points representing the mouse trail
 * @returns Direction based on mean displacement, or null if no valid direction found
 */
function calculateMeanDirection(trail: { x: number; y: number }[]): GestureDirection | null {
  if (trail.length < 2) {
    return null;
  }

  const start = trail[0];

  // Find points in the sampling range (between DIRECTION_SAMPLE_START and DIRECTION_SAMPLE_END)
  const sampledPoints: { x: number; y: number }[] = [];
  
  for (let i = 1; i < trail.length; i++) {
    const distance = distanceBetween(start, trail[i]);
    
    // Collect points within the sampling range
    if (distance >= DIRECTION_SAMPLE_START && distance <= DIRECTION_SAMPLE_END) {
      sampledPoints.push({ x: trail[i].x, y: trail[i].y });
    }
    
    // Stop if we've gone past the sampling range
    if (distance > DIRECTION_SAMPLE_END) {
      break;
    }
  }

  // If we have points in the sampling range, calculate mean direction
  if (sampledPoints.length > 0) {
    // Calculate mean displacement
    let sumDx = 0;
    let sumDy = 0;
    
    for (const point of sampledPoints) {
      sumDx += point.x - start.x;
      sumDy += point.y - start.y;
    }
    
    const meanDx = sumDx / sampledPoints.length;
    const meanDy = sumDy / sampledPoints.length;
    
    return deltaToDirection(meanDx, meanDy);
  }

  // Fallback: if trail is shorter than sampling range, use the farthest point
  // that's at least MIN_MOVEMENT_DISTANCE away
  for (let i = trail.length - 1; i >= 1; i--) {
    if (distanceBetween(start, trail[i]) >= MIN_MOVEMENT_DISTANCE) {
      const dx = trail[i].x - start.x;
      const dy = trail[i].y - start.y;
      return deltaToDirection(dx, dy);
    }
  }

  // Last resort: use the last point if there's any movement
  const last = trail[trail.length - 1];
  if (distanceBetween(start, last) > 0) {
    const dx = last.x - start.x;
    const dy = last.y - start.y;
    return deltaToDirection(dx, dy);
  }

  return null;
}

/**
 * Get the first significant movement direction from a mouse trail.
 * Calculates the mean direction from points between DIRECTION_SAMPLE_START
 * and DIRECTION_SAMPLE_END to filter out initial hand jitter and get
 * a more stable, intuitive direction.
 * Uses the shared angleToDirection() function for direction detection.
 * Returns one of 8 directions, or null if no significant movement is found.
 */
function getTrailFirstDirection(trail: { x: number; y: number }[]): GestureDirection | null {
  return calculateMeanDirection(trail);
}

/**
 * Full circle in radians, used for angle wraparound calculations.
 */
const TWO_PI = 2 * Math.PI;

/**
 * Ratio threshold for determining if a gesture is oscillating along one axis.
 * If one dimension is more than 3x the other, the gesture is considered oscillating.
 */
const OSCILLATION_RATIO = 3;

/**
 * Displacement threshold for confirming oscillation.
 * If the start-to-end displacement is less than 50% of the total range,
 * the gesture is considered to have oscillated back-and-forth.
 */
const OSCILLATION_DISPLACEMENT_THRESHOLD = 0.5;

/**
 * Confidence score for gestures detected by geometric analysis.
 * Set to 1.0 (maximum) since geometric detection is very reliable for simple patterns.
 */
const GEOMETRIC_DETECTION_CONFIDENCE = 1.0;

/**
 * Result of oscillating line detection.
 */
interface OscillationResult {
  isOscillating: boolean;
  axis: "vertical" | "horizontal" | null;
}

/**
 * Maximum ratio of path length to expected oscillation length.
 * For a simple up-down or left-right oscillation, the path length should be
 * approximately 2x the dominant axis (go there and back).
 * Allow some tolerance for natural hand movement variations.
 * A value of 2.5 means the path can be up to 150% longer than expected,
 * which accommodates carat-shaped gestures (e.g., ^ for up-down or > for left-right).
 */
const OSCILLATION_PATH_RATIO_THRESHOLD = 2.5;

/**
 * Calculate the total path length of a trail (sum of distances between consecutive points).
 */
function calculatePathLength(trail: { x: number; y: number }[]): number {
  if (trail.length < 2) {
    return 0;
  }
  let totalLength = 0;
  for (let i = 1; i < trail.length; i++) {
    totalLength += distanceBetween(trail[i - 1], trail[i]);
  }
  return totalLength;
}

/**
 * Check if the trail is oscillating back-and-forth along one axis.
 * Returns the axis if oscillating, null otherwise.
 * 
 * A true oscillating gesture:
 * - Has one dominant axis (vertical or horizontal)
 * - Starts and ends near the same position on the dominant axis
 * - Has a path length close to 2x the dominant axis size (simple back-and-forth)
 */
function isOscillatingLine(trail: { x: number; y: number }[]): OscillationResult {
  if (trail.length < 2) {
    return { isOscillating: false, axis: null };
  }

  const xValues = trail.map((p) => p.x);
  const yValues = trail.map((p) => p.y);

  const width = Math.max(...xValues) - Math.min(...xValues);
  const height = Math.max(...yValues) - Math.min(...yValues);

  // Check if one dimension is dominant
  const hasVerticalDominance = height > OSCILLATION_RATIO * width;
  const hasHorizontalDominance = width > OSCILLATION_RATIO * height;

  if (!hasVerticalDominance && !hasHorizontalDominance) {
    return { isOscillating: false, axis: null };
  }

  // To be truly oscillating, the gesture must change direction along the dominant axis
  // Check if there's a reversal in direction by comparing start-to-end displacement
  // with the bounding box size
  const start = trail[0];
  const end = trail[trail.length - 1];
  const startToEndDx = Math.abs(end.x - start.x);
  const startToEndDy = Math.abs(end.y - start.y);

  // Calculate path length for complexity check
  const pathLength = calculatePathLength(trail);

  if (hasVerticalDominance) {
    // For vertical oscillation, the start-to-end vertical displacement should be
    // significantly less than the total vertical range (indicating back-and-forth movement)
    if (startToEndDy < height * OSCILLATION_DISPLACEMENT_THRESHOLD) {
      // Check path complexity: a simple up-down should have path length ~2x height
      // If path is too long, it's likely a complex gesture that happens to end near start
      const expectedPathLength = 2 * height;
      if (pathLength <= expectedPathLength * OSCILLATION_PATH_RATIO_THRESHOLD) {
        return { isOscillating: true, axis: "vertical" };
      }
    }
  } else if (hasHorizontalDominance) {
    // For horizontal oscillation, same logic applies
    if (startToEndDx < width * OSCILLATION_DISPLACEMENT_THRESHOLD) {
      // Check path complexity: a simple left-right should have path length ~2x width
      const expectedPathLength = 2 * width;
      if (pathLength <= expectedPathLength * OSCILLATION_PATH_RATIO_THRESHOLD) {
        return { isOscillating: true, axis: "horizontal" };
      }
    }
  }

  return { isOscillating: false, axis: null };
}

/**
 * Get the first significant movement direction from the trail.
 * Calculates the mean direction from points between DIRECTION_SAMPLE_START
 * and DIRECTION_SAMPLE_END to filter out initial hand jitter and get
 * a more stable, intuitive direction.
 * Returns one of 8 directions based on the initial movement.
 */
function getFirstMovementDirection(
  trail: { x: number; y: number }[],
): GestureDirection | null {
  return calculateMeanDirection(trail);
}

/**
 * Angle boundaries for 8-direction detection.
 * The angle space is divided into 8 sectors of 45 degrees each.
 * PI/8 = 22.5 degrees, which is the half-width of each sector.
 */
const ANGLE_SECTOR_HALF_WIDTH = Math.PI / 8;

/**
 * Convert an angle (in radians) to one of 8 directions.
 *
 * This function is the core direction detection logic shared by:
 * - Simple line recognition (via deltaToDirection)
 * - Complex shape direction matching (via getTrailFirstDirection)
 *
 * @param angle - The angle in radians, where:
 *   - 0 = right
 *   - PI/2 = down
 *   - ±PI = left
 *   - -PI/2 = up
 *   (using screen coordinates where Y increases downward)
 * @returns One of 8 directions: "up", "down", "left", "right", "upRight", "upLeft", "downRight", "downLeft"
 *
 * @example
 * angleToDirection(0) // returns "right"
 * angleToDirection(Math.PI / 4) // returns "downRight"
 * angleToDirection(-Math.PI / 2) // returns "up"
 */
export function angleToDirection(angle: number): GestureDirection {
  // Normalize angle to [0, 2*PI) range for easier sector calculation
  const normalizedAngle = angle < 0 ? angle + TWO_PI : angle;

  // Divide into 8 sectors of 45 degrees each
  // Sector boundaries (in radians, starting from right=0 and going clockwise):
  // right:      -PI/8 to PI/8       (or 15*PI/8 to PI/8)
  // downRight:  PI/8 to 3*PI/8
  // down:       3*PI/8 to 5*PI/8
  // downLeft:   5*PI/8 to 7*PI/8
  // left:       7*PI/8 to 9*PI/8
  // upLeft:     9*PI/8 to 11*PI/8
  // up:         11*PI/8 to 13*PI/8
  // upRight:    13*PI/8 to 15*PI/8

  if (normalizedAngle < ANGLE_SECTOR_HALF_WIDTH || normalizedAngle >= TWO_PI - ANGLE_SECTOR_HALF_WIDTH) {
    return "right";
  } else if (normalizedAngle < 3 * ANGLE_SECTOR_HALF_WIDTH) {
    return "downRight";
  } else if (normalizedAngle < 5 * ANGLE_SECTOR_HALF_WIDTH) {
    return "down";
  } else if (normalizedAngle < 7 * ANGLE_SECTOR_HALF_WIDTH) {
    return "downLeft";
  } else if (normalizedAngle < 9 * ANGLE_SECTOR_HALF_WIDTH) {
    return "left";
  } else if (normalizedAngle < 11 * ANGLE_SECTOR_HALF_WIDTH) {
    return "upLeft";
  } else if (normalizedAngle < 13 * ANGLE_SECTOR_HALF_WIDTH) {
    return "up";
  } else {
    return "upRight";
  }
}

/**
 * Convert dx/dy deltas to one of 8 directions.
 *
 * Uses the shared angleToDirection() function internally.
 *
 * @param dx - Horizontal displacement in pixels (positive = right, negative = left)
 * @param dy - Vertical displacement in pixels (positive = down, negative = up)
 *             Note: Uses screen coordinates where Y increases downward.
 * @returns One of 8 directions: "up", "down", "left", "right", "upRight", "upLeft", "downRight", "downLeft"
 *
 * @example
 * deltaToDirection(100, 0) // returns "right"
 * deltaToDirection(100, 100) // returns "downRight"
 * deltaToDirection(0, -100) // returns "up"
 */
export function deltaToDirection(dx: number, dy: number): GestureDirection {
  // Use atan2 to get the angle (dy, dx order for screen coordinates where Y increases downward)
  const angle = Math.atan2(dy, dx);
  return angleToDirection(angle);
}

/**
 * Ratio threshold for determining if movement is diagonal.
 * If both dimensions are within this ratio of each other (neither dominates),
 * the movement is considered diagonal.
 */
const DIAGONAL_RATIO_TOLERANCE = 3;

/**
 * Maximum ratio of path length to displacement for a gesture to be considered a straight line.
 * If the path length is more than this factor times the displacement, the gesture
 * contains too much deviation (loops, curves, etc.) to be considered straight.
 * A value of 1.5 means the path can be up to 50% longer than the direct distance.
 */
const LINEARITY_RATIO_THRESHOLD = 1.5;

/**
 * Maximum angular deviation (in radians) allowed for a gesture to be considered a straight line.
 * This is roughly 30 degrees - direction changes greater than this indicate a multi-segment gesture.
 */
const MAX_ANGULAR_DEVIATION = Math.PI / 6;

/**
 * Minimum segment length for angular deviation check.
 * Segments shorter than this are ignored to filter out noise from small mouse movements.
 */
const MIN_SEGMENT_LENGTH_FOR_ANGLE_CHECK = 30;

/**
 * Calculate the angular difference between two angles, handling wraparound.
 * Returns a value in the range [0, PI].
 * 
 * @param angle1 - First angle in radians
 * @param angle2 - Second angle in radians
 * @returns The absolute angular difference in radians, always in [0, PI]
 */
function angularDifference(angle1: number, angle2: number): number {
  let diff = Math.abs(angle1 - angle2);
  if (diff > Math.PI) {
    diff = TWO_PI - diff;
  }
  return diff;
}

/**
 * Check if the trail has significant direction changes that would indicate
 * a multi-segment gesture rather than a straight line.
 * 
 * This function samples the trail at intervals and checks if the direction
 * changes significantly between segments.
 * 
 * @param trail - Array of points representing the mouse trail
 * @returns true if a significant direction change is detected
 */
function hasSignificantDirectionChange(trail: { x: number; y: number }[]): boolean {
  if (trail.length < 3) {
    return false;
  }

  // Find the first significant movement from the start
  let firstSegmentEnd = -1;
  for (let i = 1; i < trail.length; i++) {
    if (distanceBetween(trail[0], trail[i]) >= MIN_SEGMENT_LENGTH_FOR_ANGLE_CHECK) {
      firstSegmentEnd = i;
      break;
    }
  }
  
  // Need at least one point after firstSegmentEnd to form a second segment
  if (firstSegmentEnd === -1 || firstSegmentEnd >= trail.length - 1) {
    return false;
  }

  // Calculate the initial direction angle
  const initialDx = trail[firstSegmentEnd].x - trail[0].x;
  const initialDy = trail[firstSegmentEnd].y - trail[0].y;
  const initialAngle = Math.atan2(initialDy, initialDx);

  // Check remaining segments for direction changes
  let currentPos = firstSegmentEnd;
  
  while (currentPos < trail.length - 1) {
    // Find the next significant movement
    let nextSegmentEnd = -1;
    for (let i = currentPos + 1; i < trail.length; i++) {
      if (distanceBetween(trail[currentPos], trail[i]) >= MIN_SEGMENT_LENGTH_FOR_ANGLE_CHECK) {
        nextSegmentEnd = i;
        break;
      }
    }
    
    if (nextSegmentEnd === -1) {
      break;
    }

    // Calculate the direction angle of this segment
    const segmentDx = trail[nextSegmentEnd].x - trail[currentPos].x;
    const segmentDy = trail[nextSegmentEnd].y - trail[currentPos].y;
    const segmentAngle = Math.atan2(segmentDy, segmentDx);

    // If the angle changed significantly, this is not a straight line
    if (angularDifference(segmentAngle, initialAngle) > MAX_ANGULAR_DEVIATION) {
      return true;
    }

    currentPos = nextSegmentEnd;
  }

  return false;
}

/**
 * Check if the trail is a straight line (movement in a consistent direction).
 * A line is considered straight if:
 * - One axis dominates the movement (cardinal direction), OR
 * - Both axes are similar in magnitude (diagonal direction)
 * - The path length is close to the displacement (no loops or major deviations)
 * - There are no significant direction changes during the gesture
 *
 * The total displacement must also exceed the minimum movement distance.
 */
function isStraightLine(trail: { x: number; y: number }[]): boolean {
  if (trail.length < 2) {
    return false;
  }

  const start = trail[0];
  const end = trail[trail.length - 1];

  const dx = Math.abs(end.x - start.x);
  const dy = Math.abs(end.y - start.y);

  // Check for sufficient movement in at least one direction
  if (dx < MIN_MOVEMENT_DISTANCE && dy < MIN_MOVEMENT_DISTANCE) {
    return false;
  }

  // Cardinal directions: one axis dominates significantly
  const isCardinal = dx > OSCILLATION_RATIO * dy || dy > OSCILLATION_RATIO * dx;

  // Diagonal directions: both axes have similar magnitude
  // Check if neither axis dominates too much (ratio within tolerance)
  const isDiagonal = dx > 0 && dy > 0 &&
    dx <= DIAGONAL_RATIO_TOLERANCE * dy && dy <= DIAGONAL_RATIO_TOLERANCE * dx;

  // Check direction type (must be cardinal or diagonal)
  if (!isCardinal && !isDiagonal) {
    return false;
  }

  // Linearity check: compare path length to displacement
  // A truly straight line should have path length very close to displacement
  // Complex gestures with loops will have much longer path lengths
  const displacement = Math.sqrt(dx * dx + dy * dy);
  const pathLength = calculatePathLength(trail);
  // If path length is too much longer than displacement, it's not a straight line
  // This catches cases like [down, circle, down] where displacement looks straight
  // but the actual path is much longer due to the circle
  if (pathLength > displacement * LINEARITY_RATIO_THRESHOLD) {
    return false;
  }

  // Check for significant direction changes during the gesture
  // This catches cases like [upRight, right] where the overall displacement looks diagonal
  // but the gesture has a clear direction change
  if (hasSignificantDirectionChange(trail)) {
    return false;
  }

  return true;
}

/**
 * Get the direction based on maximum displacement from start to end.
 * Returns one of 8 directions based on the overall movement vector.
 */
function getMaxDisplacementDirection(
  trail: { x: number; y: number }[],
): GestureDirection | null {
  if (trail.length < 2) {
    return null;
  }

  const start = trail[0];
  const end = trail[trail.length - 1];

  const dx = end.x - start.x;
  const dy = end.y - start.y;

  if (Math.abs(dx) < MIN_MOVEMENT_DISTANCE && Math.abs(dy) < MIN_MOVEMENT_DISTANCE) {
    return null;
  }

  return deltaToDirection(dx, dy);
}

/**
 * Check if a pattern exists in the shape database.
 * Returns true if pattern is found or if no shapeDb is provided.
 * Searches both top-level keys and variants within each shape entry.
 */
function isPatternConfigured(patternName: string, shapeDb?: ShapeDatabase): boolean {
  if (!shapeDb) {
    return true;
  }
  // Check if pattern is a top-level key
  if (shapeDb.has(patternName)) {
    return true;
  }
  // Search through all variants in each shape entry
  for (const shapeEntry of shapeDb.values()) {
    for (const variant of shapeEntry.variants) {
      if (variant.patternName === patternName) {
        return true;
      }
    }
  }
  return false;
}

/**
 * Recognize a gesture from mouse trail points using the shape database.
 *
 * This function uses a multi-step recognition strategy:
 * 1. Check for oscillating gestures (back-and-forth along one axis like up-down, left-right)
 * 2. Check for straight lines (single direction gestures like up, down, left, right)
 * 3. Fall back to $1 Recognizer for complex shapes
 *
 * For oscillating and straight line gestures, we use simple geometric analysis
 * which is more reliable than $1 for these basic patterns. If a simple pattern
 * is detected geometrically but not configured, we return null rather than
 * falling through to $1 Recognizer. This prevents simple gestures from
 * incorrectly matching against complex shape templates.
 *
 * Returns the matched pattern name and confidence score if successful.
 */
export function recognize(
  recognizer: IDollarRecognizer,
  trail: { x: number; y: number }[],
  minScore = 0.7,
  shapeDb?: ShapeDatabase,
): RecognitionResult | null {
  // Need at least 2 points to recognize
  if (trail.length < 2) {
    return null;
  }

  // Step 1: Check for oscillating gestures (back-and-forth along one axis)
  const oscillation = isOscillatingLine(trail);
  if (oscillation.isOscillating) {
    const firstDir = getFirstMovementDirection(trail);
    if (firstDir) {
      let patternName: string;
      if (oscillation.axis === "vertical") {
        // Check if the first direction is predominantly upward
        const isUpward = firstDir === "up" || firstDir === "upRight" || firstDir === "upLeft";
        patternName = isUpward ? "up-down" : "down-up";
      } else {
        // Check if the first direction is predominantly leftward
        const isLeftward = firstDir === "left" || firstDir === "upLeft" || firstDir === "downLeft";
        patternName = isLeftward ? "left-right" : "right-left";
      }

      if (isPatternConfigured(patternName, shapeDb)) {
        return {
          patternName,
          score: GEOMETRIC_DETECTION_CONFIDENCE,
        };
      }
    }
  }

  // Step 2: Check for straight lines (single direction gestures)
  if (isStraightLine(trail)) {
    const direction = getMaxDisplacementDirection(trail);
    if (direction) {
      const patternName = direction;

      if (isPatternConfigured(patternName, shapeDb)) {
        return {
          patternName,
          score: GEOMETRIC_DETECTION_CONFIDENCE,
        };
      }
    }
  }

  // Step 3: Complex shapes use $1 Recognizer
  // Convert mouse trail to $1 Recognizer format
  const points = convertTrailToPoints(trail);

  // Use Protractor algorithm (useProtractor = true) for faster recognition
  const result = recognizer.Recognize(points, true);

  // Check if we got a valid match above the threshold
  if (result.Name !== "No match." && result.Score >= minScore) {
    // The result.Name is the shape key used to register with $1
    const shapeKey = result.Name;

    // If we have a shape database, look up variants and match by direction
    if (shapeDb) {
      const shapeEntry = shapeDb.get(shapeKey);
      if (shapeEntry) {
        // Get the trail's first direction using shared direction detection logic
        const trailDirection = getTrailFirstDirection(trail);

        // Find the first variant that matches the trail's direction
        for (const variant of shapeEntry.variants) {
          // If either direction couldn't be determined, use this variant
          if (trailDirection === null || variant.firstDirection === null) {
            return {
              patternName: variant.patternName,
              score: result.Score,
            };
          }

          // Compare directions directly using the shared 8-direction detection
          if (trailDirection === variant.firstDirection) {
            return {
              patternName: variant.patternName,
              score: result.Score,
            };
          }
        }

        // No variant matched the direction
        return null;
      }
    }

    // Fallback: if no shape database, just return the shape key as the pattern name
    // (for backward compatibility during transition)
    return {
      patternName: result.Name,
      score: result.Score,
    };
  }

  return null;
}
