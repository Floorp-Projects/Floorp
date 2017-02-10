/* global define */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Disable eslint strict rule because the eslint env ("es6") is incompatible with eslint
// validation in MC (https://github.com/devtools-html/devtools-reps/issues/58)
/* eslint-disable strict */

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  let REPS;
  let MODE;
  let createFactories;
  let parseURLEncodedText;
  let parseURLParams;

  // useRepsBundle hardcoded to true to use the bundle from github. Switch back to rely
  // on individual reps files by flipping it to false.
  let useRepsBundle = true;
  if (useRepsBundle) {
    const bundle = require("devtools/client/shared/components/reps/reps");
    REPS = bundle.REPS;
    MODE = bundle.MODE;
    createFactories = bundle.createFactories;
    parseURLEncodedText = bundle.parseURLEncodedText;
    parseURLParams = bundle.parseURLParams;
  } else {
    // Commenting out all requires, otherwise requirejs will agressively load all
    // dependencies when loading load-reps.js, which will fail because files have been
    // removed.
    // ({ createFactories, parseURLEncodedText, parseURLParams } =
    //   require("devtools/client/shared/components/reps/rep-utils"));
    // REPS = require("devtools/client/shared/components/reps/rep").REPS;
    // MODE = require("devtools/client/shared/components/reps/constants").MODE;
  }

  exports.REPS = REPS;
  exports.MODE = MODE;
  exports.createFactories = createFactories;
  exports.parseURLEncodedText = parseURLEncodedText;
  exports.parseURLParams = parseURLParams;
});
