function handleRequest(request, response) {
  if (request.queryString == "reset") {
    // Reset the internal state.
    setState("redirected", "");

    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/plain", false);
    const body = "reset";
    response.bodyOutputStream.write(body, body.length);
  } else if (
    request.queryString == "redirect-cache" ||
    request.queryString == "redirect-nocache"
  ) {
    // Redirect to different CSS between the first and the second requests.

    response.setStatusLine(request.httpVersion, 301, "Moved Permanently");
    if (getState("redirected")) {
      response.setHeader(
        "Location",
        "file_resource_cache_server.sjs?blue",
        false
      );
    } else {
      response.setHeader(
        "Location",
        "file_resource_cache_server.sjs?red",
        false
      );
    }

    if (request.queryString == "redirect-nocache") {
      response.setHeader("Cache-Control", "no-cache", false);
    } else {
      response.setHeader("Cache-Control", "max-age=10000", false);
    }

    setState("redirected", "1");
  } else if (request.queryString == "blue") {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Cache-Control", "max-age=10000", false);
    response.setHeader("Content-Type", "text/css", false);
    const body = `body { color: blue; }`;
    response.bodyOutputStream.write(body, body.length);
  } else {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Cache-Control", "max-age=10000", false);
    response.setHeader("Content-Type", "text/css", false);
    const body = `body { color: red; }`;
    response.bodyOutputStream.write(body, body.length);
  }
}
