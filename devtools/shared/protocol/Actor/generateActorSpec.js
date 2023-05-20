/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { Request } = require("resource://devtools/shared/protocol/Request.js");
const { Response } = require("resource://devtools/shared/protocol/Response.js");
var {
  types,
  registeredTypes,
} = require("resource://devtools/shared/protocol/types.js");

/**
 * Generates an actor specification from an actor description.
 */
var generateActorSpec = function (actorDesc) {
  const actorSpec = {
    typeName: actorDesc.typeName,
    methods: [],
  };

  // Find additional method specifications
  if (actorDesc.methods) {
    for (const name in actorDesc.methods) {
      const methodSpec = actorDesc.methods[name];
      const spec = {};

      spec.name = methodSpec.name || name;
      spec.request = new Request(
        Object.assign({ type: spec.name }, methodSpec.request || undefined)
      );
      spec.response = new Response(methodSpec.response || undefined);
      spec.release = methodSpec.release;
      spec.oneway = methodSpec.oneway;

      actorSpec.methods.push(spec);
    }
  }

  // Find event specifications
  if (actorDesc.events) {
    actorSpec.events = new Map();
    for (const name in actorDesc.events) {
      const eventRequest = actorDesc.events[name];
      Object.freeze(eventRequest);
      actorSpec.events.set(
        name,
        new Request(Object.assign({ type: name }, eventRequest))
      );
    }
  }

  if (!registeredTypes.has(actorSpec.typeName)) {
    types.addActorType(actorSpec.typeName);
  }
  registeredTypes.get(actorSpec.typeName).actorSpec = actorSpec;

  return actorSpec;
};

exports.generateActorSpec = generateActorSpec;
