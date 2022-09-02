/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * List of all descriptor types.
 *
 * This will be expose via `descriptorType` on all descriptor fronts.
 */
module.exports = {
  PROCESS: "process",
  WORKER: "worker",
  TAB: "tab",
  EXTENSION: "extension",
};
