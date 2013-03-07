/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "experimental"
};

var { Cc, Ci } = require("chrome");

var appInfo = Cc["@mozilla.org/xre/app-info;1"]
              .getService(Ci.nsIXULAppInfo);
var vc = Cc["@mozilla.org/xpcom/version-comparator;1"]
         .getService(Ci.nsIVersionComparator);

var ID = exports.ID = appInfo.ID;
var name = exports.name = appInfo.name;
var version = exports.version = appInfo.version;
var platformVersion = exports.platformVersion = appInfo.platformVersion;

// The following mapping of application names to GUIDs was taken from:
//
//   https://addons.mozilla.org/en-US/firefox/pages/appversions
//
// Using the GUID instead of the app's name is preferable because sometimes
// re-branded versions of a product have different names: for instance,
// Firefox, Minefield, Iceweasel, and Shiretoko all have the same
// GUID.
// This mapping is duplicated in `app-extensions/bootstrap.js`. They should keep
// in sync, so if you change one, change the other too!

var ids = exports.ids = {
  Firefox: "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}",
  Mozilla: "{86c18b42-e466-45a9-ae7a-9b95ba6f5640}",
  Sunbird: "{718e30fb-e89b-41dd-9da7-e25a45638b28}",
  SeaMonkey: "{92650c4d-4b8e-4d2a-b7eb-24ecf4f6b63a}",
  Fennec: "{aa3c5121-dab2-40e2-81ca-7ea25febc110}",
  Thunderbird: "{3550f703-e582-4d05-9a08-453d09bdfdc6}"
};

function is(name) {
  if (!(name in ids))
    throw new Error("Unkown Mozilla Application: " + name);
  return ID == ids[name];
};
exports.is = is;

function isOneOf(names) {
  for (var i = 0; i < names.length; i++)
    if (is(names[i]))
      return true;
  return false;
};
exports.isOneOf = isOneOf;

/**
 * Use this to check whether the given version (e.g. xulApp.platformVersion)
 * is in the given range. Versions must be in version comparator-compatible
 * format. See MDC for details:
 * https://developer.mozilla.org/en/XPCOM_Interface_Reference/nsIVersionComparator
 */
var versionInRange = exports.versionInRange =
function versionInRange(version, lowInclusive, highExclusive) {
  return (vc.compare(version, lowInclusive) >= 0) &&
         (vc.compare(version, highExclusive) < 0);
}

const reVersionRange = /^((?:<|>)?=?)?\s*((?:\d+[\S]*)|\*)(?:\s+((?:<|>)=?)?(\d+[\S]+))?$/;
const reOnlyInifinity = /^[<>]?=?\s*[*x]$/;
const reSubInfinity = /\.[*x]/g;
const reHyphenRange = /^(\d+.*?)\s*-\s*(\d+.*?)$/;
const reRangeSeparator = /\s*\|\|\s*/;

const compares = {
  "=": function (c) { return c === 0 },
  ">=": function (c) { return c >= 0 },
  "<=": function (c) { return c <= 0},
  "<": function (c) { return c < 0 },
  ">": function (c) { return c > 0 }
}

function normalizeRange(range) {
    return range
        .replace(reOnlyInifinity, "")
        .replace(reSubInfinity, ".*")
        .replace(reHyphenRange, ">=$1 <=$2")
}

/**
 * Compare the versions given, using the comparison operator provided.
 * Internal use only.
 *
 * @example
 *  compareVersion("1.2", "<=", "1.*") // true
 *
 * @param {String} version
 *  A version to compare
 *
 * @param {String} comparison
 *  The comparison operator
 *
 * @param {String} compareVersion
 *  A version to compare
 */
function compareVersion(version, comparison, compareVersion) {
  let hasWildcard = compareVersion.indexOf("*") !== -1;

  comparison = comparison || "=";

  if (hasWildcard) {
    switch (comparison) {
      case "=":
        let zeroVersion = compareVersion.replace(reSubInfinity, ".0");
        return versionInRange(version, zeroVersion, compareVersion);
      case ">=":
        compareVersion = compareVersion.replace(reSubInfinity, ".0");
        break;
    }
  }

  let compare = compares[comparison];

  return typeof compare === "function" && compare(vc.compare(version, compareVersion));
}

/**
 * Returns `true` if `version` satisfies the `versionRange` given.
 * If only an argument is passed, is used as `versionRange` and compared against
 * `xulApp.platformVersion`.
 *
 * `versionRange` is either a string which has one or more space-separated
 * descriptors, or a range like "fromVersion - toVersion".
 * Version range descriptors may be any of the following styles:
 *
 * - "version" Must match `version` exactly
 * - "=version" Same as just `version`
 * - ">version" Must be greater than `version`
 * - ">=version" Must be greater or equal than `version`
 * - "<version" Must be less than `version`
 * - "<=version" Must be less or equal than `version`
 * - "1.2.x" or "1.2.*" See 'X version ranges' below
 * - "*" or "" (just an empty string) Matches any version
 * - "version1 - version2" Same as ">=version1 <=version2"
 * - "range1 || range2" Passes if either `range1` or `range2` are satisfied
 *
 * For example, these are all valid:
 * - "1.0.0 - 2.9999.9999"
 * - ">=1.0.2 <2.1.2"
 * - ">1.0.2 <=2.3.4"
 * - "2.0.1"
 * - "<1.0.0 || >=2.3.1 <2.4.5 || >=2.5.2 <3.0.0"
 * - "2.x" (equivalent to "2.*")
 * - "1.2.x" (equivalent to "1.2.*" and ">=1.2.0 <1.3.0")
 */
function satisfiesVersion(version, versionRange) {
  if (arguments.length === 1) {
    versionRange = version;
    version = appInfo.version;
  }

  let ranges = versionRange.trim().split(reRangeSeparator);

  return ranges.some(function(range) {
    range = normalizeRange(range);

    // No versions' range specified means that any version satisfies the
    // requirements.
    if (range === "")
      return true;

    let matches = range.match(reVersionRange);

    if (!matches)
      return false;

    let [, lowMod, lowVer, highMod, highVer] = matches;

    return compareVersion(version, lowMod, lowVer) && (highVer !== undefined
      ? compareVersion(version, highMod, highVer)
      : true);
  });
}
exports.satisfiesVersion = satisfiesVersion;
