ver1manifest =
  "CACHE MANIFEST\n" +
  "# v1\n" +
  "\n" +
  "http://mochi.test:8888/tests/dom/tests/mochitest/ajax/offline/offlineTests.js\n" +
  "http://mochi.test:8888/tests/dom/tests/mochitest/ajax/offline/updatingIframe.sjs\n" +
  "\n" +
  "FALLBACK:\n" +
  "namespace1/ fallback.html\n" +
  "\n" +
  "NETWORK:\n" +
  "onwhitelist.html\n";

ver2manifest =
  "CACHE MANIFEST\n" +
  "# v2\n" +
  "\n" +
  "http://mochi.test:8888/tests/SimpleTest/SimpleTest.js\n" +
  "http://mochi.test:8888/tests/dom/tests/mochitest/ajax/offline/offlineTests.js\n" +
  "http://mochi.test:8888/tests/dom/tests/mochitest/ajax/offline/updatingIframe.sjs" +
  "\n" +
  "FALLBACK:\n" +
  "namespace1/ fallback.html\n" +
  "namespace1/sub/ fallback2.html\n";

ver3manifest =
  "CACHE MANIFEST\n" +
  "# v3\n" +
  "\n" +
  "http://mochi.test:8888/tests/dom/tests/mochitest/ajax/offline/offlineTests.js\n" +
  "http://mochi.test:8888/tests/dom/tests/mochitest/ajax/offline/updatingIframe.sjs" +
  "\n" +
  "FALLBACK:\n" +
  "namespace1/sub fallback2.html\n" +
  "\n" +
  "NETWORK:\n" +
  "onwhitelist.html\n";

function handleRequest(request, response)
{
  var match = request.queryString.match(/^state=(.*)$/);
  if (match)
  {
    response.setStatusLine(request.httpVersion, 204, "No content");
    setState("offline.updatingManifest", match[1]);
  }

  if (request.queryString == "")
  {
    response.setStatusLine(request.httpVersion, 200, "Ok");
    response.setHeader("Content-Type", "text/cache-manifest");
    response.setHeader("Cache-Control", "no-cache");
    switch (getState("offline.updatingManifest"))
    {
      case "": // The default value
        response.write(ver1manifest + "\n#" + getState("offline.updatingManifest"));
        break;
      case "second":
        response.write(ver2manifest + "\n#" + getState("offline.updatingManifest"));
        break;
      case "third":
        response.write(ver3manifest + "\n#" + getState("offline.updatingManifest"));
        break;
    }
  }
}
