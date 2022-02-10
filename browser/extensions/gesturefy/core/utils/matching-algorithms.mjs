import { vectorDirectionDifference } from "/core/utils/commons.mjs";


/**
 * Returns the gesture of an iterable that matches the given pattern the most
 * if no gesture matches with a value below the deviation value null will be returned
 * allowed algorithms: strict, shape-independent & combined (default)
 **/
export function getClosestGestureByPattern (pattern, gestures, maxDeviation = 1, algorithm) {
  let bestMatchingGesture = null;

  switch (algorithm) {
    case "strict": {
      let lowestMismatchRatio = maxDeviation;

      for (const gesture of gestures) {
        const difference = patternSimilarityByProportion(pattern, gesture.getPattern());
        if (difference < lowestMismatchRatio) {
          lowestMismatchRatio = difference;
          bestMatchingGesture = gesture;
        }
      }
    } break;

    case "shape-independent": {
      let lowestMismatchRatio = maxDeviation;

      for (const gesture of gestures) {
        const difference = patternSimilarityByDTW(pattern, gesture.getPattern());
        if (difference < lowestMismatchRatio) {
          lowestMismatchRatio = difference;
          bestMatchingGesture = gesture;
        }
      }
    } break;

    case "combined":
    default: {
      let lowestMismatchRatio = Infinity;

      for (const gesture of gestures) {
        const differenceByDTW = patternSimilarityByDTW(pattern, gesture.getPattern());
        // pre-filter gestures by DTW deviation value to increase speed
        if (differenceByDTW > maxDeviation) continue;

        const differenceByProportion = patternSimilarityByProportion(pattern, gesture.getPattern());

        const difference = differenceByDTW + differenceByProportion;

        if (difference < lowestMismatchRatio) {
          lowestMismatchRatio = difference;
          bestMatchingGesture = gesture;
        }
      }
    } break;
  }

  return bestMatchingGesture;
}


/**
 * Returns the similarity value of 2 patterns
 * Range: [0, 1]
 * 0 = perfect match / identical
 * 1 maximum mismatch
 **/
export function patternSimilarityByProportion (patternA, patternB) {
  const totalAMagnitude = patternMagnitude(patternA);
  const totalBMagnitude = patternMagnitude(patternB);

  let totalDifference = 0;

  let a = 0, b = 0;

  let vectorAMagnitudeProportionStart = 0;
  let vectorBMagnitudeProportionStart = 0;

  while (a < patternA.length && b < patternB.length) {
    const vectorA = patternA[a];
    const vectorB = patternB[b];

    const vectorAMagnitude = Math.hypot(...vectorA);
    const vectorBMagnitude = Math.hypot(...vectorB);

    const vectorAMagnitudeProportion = vectorAMagnitude/totalAMagnitude;
    const vectorBMagnitudeProportion = vectorBMagnitude/totalBMagnitude;

    const vectorAMagnitudeProportionEnd = vectorAMagnitudeProportionStart + vectorAMagnitudeProportion;
    const vectorBMagnitudeProportionEnd = vectorBMagnitudeProportionStart + vectorBMagnitudeProportion;

    // calculate how much both vectors are overlapping
    const overlappingMagnitudeProportion = overlapProportion(
      vectorAMagnitudeProportionStart,
      vectorAMagnitudeProportionEnd,
      vectorBMagnitudeProportionStart,
      vectorBMagnitudeProportionEnd
    );

    // compare which vector magnitude proportion is larger / passing over the other vector
    // take the pattern with the smaller magnitude proportion and increase its index
    // so the next vector of this pattern will be compared next

    if (vectorAMagnitudeProportionEnd > vectorBMagnitudeProportionEnd) {
      // increase B pattern index / take the next B vector in the next iteration
      b++;
      // set current end to new start
      vectorBMagnitudeProportionStart = vectorBMagnitudeProportionEnd;
    }
    else if (vectorAMagnitudeProportionEnd < vectorBMagnitudeProportionEnd) {
      // increase A pattern index / take the next A vector in the next iteration
      a++;
      // set current end to new start
      vectorAMagnitudeProportionStart = vectorAMagnitudeProportionEnd;
    }
    else {
      // increase A & B pattern index / take the next A & B vector in the next iteration
      a++;
      b++;
      // set current end to new start
      vectorAMagnitudeProportionStart = vectorAMagnitudeProportionEnd;
      vectorBMagnitudeProportionStart = vectorBMagnitudeProportionEnd;
    }

    // calculate the difference of both vectors
    // this will result in a value of 0 - 1
    const vectorDifference = Math.abs(vectorDirectionDifference(vectorA[0], vectorA[1], vectorB[0], vectorB[1]));

    // weight the value by its corresponding magnitude proportion
    // all magnitude proportion should add up to a value of 1 in total (ignoring floating point errors)
    totalDifference += vectorDifference * overlappingMagnitudeProportion;
  }

  return totalDifference;
}


/**
 * Modified version of dynamic time warping algorithm
 * Range: [0, 1]
 * 0 = perfect match / identical
 * 1 maximum mismatch
 **/
export function patternSimilarityByDTW (patternA, patternB) {
  const rows = patternA.length;
  const columns = patternB.length;

  // create 2-dimensional array
  const DTW = Array.from(Array(rows), () => Array(columns).fill(Infinity));

  for (let i = 0; i < rows; i++) {
    for (let j = 0; j < columns; j++) {
      const cost = Math.abs(vectorDirectionDifference(patternA[i][0], patternA[i][1], patternB[j][0], patternB[j][1]));

      if (i !== 0 && j !== 0) {
        DTW[i][j] = cost + Math.min(DTW[i - 1][j], DTW[i][j - 1], DTW[i - 1][j - 1]);
      }
      else if (i !== 0) {
        DTW[i][j] = cost + DTW[i - 1][j];
      }
      else if (j !== 0) {
        DTW[i][j] = cost + DTW[i][j - 1];
      }
      else {
        DTW[i][j] = cost;
      }
    }
  }

  // divide by amount of vectors
  return DTW[rows - 1][columns - 1] / Math.max(rows, columns);
}


/**
 * Calculates the overlap range of 2 line segments
 **/
function overlapProportion (minA, maxA, minB, maxB) {
  return Math.max(0, Math.min(maxA, maxB) - Math.max(minA, minB))
}


/**
 * Calculates the length/magnitude of a pattern
 **/
function patternMagnitude (pattern) {
  return pattern.reduce( (total, vector) => total + Math.hypot(...vector), 0 );
}