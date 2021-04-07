/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  generateActorSpec,
  Arg,
  RetVal,
  types,
} = require("devtools/shared/protocol");

types.addDictType("target-configuration.configuration", {
  cacheDisabled: "nullable:boolean",
  colorSchemeSimulation: "nullable:string",
  javascriptEnabled: "nullable:boolean",
  overrideDPPX: "nullable:number",
  paintFlashing: "nullable:boolean",
  printSimulationEnabled: "nullable:boolean",
  restoreFocus: "nullable:boolean",
  serviceWorkersTestingEnabled: "nullable:boolean",
});

const targetConfigurationSpec = generateActorSpec({
  typeName: "target-configuration",

  methods: {
    updateConfiguration: {
      request: {
        configuration: Arg(0, "target-configuration.configuration"),
      },
      response: {
        configuration: RetVal("target-configuration.configuration"),
      },
    },
  },
});

exports.targetConfigurationSpec = targetConfigurationSpec;
