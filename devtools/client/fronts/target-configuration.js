/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const {
  targetConfigurationSpec,
} = require("devtools/shared/specs/target-configuration");

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
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    // Map of configuration options. Maps flag key (String) to flag value.
    // Value is not restricted to a single type and will vary based on the
    // option
    this._configuration = {};
  }

  form(json) {
    // Read the initial configuration.
    this._configuration = json.configuration;

    this._traits = json.traits;
  }

  /**
   * Retrieve the current map of configuration options pushed to the server.
   */
  get configuration() {
    return this._configuration;
  }

  // XXX: No call site for this at the moment, just adding this as an example of
  // the API we could expose here.
  supportsConfigurationOption(optionName) {
    return !!this._traits.supportedOptions[optionName];
  }

  async updateConfiguration(configuration) {
    const updatedConfiguration = await super.updateConfiguration(configuration);
    // Update the client-side copy of the DevTools configuration
    this._configuration = updatedConfiguration;
  }
}

registerFront(TargetConfigurationFront);
