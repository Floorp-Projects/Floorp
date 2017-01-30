/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/Task.jsm");
Cu.importGlobalProperties(["crypto", "TextEncoder"]);

this.EXPORTED_SYMBOLS = ["Sampling"];

const hashBits = 48;
const hashLength = hashBits / 4;  // each hexadecimal digit represents 4 bits
const hashMultiplier = Math.pow(2, hashBits) - 1;

this.Sampling = {
  /**
   * Map from the range [0, 1] to [0, 2^48].
   * @param  {number} frac A float from 0.0 to 1.0.
   * @return {string} A 48 bit number represented in hex, padded to 12 characters.
   */
  fractionToKey(frac) {
    if (frac < 0 || frac > 1) {
      throw new Error(`frac must be between 0 and 1 inclusive (got ${frac})`);
    }

    return Math.floor(frac * hashMultiplier).toString(16).padStart(hashLength, "0");
  },

  /**
   * @param {ArrayBuffer} buffer Data to convert
   * @returns {String}    `buffer`'s content, converted to a hexadecimal string.
   */
  bufferToHex(buffer) {
    const hexCodes = [];
    const view = new DataView(buffer);
    for (let i = 0; i < view.byteLength; i += 4) {
      // Using getUint32 reduces the number of iterations needed (we process 4 bytes each time)
      const value = view.getUint32(i);
      // toString(16) will give the hex representation of the number without padding
      hexCodes.push(value.toString(16).padStart(8, "0"));
    }

    // Join all the hex strings into one
    return hexCodes.join("");
  },

  /**
   * Check if an input hash is contained in a bucket range.
   *
   * isHashInBucket(fractionToKey(0.5), 3, 6, 10) -> returns true
   *
   *              minBucket
   *              |     hash
   *              v     v
   *    [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
   *                       ^
   *                       maxBucket
   *
   * @param inputHash {String}
   * @param minBucket {int} The lower boundary, inclusive, of the range to check.
   * @param maxBucket {int} The upper boundary, exclusive, of the range to check.
   * @param bucketCount {int} The total number of buckets. Should be greater than
   *                          or equal to maxBucket.
   */
  isHashInBucket(inputHash, minBucket, maxBucket, bucketCount) {
    const minHash = Sampling.fractionToKey(minBucket / bucketCount);
    const maxHash = Sampling.fractionToKey(maxBucket / bucketCount);
    return (minHash <= inputHash) && (inputHash < maxHash);
  },

  /**
   * @promise A hash of `data`, truncated to the 12 most significant characters.
   */
  truncatedHash: Task.async(function* (data) {
    const hasher = crypto.subtle;
    const input = new TextEncoder("utf-8").encode(JSON.stringify(data));
    const hash = yield hasher.digest("SHA-256", input);
    // truncate hash to 12 characters (2^48), because the full hash is larger
    // than JS can meaningfully represent as a number.
    return Sampling.bufferToHex(hash).slice(0, 12);
  }),

  /**
   * Sample by splitting the input into two buckets, one with a size (rate) and
   * another with a size (1.0 - rate), and then check if the input's hash falls
   * into the first bucket.
   *
   * @param    {object}  input Input to hash to determine the sample.
   * @param    {Number}  rate  Number between 0.0 and 1.0 to sample at. A value of
   *                           0.25 returns true 25% of the time.
   * @promises {boolean} True if the input is in the sample.
   */
  stableSample: Task.async(function* (input, rate) {
    const inputHash = yield Sampling.truncatedHash(input);
    const samplePoint = Sampling.fractionToKey(rate);

    return inputHash < samplePoint;
  }),

  /**
   * Sample by splitting the input space into a series of buckets, and checking
   * if the given input is in a range of buckets.
   *
   * The range to check is defined by a start point and length, and can wrap
   * around the input space. For example, if there are 100 buckets, and we ask to
   * check 50 buckets starting from bucket 70, then buckets 70-99 and 0-19 will
   * be checked.
   *
   * @param {object}     input Input to hash to determine the matching bucket.
   * @param {integer}    start Index of the bucket to start checking.
   * @param {integer}    count Number of buckets to check.
   * @param {integer}    total Total number of buckets to group inputs into.
   * @promises {boolean} True if the given input is within the range of buckets
   *                     we're checking. */
  bucketSample: Task.async(function* (input, start, count, total) {
    const inputHash = yield Sampling.truncatedHash(input);
    const wrappedStart = start % total;
    const end = wrappedStart + count;

    // If the range we're testing wraps, we have to check two ranges: from start
    // to max, and from min to end.
    if (end > total) {
      return (
        Sampling.isHashInBucket(inputHash, 0, end % total, total)
        || Sampling.isHashInBucket(inputHash, wrappedStart, total, total)
      );
    }

    return Sampling.isHashInBucket(inputHash, wrappedStart, end, total);
  }),
};
