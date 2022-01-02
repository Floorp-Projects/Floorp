function parseQuery(query, key) {
  for (let p of query.split("&")) {
    if (p == key) {
      return true;
    }
    if (p.startsWith(key + "=")) {
      return p.substring(key.length + 1);
    }
  }
}

// Return the first few bytes in a short byte range response. When Firefox
// requests subsequent bytes in a second range request, respond with a
// redirect. Requests after the first redirected are serviced as expected.
function handleRequest(request, response) {
  var query = request.queryString;
  var resource = parseQuery(query, "resource");
  var type = parseQuery(query, "type") || "application/octet-stream";
  var redirected = parseQuery(query, "redirected") || false;
  var useCors = parseQuery(query, "cors") || false;

  var file = Components.classes["@mozilla.org/file/directory_service;1"]
    .getService(Components.interfaces.nsIProperties)
    .get("CurWorkD", Components.interfaces.nsIFile);
  var fis = Components.classes[
    "@mozilla.org/network/file-input-stream;1"
  ].createInstance(Components.interfaces.nsIFileInputStream);
  var bis = Components.classes[
    "@mozilla.org/binaryinputstream;1"
  ].createInstance(Components.interfaces.nsIBinaryInputStream);
  var paths = "tests/dom/media/test/" + resource;
  var split = paths.split("/");
  for (var i = 0; i < split.length; ++i) {
    file.append(split[i]);
  }
  fis.init(file, -1, -1, false);

  bis.setInputStream(fis);
  var bytes = bis.readBytes(bis.available());
  let [from, to] = request
    .getHeader("range")
    .split("=")[1]
    .split("-")
    .map(s => parseInt(s));

  if (!redirected && from > 0) {
    var origin =
      request.host == "mochi.test" ? "example.org" : "mochi.test:8888";
    response.setStatusLine(request.httpVersion, 303, "See Other");
    let url =
      "http://" +
      origin +
      "/tests/dom/media/test/midflight-redirect.sjs?redirected&" +
      query;
    response.setHeader("Location", url);
    response.setHeader("Content-Type", "text/html");
    return;
  }

  if (isNaN(to)) {
    to = bytes.length - 1;
  }

  if (from == 0 && !redirected) {
    to =
      parseInt(parseQuery(query, "redirectAt")) || Math.floor(bytes.length / 4);
  }
  to = Math.min(to, bytes.length - 1);

  // Note: 'to' is the first index *excluded*, so we need (to + 1)
  // in the substring end here.
  byterange = bytes.substring(from, to + 1);

  let contentRange = "bytes " + from + "-" + to + "/" + bytes.length;
  let contentLength = byterange.length.toString();

  response.setStatusLine(request.httpVersion, 206, "Partial Content");
  response.setHeader("Content-Range", contentRange);
  response.setHeader("Content-Length", contentLength, false);
  response.setHeader("Content-Type", type, false);
  response.setHeader("Accept-Ranges", "bytes", false);
  response.setHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  if (redirected && useCors) {
    response.setHeader("Access-Control-Allow-Origin", "*");
  }
  response.write(byterange, byterange.length);
  bis.close();
}
