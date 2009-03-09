function handleRequest(request, response)
{
  var match = request.queryString.match(/^state=(.*)$/);
  if (match)
  {
    response.setStatusLine(request.httpVersion, 204, "No content");
    setState("offline.changingManifest", match[1]);
  }

  if (request.queryString == "")
  {
    response.setStatusLine(request.httpVersion, 200, "Ok");
    response.setHeader("Content-Type", "text/cache-manifest");
    response.setHeader("Cache-Control", "no-cache");
    response.write(
      "CACHE MANIFEST\n" +
      "# v" + getState("offline.changingManifest") + "\n" +
      "changing1Hour.sjs\n" +
      "changing1Sec.sjs\n");
      
    if (getState("offline.changingManifest") != "2") {
      response.write(
        "NETWORK:\n" +
        "onwhitelist.html\n");
    }
  }
}
