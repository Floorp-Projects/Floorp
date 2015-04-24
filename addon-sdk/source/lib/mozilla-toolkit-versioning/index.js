/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var versionParse = require('./lib/utils').versionParse;

var COMPARATORS = ['>=', '<=', '>', '<', '=', '~', '^'];

exports.parse = function (input) {
  input = input || '';
  input = input.trim();
  if (!input)
    throw new Error('`parse` argument must be a populated string.');

  // Handle the "*" case
  if (input === "*") {
    return { min: undefined, max: undefined };
  }

  var inputs = input.split(' ');
  var min;
  var max;

  // 1.2.3 - 2.3.4
  if (inputs.length === 3 && inputs[1] === '-') {
    return { min: inputs[0], max: inputs[2] };
  }

  inputs.forEach(function (input) {
    var parsed = parseExpression(input);
    var version = parsed.version;
    var comparator = parsed.comparator;

    // 1.2.3
    if (inputs.length === 1 && !comparator)
      min = max = version;

    // Parse min
    if (~comparator.indexOf('>')) {
      if (~comparator.indexOf('='))
        min = version; // >=1.2.3
      else
        min = increment(version); // >1.2.3
    }
    else if (~comparator.indexOf('<')) {
      if (~comparator.indexOf('='))
        max = version; // <=1.2.3
      else
        max = decrement(version); // <1.2.3
    }
  });

  return {
    min: min,
    max : max
  };
};

function parseExpression (input) {
  for (var i = 0; i < COMPARATORS.length; i++)
    if (~input.indexOf(COMPARATORS[i]))
      return {
        comparator: COMPARATORS[i],
        version: input.substr(COMPARATORS[i].length)
      };
  return { version: input, comparator: '' };
}

/**
 * Takes a version string ('1.2.3') and returns a version string
 * that'll parse as one less than the input string ('1.2.3.-1').
 *
 * @param {String} vString
 * @return {String}
 */
function decrement (vString) {
  return vString + (vString.charAt(vString.length - 1) === '.' ? '' : '.') + '-1';
}
exports.decrement = decrement;

/**
 * Takes a version string ('1.2.3') and returns a version string
 * that'll parse as greater than the input string by the smallest margin
 * possible ('1.2.3.1').
 * listed as number-A, string-B, number-C, string-D in
 * Mozilla's Toolkit Format.
 * https://developer.mozilla.org/en-US/docs/Toolkit_version_format
 *
 * @param {String} vString
 * @return {String}
 */
function increment (vString) {
  var match = versionParse(vString);
  var a = match[1];
  var b = match[2];
  var c = match[3];
  var d = match[4];
  var lastPos = vString.length - 1;
  var lastChar = vString.charAt(lastPos);

  if (!b) {
    return vString + (lastChar === '.' ? '' : '.') + '1';
  }
  if (!c) {
    return vString + '1';
  }
  if (!d) {
    return vString.substr(0, lastPos) + (++lastChar);
  }
  return vString.substr(0, lastPos) + String.fromCharCode(lastChar.charCodeAt(0) + 1);
}
exports.increment = increment;
