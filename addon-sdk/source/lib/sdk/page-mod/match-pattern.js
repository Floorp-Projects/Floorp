/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 "use strict";

let { deprecateUsage } = require("../util/deprecate");

deprecateUsage("Module 'sdk/page-mod/match-pattern' is deprecated use 'sdk/util/match-pattern' instead");

module.exports = require("../util/match-pattern");
