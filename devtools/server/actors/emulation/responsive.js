/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor } = require("resource://devtools/shared/protocol.js");
const {
  responsiveSpec,
} = require("resource://devtools/shared/specs/responsive.js");

/**
 * This actor overrides various browser features to simulate different environments to
 * test how pages perform under various conditions.
 *
 * The design below, which saves the previous value of each property before setting, is
 * needed because it's possible to have multiple copies of this actor for a single page.
 * When some instance of this actor changes a property, we want it to be able to restore
 * that property to the way it was found before the change.
 *
 * A subtle aspect of the code below is that all get* methods must return non-undefined
 * values, so that the absence of a previous value can be distinguished from the value for
 * "no override" for each of the properties.
 */
class ResponsiveActor extends Actor {
  constructor(conn, targetActor) {
    super(conn, responsiveSpec);
    this.targetActor = targetActor;
    this.docShell = targetActor.docShell;
  }

  destroy() {
    this.targetActor = null;
    this.docShell = null;

    super.destroy();
  }

  get win() {
    return this.docShell.chromeEventHandler.ownerGlobal;
  }

  /* Touch events override */

  _previousTouchEventsOverride = undefined;

  /**
   * Set the current element picker state.
   *
   * True means the element picker is currently active and we should not be emulating
   * touch events.
   * False means the element picker is not active and it is ok to emulate touch events.
   *
   * This actor method is meant to be called by the DevTools front-end. The reason for
   * this is the following:
   * RDM is the only current consumer of the touch simulator. RDM instantiates this actor
   * on its own, whether or not the Toolbox is opened. That means it does so in its own
   * DevTools Server instance.
   * When the Toolbox is running, it uses a different DevToolsServer. Therefore, it is not
   * possible for the touch simulator to know whether the picker is active or not. This
   * state has to be sent by the client code of the Toolbox to this actor.
   * If a future use case arises where we want to use the touch simulator from the Toolbox
   * too, then we could add code in here to detect the picker mode as described in
   * https://bugzilla.mozilla.org/show_bug.cgi?id=1409085#c3

   * @param {Boolean} state
   * @param {String} pickerType
   */
  setElementPickerState(state, pickerType) {
    this.targetActor.touchSimulator.setElementPickerState(state, pickerType);
  }

  /**
   * Dispatches an "orientationchange" event.
   */
  async dispatchOrientationChangeEvent() {
    const { CustomEvent } = this.win;
    const orientationChangeEvent = new CustomEvent("orientationchange");
    this.win.dispatchEvent(orientationChangeEvent);
  }
}

exports.ResponsiveActor = ResponsiveActor;
