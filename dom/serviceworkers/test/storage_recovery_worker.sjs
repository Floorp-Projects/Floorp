const BASE_URI = "http://mochi.test:8888/browser/dom/serviceworkers/test/";

function handleRequest(request, response) {
  let redirect = getState("redirect");
  setState("redirect", "false");

  if (request.queryString.includes("set-redirect")) {
    setState("redirect", "true");
  } else if (request.queryString.includes("clear-redirect")) {
    setState("redirect", "false");
  }

  response.setHeader("Cache-Control", "no-store");

  if (redirect === "true") {
    response.setStatusLine(request.httpVersion, 307, "Moved Temporarily");
    response.setHeader("Location", BASE_URI + "empty.js");
    return;
  }

  response.setHeader("Content-Type", "application/javascript");
  response.write("");
}
