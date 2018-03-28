function parseQuery(query, key) {
  for (let p of query.split('&')) {
    if (p == key) {
      return true;
    }
    if (p.startsWith(key + "=")) {
      return p.substring(key.length + 1);
    }
  }
}

// Return seek.ogv file content for the first request with a given key.
// All subsequent requests return a redirect to a different-origin resource.
function handleRequest(request, response)
{
  var query = request.queryString;
  var key = parseQuery(query, "key");
  var type = parseQuery(query, "type") || "application/octet-stream";
  var resource = parseQuery(query, "res");
  var nested = parseQuery(query, "nested") || false;

  dump("Received request for key = "+ key +"\n");
  if (!nested) {
    if (getState(key) == "redirect") {
      var origin = request.host == "mochi.test" ? "example.org" : "mochi.test:8888";
      response.setStatusLine(request.httpVersion, 303, "See Other");
      let url = "http://" + origin +
                "/tests/dom/media/test/dynamic_redirect.sjs?nested&" + query;
      dump("Redirecting to "+ url + "\n");
      response.setHeader("Location", url);
      response.setHeader("Content-Type", "text/html");
      return;
    }
    setState(key, "redirect");
  }
  var file = Components.classes["@mozilla.org/file/directory_service;1"].
                        getService(Components.interfaces.nsIProperties).
                        get("CurWorkD", Components.interfaces.nsIFile);
  var fis  = Components.classes['@mozilla.org/network/file-input-stream;1'].
                        createInstance(Components.interfaces.nsIFileInputStream);
  var bis  = Components.classes["@mozilla.org/binaryinputstream;1"].
                        createInstance(Components.interfaces.nsIBinaryInputStream);
  var paths = "tests/dom/media/test/" + resource;
  var split = paths.split("/");
  for (var i = 0; i < split.length; ++i) {
    file.append(split[i]);
  }
  fis.init(file, -1, -1, false);

  bis.setInputStream(fis);
  var bytes = bis.readBytes(bis.available());
  let [from, to] = request.getHeader("range").split("=")[1].split("-").map(s => parseInt(s));
  to = to || Math.max(from, bytes.length - 1);
  byterange = bytes.substring(from, to + 1);

  let contentRange = "bytes "+ from +"-"+ to +"/"+ bytes.length;
  let contentLength = (to - from + 1).toString();
  dump("Response Content-Range = "+ contentRange +"\n");
  dump("Response Content-Length = "+ contentLength +"\n");

  response.setStatusLine(request.httpVersion, 206, "Partial Content");
  response.setHeader("Content-Range", contentRange);
  response.setHeader("Content-Length", contentLength, false);
  response.setHeader("Content-Type", type, false);
  response.setHeader("Accept-Ranges", "bytes", false);
  response.write(byterange, byterange.length);
  bis.close();
}
