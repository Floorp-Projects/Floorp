Components.utils.import("resource://gre/modules/NetUtil.jsm");

function loadContentFromFile(path) {
  // Load the content to return in the response from file.
  // Since it's relative to the cwd of the test runner, we start there and
  // append to get to the actual path of the file.
  var testContentFile =
    Components.classes["@mozilla.org/file/directory_service;1"].
    getService(Components.interfaces.nsIProperties).
    get("CurWorkD", Components.interfaces.nsIFile);
  var dirs = path.split("/");
  for (var i = 0; i < dirs.length; i++) {
    testContentFile.append(dirs[i]);
  }
  var testContentFileStream =
    Components.classes["@mozilla.org/network/file-input-stream;1"].
    createInstance(Components.interfaces.nsIFileInputStream);
  testContentFileStream.init(testContentFile, -1, 0, 0);
  var testContent = NetUtil.readInputStreamToString(testContentFileStream, testContentFileStream.available());
  return testContent;
}

function handleRequest(request, response)
{
  const { scheme, host, path } = request;
  // get the Content-Type to serve from the query string
  var contentType = null;
  var showLastRequest = false;
  request.queryString.split('&').forEach(function (val) {
     var [name, value] = val.split('=');
       if (name == "type") {
         contentType = unescape(value);
       }
       if (name == "lastRequest") {
         showLastRequest = true;
       }
  });

  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  if (showLastRequest) {
    response.setHeader("Content-Type", "text/html", false);
    response.write(getState("lastRequest"));
    return;
  }

  setState("lastRequest", JSON.stringify({
    scheme,
    host,
    path,
    contentType: contentType || "other"
  }));

  switch (contentType) {
    case "img":
      response.setHeader("Content-Type", "image/png", false);
      response.write(loadContentFromFile("tests/image/test/mochitest/blue.png"));
      break;

    case "media":
      response.setHeader("Content-Type", "video/ogg", false);
      response.write(loadContentFromFile("tests/dom/media/test/320x240.ogv"));
      break;

    case "iframe":
      response.setHeader("Content-Type", "text/html", false);
      response.write("frame content");
      break;

    case "script":
      response.setHeader("Content-Type", "application/javascript", false);
      break;

    case "stylesheet":
      response.setHeader("Content-Type", "text/css", false);
      break;

    case "object":
      response.setHeader("Content-Type", "application/x-test", false);
      break;

   case "xhr":
      response.setHeader("Content-Type", "text/xml", false);
      response.setHeader("Access-Control-Allow-Origin", "https://example.com");
      response.write('<?xml version="1.0" encoding="UTF-8" ?><test></test>');
      break;

    default:
      response.setHeader("Content-Type", "text/html", false);
      response.write("<html><body>Hello World</body></html>");
      break;
  }
}
