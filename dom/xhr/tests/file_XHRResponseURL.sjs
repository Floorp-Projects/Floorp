function handleRequest(aRequest, aResponse)
{
  var url = aRequest.queryString.match(/\burl=([^#&]*)/);
  if (!url) {
    aResponse.setStatusLine(aRequest.httpVersion, 404, "Not Found");
    return;
  }
  url = decodeURIComponent(url[1]);

  aResponse.setStatusLine(aRequest.httpVersion, 302, "Found");
  aResponse.setHeader("Access-Control-Allow-Origin", "*", false);
  aResponse.setHeader("Cache-Control", "no-cache", false);
  aResponse.setHeader("Location", url, false);
}
