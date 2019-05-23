/*
 * Natural Sort algorithm for Javascript - Version 0.8.1 - Released under MIT license
 * Author: Jim Palmer (based on chunking idea from Dave Koelle)
 *
 * Includes pull request to move regexes out of main function for performance
 * increases.
 *
 * Repository:
 *   https://github.com/overset/javascript-natural-sort/
 */

"use strict";

var re = /(^([+\-]?\d+(?:\.\d*)?(?:[eE][+\-]?\d+)?(?=\D|\s|$))|^0x[\da-fA-F]+$|\d+)/g;
var sre = /^\s+|\s+$/g; // trim pre-post whitespace
var snre = /\s+/g; // normalize all whitespace to single ' ' character

// eslint-disable-next-line
var dre = /(^([\w ]+,?[\w ]+)?[\w ]+,?[\w ]+\d+:\d+(:\d+)?[\w ]?|^\d{1,4}[\/\-]\d{1,4}[\/\-]\d{1,4}|^\w+, \w+ \d+, \d{4})/;
var hre = /^0x[0-9a-f]+$/i;
var ore = /^0/;
var b0re = /^\0/;
var e0re = /\0$/;

exports.naturalSortCaseSensitive =
function naturalSortCaseSensitive(a, b) {
  return naturalSort(a, b, false);
};

exports.naturalSortCaseInsensitive =
function naturalSortCaseInsensitive(a, b) {
  return naturalSort(a, b, true);
};

/**
 * Sort numbers, strings, IP Addresses, Dates, Filenames, version numbers etc.
 * "the way humans do."
 *
 * This function should only be called via naturalSortCaseSensitive and
 * naturalSortCaseInsensitive.
 *
 * e.g. [3, 2, 1, 10].sort(naturalSort)
 *
 * @param  {Object} a
 *         Passed in by Array.sort(a, b)
 * @param  {Object} b
 *         Passed in by Array.sort(a, b)
 * @param  {Boolean} insensitive
 *         Should the search be case insensitive?
 */
function naturalSort(a, b, insensitive) {
  // convert all to strings strip whitespace
  const i = function(s) {
    return (insensitive && ("" + s).toLowerCase() || "" + s)
                                   .replace(sre, "");
  };
  const x = i(a) || "";
  const y = i(b) || "";
  // chunk/tokenize
  const xN = x.replace(re, "\0$1\0").replace(e0re, "").replace(b0re, "").split("\0");
  const yN = y.replace(re, "\0$1\0").replace(e0re, "").replace(b0re, "").split("\0");
  // numeric, hex or date detection
  const xD = parseInt(x.match(hre), 16) || (xN.length !== 1 && Date.parse(x));
  const yD = parseInt(y.match(hre), 16) || xD && y.match(dre) && Date.parse(y) || null;
  const normChunk = function(s, l) {
    // normalize spaces; find floats not starting with '0', string or 0 if
    // not defined (Clint Priest)
    return (!s.match(ore) || l == 1) &&
           parseFloat(s) || s.replace(snre, " ").replace(sre, "") || 0;
  };
  let oFxNcL;
  let oFyNcL;

  // first try and sort Hex codes or Dates
  if (yD) {
    if (xD < yD) {
      return -1;
    } else if (xD > yD) {
      return 1;
    }
  }

  // natural sorting through split numeric strings and default strings
  // eslint-disable-next-line
  for (let cLoc = 0, xNl = xN.length, yNl = yN.length, numS = Math.max(xNl, yNl); cLoc < numS; cLoc++) {
    oFxNcL = normChunk(xN[cLoc] || "", xNl);
    oFyNcL = normChunk(yN[cLoc] || "", yNl);

    // handle numeric vs string comparison - number < string - (Kyle Adams)
    if (isNaN(oFxNcL) !== isNaN(oFyNcL)) {
      return isNaN(oFxNcL) ? 1 : -1;
    }
    // if unicode use locale comparison
    // eslint-disable-next-line
    if (/[^\x00-\x80]/.test(oFxNcL + oFyNcL) && oFxNcL.localeCompare) {
      const comp = oFxNcL.localeCompare(oFyNcL);
      return comp / Math.abs(comp);
    }
    if (oFxNcL < oFyNcL) {
      return -1;
    } else if (oFxNcL > oFyNcL) {
      return 1;
    }
  }
  return null;
}
