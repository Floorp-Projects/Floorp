/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

"use strict";

const fs = require("fs");
const path = require("path");

function getConfig() {
  if (process.env.TARGET === "firefox-panel") {
    return require("../configs/firefox-panel.json");
  }

  const developmentConfig = require("../configs/development.json");

  let localConfig = {};
  if (fs.existsSync(path.resolve(__dirname, "../configs/local.json"))) {
    localConfig = require("../configs/local.json");
  }

  return Object.assign({}, developmentConfig, localConfig);
}

module.exports = {
  getConfig,
};
