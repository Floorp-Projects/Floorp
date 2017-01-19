/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  let REPS;
  let MODE;
  let createFactories;
  let parseURLEncodedText;
  let parseURLParams;

  // useRepsBundle hardcoded to false while we experiment with the reps bundle in
  // bugs 1325401 and 1330779. Set it to true to test devtools using the reps bundle.
  let useRepsBundle = false;
  if (useRepsBundle) {
    const bundle = require("devtools/client/shared/components/reps/reps");
    REPS = bundle.REPS;
    MODE = bundle.MODE;
    createFactories = bundle.createFactories;
    parseURLEncodedText = bundle.parseURLEncodedText;
    parseURLParams = bundle.parseURLParams;
  } else {
    ({ createFactories, parseURLEncodedText, parseURLParams } =
      require("devtools/client/shared/components/reps/rep-utils"));
    REPS = require("devtools/client/shared/components/reps/rep").REPS;
    MODE = require("devtools/client/shared/components/reps/constants").MODE;
  }

  exports.REPS = REPS;
  exports.MODE = MODE;
  exports.createFactories = createFactories;
  exports.parseURLEncodedText = parseURLEncodedText;
  exports.parseURLParams = parseURLParams;
});
