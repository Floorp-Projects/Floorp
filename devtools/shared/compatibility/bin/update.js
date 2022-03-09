/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// The Compatibility panel detects issues by comparing against official MDN compatibility data
// at https://github.com/mdn/browser-compat-data). It uses a local snapshot of the dataset.
// This dataset needs to be manually synchronized periodically

// The subsets from the dataset required by the Compatibility panel are:
// * css.properties: https://github.com/mdn/browser-compat-data/tree/master/css

// The MDN compatibility data is available as a node package ("@mdn/browser-compat-data"),
// which is used here to update `../dataset/css-properties.json`.

/* global __dirname */

"use strict";

const compatData = require("@mdn/browser-compat-data");
exportData(compatData.css.properties, "css-properties.json");

function exportData(data, fileName) {
  const fs = require("fs");
  const path = require("path");

  const content = `${JSON.stringify(data)}`;

  fs.writeFile(
    path.resolve(__dirname, "../dataset", fileName),
    content,
    err => {
      if (err) {
        console.error(err);
      } else {
        console.log(`${fileName} downloaded`);
      }
    }
  );
}
