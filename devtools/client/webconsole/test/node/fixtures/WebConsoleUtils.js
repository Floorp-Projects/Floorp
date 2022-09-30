/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const L10n = require("resource://devtools/client/webconsole/test/node/fixtures/L10n.js");

const Utils = {
  L10n,
  supportsString(s) {
    return s;
  },
};

module.exports = {
  Utils,
};
