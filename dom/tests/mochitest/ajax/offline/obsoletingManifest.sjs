var manifest =
  "CACHE MANIFEST\n" +
  "obsolete.html\n";

function handleRequest(request, response)
{
  var match = request.queryString.match(/^state=(.*)$/);
  if (match)
  {
    response.setStatusLine(request.httpVersion, 204, "No content");
    response.setHeader("Cache-Control", "no-cache");
    setState("offline.obsoletingManifest", match[1]);
  }

  if (request.queryString == "")
  {
    switch (getState("offline.obsoletingManifest"))
    {
      case "": // The default value
        response.setStatusLine(request.httpVersion, 404, "Not found");
        response.setHeader("Cache-Control", "no-cache");
        break;
      case "manifestPresent":
        response.setStatusLine(request.httpVersion, 200, "Ok");
        response.setHeader("Content-Type", "text/cache-manifest");
        response.setHeader("Cache-Control", "no-cache");
        response.write(manifest);
        break;
    }
  }
}
