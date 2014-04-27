/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "unstable",
  "engines": {
    "Firefox": "*",
    "Fennec": "*"
  }
};

const { modelFor } = require("./model/core");
const { viewFor } = require("./view/core");
const { isTab } = require("./tabs/utils");


if (require("./system/xul-app").is("Fennec")) {
  module.exports = require("./windows/tabs-fennec").tabs;
}
else {
  module.exports = require("./tabs/tabs-firefox");
}

const tabs = module.exports;

// Implement `modelFor` function for the Tab instances.
// Finds a right model by iterating over all tab models
// and finding one that wraps given `view`.
modelFor.when(isTab, view => {
  for (let model of tabs) {
    if (viewFor(model) === view)
      return model;
  }
  return null;
});
