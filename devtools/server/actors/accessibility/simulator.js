/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor } = require("resource://devtools/shared/protocol.js");
const {
  simulatorSpec,
} = require("resource://devtools/shared/specs/accessibility.js");

const {
  simulation: { COLOR_TRANSFORMATION_MATRICES },
} = require("resource://devtools/server/actors/accessibility/constants.js");

/**
 * The SimulatorActor is responsible for setting color matrices
 * based on the simulation type specified.
 */
class SimulatorActor extends Actor {
  constructor(conn, targetActor) {
    super(conn, simulatorSpec);
    this.targetActor = targetActor;
  }

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
  }

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
  }

  /**
   * Disables all simulations by setting the default color matrix.
   */
  disable() {
    this.simulate({ types: [] });
  }

  destroy() {
    super.destroy();

    this.disable();
    this.targetActor = null;
  }

  get docShell() {
    return this.targetActor && this.targetActor.docShell;
  }
}

exports.SimulatorActor = SimulatorActor;
