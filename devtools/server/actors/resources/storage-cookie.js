/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  TYPES: { COOKIE },
} = require("devtools/server/actors/resources/index");

const ParentProcessStorage = require("devtools/server/actors/resources/utils/parent-process-storage");

class CookiesWatcher extends ParentProcessStorage {
  constructor() {
    super("cookies", COOKIE);
  }
}

module.exports = CookiesWatcher;
