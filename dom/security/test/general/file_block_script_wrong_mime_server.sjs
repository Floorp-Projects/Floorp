// Custom *.sjs specifically for the needs of:
// Bug 1288361 - Block scripts with wrong MIME type

"use strict";
Components.utils.importGlobalProperties(["URLSearchParams"]);

const WORKER = `
  onmessage = function(event) {
    postMessage("worker-loaded");
  };`;

function handleRequest(request, response) {
  const query = new URLSearchParams(request.queryString);

  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  // Set MIME type
  response.setHeader("Content-Type", query.get("mime"), false);

  // Deliver response
  switch (query.get("type")) {
    case "script":
      response.write("");
      break;
    case "worker":
      response.write(WORKER);
      break;
    case "worker-import":
      response.write(`importScripts("file_block_script_wrong_mime_server.sjs?type=script&mime=${query.get("mime")}");`);
      response.write(WORKER);
      break;
  }
}
