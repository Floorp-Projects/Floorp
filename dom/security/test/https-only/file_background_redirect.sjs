// Custom *.sjs file specifically for the needs of Bug 1683015
"use strict";

Cu.import("resource://gre/modules/Timer.jsm");

async function handleRequest(request, response) {
  // avoid confusing cache behaviour
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);

  let query = request.queryString;
  if (request.scheme === "https" && query === "start") {
    // Simulating long repsonse time by processing the https request
    // using a 5 seconds delay. Note that the http background request
    // gets send after 3 seconds
    response.processAsync();
    setTimeout(() => {
      response.setStatusLine("1.1", 200, "OK");
      response.write("<html><body>Test Page for Bug 1683015 loaded</body></html>");
      response.finish();
    }, 5000); /* wait 5 seconds */
    return;
  }

  // we should never get here, but just in case return something unexpected
  response.setStatusLine("1.1", 404, "Not Found");
  response.write("<html><body>SHOULDN'T DISPLAY THIS PAGE</body></html>");
}
