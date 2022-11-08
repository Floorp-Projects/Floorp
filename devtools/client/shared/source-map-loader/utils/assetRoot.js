/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const {
  SourceMapConsumer,
} = require("resource://devtools/client/shared/vendor/source-map/source-map.js");

function setAssetRootURL() {
  SourceMapConsumer.initialize({
    "lib/mappings.wasm":
      "resource://devtools/client/shared/vendor/source-map/lib/mappings.wasm",
  });
}

module.exports = {
  setAssetRootURL,
};
