// Custom *.sjs file specifically for the needs of Bug:
// Bug 1447784 - Implement CSP upgrade-insecure-requests directive

function handleRequest(request, response) {
  response.setHeader("Access-Control-Allow-Headers", "content-type", false);
  response.setHeader("Access-Control-Allow-Methods", "GET", false);
  response.setHeader("Access-Control-Allow-Origin", "*", false);

  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  // perform sanity check and make sure that all requests get upgraded to use https
  if (request.scheme !== "https") {
    response.write("request-not-https");
    return;
  } else {
    response.write("request-is-https");
  }

  // we should not get here, but just in case return something unexpected
  response.write("d'oh");
}
