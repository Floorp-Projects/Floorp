/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const getConfig = require("./src/config").getConfig;
const setConfig = require("./src/feature").setConfig;

function registerConfig() {
  setConfig(getConfig());
}

module.exports = {
  getConfig,
  registerConfig
};
