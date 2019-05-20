/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { throttle } from "lodash";
import type { QueuedSourceData } from "../types";

let newQueuedSources;
let queuedSources;
let currentWork;

async function dispatchNewSources() {
  const sources = queuedSources;
  queuedSources = [];
  currentWork = await newQueuedSources(sources);
}

const queue = throttle(dispatchNewSources, 100);

export default {
  initialize: (actions: Object) => {
    newQueuedSources = actions.newQueuedSources;
    queuedSources = [];
  },
  queue: (source: QueuedSourceData) => {
    queuedSources.push(source);
    queue();
  },
  queueSources: (sources: QueuedSourceData[]) => {
    if (sources.length > 0) {
      queuedSources = queuedSources.concat(sources);
      queue();
    }
  },

  flush: () => Promise.all([queue.flush(), currentWork]),
  clear: () => {
    queuedSources = [];
    queue.cancel();
  },
};
