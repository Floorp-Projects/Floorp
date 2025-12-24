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
 * Shape variant entry containing pattern name and its first direction angle.
 * Used to store multiple patterns that share the same shape signature.
 */
interface ShapeVariant {
  patternName: string;
  pattern: GestureDirection[];
  firstAngle: number | null;
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
 * Calculate the angle of the first direction in a pattern using arctan.
 * Returns the angle in radians, where 0 = right, PI/2 = down, ±PI = left, -PI/2 = up.
 * This helps distinguish opposite gestures like "up-down" vs "down-up" by
 * comparing first movement direction.
 */
function getPatternFirstAngle(pattern: GestureDirection[]): number | null {
  if (pattern.length === 0) {
    return null;
  }

  // Get the first direction's vector
  const firstDirection = pattern[0];
  const vector = DIRECTION_VECTORS[firstDirection];

  // Use atan2 to get the angle (dy, dx order for screen coordinates where Y increases downward)
  return Math.atan2(vector.dy, vector.dx);
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
 * to the recognizer. Only unique shapes are registered to $1; patterns that
 * produce the same shape signature are stored as variants in the shape database.
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

      const firstAngle = getPatternFirstAngle(action.pattern);

      // Create the shape variant entry
      const variant: ShapeVariant = {
        patternName,
        pattern: action.pattern,
        firstAngle,
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
        shapeDb.set(patternName, {
          shapeKey: patternName,
          variants: [variant],
        });

        // Add templates at multiple sizes for better recognition
        for (const segmentLength of SEGMENT_LENGTHS) {
          const points = convertPatternToPoints(action.pattern, segmentLength);
          recognizer.AddGesture(patternName, points);
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
 * Calculate the angle of the first significant movement from a mouse trail using arctan.
 * Finds the first point that is far enough from the start to determine direction.
 * Returns the angle in radians, or null if no significant movement is found.
 */
function getTrailFirstAngle(trail: { x: number; y: number }[]): number | null {
  if (trail.length < 2) {
    return null;
  }

  const start = trail[0];

  // Find the first point that is far enough to determine direction
  for (let i = 1; i < trail.length; i++) {
    if (distanceBetween(start, trail[i]) >= MIN_MOVEMENT_DISTANCE) {
      const dx = trail[i].x - start.x;
      const dy = trail[i].y - start.y;
      // Use atan2 to get the angle (dy, dx order for screen coordinates)
      return Math.atan2(dy, dx);
    }
  }

  // If no point is far enough, use the last point
  const last = trail[trail.length - 1];
  if (distanceBetween(start, last) > 0) {
    const dx = last.x - start.x;
    const dy = last.y - start.y;
    return Math.atan2(dy, dx);
  }

  return null;
}

/**
 * Maximum allowed angle difference (in radians) for direction validation.
 * PI/8 (12.5 degrees) allows for some tolerance
 */
const MAX_ANGLE_DIFFERENCE = Math.PI / 8;

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
 * Check if the trail is oscillating back-and-forth along one axis.
 * Returns the axis if oscillating, null otherwise.
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

  if (hasVerticalDominance) {
    // For vertical oscillation, the start-to-end vertical displacement should be
    // significantly less than the total vertical range (indicating back-and-forth movement)
    if (startToEndDy < height * OSCILLATION_DISPLACEMENT_THRESHOLD) {
      return { isOscillating: true, axis: "vertical" };
    }
  } else if (hasHorizontalDominance) {
    // For horizontal oscillation, same logic applies
    if (startToEndDx < width * OSCILLATION_DISPLACEMENT_THRESHOLD) {
      return { isOscillating: true, axis: "horizontal" };
    }
  }

  return { isOscillating: false, axis: null };
}

/**
 * Get the first significant movement direction from the trail.
 * Returns "up", "down", "left", or "right" based on the initial movement.
 */
function getFirstMovementDirection(
  trail: { x: number; y: number }[],
): "up" | "down" | "left" | "right" | null {
  if (trail.length < 2) {
    return null;
  }

  const start = trail[0];

  // Find the first point that is far enough to determine direction
  for (let i = 1; i < trail.length; i++) {
    if (distanceBetween(start, trail[i]) >= MIN_MOVEMENT_DISTANCE) {
      const dx = trail[i].x - start.x;
      const dy = trail[i].y - start.y;
      return deltaToDirection(dx, dy);
    }
  }

  // If no point is far enough, use the last point
  const last = trail[trail.length - 1];
  if (distanceBetween(start, last) > 0) {
    const dx = last.x - start.x;
    const dy = last.y - start.y;
    return deltaToDirection(dx, dy);
  }

  return null;
}

/**
 * Convert dx/dy deltas to a cardinal direction.
 * Returns "up", "down", "left", or "right" based on which axis dominates.
 */
function deltaToDirection(dx: number, dy: number): "up" | "down" | "left" | "right" {
  if (Math.abs(dy) > Math.abs(dx)) {
    return dy < 0 ? "up" : "down";
  } else {
    return dx > 0 ? "right" : "left";
  }
}

/**
 * Check if the trail is a straight line (movement primarily along one axis).
 * A line is considered straight if one axis dominates the movement.
 */
function isStraightLine(trail: { x: number; y: number }[]): boolean {
  if (trail.length < 2) {
    return false;
  }

  const start = trail[0];
  const end = trail[trail.length - 1];

  const dx = Math.abs(end.x - start.x);
  const dy = Math.abs(end.y - start.y);

  // The dominant axis should be significantly larger than the other
  // Using the same ratio as oscillation detection for consistency
  return dx > OSCILLATION_RATIO * dy || dy > OSCILLATION_RATIO * dx;
}

/**
 * Get the direction based on maximum displacement from start to end.
 * Returns the cardinal direction of the overall movement.
 */
function getMaxDisplacementDirection(
  trail: { x: number; y: number }[],
): "up" | "down" | "left" | "right" | null {
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
 * Calculate the angular difference between two angles, accounting for wraparound.
 * Returns the smallest angle between the two directions (0 to PI).
 */
function angleDifference(angle1: number, angle2: number): number {
  let diff = Math.abs(angle1 - angle2);
  // Normalize to [0, PI] range since we want the smallest angle between directions
  if (diff > Math.PI) {
    diff = TWO_PI - diff;
  }
  return diff;
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
 * which is more reliable than $1 for these basic patterns.
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
        patternName = firstDir === "up" ? "up-down" : "down-up";
      } else {
        patternName = firstDir === "left" ? "left-right" : "right-left";
      }

      if (isPatternConfigured(patternName, shapeDb)) {
        return {
          patternName,
          score: GEOMETRIC_DETECTION_CONFIDENCE,
        };
      }
      // Pattern not configured, fall through to straight line check
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
      // Pattern not configured, fall through to $1 recognizer
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
        // Get the trail's first direction angle
        const trailAngle = getTrailFirstAngle(trail);

        // Find the first variant that matches the trail's direction
        for (const variant of shapeEntry.variants) {
          // If either angle couldn't be determined, use this variant
          if (trailAngle === null || variant.firstAngle === null) {
            return {
              patternName: variant.patternName,
              score: result.Score,
            };
          }

          // Compare angles - they should be within 90 degrees of each other
          const diff = angleDifference(variant.firstAngle, trailAngle);
          if (diff <= MAX_ANGLE_DIFFERENCE) {
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
