// Custom *.sjs specifically for the needs of:
// Bug 1288361 - Block scripts with wrong MIME type

"use strict";
Components.utils.importGlobalProperties(["URLSearchParams"]);

// Please note that SCRIPT is loaded for both, workers and <script src=""> where
// for the latter the contents of the script does not really matter since the
// test is listening on the onload() and onerrer() event handlers.
const SCRIPT = `
  onmessage = function(event) {
    postMessage("worker-loaded");
  };`;

function handleRequest(request, response) {
  const query = new URLSearchParams(request.queryString);

  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  if (query.has("scriptMIMEScript")) {
    response.setHeader("Content-Type", "  appLIcATion/jAvaSCRiPt  ", false);
    response.write(SCRIPT);
    return;
  }

  if (query.has("scriptMIMEImage")) {
    response.setHeader("Content-Type", "  iMAGe/jPG  ", false);
    response.write(SCRIPT);
    return;
  }

  // we should never get here, but just in case
  response.setHeader("Content-Type", "text/html", false);
  response.write("do'h");
}
