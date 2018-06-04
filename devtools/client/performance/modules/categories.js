/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { L10N } = require("devtools/client/performance/modules/global");

/**
 * Details about each label stack frame category.
 * To be kept in sync with the js::ProfilingStackFrame::Category in ProfilingStack.h
 */
const CATEGORIES = [{
  color: "#d99b28",
  abbrev: "idle",
  label: L10N.getStr("category.idle")
}, {
  color: "#5e88b0",
  abbrev: "other",
  label: L10N.getStr("category.other")
}, {
  color: "#46afe3",
  abbrev: "layout",
  label: L10N.getStr("category.layout")
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
  abbrev: "dom",
  label: L10N.getStr("category.dom")
}, {
  // The devtools-only "tools" category which gets computed by
  // computeIsContentAndCategory and is not part of the category list in
  // ProfilingStack.h.
  color: "#8fa1b2",
  abbrev: "tools",
  label: L10N.getStr("category.tools")
}];

/**
 * Get the numeric index for the given category abbreviation.
 * See `CATEGORIES` above.
 */
const CATEGORY_INDEX = (() => {
  const indexForCategory = {};
  for (let categoryIndex = 0; categoryIndex < CATEGORIES.length; categoryIndex++) {
    const category = CATEGORIES[categoryIndex];
    indexForCategory[category.abbrev] = categoryIndex;
  }

  return function(name) {
    if (!(name in indexForCategory)) {
      throw new Error(`Category abbreviation "${name}" does not exist.`);
    }
    return indexForCategory[name];
  };
})();

exports.CATEGORIES = CATEGORIES;
exports.CATEGORY_INDEX = CATEGORY_INDEX;
