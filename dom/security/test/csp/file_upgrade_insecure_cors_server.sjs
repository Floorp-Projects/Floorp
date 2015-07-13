// Custom *.sjs file specifically for the needs of Bug:
// Bug 1139297 - Implement CSP upgrade-insecure-requests directive

function handleRequest(request, response)
{
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  // perform sanity check and make sure that all requests get upgraded to use https
  if (request.scheme !== "https") {
    response.write("request not https");
    return;
  }

  var queryString = request.queryString;

  // TEST 1
  if (queryString === "test1") {
    var newLocation =
      "http://test1.example.com/tests/dom/security/test/csp/file_upgrade_insecure_cors_server.sjs?redir-test1";
    response.setStatusLine("1.1", 302, "Found");
    response.setHeader("Location", newLocation, false);
    return;
  }
  if (queryString === "redir-test1") {
    response.write("test1-no-cors-ok");
    return;
  }

  // TEST 2
  if (queryString === "test2") {
    var newLocation =
      "http://test1.example.com:443/tests/dom/security/test/csp/file_upgrade_insecure_cors_server.sjs?redir-test2";
    response.setStatusLine("1.1", 302, "Found");
    response.setHeader("Location", newLocation, false);
    return;
  }
  if (queryString === "redir-test2") {
    response.write("test2-no-cors-diffport-ok");
    return;
  }

  // TEST 3
  response.setHeader("Access-Control-Allow-Headers", "content-type", false);
  response.setHeader("Access-Control-Allow-Methods", "POST, GET", false);
  response.setHeader("Access-Control-Allow-Origin", "*", false);

  if (queryString === "test3") {
    var newLocation =
      "http://test1.example.com/tests/dom/security/test/csp/file_upgrade_insecure_cors_server.sjs?redir-test3";
    response.setStatusLine("1.1", 302, "Found");
    response.setHeader("Location", newLocation, false);
    return;
  }
  if (queryString === "redir-test3") {
    response.write("test3-cors-ok");
    return;
  }

  // we should not get here, but just in case return something unexpected
  response.write("d'oh");
}
