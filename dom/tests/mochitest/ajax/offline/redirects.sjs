ver1manifest =
  "CACHE MANIFEST\n" +
  "# v1\n" +
  "\n" +
  "http://mochi.test:8888/tests/SimpleTest/SimpleTest.js\n" +
  "http://mochi.test:8888/tests/dom/tests/mochitest/ajax/offline/offlineTests.js\n" +
  "http://mochi.test:8888/tests/dom/tests/mochitest/ajax/offline/explicitRedirect.sjs";

ver2manifest =
  "CACHE MANIFEST\n" +
  "# v2\n" +
  "\n" +
  "http://mochi.test:8888/tests/SimpleTest/SimpleTest.js\n" +
  "http://mochi.test:8888/tests/dom/tests/mochitest/ajax/offline/offlineTests.js\n" +
  "http://mochi.test:8888/tests/dom/tests/mochitest/ajax/offline/explicitRedirect.sjs";

ver3manifest =
  "CACHE MANIFEST\n" +
  "# v3\n" +
  "\n" +
  "http://mochi.test:8888/tests/SimpleTest/SimpleTest.js\n" +
  "http://mochi.test:8888/tests/dom/tests/mochitest/ajax/offline/offlineTests.js\n" +
  "http://mochi.test:8888/tests/dom/tests/mochitest/ajax/offline/explicitRedirect.sjs";

function handleRequest(request, response)
{
  var match = request.queryString.match(/^state=(.*)$/);
  if (match)
  {
    response.setStatusLine(request.httpVersion, 204, "No content");
    setState("state", match[1]);
  }

  if (request.queryString == "")
  {
    response.setStatusLine(request.httpVersion, 200, "Ok");
    response.setHeader("Content-Type", "text/cache-manifest");
    response.setHeader("Cache-Control", "no-cache");
    switch (getState("state"))
    {
      case "": // The default value
        response.write(ver1manifest + "\n#" + getState("state"));
        break;
      case "second":
        response.write(ver2manifest + "\n#" + getState("state"));
        break;
      case "third":
        response.write(ver3manifest + "\n#" + getState("state"));
        break;
    }
  }
}
