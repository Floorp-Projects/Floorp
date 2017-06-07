/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const WORKER_1 = `
  importScripts("bug1290951_worker_imported.sjs");
`;

const WORKER_2 = `
  // Remove "importScripts(...)" for testing purpose.
`;

function handleRequest(request, response) {
  let count = getState("count");
  if (count === "") {
    count = "1";
  }

  // This header is necessary for making this script able to be loaded.
  response.setHeader("Content-Type", "application/javascript");

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

