/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Cc, Ci, Cu, Cr} = require("chrome");

Cu.import("resource:///modules/devtools/ViewHelpers.jsm");

/**
 * Localization convenience methods.
 */
const STRINGS_URI = "chrome://browser/locale/devtools/profiler.properties";
const L10N = new ViewHelpers.L10N(STRINGS_URI);

/**
 * Details about each profile entry cateogry.
 * @see CATEGORY_MAPPINGS.
 */
const CATEGORIES = [
  { ordinal: 7, color: "#5e88b0", abbrev: "other", label: L10N.getStr("category.other") },
  { ordinal: 4, color: "#46afe3", abbrev: "css", label: L10N.getStr("category.css") },
  { ordinal: 1, color: "#d96629", abbrev: "js", label: L10N.getStr("category.js") },
  { ordinal: 2, color: "#eb5368", abbrev: "gc", label: L10N.getStr("category.gc") },
  { ordinal: 0, color: "#df80ff", abbrev: "network", label: L10N.getStr("category.network") },
  { ordinal: 5, color: "#70bf53", abbrev: "graphics", label: L10N.getStr("category.graphics") },
  { ordinal: 6, color: "#8fa1b2", abbrev: "storage", label: L10N.getStr("category.storage") },
  { ordinal: 3, color: "#d99b28", abbrev: "events", label: L10N.getStr("category.events") }
];

/**
 * Mapping from category bitmasks in the profiler data to additional details.
 * To be kept in sync with the js::ProfileEntry::Category in ProfilingStack.h
 */
const CATEGORY_MAPPINGS = {
  "16": CATEGORIES[0],      // js::ProfileEntry::Category::OTHER
  "32": CATEGORIES[1],      // js::ProfileEntry::Category::CSS
  "64": CATEGORIES[2],      // js::ProfileEntry::Category::JS
  "128": CATEGORIES[3],     // js::ProfileEntry::Category::GC
  "256": CATEGORIES[3],     // js::ProfileEntry::Category::CC
  "512": CATEGORIES[4],     // js::ProfileEntry::Category::NETWORK
  "1024": CATEGORIES[5],    // js::ProfileEntry::Category::GRAPHICS
  "2048": CATEGORIES[6],    // js::ProfileEntry::Category::STORAGE
  "4096": CATEGORIES[7],    // js::ProfileEntry::Category::EVENTS
};

/**
 * Get the numeric bitmask (or set of masks) for the given category
 * abbreviation. See CATEGORIES and CATEGORY_MAPPINGS above.
 *
 * CATEGORY_MASK can be called with just a name if it is expected that the
 * category is mapped to by exactly one bitmask.  If the category is mapped
 * to by multiple masks, CATEGORY_MASK for that name must be called with
 * an additional argument specifying the desired id (in ascending order).
 */
const [CATEGORY_MASK, CATEGORY_MASK_LIST] = (function () {
  let mappings = {};
  for (let category of CATEGORIES) {
    let numList = Object.keys(CATEGORY_MAPPINGS)
          .filter(k => CATEGORY_MAPPINGS[k] == category)
          .map(k => +k);
    numList.sort();
    mappings[category.abbrev] = numList;
  }

  return [
    function (name, num) {
      if (!(name in mappings)) {
        throw new Error(`Category abbreviation '${name}' does not exist.`);
      }
      if (arguments.length == 1) {
        if (mappings[name].length != 1) {
          throw new Error(`Expected exactly one category number for '${name}'.`);
        }
        return mappings[name][0];
      }
      if (num > mappings[name].length) {
        throw new Error(`Num '${num}' too high for category '${name}'.`);
      }
      return mappings[name][num - 1];
    },

    function (name) {
      if (!(name in mappings)) {
        throw new Error(`Category abbreviation '${name}' does not exist.`);
      }
      return mappings[name];
    }
  ];
})();

// Human-readable "other" category bitmask. Older Geckos don't have all the
// necessary instrumentation in the sampling profiler backend for creating
// a categories graph, in which case we default to the "other" category.
const CATEGORY_OTHER = CATEGORY_MASK('other');

// Human-readable JIT category bitmask. Certain pseudo-frames in a sample,
// like "EnterJIT", don't have any associated `cateogry` information.
const CATEGORY_JIT = CATEGORY_MASK('js');

// Exported symbols.
exports.L10N = L10N;
exports.CATEGORIES = CATEGORIES;
exports.CATEGORY_MAPPINGS = CATEGORY_MAPPINGS;
exports.CATEGORY_OTHER = CATEGORY_OTHER;
exports.CATEGORY_JIT = CATEGORY_JIT;
exports.CATEGORY_MASK = CATEGORY_MASK;
exports.CATEGORY_MASK_LIST = CATEGORY_MASK_LIST;
