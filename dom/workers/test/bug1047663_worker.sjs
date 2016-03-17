/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const WORKER_1 = `
  "use strict";

  self.onmessage = function () {
    postMessage("one");
  };
`;

const WORKER_2 = `
  "use strict";

  self.onmessage = function () {
    postMessage("two");
  };
`;

function handleRequest(request, response) {
  let count = getState("count");
  if (count === "") {
    count = "1";
  }

  // This header is necessary for the cache to trigger.
  response.setHeader("Cache-control", "max-age=3600");

  // If this is the first request, return the first source.
  if (count === "1") {
    response.write(WORKER_1);
    setState("count", "2");
  }
  // For all subsequent requests, return the second source.
  else {
    response.write(WORKER_2);
  }
}
