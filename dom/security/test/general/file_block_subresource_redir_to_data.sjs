"use strict";

let SCRIPT_DATA = "alert('this alert should be blocked');";
let WORKER_DATA = "onmessage = function(event) { postMessage('worker-loaded'); }";

function handleRequest(request, response)
{
  const query = request.queryString;

  response.setHeader("Cache-Control", "no-cache", false);
  response.setStatusLine("1.1", 302, "Found");

  if (query === "script" || query === "modulescript") {
    response.setHeader("Location", "data:text/javascript," + escape(SCRIPT_DATA), false);
    return;
  }

  if (query === "worker") {
    response.setHeader("Location", "data:text/javascript," + escape(WORKER_DATA), false);
    return;
  }

  // we should never get here; just in case return something unexpected
  response.write("do'h");
}
