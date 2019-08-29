/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor, ActorClassWithSpec } = require("devtools/shared/protocol");
const { simulatorSpec } = require("devtools/shared/specs/accessibility");
const {
  simulation: { COLOR_TRANSFORMATION_MATRICES },
} = require("./constants");

/**
 * The SimulatorActor is responsible for setting color matrices
 * based on the simulation type specified.
 */
const SimulatorActor = ActorClassWithSpec(simulatorSpec, {
  initialize(conn, targetActor) {
    Actor.prototype.initialize.call(this, conn);
    this.targetActor = targetActor;
  },

  /**
   * Simulates a type of visual impairment (i.e. color blindness or contrast loss).
   *
   * @param  {Object} options
   *         Properties: {Array} types
   *                     Contains the types of visual impairment(s) to be simulated.
   *                     Set default color matrix if array is empty.
   * @return {Boolean}
   *         True if matrix was successfully applied, false otherwise.
   */
  simulate(options) {
    if (options.types.length > 1) {
      return false;
    }

    return this.setColorMatrix(
      COLOR_TRANSFORMATION_MATRICES[
        options.types.length === 1 ? options.types[0] : "NONE"
      ]
    );
  },

  setColorMatrix(colorMatrix) {
    if (!this.docShell) {
      return false;
    }

    try {
      this.docShell.setColorMatrix(colorMatrix);
    } catch (error) {
      return false;
    }

    return true;
  },

  /**
   * Disables all simulations by setting the default color matrix.
   */
  disable() {
    this.simulate({ types: [] });
  },

  destroy() {
    Actor.prototype.destroy.call(this);

    this.disable();
    this.targetActor = null;
  },

  get docShell() {
    return this.targetActor && this.targetActor.docShell;
  },
});

exports.SimulatorActor = SimulatorActor;
