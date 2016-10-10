/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/Log.jsm");
Cu.importGlobalProperties(["crypto", "TextEncoder"]);

this.EXPORTED_SYMBOLS = ["Sampling"];

const log = Log.repository.getLogger("extensions.shield-recipe-client");

/**
 * Map from the range [0, 1] to [0, max(sha256)].
 * @param  {number} frac A float from 0.0 to 1.0.
 * @return {string} A 48 bit number represented in hex, padded to 12 characters.
 */
function fractionToKey(frac) {
  const hashBits = 48;
  const hashLength = hashBits / 4;

  if (frac < 0 || frac > 1) {
    throw new Error(`frac must be between 0 and 1 inclusive (got ${frac})`);
  }

  const mult = Math.pow(2, hashBits) - 1;
  const inDecimal = Math.floor(frac * mult);
  let hexDigits = inDecimal.toString(16);
  if (hexDigits.length < hashLength) {
    // Left pad with zeroes
    // If N zeroes are needed, generate an array of nulls N+1 elements long,
    // and inserts zeroes between each null.
    hexDigits = Array(hashLength - hexDigits.length + 1).join("0") + hexDigits;
  }

  // Saturate at 2**48 - 1
  if (hexDigits.length > hashLength) {
    hexDigits = Array(hashLength + 1).join("f");
  }

  return hexDigits;
}

function bufferToHex(buffer) {
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
}

this.Sampling = {
  stableSample(input, rate) {
    const hasher = crypto.subtle;

    return hasher.digest("SHA-256", new TextEncoder("utf-8").encode(JSON.stringify(input)))
      .then(hash => {
        // truncate hash to 12 characters (2^48)
        const inputHash = bufferToHex(hash).slice(0, 12);
        const samplePoint = fractionToKey(rate);

        if (samplePoint.length !== 12 || inputHash.length !== 12) {
          throw new Error("Unexpected hash length");
        }

        return inputHash < samplePoint;

      })
      .catch(error => {
        log.error(`Error: ${error}`);
      });
  },
};
