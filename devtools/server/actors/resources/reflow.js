/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  TYPES: { REFLOW },
} = require("resource://devtools/server/actors/resources/index.js");
const Targets = require("resource://devtools/server/actors/targets/index.js");

const {
  getLayoutChangesObserver,
  releaseLayoutChangesObserver,
} = require("resource://devtools/server/actors/reflow.js");

class ReflowWatcher {
  /**
   * Start watching for reflows related to a given Target Actor.
   *
   * @param TargetActor targetActor
   *        The target actor from which we should observe reflows
   * @param Object options
   *        Dictionary object with following attributes:
   *        - onAvailable: mandatory function
   *          This will be called for each resource.
   */
  async watch(targetActor, { onAvailable }) {
    // Only track reflow for non-ParentProcess FRAME targets
    if (
      targetActor.targetType !== Targets.TYPES.FRAME ||
      targetActor.typeName === "parentProcessTarget"
    ) {
      return;
    }

    this._targetActor = targetActor;

    const onReflows = reflows => {
      onAvailable([
        {
          resourceType: REFLOW,
          reflows,
        },
      ]);
    };

    this._observer = getLayoutChangesObserver(targetActor);
    this._offReflows = this._observer.on("reflows", onReflows);
    this._observer.start();
  }

  destroy() {
    releaseLayoutChangesObserver(this._targetActor);

    if (this._offReflows) {
      this._offReflows();
      this._offReflows = null;
    }
  }
}

module.exports = ReflowWatcher;
