/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor } = require("resource://devtools/shared/protocol.js");
const {
  threadConfigurationSpec,
} = require("resource://devtools/shared/specs/thread-configuration.js");

const {
  SessionDataHelpers,
} = require("resource://devtools/server/actors/watcher/SessionDataHelpers.jsm");
const {
  SUPPORTED_DATA: { THREAD_CONFIGURATION },
} = SessionDataHelpers;

// List of options supported by this thread configuration actor.
const SUPPORTED_OPTIONS = {
  // Controls pausing on debugger statement.
  // (This is enabled by default if omitted)
  shouldPauseOnDebuggerStatement: true,
  // Enable pausing on exceptions.
  pauseOnExceptions: true,
  // Disable pausing on caught exceptions.
  ignoreCaughtExceptions: true,
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
  observeWasm: true,
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
class ThreadConfigurationActor extends Actor {
  constructor(watcherActor) {
    super(watcherActor.conn, threadConfigurationSpec);
    this.watcherActor = watcherActor;
  }

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

    await this.watcherActor.addOrSetDataEntry(
      THREAD_CONFIGURATION,
      configArray,
      "add"
    );
  }
}

exports.ThreadConfigurationActor = ThreadConfigurationActor;
