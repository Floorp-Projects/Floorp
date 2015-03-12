/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "experimental"
};

const { method } = require("method/core");

const onEnable = method("dev/theme/hooks#onEnable");
const onDisable = method("dev/theme/hooks#onDisable");

exports.onEnable = onEnable;
exports.onDisable = onDisable;
