/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Runtime = require("./runtime");

/**
 * This class represents the Firefox instance which runs in the same environment that
 * opened about:debugging.
 */
class ThisFirefox extends Runtime {
  getIcon() {
    return "chrome://devtools/skin/images/firefox-logo-glyph.svg";
  }

  getName() {
    return "This Firefox";
  }
}

module.exports = ThisFirefox;
