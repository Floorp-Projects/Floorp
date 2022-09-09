/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { settleAll } = require("devtools/shared/DevToolsUtils");
var EventEmitter = require("devtools/shared/event-emitter");

var { Pool } = require("devtools/shared/protocol/Pool");
var {
  getStack,
  callFunctionWithAsyncStack,
} = require("devtools/shared/platform/stack");
const defer = require("devtools/shared/defer");

/**
 * Base class for client-side actor fronts.
 *
 * @param [DevToolsClient|null] conn
 *   The conn must either be DevToolsClient or null. Must have
 *   addActorPool, removeActorPool, and poolFor.
 *   conn can be null if the subclass provides a conn property.
 * @param [Target|null] target
 *   If we are instantiating a target-scoped front, this is a reference to the front's
 *   Target instance, otherwise this is null.
 * @param [Front|null] parentFront
 *   The parent front. This is only available if the Front being initialized is a child
 *   of a parent front.
 * @constructor
 */
class Front extends Pool {
  constructor(conn = null, targetFront = null, parentFront = null) {
    super(conn);
    if (!conn) {
      throw new Error("Front without conn");
    }
    this.actorID = null;
    // The targetFront attribute represents the debuggable context. Only target-scoped
    // fronts and their children fronts will have the targetFront attribute set.
    this.targetFront = targetFront;
    // The parentFront attribute points to its parent front. Only children of
    // target-scoped fronts will have the parentFront attribute set.
    this.parentFront = parentFront;
    this._requests = [];

    // Front listener functions registered via `watchFronts`
    this._frontCreationListeners = null;
    this._frontDestructionListeners = null;

    // List of optional listener for each event, that is processed immediatly on packet
    // receival, before emitting event via EventEmitter on the Front.
    // These listeners are register via Front.before function.
    // Map(Event Name[string] => Event Listener[function])
    this._beforeListeners = new Map();

    // This flag allows to check if the `initialize` method has resolved.
    // Used to avoid notifying about initialized fronts in `watchFronts`.
    this._initializeResolved = false;
  }

  /**
   * Return the parent front.
   */
  getParent() {
    return this.parentFront && this.parentFront.actorID
      ? this.parentFront
      : null;
  }

  destroy() {
    // Prevent destroying twice if a `forwardCancelling` event has already been received
    // and already called `baseFrontClassDestroy`
    this.baseFrontClassDestroy();

    // Keep `clearEvents` out of baseFrontClassDestroy as we still want TargetMixin to be
    // able to emit `target-destroyed` after we called baseFrontClassDestroy from DevToolsClient.purgeRequests.
    this.clearEvents();
  }

  // This method is also called from `DevToolsClient`, when a connector is destroyed
  // and we should:
  // - reject all pending request made to the remote process/target/thread.
  // - avoid trying to do new request against this remote context.
  // - unmanage this front, so that DevToolsClient.getFront no longer returns it.
  //
  // When a connector is destroyed a `forwardCancelling` RDP event is sent by the server.
  // This is done in a distinct method from `destroy` in order to do all that immediately,
  // even if `Front.destroy` is overloaded by an async method.
  baseFrontClassDestroy() {
    // Reject all outstanding requests, they won't make sense after
    // the front is destroyed.
    while (this._requests.length) {
      const { deferred, to, type, stack } = this._requests.shift();
      // Note: many tests are ignoring `Connection closed` promise rejections,
      // via PromiseTestUtils.allowMatchingRejectionsGlobally.
      // Do not update the message without updating the tests.
      const msg =
        "Connection closed, pending request to " +
        to +
        ", type " +
        type +
        " failed" +
        "\n\nRequest stack:\n" +
        stack.formattedStack;
      deferred.reject(new Error(msg));
    }

    if (this.actorID) {
      super.destroy();
      this.actorID = null;
    }
    this._isDestroyed = true;

    this.targetFront = null;
    this.parentFront = null;
    this._frontCreationListeners = null;
    this._frontDestructionListeners = null;
    this._beforeListeners = null;
  }

  async manage(front, form, ctx) {
    if (!front.actorID) {
      throw new Error(
        "Can't manage front without an actor ID.\n" +
          "Ensure server supports " +
          front.typeName +
          "."
      );
    }

    if (front.parentFront && front.parentFront !== this) {
      throw new Error(
        `${this.actorID} (${this.typeName}) can't manage ${front.actorID}
        (${front.typeName}) since it has a different parentFront ${
          front.parentFront
            ? front.parentFront.actorID + "(" + front.parentFront.typeName + ")"
            : "<no parentFront>"
        }`
      );
    }

    super.manage(front);

    if (typeof front.initialize == "function") {
      await front.initialize();
    }
    front._initializeResolved = true;

    // Ensure calling form() *before* notifying about this front being just created.
    // We exprect the front to be fully initialized, especially via its form attributes.
    // But do that *after* calling manage() so that the front is already registered
    // in Pools and can be fetched by its ID, in case a child actor, created in form()
    // tries to get a reference to its parent via the actor ID.
    if (form) {
      front.form(form, ctx);
    }

    // Call listeners registered via `watchFronts` method
    // (ignore if this front has been destroyed)
    if (this._frontCreationListeners) {
      this._frontCreationListeners.emit(front.typeName, front);
    }
  }

  async unmanage(front) {
    super.unmanage(front);

    // Call listeners registered via `watchFronts` method
    if (this._frontDestructionListeners) {
      this._frontDestructionListeners.emit(front.typeName, front);
    }
  }

  /*
   * Listen for the creation and/or destruction of fronts matching one of the provided types.
   *
   * @param {String} typeName
   *        Actor type to watch.
   * @param {Function} onAvailable (optional)
   *        Callback fired when a front has been just created or was already available.
   *        The function is called with one arguments, the front.
   * @param {Function} onDestroy (optional)
   *        Callback fired in case of front destruction.
   *        The function is called with the same argument than onAvailable.
   */
  watchFronts(typeName, onAvailable, onDestroy) {
    if (this.isDestroyed()) {
      // The front was already destroyed, bail out.
      console.error(
        `Tried to call watchFronts for the '${typeName}' type on an ` +
          `already destroyed front '${this.typeName}'.`
      );
      return;
    }

    if (onAvailable) {
      // First fire the callback on fronts with the correct type and which have
      // been initialized. If initialize() is still in progress, the front will
      // be emitted via _frontCreationListeners shortly after.
      for (const front of this.poolChildren()) {
        if (front.typeName == typeName && front._initializeResolved) {
          onAvailable(front);
        }
      }

      if (!this._frontCreationListeners) {
        this._frontCreationListeners = new EventEmitter();
      }
      // Then register the callback for fronts instantiated in the future
      this._frontCreationListeners.on(typeName, onAvailable);
    }

    if (onDestroy) {
      if (!this._frontDestructionListeners) {
        this._frontDestructionListeners = new EventEmitter();
      }
      this._frontDestructionListeners.on(typeName, onDestroy);
    }
  }

  /**
   * Stop listening for the creation and/or destruction of a given type of fronts.
   * See `watchFronts()` for documentation of the arguments.
   */
  unwatchFronts(typeName, onAvailable, onDestroy) {
    if (this.isDestroyed()) {
      // The front was already destroyed, bail out.
      console.error(
        `Tried to call unwatchFronts for the '${typeName}' type on an ` +
          `already destroyed front '${this.typeName}'.`
      );
      return;
    }

    if (onAvailable && this._frontCreationListeners) {
      this._frontCreationListeners.off(typeName, onAvailable);
    }
    if (onDestroy && this._frontDestructionListeners) {
      this._frontDestructionListeners.off(typeName, onDestroy);
    }
  }

  /**
   * Register an event listener that will be called immediately on packer receival.
   * The given callback is going to be called before emitting the event via EventEmitter
   * API on the Front. Event emitting will be delayed if the callback is async.
   * Only one such listener can be registered per type of event.
   *
   * @param String type
   *   Event emitted by the actor to intercept.
   * @param Function callback
   *   Function that will process the event.
   */
  before(type, callback) {
    if (this._beforeListeners.has(type)) {
      throw new Error(
        `Can't register multiple before listeners for "${type}".`
      );
    }
    this._beforeListeners.set(type, callback);
  }

  toString() {
    return "[Front for " + this.typeName + "/" + this.actorID + "]";
  }

  /**
   * Update the actor from its representation.
   * Subclasses should override this.
   */
  form(form) {}

  /**
   * Send a packet on the connection.
   */
  send(packet) {
    if (packet.to) {
      this.conn._transport.send(packet);
    } else {
      packet.to = this.actorID;
      // The connection might be closed during the promise resolution
      if (this.conn && this.conn._transport) {
        this.conn._transport.send(packet);
      }
    }
  }

  /**
   * Send a two-way request on the connection.
   */
  request(packet) {
    const deferred = defer();
    // Save packet basics for debugging
    const { to, type } = packet;
    this._requests.push({
      deferred,
      to: to || this.actorID,
      type,
      stack: getStack(),
    });
    this.send(packet);
    return deferred.promise;
  }

  /**
   * Handler for incoming packets from the client's actor.
   */
  onPacket(packet) {
    if (this.isDestroyed()) {
      // If the Front was already destroyed, all the requests have been purged
      // and rejected with detailed error messages in baseFrontClassDestroy.
      return;
    }

    // Pick off event packets
    const type = packet.type || undefined;
    if (this._clientSpec.events && this._clientSpec.events.has(type)) {
      const event = this._clientSpec.events.get(packet.type);
      let args;
      try {
        args = event.request.read(packet, this);
      } catch (ex) {
        console.error("Error reading event: " + packet.type);
        console.exception(ex);
        throw ex;
      }
      // Check for "pre event" callback to be processed before emitting events on fronts
      // Use event.name instead of packet.type to use specific event name instead of RDP
      // packet's type.
      const beforeEvent = this._beforeListeners.get(event.name);
      if (beforeEvent) {
        const result = beforeEvent.apply(this, args);
        // Check to see if the beforeEvent returned a promise -- if so,
        // wait for their resolution before emitting. Otherwise, emit synchronously.
        if (result && typeof result.then == "function") {
          result.then(() => {
            super.emit(event.name, ...args);
            ChromeUtils.addProfilerMarker(
              "DevTools:RDP Front",
              null,
              `${this.typeName}.${event.name}`
            );
          });
          return;
        }
      }

      super.emit(event.name, ...args);
      ChromeUtils.addProfilerMarker(
        "DevTools:RDP Front",
        null,
        `${this.typeName}.${event.name}`
      );
      return;
    }

    // Remaining packets must be responses.
    if (this._requests.length === 0) {
      const msg =
        "Unexpected packet " + this.actorID + ", " + JSON.stringify(packet);
      const err = Error(msg);
      console.error(err);
      throw err;
    }

    const { deferred, stack } = this._requests.shift();
    callFunctionWithAsyncStack(
      () => {
        if (packet.error) {
          let message;
          if (packet.error && packet.message) {
            message =
              "Protocol error (" + packet.error + "): " + packet.message;
          } else {
            message = packet.error;
          }
          message += " from: " + this.actorID;
          if (packet.fileName) {
            const { fileName, columnNumber, lineNumber } = packet;
            message += ` (${fileName}:${lineNumber}:${columnNumber})`;
          }
          const packetError = new Error(message);
          deferred.reject(packetError);
        } else {
          deferred.resolve(packet);
        }
      },
      stack,
      "DevTools RDP"
    );
  }

  hasRequests() {
    return !!this._requests.length;
  }

  /**
   * Wait for all current requests from this front to settle.  This is especially useful
   * for tests and other utility environments that may not have events or mechanisms to
   * await the completion of requests without this utility.
   *
   * @return Promise
   *         Resolved when all requests have settled.
   */
  waitForRequestsToSettle() {
    return settleAll(this._requests.map(({ deferred }) => deferred.promise));
  }
}

exports.Front = Front;
