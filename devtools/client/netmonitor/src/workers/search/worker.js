/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

/* import-globals-from search.js */
/* import-globals-from ../../../../shared/worker-utils.js */
importScripts(
  "resource://devtools/client/netmonitor/src/workers/search/search.js",
  "resource://devtools/client/shared/worker-utils.js"
);

// Implementation of search worker (runs in worker scope).
self.onmessage = workerHandler({ searchInResource });
