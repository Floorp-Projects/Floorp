/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const WORKER = `
  onmessage = function(event) {
    fetch(event.data, {
      mode: "no-cors",
      credentials: "include"
    }).then(function() {
      postMessage("fetch done");
    });
  }
`;

function handleRequest(request, response) {
  if (request.queryString === "credentialless") {
    response.setHeader("Cross-Origin-Embedder-Policy", "credentialless", true);
  }

  response.setHeader("Content-Type", "application/javascript", false);
  response.setStatusLine(request.httpVersion, "200", "Found");
  response.write(WORKER);
}
