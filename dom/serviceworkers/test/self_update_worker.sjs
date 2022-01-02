/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const WORKER_BODY = `
onactivate = function(event) {
  let promise = clients.matchAll({includeUncontrolled: true}).then(function(clients) {
    for (i = 0; i < clients.length; i++) {
      clients[i].postMessage({version: version});
    }
  }).then(function() {
    return self.registration.update();
  });
  event.waitUntil(promise);
};
`;

function handleRequest(request, response) {
  if (request.queryString == "clearcounter") {
    setState("count", "1");
    response.write("ok");
    return;
  }

  let count = getState("count");
  if (count === "") {
    count = 1;
  } else {
    count = parseInt(count);
  }

  let worker = "var version = " + count + ";\n";
  worker = worker + WORKER_BODY;

  // This header is necessary for making this script able to be loaded.
  response.setHeader("Content-Type", "application/javascript");

  // If this is the first request, return the first source.
  response.write(worker);
  setState("count", "" + (count + 1));
}
