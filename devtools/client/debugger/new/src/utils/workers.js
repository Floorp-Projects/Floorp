/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { basename } from "./path";
import type { Thread } from "../types";

export function getDisplayName(thread: Thread) {
  return basename(thread.url);
}

export function isWorker(thread: Thread) {
  return thread.actor.includes("workerTarget");
}
