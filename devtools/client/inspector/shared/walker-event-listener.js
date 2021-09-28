/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * WalkerEventListener provides a mechanism to listen the walker event of the inspector
 * while reflecting the updating of TargetCommand.
 */
class WalkerEventListener {
  /**
   * @param {Inspector} - inspector
   * @param {Object} - listenerMap
   *        The structure of listenerMap should be as follows.
   *        {
   *          walkerEventName1: eventHandler1,
   *          walkerEventName2: eventHandler2,
   *          ...
   *        }
   */
  constructor(inspector, listenerMap) {
    this._inspector = inspector;
    this._listenerMap = listenerMap;
    this._onTargetAvailable = this._onTargetAvailable.bind(this);
    this._onTargetDestroyed = this._onTargetDestroyed.bind(this);

    this._init();
  }

  /**
   * Clean up function.
   */
  destroy() {
    this._inspector.commands.targetCommand.unwatchTargets(
      [this._inspector.commands.targetCommand.TYPES.FRAME],
      this._onTargetAvailable,
      this._onTargetDestroyed
    );

    const targets = this._inspector.commands.targetCommand.getAllTargets([
      this._inspector.commands.targetCommand.TYPES.FRAME,
    ]);
    for (const targetFront of targets) {
      this._onTargetDestroyed({
        targetFront,
      });
    }

    this._inspector = null;
    this._listenerMap = null;
  }

  _init() {
    this._inspector.commands.targetCommand.watchTargets(
      [this._inspector.commands.targetCommand.TYPES.FRAME],
      this._onTargetAvailable,
      this._onTargetDestroyed
    );
  }

  async _onTargetAvailable({ targetFront }) {
    const inspectorFront = await targetFront.getFront("inspector");
    const { walker } = inspectorFront;
    for (const [name, listener] of Object.entries(this._listenerMap)) {
      walker.on(name, listener);
    }
  }

  _onTargetDestroyed({ targetFront }) {
    const inspectorFront = targetFront.getCachedFront("inspector");
    if (inspectorFront) {
      const { walker } = inspectorFront;
      for (const [name, listener] of Object.entries(this._listenerMap)) {
        walker.off(name, listener);
      }
    }
  }
}

module.exports = WalkerEventListener;
