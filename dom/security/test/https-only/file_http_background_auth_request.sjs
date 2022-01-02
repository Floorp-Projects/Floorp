// Custom *.sjs file specifically for the needs of Bug 1665062

function handleRequest(request, response) {
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  if (request.scheme === "https") {
    response.setHeader("Content-Type", "text/html;charset=utf-8", false);
    response.setStatusLine(request.httpVersion, 401, "Unauthorized");
    response.setHeader("WWW-Authenticate", 'Basic realm="bug1665062"');
    return;
  }

  // we should never get here; just in case, return something unexpected
  response.write("do'h");
}
