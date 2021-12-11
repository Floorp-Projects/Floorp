/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const {
  threadConfigurationSpec,
} = require("devtools/shared/specs/thread-configuration");

/**
 * The ThreadConfigurationFront/Actor should be used to maintain thread settings
 * sent from the client for the thread actor.
 *
 * See the ThreadConfigurationActor for a list of supported configuration options.
 */
class ThreadConfigurationFront extends FrontClassWithSpec(
  threadConfigurationSpec
) {}

registerFront(ThreadConfigurationFront);
