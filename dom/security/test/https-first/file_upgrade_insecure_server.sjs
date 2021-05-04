// SJS file for https-first Mode mochitests
// Bug 1704454  - HTTPS First Mode

const TOTAL_EXPECTED_REQUESTS = 12;

const IFRAME_CONTENT =
  "<!DOCTYPE HTML>" +
  "<html>" +
  "<head><meta charset='utf-8'>" +
  "<title>Bug 1704454 - Test HTTPS First Mode</title>" +
  "</head>" +
  "<body>" +
  "<img src='http://example.com/tests/dom/security/test/https-first/file_upgrade_insecure_server.sjs?nested-img'></img>" +
  "</body>" +
  "</html>";

const expectedQueries = [
  "script",
  "style",
  "img",
  "iframe",
  "form",
  "xhr",
  "media",
  "object",
  "font",
  "img-redir",
  "nested-img",
  "top-level",
];

function handleRequest(request, response) {
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);
  var queryString = request.queryString;

  // initialize server variables and save the object state
  // of the initial request, which returns async once the
  // server has processed all requests.
  if (queryString == "queryresult") {
    setState("totaltests", TOTAL_EXPECTED_REQUESTS.toString());
    setState("receivedQueries", "");
    response.processAsync();
    setObjectState("queryResult", response);
    return;
  }

  // handle img redirect (https->http)
  if (queryString == "redirect-image") {
    var newLocation =
      "http://example.com/tests/dom/security/test/https-first/file_upgrade_insecure_server.sjs?img-redir";
    response.setStatusLine("1.1", 302, "Found");
    response.setHeader("Location", newLocation, false);
    return;
  }

  // just in case error handling for unexpected queries
  if (expectedQueries.indexOf(queryString) == -1) {
    response.write("unexpected-response");
    return;
  }

  // make sure all the requested queries aren't upgraded to https
  // except of toplevel requests
  if (queryString === "top-level") {
    queryString += request.scheme === "https" ? "-ok" : "-error";
  } else {
    queryString += request.scheme === "http" ? "-ok" : "-error";
  }
  var receivedQueries = getState("receivedQueries");

  // images, scripts, etc. get queried twice, do not
  // confuse the server by storing the preload as
  // well as the actual load. If either the preload
  // or the actual load is not https, then we would
  // append "-error" in the array and the test would
  // fail at the end.

  // append the result to the total query string array
  if (receivedQueries != "") {
    receivedQueries += ",";
  }
  receivedQueries += queryString;
  setState("receivedQueries", receivedQueries);

  // keep track of how many more requests the server
  // is expecting
  var totaltests = parseInt(getState("totaltests"));
  totaltests -= 1;
  setState("totaltests", totaltests.toString());

  // return content (img) for the nested iframe to test
  // that subresource requests within nested contexts
  // get upgraded as well. We also have to return
  // the iframe context in case of an error so we
  // can test both, using upgrade-insecure as well
  // as the base case of not using upgrade-insecure.
  if (queryString == "iframe-ok" || queryString == "iframe-error") {
    response.write(IFRAME_CONTENT);
  }

  // if we have received all the requests, we return
  // the result back.
  if (totaltests == 0) {
    getObjectState("queryResult", function(queryResponse) {
      if (!queryResponse) {
        return;
      }
      var receivedQueries = getState("receivedQueries");
      queryResponse.write(receivedQueries);
      queryResponse.finish();
    });
  }
}
