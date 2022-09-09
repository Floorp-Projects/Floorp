/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor, ActorClassWithSpec } = require("devtools/shared/protocol");
const { accessibilitySpec } = require("devtools/shared/specs/accessibility");

loader.lazyRequireGetter(
  this,
  "AccessibleWalkerActor",
  "devtools/server/actors/accessibility/walker",
  true
);
loader.lazyRequireGetter(
  this,
  "SimulatorActor",
  "devtools/server/actors/accessibility/simulator",
  true
);
loader.lazyRequireGetter(
  this,
  "isWebRenderEnabled",
  "devtools/server/actors/utils/accessibility",
  true
);

/**
 * The AccessibilityActor is a top level container actor that initializes
 * accessible walker and is the top-most point of interaction for accessibility
 * tools UI for a top level content process.
 */
const AccessibilityActor = ActorClassWithSpec(accessibilitySpec, {
  initialize(conn, targetActor) {
    Actor.prototype.initialize.call(this, conn);
    // This event is fired when accessibility service is initialized or shut
    // down. "init" and "shutdown" events are only relayed when the enabled
    // state matches the event (e.g. the event came from the same process as
    // the actor).
    Services.obs.addObserver(this, "a11y-init-or-shutdown");
    this.targetActor = targetActor;
  },

  getTraits() {
    // The traits are used to know if accessibility actors support particular
    // API on the server side.
    return {
      // @backward-compat { version 84 } Fixed on the server by Bug 1654956.
      tabbingOrder: true,
    };
  },

  bootstrap() {
    return {
      enabled: this.enabled,
    };
  },

  get enabled() {
    return Services.appinfo.accessibilityEnabled;
  },

  /**
   * Observe Accessibility service init and shutdown events. It relays these
   * events to AccessibilityFront if the event is fired for the a11y service
   * that lives in the same process.
   *
   * @param  {null} subject
   *         Not used.
   * @param  {String} topic
   *         Name of the a11y service event: "a11y-init-or-shutdown".
   * @param  {String} data
   *         "0" corresponds to shutdown and "1" to init.
   */
  observe(subject, topic, data) {
    const enabled = data === "1";
    if (enabled && this.enabled) {
      this.emit("init");
    } else if (!enabled && !this.enabled) {
      if (this.walker) {
        this.walker.reset();
      }

      this.emit("shutdown");
    }
  },

  /**
   * Get or create AccessibilityWalker actor, similar to WalkerActor.
   *
   * @return {Object}
   *         AccessibleWalkerActor for the current tab.
   */
  getWalker() {
    if (!this.walker) {
      this.walker = new AccessibleWalkerActor(this.conn, this.targetActor);
      this.manage(this.walker);
    }
    return this.walker;
  },

  /**
   * Get or create Simulator actor, managed by AccessibilityActor,
   * only if webrender is enabled. Simulator applies color filters on an entire
   * viewport. This needs to be done using webrender and not an SVG
   * <feColorMatrix> since it is accelerated and scrolling with filter applied
   * needs to be smooth (Bug1431466).
   *
   * @return {Object|null}
   *         SimulatorActor for the current tab.
   */
  getSimulator() {
    // TODO: Remove this check after Bug1570667
    if (!isWebRenderEnabled(this.targetActor.window)) {
      return null;
    }

    if (!this.simulator) {
      this.simulator = new SimulatorActor(this.conn, this.targetActor);
      this.manage(this.simulator);
    }

    return this.simulator;
  },

  /**
   * Destroy accessibility actor. This method also shutsdown accessibility
   * service if possible.
   */
  async destroy() {
    Actor.prototype.destroy.call(this);
    Services.obs.removeObserver(this, "a11y-init-or-shutdown");
    this.walker = null;
    this.targetActor = null;
  },
});

exports.AccessibilityActor = AccessibilityActor;
