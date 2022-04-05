function handleRequest(request, response) {
  if (request.queryString == "loaded") {
    response.write(getState("loaded") || "false");
    return;
  }

  setState("loaded", "true");
  response.setHeader("Content-Type", "image/svg+xml", false);
  response.write(`<svg xmlns="http://www.w3.org/2000/svg"></svg>`);
}
