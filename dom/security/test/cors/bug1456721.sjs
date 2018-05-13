
function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);
  let queryStr = request.queryString;

  if (queryStr === "redirect") {
    response.setStatusLine("1.1", 302, "Found");
    response.setHeader("Location", "bug1456721.sjs?load", false);
    response.setHeader("Access-Control-Allow-Origin", "*", false);
    return;
  }

  if (queryStr === "load") {
    response.setHeader("Content-Type", "text/html", false);
    response.setHeader("Access-Control-Allow-Origin", "*", false);
    response.write("foo");
    return;
  }
  // we should never get here - return something unexpected
  response.write("d'oh");
}
