/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Arg, Option, RetVal, generateActorSpec} = require("devtools/shared/protocol");

const directorRegistrySpec = generateActorSpec({
  typeName: "director-registry",

  methods: {
    finalize: {
      oneway: true
    },
    install: {
      request: {
        scriptId: Arg(0, "string"),
        scriptCode: Option(1, "string"),
        scriptOptions: Option(1, "nullable:json")
      },
      response: {
        success: RetVal("boolean")
      }
    },
    uninstall: {
      request: {
        scritpId: Arg(0, "string")
      },
      response: {
        success: RetVal("boolean")
      }
    },
    list: {
      response: {
        directorScripts: RetVal("array:string")
      }
    },
  },
});

exports.directorRegistrySpec = directorRegistrySpec;
