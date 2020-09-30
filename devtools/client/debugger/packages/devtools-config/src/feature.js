/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {get: pick, set: put} = require("lodash");
const fs = require("fs");
const path = require("path");

let config;
/**
 * Gets a config value for a given key
 * e.g "chrome.webSocketPort"
 */
function getValue(key) {
  return pick(config, key);
}

function setValue(key, value) {
  return put(config, key, value);
}

function setConfig(value) {
  config = value;
}

function getConfig() {
  return config;
}

function updateLocalConfig(relativePath) {
  const localConfigPath = path.resolve(relativePath, "../configs/local.json");
  const output = JSON.stringify(config, null, 2);
  fs.writeFileSync(localConfigPath, output, { flag: "w" });
  return output;
}

module.exports = {
  getValue,
  setValue,
  getConfig,
  setConfig,
  updateLocalConfig,
};
