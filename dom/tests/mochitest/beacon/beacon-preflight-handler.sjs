function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache, must-revalidate", false);

  if (request.queryString === "verify") {
    var preflightState = getState("preflight");
    response.write(preflightState === "done" ? "green" : "red");
    return;
  }

  var originHeader = request.getHeader("origin");
  response.setHeader("Access-Control-Allow-Headers", "content-type", false);
  response.setHeader("Access-Control-Allow-Methods", "POST, GET", false);
  response.setHeader("Access-Control-Allow-Origin", originHeader, false);
  response.setHeader("Access-Control-Allow-Credentials", "true", false);

  if (request.queryString === "beacon") {
    if (request.method == "OPTIONS") {
      setState("preflight", "done");
      response.setStatusLine(null, 200, "OK");
      return;
    }
    response.setStatusLine(null, 200, "OK");
    response.write("DONE");
    return;
  }

  if (request.queryString === "fail") {
    if (request.method == "OPTIONS") {
      setState("preflight", "done");
      response.setStatusLine(null, 400, "Bad Request");
      return;
    }
    setState("preflight", "oops");
    response.setStatusLine(null, 200, "OK");
    response.write("DONE");
  }
}
