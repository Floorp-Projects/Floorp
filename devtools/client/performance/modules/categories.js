/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { L10N } = require("devtools/client/performance/modules/global");

/**
 * Details about each label stack frame category.
 * @see CATEGORY_MAPPINGS.
 */
const CATEGORIES = [{
  color: "#5e88b0",
  abbrev: "other",
  label: L10N.getStr("category.other")
}, {
  color: "#46afe3",
  abbrev: "css",
  label: L10N.getStr("category.css")
}, {
  color: "#d96629",
  abbrev: "js",
  label: L10N.getStr("category.js")
}, {
  color: "#eb5368",
  abbrev: "gc",
  label: L10N.getStr("category.gc")
}, {
  color: "#df80ff",
  abbrev: "network",
  label: L10N.getStr("category.network")
}, {
  color: "#70bf53",
  abbrev: "graphics",
  label: L10N.getStr("category.graphics")
}, {
  color: "#8fa1b2",
  abbrev: "storage",
  label: L10N.getStr("category.storage")
}, {
  color: "#d99b28",
  abbrev: "events",
  label: L10N.getStr("category.events")
}, {
  color: "#8fa1b2",
  abbrev: "tools",
  label: L10N.getStr("category.tools")
}];

/**
 * Mapping from category bitmasks in the profiler data to additional details.
 * To be kept in sync with the js::ProfilingStackFrame::Category in ProfilingStack.h
 */
const CATEGORY_MAPPINGS = {
  // js::ProfilingStackFrame::Category::OTHER
  "16": CATEGORIES[0],
  // js::ProfilingStackFrame::Category::CSS
  "32": CATEGORIES[1],
  // js::ProfilingStackFrame::Category::JS
  "64": CATEGORIES[2],
  // js::ProfilingStackFrame::Category::GC
  "128": CATEGORIES[3],
  // js::ProfilingStackFrame::Category::CC
  "256": CATEGORIES[3],
  // js::ProfilingStackFrame::Category::NETWORK
  "512": CATEGORIES[4],
  // js::ProfilingStackFrame::Category::GRAPHICS
  "1024": CATEGORIES[5],
  // js::ProfilingStackFrame::Category::STORAGE
  "2048": CATEGORIES[6],
  // js::ProfilingStackFrame::Category::EVENTS
  "4096": CATEGORIES[7],
  // non-bitmasks for specially-assigned categories
  "9000": CATEGORIES[8],
};

/**
 * Get the numeric bitmask (or set of masks) for the given category
 * abbreviation. See `CATEGORIES` and `CATEGORY_MAPPINGS` above.
 *
 * CATEGORY_MASK can be called with just a name if it is expected that the
 * category is mapped to by exactly one bitmask. If the category is mapped
 * to by multiple masks, CATEGORY_MASK for that name must be called with
 * an additional argument specifying the desired id (in ascending order).
 */
const [CATEGORY_MASK, CATEGORY_MASK_LIST] = (() => {
  let bitmasksForCategory = {};
  let all = Object.keys(CATEGORY_MAPPINGS);

  for (let category of CATEGORIES) {
    bitmasksForCategory[category.abbrev] = all
      .filter(mask => CATEGORY_MAPPINGS[mask] == category)
      .map(mask => +mask)
      .sort();
  }

  return [
    function(name, index) {
      if (!(name in bitmasksForCategory)) {
        throw new Error(`Category abbreviation "${name}" does not exist.`);
      }
      if (arguments.length == 1) {
        if (bitmasksForCategory[name].length != 1) {
          throw new Error(`Expected exactly one category number for "${name}".`);
        } else {
          return bitmasksForCategory[name][0];
        }
      } else {
        if (index > bitmasksForCategory[name].length) {
          throw new Error(`Index "${index}" too high for category "${name}".`);
        }
        return bitmasksForCategory[name][index - 1];
      }
    },

    function(name) {
      if (!(name in bitmasksForCategory)) {
        throw new Error(`Category abbreviation "${name}" does not exist.`);
      }
      return bitmasksForCategory[name];
    }
  ];
})();

exports.CATEGORIES = CATEGORIES;
exports.CATEGORY_MAPPINGS = CATEGORY_MAPPINGS;
exports.CATEGORY_MASK = CATEGORY_MASK;
exports.CATEGORY_MASK_LIST = CATEGORY_MASK_LIST;
