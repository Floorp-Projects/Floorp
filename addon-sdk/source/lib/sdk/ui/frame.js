/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "experimental",
  "engines": {
    "Firefox": "> 28"
  }
};

// Because Firefox Holly, we still need to check if `CustomizableUI` is
// available. Once Australis will officially land, we can safely remove it.
// See Bug 959142
try {
  require("chrome").Cu.import("resource:///modules/CustomizableUI.jsm", {});
}
catch (e) {
  throw Error("Unsupported Application: The module"  + module.id +
              " does not support this application.");
}

require("./frame/view");
const { Frame } = require("./frame/model");

exports.Frame = Frame;
