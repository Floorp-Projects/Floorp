/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Bug 1593937 made this code only used to support FF76- and can be removed
// once FF77 reach release channel.
class LegacyFramesWatcher {
  constructor(targetList, onTargetAvailable) {
    this.targetList = targetList;
    this.rootFront = targetList.rootFront;
    this.target = targetList.targetFront;

    this.onTargetAvailable = onTargetAvailable;
  }

  async listen() {
    // Note that even if we are calling listRemoteFrames on `this.target`, this ends up
    // being forwarded to the RootFront. So that the Descriptors are managed
    // by RootFront.
    // TODO: support frame listening. For now, this only fetches already existing targets
    const { frames } = await this.target.listRemoteFrames();

    const promises = frames
      .filter(
        // As we listen for frameDescriptor's on the RootFront, we get
        // all the frames and not only the one related to the given `target`.
        // TODO: support deeply nested frames
        descriptor =>
          descriptor.parentID == this.target.browsingContextID ||
          descriptor.id == this.target.browsingContextID
      )
      .map(async descriptor => {
        const target = await descriptor.getTarget();
        if (!target) {
          console.error(
            "Wasn't able to retrieve the target for",
            descriptor.actorID
          );
          return;
        }
        await this.onTargetAvailable(target);
      });

    await Promise.all(promises);
  }

  unlisten() {}
}

module.exports = { LegacyFramesWatcher };
