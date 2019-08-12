/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const merge = require("lodash/merge");
const fs = require("fs");
const path = require("path");

function getConfig() {
  const firefoxConfig = require("../configs/firefox-panel.json");
  const developmentConfig = require("../configs/development.json");

  let localConfig = {};
  if (fs.existsSync(path.resolve(__dirname, "../configs/local.json"))) {
    localConfig = require("../configs/local.json");
  }

  if (process.env.TARGET === "firefox-panel") {
    return firefoxConfig;
  }

  const envConfig = developmentConfig;

  return merge({}, envConfig, localConfig);
}

module.exports = getConfig;