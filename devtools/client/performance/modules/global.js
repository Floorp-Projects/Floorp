/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { ViewHelpers } = require("resource://devtools/client/shared/widgets/ViewHelpers.jsm");

/**
 * Localization convenience methods.
 */
const L10N = new ViewHelpers.MultiL10N([
  "chrome://devtools/locale/markers.properties",
  "chrome://devtools/locale/performance.properties"
]);

/**
 * A list of preferences for this tool. The values automatically update
 * if somebody edits edits about:config or the prefs change somewhere else.
 */
const PREFS = new ViewHelpers.Prefs("devtools.performance", {
  "show-triggers-for-gc-types": ["Char", "ui.show-triggers-for-gc-types"],
  "show-platform-data": ["Bool", "ui.show-platform-data"],
  "hidden-markers": ["Json", "timeline.hidden-markers"],
  "memory-sample-probability": ["Float", "memory.sample-probability"],
  "memory-max-log-length": ["Int", "memory.max-log-length"],
  "profiler-buffer-size": ["Int", "profiler.buffer-size"],
  "profiler-sample-frequency": ["Int", "profiler.sample-frequency-khz"],
  // TODO re-enable once we flame charts via bug 1148663
  "enable-memory-flame": ["Bool", "ui.enable-memory-flame"],
}, {
  monitorChanges: true
});

/**
 * Details about each profile entry cateogry.
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
 * To be kept in sync with the js::ProfileEntry::Category in ProfilingStack.h
 */
const CATEGORY_MAPPINGS = {
  "16": CATEGORIES[0],    // js::ProfileEntry::Category::OTHER
  "32": CATEGORIES[1],    // js::ProfileEntry::Category::CSS
  "64": CATEGORIES[2],    // js::ProfileEntry::Category::JS
  "128": CATEGORIES[3],   // js::ProfileEntry::Category::GC
  "256": CATEGORIES[3],   // js::ProfileEntry::Category::CC
  "512": CATEGORIES[4],   // js::ProfileEntry::Category::NETWORK
  "1024": CATEGORIES[5],  // js::ProfileEntry::Category::GRAPHICS
  "2048": CATEGORIES[6],  // js::ProfileEntry::Category::STORAGE
  "4096": CATEGORIES[7],  // js::ProfileEntry::Category::EVENTS

  // non-bitmasks for specially-assigned categories
  "9000": CATEGORIES[8],
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
  let bitmasksForCategory = {};
  let all = Object.keys(CATEGORY_MAPPINGS);

  for (let category of CATEGORIES) {
    bitmasksForCategory[category.abbrev] = all
      .filter(mask => CATEGORY_MAPPINGS[mask] == category)
      .map(mask => +mask)
      .sort();
  }

  return [
    function (name, index) {
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
        } else {
          return bitmasksForCategory[name][index - 1];
        }
      }
    },

    function (name) {
      if (!(name in bitmasksForCategory)) {
        throw new Error(`Category abbreviation "${name}" does not exist.`);
      }
      return bitmasksForCategory[name];
    }
  ];
})();

// Human-readable "other" category bitmask. Older Geckos don't have all the
// necessary instrumentation in the sampling profiler backend for creating
// a categories graph, in which case we default to the "other" category.
const CATEGORY_OTHER = CATEGORY_MASK("other");

// Human-readable JIT category bitmask. Certain pseudo-frames in a sample,
// like "EnterJIT", don't have any associated `category` information.
const CATEGORY_JIT = CATEGORY_MASK("js");

// Human-readable "devtools" category bitmask. Not emitted from frames themselves,
// but used manually in the client.
const CATEGORY_DEVTOOLS = CATEGORY_MASK("tools");

// Exported symbols.
exports.L10N = L10N;
exports.PREFS = PREFS;
exports.CATEGORIES = CATEGORIES;
exports.CATEGORY_MAPPINGS = CATEGORY_MAPPINGS;
exports.CATEGORY_MASK = CATEGORY_MASK;
exports.CATEGORY_MASK_LIST = CATEGORY_MASK_LIST;
exports.CATEGORY_OTHER = CATEGORY_OTHER;
exports.CATEGORY_JIT = CATEGORY_JIT;
exports.CATEGORY_DEVTOOLS = CATEGORY_DEVTOOLS;
