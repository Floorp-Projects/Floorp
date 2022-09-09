/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu } = require("chrome");
var { Front } = require("devtools/shared/protocol/Front");

/**
 * Generates request methods as described by the given actor specification on
 * the given front prototype. Returns the front prototype.
 */
var generateRequestMethods = function(actorSpec, frontProto) {
  if (frontProto._actorSpec) {
    throw new Error("frontProto called twice on the same front prototype!");
  }

  frontProto.typeName = actorSpec.typeName;

  // Generate request methods.
  const methods = actorSpec.methods;
  methods.forEach(spec => {
    const name = spec.name;

    frontProto[name] = function(...args) {
      // If the front is destroyed, the request will not be able to complete.
      if (this.isDestroyed()) {
        throw new Error(
          `Can not send request '${name}' because front '${this.typeName}' is already destroyed.`
        );
      }

      const startTime = Cu.now();
      let packet;
      try {
        packet = spec.request.write(args, this);
      } catch (ex) {
        console.error("Error writing request: " + name);
        throw ex;
      }
      if (spec.oneway) {
        // Fire-and-forget oneway packets.
        this.send(packet);
        return undefined;
      }

      return this.request(packet).then(response => {
        let ret;
        if (!this.conn) {
          throw new Error("Missing conn on " + this);
        }
        if (this.isDestroyed()) {
          throw new Error(
            `Can not interpret '${name}' response because front '${this.typeName}' is already destroyed.`
          );
        }
        try {
          ret = spec.response.read(response, this);
        } catch (ex) {
          console.error("Error reading response to: " + name + "\n" + ex);
          throw ex;
        }
        ChromeUtils.addProfilerMarker(
          "RDP Front",
          startTime,
          `${this.typeName}:${name}()`
        );
        return ret;
      });
    };

    // Release methods should call the destroy function on return.
    if (spec.release) {
      const fn = frontProto[name];
      frontProto[name] = function(...args) {
        return fn.apply(this, args).then(result => {
          this.destroy();
          return result;
        });
      };
    }
  });

  // Process event specifications
  frontProto._clientSpec = {};

  const actorEvents = actorSpec.events;
  if (actorEvents) {
    frontProto._clientSpec.events = new Map();

    for (const [name, request] of actorEvents) {
      frontProto._clientSpec.events.set(request.type, {
        name,
        request,
      });
    }
  }

  frontProto._actorSpec = actorSpec;

  return frontProto;
};

/**
 * Create a front class for the given actor specification and front prototype.
 *
 * @param object actorSpec
 *    The actor specification you're creating a front for.
 * @param object proto
 *    The object prototype.  Must have a 'typeName' property,
 *    should have method definitions, can have event definitions.
 */
var FrontClassWithSpec = function(actorSpec) {
  class OneFront extends Front {}
  generateRequestMethods(actorSpec, OneFront.prototype);
  return OneFront;
};
exports.FrontClassWithSpec = FrontClassWithSpec;
