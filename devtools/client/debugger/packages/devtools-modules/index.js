/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { PrefsHelper } = require("./src/prefs");
const KeyShortcuts = require("./src/key-shortcuts");
const EventEmitter = require("./src/utils/event-emitter");
const Telemetry = require("./src/utils/telemetry");
const { getUnicodeHostname, getUnicodeUrlPath, getUnicodeUrl } =
  require("./src/unicode-url");
const PluralForm = require("./src/plural-form");
const saveAs = require("./src/saveAs")

module.exports = {
  KeyShortcuts,
  PrefsHelper,
  EventEmitter,
  Telemetry,
  getUnicodeHostname,
  getUnicodeUrlPath,
  getUnicodeUrl,
  PluralForm,
  saveAs
};
