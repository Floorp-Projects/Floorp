/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const L10n = require("devtools/client/webconsole/test/node/fixtures/L10n");

const Utils = {
  L10n,
  supportsString: function(s) {
    return s;
  },
};

module.exports = {
  Utils,
};
