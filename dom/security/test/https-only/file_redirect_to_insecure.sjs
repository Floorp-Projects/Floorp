// Redirect back to http if visited via https. This way we can simulate
// a site which can not be upgraded by HTTPS-Only.

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);
  if (request.scheme === "https") {
    response.setStatusLine(request.httpVersion, "302", "Found");
    response.setHeader(
      "Location",
      // We explicitly want a insecure URL here, so disable eslint
      // eslint-disable-next-line @microsoft/sdl/no-insecure-url
      `http://${request.host}${request.path}`,
      false
    );
  }
}
