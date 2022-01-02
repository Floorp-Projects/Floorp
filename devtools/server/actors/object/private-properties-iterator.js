/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const protocol = require("devtools/shared/protocol");
const {
  privatePropertiesIteratorSpec,
} = require("devtools/shared/specs/private-properties-iterator");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");

/**
 * Creates an actor to iterate over an object's private properties.
 *
 * @param objectActor ObjectActor
 *        The object actor.
 */
const PrivatePropertiesIteratorActor = protocol.ActorClassWithSpec(
  privatePropertiesIteratorSpec,
  {
    initialize(objectActor, conn) {
      protocol.Actor.prototype.initialize.call(this, conn);

      let privateProperties = [];
      if (DevToolsUtils.isSafeDebuggerObject(objectActor.obj)) {
        try {
          privateProperties = objectActor.obj.getOwnPrivateProperties();
        } catch (err) {
          // The above can throw when the debuggee does not subsume the object's
          // compartment, or for some WrappedNatives like Cu.Sandbox.
        }
      }

      this.iterator = {
        size: privateProperties.length,
        propertyDescription(index) {
          // private properties are represented as Symbols on platform
          const symbol = privateProperties[index];
          return {
            name: symbol.description,
            descriptor: objectActor._propertyDescriptor(symbol),
          };
        },
      };
    },

    form() {
      return {
        type: this.typeName,
        actor: this.actorID,
        count: this.iterator.size,
      };
    },

    slice({ start, count }) {
      const privateProperties = [];
      for (let i = start, m = start + count; i < m; i++) {
        privateProperties.push(this.iterator.propertyDescription(i));
      }
      return {
        privateProperties,
      };
    },

    all() {
      return this.slice({ start: 0, count: this.iterator.size });
    },
  }
);

exports.PrivatePropertiesIteratorActor = PrivatePropertiesIteratorActor;
