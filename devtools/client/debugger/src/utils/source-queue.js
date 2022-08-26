/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { throttle } = require("devtools/shared/throttle");

// This SourceQueue module is now only used for source mapped sources
let newOriginalQueuedSources;
let queuedOriginalSources;
let currentWork;

async function dispatchNewSources() {
  const sources = queuedOriginalSources;
  queuedOriginalSources = [];
  currentWork = await newOriginalQueuedSources(sources);
}

const queue = throttle(dispatchNewSources, 100);

export default {
  initialize: actions => {
    newOriginalQueuedSources = actions.newOriginalSources;
    queuedOriginalSources = [];
  },
  queueOriginalSources: sources => {
    if (sources.length) {
      queuedOriginalSources = queuedOriginalSources.concat(sources);
      queue();
    }
  },

  flush: () => Promise.all([queue.flush(), currentWork]),
  clear: () => {
    queuedOriginalSources = [];
    queue.cancel();
  },
};
