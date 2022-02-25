import { vectorDirectionDifference } from "/core/utils/commons.mjs";


/**
 * Helper class to create a pattern alongside mouse movement from points
 * A Pattern is a combination/array of 2D Vectors while each Vector is an array
 **/
export default class PatternConstructor {

  constructor (differenceThreshold = 0, distanceThreshold = 0) {
    this.differenceThreshold = differenceThreshold;
    this.distanceThreshold = distanceThreshold;

    this._lastExtractedPointX = null;
    this._lastExtractedPointY = null;
    this._previousPointX = null;
    this._previousPointY = null;
    this._lastPointX = null;
    this._lastPointY = null;
    this._previousVectorX = null;
    this._previousVectorY = null;

    this._extractedVectors = [];
  }

  /**
   * Resets the internal class variables and clears the constructed pattern
   **/
  clear () {
    // clear extracted vectors
    this._extractedVectors.length = 0;
    // reset variables
    this._lastExtractedPointX = null;
    this._lastExtractedPointY = null;
    this._previousPointX = null;
    this._previousPointY = null;
    this._lastPointX = null;
    this._lastPointY = null;
    this._previousVectorX = null;
    this._previousVectorY = null;
  }


  /**
   * Add a point to the constructor
   * Returns an integer value:
   * 1 if the added point passed the distance threshold [PASSED_DISTANCE_THRESHOLD]
   * 2 if the added point also passed the difference threshold [PASSED_DIFFERENCE_THRESHOLD]
   * else 0 [PASSED_NO_THRESHOLD]
   **/
  addPoint (x, y) {
    // return variable
    let changeIndicator = 0;
    // on first point / if no previous point exists
    if (this._previousPointX === null || this._previousPointY === null) {
      // set first extracted point
      this._lastExtractedPointX = x;
      this._lastExtractedPointY = y;
      // set previous point to first point
      this._previousPointX = x;
      this._previousPointY = y;
    }

    else {
      const newVX = x - this._previousPointX;
      const newVY = y - this._previousPointY;

      const vectorDistance = Math.hypot(newVX, newVY);
      if (vectorDistance > this.distanceThreshold) {
        // on second point / if no previous vector exists
        if (this._previousVectorX === null || this._previousVectorY === null) {
          // store previous vector
          this._previousVectorX = newVX;
          this._previousVectorY = newVY;
        }
        else {
          // calculate vector difference
          const vectorDifference = vectorDirectionDifference(this._previousVectorX, this._previousVectorY, newVX, newVY);
          if (Math.abs(vectorDifference) > this.differenceThreshold) {
            // store new extracted vector
            this._extractedVectors.push([
              this._previousPointX - this._lastExtractedPointX,
              this._previousPointY - this._lastExtractedPointY
            ]);
            // update previous vector
            this._previousVectorX = newVX;
            this._previousVectorY = newVY;
            // update last extracted point
            this._lastExtractedPointX = this._previousPointX;
            this._lastExtractedPointY = this._previousPointY;
            // update change variable
            changeIndicator++;
          }
        }
        // update previous point
        this._previousPointX = x;
        this._previousPointY = y;
        // update change variable
        changeIndicator++;
      }
    }
    // always store the last point
    this._lastPointX = x;
    this._lastPointY = y;

    return changeIndicator;
  }


  /**
   * Returns the current constructed pattern
   * Adds the last added point as the current end point
   **/
  getPattern () {
    // check if variables contain point values
    if (this._lastPointX === null || this._lastPointY === null || this._lastExtractedPointX === null || this._lastExtractedPointY === null) {
      return [];
    }
    // calculate vector from last extracted point to ending point
    const lastVector = [
      this._lastPointX - this._lastExtractedPointX,
      this._lastPointY - this._lastExtractedPointY
    ];
    return [...this._extractedVectors, lastVector];
  }
}

// TODO: move these inside the class using the "static" keyword once eslint finally supports it
PatternConstructor.PASSED_NO_THRESHOLD = 0;

PatternConstructor.PASSED_DISTANCE_THRESHOLD = 1;

PatternConstructor.PASSED_DIFFERENCE_THRESHOLD = 2;