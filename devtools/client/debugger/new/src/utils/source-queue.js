/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { throttle } from "lodash";
import type { Source } from "../types";

let newSources;
let queuedSources;
let currentWork;

async function dispatchNewSources() {
  const sources = queuedSources;
  queuedSources = [];
  currentWork = await newSources(sources);
}

const queue = throttle(dispatchNewSources, 100);

export default {
  initialize: (actions: Object) => {
    newSources = actions.newSources;
    queuedSources = [];
  },
  queue: (source: Source) => {
    queuedSources.push(source);
    queue();
  },
  queueSources: (sources: Source[]) => {
    queuedSources = queuedSources.concat(sources);
    queue();
  },

  flush: () => Promise.all([queue.flush(), currentWork]),
  clear: () => queue.cancel()
};
