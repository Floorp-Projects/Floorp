/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("resource://devtools/shared/protocol.js");
const {
  targetConfigurationSpec,
} = require("resource://devtools/shared/specs/target-configuration.js");

/**
 * The TargetConfigurationFront/Actor should be used to populate the DevTools server
 * with settings read from the client side but which impact the server.
 * For instance, "disable cache" is a feature toggled via DevTools UI (client),
 * but which should be communicated to the targets (server).
 *
 * See the TargetConfigurationActor for a list of supported configuration options.
 */
class TargetConfigurationFront extends FrontClassWithSpec(
  targetConfigurationSpec
) {
  form(json) {
    // Read the initial configuration.
    this.initialConfiguration = json.configuration;
    this.traits = json.traits;
  }
}

registerFront(TargetConfigurationFront);
