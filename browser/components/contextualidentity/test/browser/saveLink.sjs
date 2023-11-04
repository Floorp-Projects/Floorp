const HTTP_ORIGIN = "http://example.com";
const HTTPS_ORIGIN = "https://example.com";
const URI_PATH =
  "/browser/browser/components/contextualidentity/test/browser/saveLink.sjs";

function handleRequest(aRequest, aResponse) {
  var params = new URLSearchParams(aRequest.queryString);

  // This is the first request, where we set the cookie.
  if (params.has("UCI")) {
    aResponse.setStatusLine(aRequest.httpVersion, 200);
    aResponse.setHeader("Set-Cookie", "UCI=" + params.get("UCI"));
    aResponse.write(
      "<html><body><a href='" +
        HTTPS_ORIGIN +
        URI_PATH +
        "?redirect=1' id='fff'>this is a link</a></body></html>"
    );
    return;
  }

  // Second request. This is the save-as content, but we make a redirect to see
  // if we are able to follow it.
  if (params.has("redirect")) {
    aResponse.setStatusLine(aRequest.httpVersion, 302, "Found");
    aResponse.setHeader(
      "Location",
      HTTP_ORIGIN + URI_PATH + "?download=1",
      false
    );
    aResponse.write("Redirect!");
    return;
  }

  // This is the 3rd request where we offer the content to be saved.
  if (params.has("download")) {
    setState("downloadUCI", aRequest.getHeader("Cookie"));
    aResponse.setStatusLine(aRequest.httpVersion, 200);
    aResponse.write("All Good!");
    return;
  }

  // This is the last request to check that the download happened with the correct cookie
  if (params.has("result")) {
    aResponse.setStatusLine(aRequest.httpVersion, 200);
    aResponse.write("Result:" + getState("downloadUCI"));
    return;
  }

  // We should not be here!
  aResponse.setStatusLine(aRequest.httpVersion, 500);
  aResponse.write("ERROR!!!");
}
