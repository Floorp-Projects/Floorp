/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { extend } = require("devtools/shared/extend");
var { Pool } = require("devtools/shared/protocol/Pool");
const { Cu } = require("chrome");
const ChromeUtils = require("ChromeUtils");

/**
 * Keep track of which actorSpecs have been created. If a replica of a spec
 * is created, it can be caught, and specs which inherit from other specs will
 * not overwrite eachother.
 */
var actorSpecs = new WeakMap();

exports.actorSpecs = actorSpecs;

/**
 * An actor in the actor tree.
 *
 * @param optional conn
 *   Either a DevToolsServerConnection or a DevToolsClient.  Must have
 *   addActorPool, removeActorPool, and poolFor.
 *   conn can be null if the subclass provides a conn property.
 * @constructor
 */
class Actor extends Pool {
  // Existing Actors extending this class expect initialize to contain constructor logic.
  initialize(conn) {
    // Repeat Pool.constructor here as we can't call it from initialize
    // This is to be removed once actors switch to es classes and are able to call
    // Actor's contructor.
    if (conn) {
      this.conn = conn;
    }

    // Will contain the actor's ID
    this.actorID = null;

    this._actorSpec = actorSpecs.get(Object.getPrototypeOf(this));
    // Forward events to the connection.
    if (this._actorSpec && this._actorSpec.events) {
      for (const [name, request] of this._actorSpec.events.entries()) {
        this.on(name, (...args) => {
          this._sendEvent(name, request, ...args);
        });
      }
    }
  }

  toString() {
    return "[Actor " + this.typeName + "/" + this.actorID + "]";
  }

  _sendEvent(name, request, ...args) {
    if (this.isDestroyed()) {
      console.error(
        `Tried to send a '${name}' event on an already destroyed actor` +
          ` '${this.typeName}'`
      );
      return;
    }
    const startTime = isWorker ? null : Cu.now();
    let packet;
    try {
      packet = request.write(args, this);
    } catch (ex) {
      console.error("Error sending event: " + name);
      throw ex;
    }
    packet.from = packet.from || this.actorID;
    this.conn.send(packet);

    ChromeUtils.addProfilerMarker(
      "DevTools:RDP Actor",
      startTime,
      `${this.typeName}.${name}`
    );
  }

  destroy() {
    super.destroy();
    this.actorID = null;
    this._isDestroyed = true;
  }

  /**
   * Override this method in subclasses to serialize the actor.
   * @param [optional] string hint
   *   Optional string to customize the form.
   * @returns A jsonable object.
   */
  form(hint) {
    return { actor: this.actorID };
  }

  writeError(error, typeName, method) {
    console.error(
      `Error while calling actor '${typeName}'s method '${method}'`,
      error.message || error
    );
    // Also log the error object as-is in order to log the server side stack
    // nicely in the console, while the previous log will log the client side stack only.
    if (error.stack) {
      console.error(error);
    }

    // Do not try to send the error if the actor is destroyed
    // as the connection is probably also destroyed and may throw.
    if (this.isDestroyed()) {
      return;
    }

    this.conn.send({
      from: this.actorID,
      // error.error -> errors created using the throwError() helper
      // error.name -> errors created using `new Error` or Components.exception
      // typeof(error)=="string" -> a method thrown like this `throw "a string"`
      error:
        error.error ||
        error.name ||
        (typeof error == "string" ? error : "unknownError"),
      message: error.message,
      // error.fileName -> regular Error instances
      // error.filename -> errors created using Components.exception
      fileName: error.fileName || error.filename,
      lineNumber: error.lineNumber,
      columnNumber: error.columnNumber,
    });
  }

  _queueResponse(create) {
    const pending = this._pendingResponse || Promise.resolve(null);
    const response = create(pending);
    this._pendingResponse = response;
  }

  /**
   * Throw an error with the passed message and attach an `error` property to the Error
   * object so it can be consumed by the writeError function.
   * @param {String} error: A string (usually a single word serving as an id) that will
   *                        be assign to error.error.
   * @param {String} message: The string that will be passed to the Error constructor.
   * @throws This always throw.
   */
  throwError(error, message) {
    const err = new Error(message);
    err.error = error;
    throw err;
  }
}

exports.Actor = Actor;

/**
 * Generates request handlers as described by the given actor specification on
 * the given actor prototype. Returns the actor prototype.
 */
var generateRequestHandlers = function(actorSpec, actorProto) {
  actorProto.typeName = actorSpec.typeName;

  // Generate request handlers for each method definition
  actorProto.requestTypes = Object.create(null);
  actorSpec.methods.forEach(spec => {
    const handler = function(packet, conn) {
      try {
        const startTime = isWorker ? null : Cu.now();
        let args;
        try {
          args = spec.request.read(packet, this);
        } catch (ex) {
          console.error("Error reading request: " + packet.type);
          throw ex;
        }

        if (!this[spec.name]) {
          throw new Error(
            `Spec for '${actorProto.typeName}' specifies a '${spec.name}'` +
              ` method that isn't implemented by the actor`
          );
        }
        const ret = this[spec.name].apply(this, args);

        const sendReturn = retToSend => {
          if (spec.oneway) {
            // No need to send a response.
            return;
          }
          if (this.isDestroyed()) {
            console.error(
              `Tried to send a '${spec.name}' method reply on an already destroyed actor` +
                ` '${this.typeName}'`
            );
            return;
          }

          let response;
          try {
            response = spec.response.write(retToSend, this);
          } catch (ex) {
            console.error("Error writing response to: " + spec.name);
            throw ex;
          }
          response.from = this.actorID;
          // If spec.release has been specified, destroy the object.
          if (spec.release) {
            try {
              this.destroy();
            } catch (e) {
              this.writeError(e, actorProto.typeName, spec.name);
              return;
            }
          }

          conn.send(response);

          ChromeUtils.addProfilerMarker(
            "DevTools:RDP Actor",
            startTime,
            `${actorSpec.typeName}:${spec.name}()`
          );
        };

        this._queueResponse(p => {
          return p
            .then(() => ret)
            .then(sendReturn)
            .catch(e => this.writeError(e, actorProto.typeName, spec.name));
        });
      } catch (e) {
        this._queueResponse(p => {
          return p.then(() =>
            this.writeError(e, actorProto.typeName, spec.name)
          );
        });
      }
    };

    actorProto.requestTypes[spec.request.type] = handler;
  });

  return actorProto;
};

/**
 * Create an actor class for the given actor specification and prototype.
 *
 * @param object actorSpec
 *    The actor specification. Must have a 'typeName' property.
 * @param object actorProto
 *    The actor prototype. Should have method definitions, can have event
 *    definitions.
 */
var ActorClassWithSpec = function(actorSpec, actorProto) {
  if (!actorSpec.typeName) {
    throw Error("Actor specification must have a typeName member.");
  }

  // Existing Actors are relying on the initialize instead of constructor methods.
  const cls = function() {
    const instance = Object.create(cls.prototype);
    instance.initialize.apply(instance, arguments);
    return instance;
  };
  cls.prototype = extend(
    Actor.prototype,
    generateRequestHandlers(actorSpec, actorProto)
  );

  actorSpecs.set(cls.prototype, actorSpec);

  return cls;
};
exports.ActorClassWithSpec = ActorClassWithSpec;
