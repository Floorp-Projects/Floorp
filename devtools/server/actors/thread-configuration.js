/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ActorClassWithSpec, Actor } = require("devtools/shared/protocol");
const {
  threadConfigurationSpec,
} = require("devtools/shared/specs/thread-configuration");
const {
  WatchedDataHelpers,
} = require("devtools/server/actors/watcher/WatchedDataHelpers.jsm");
const {
  SUPPORTED_DATA: { THREAD_CONFIGURATION },
} = WatchedDataHelpers;

// List of options supported by this thread configuration actor.
const SUPPORTED_OPTIONS = {
  // Enable pausing on exceptions.
  pauseOnExceptions: true,
  // Disable pausing on caught exceptions.
  ignoreCaughtExceptions: true,
  // Shows the pause overlay.
  shouldShowOverlay: true,
  // Include previously saved stack frames when paused.
  shouldIncludeSavedFrames: true,
  // Include async stack frames when paused.
  shouldIncludeAsyncLiveFrames: true,
  // Stop pausing on breakpoints.
  skipBreakpoints: true,
  // Log the event break points.
  logEventBreakpoints: true,
  // Enable debugging asm & wasm.
  // See https://searchfox.org/mozilla-central/source/js/src/doc/Debugger/Debugger.md#16-26
  observeAsmJS: true,
  // Should pause all the workers untill thread has attached.
  pauseWorkersUntilAttach: true,
};

/**
 * This actor manages the configuration options which apply to thread actor for all the targets.
 *
 * Configuration options should be applied to all concerned targets when the
 * configuration is updated, and new targets should also be able to read the
 * flags when they are created. The flags will be forwarded to the WatcherActor
 * and stored as THREAD_CONFIGURATION data entries.
 *
 * @constructor
 *
 */
const ThreadConfigurationActor = ActorClassWithSpec(threadConfigurationSpec, {
  initialize(watcherActor) {
    this.watcherActor = watcherActor;
    Actor.prototype.initialize.call(this, this.watcherActor.conn);
  },

  async updateConfiguration(configuration) {
    const configArray = Object.keys(configuration)
      .filter(key => {
        if (!SUPPORTED_OPTIONS[key]) {
          console.warn(`Unsupported option for ThreadConfiguration: ${key}`);
          return false;
        }
        return true;
      })
      .map(key => ({ key, value: configuration[key] }));

    await this.watcherActor.addDataEntry(THREAD_CONFIGURATION, configArray);
  },
});

exports.ThreadConfigurationActor = ThreadConfigurationActor;
