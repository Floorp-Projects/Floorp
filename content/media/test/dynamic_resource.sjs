function parseQuery(request, key) {
  var params = request.queryString.split('&');
  for (var j = 0; j < params.length; ++j) {
    var p = params[j];
	if (p == key)
	  return true;
    if (p.indexOf(key + "=") == 0)
	  return p.substring(key.length + 1);
	if (p.indexOf("=") < 0 && key == "")
	  return p;
  }
  return false;
}

// Return resource1 file content for the first request with a given key.
// All subsequent requests return resource2. Both must be video/ogg.
function handleRequest(request, response)
{
  var key = parseQuery(request, "key");
  var resource1 = parseQuery(request, "res1");
  var resource2 = parseQuery(request, "res2");

  var resource = getState(key) == "2" ? resource2 : resource1;
  setState(key, "2");

  var file = Components.classes["@mozilla.org/file/directory_service;1"].
                        getService(Components.interfaces.nsIProperties).
                        get("CurWorkD", Components.interfaces.nsILocalFile);
  var fis  = Components.classes['@mozilla.org/network/file-input-stream;1'].
                        createInstance(Components.interfaces.nsIFileInputStream);
  var bis  = Components.classes["@mozilla.org/binaryinputstream;1"].
                        createInstance(Components.interfaces.nsIBinaryInputStream);
  var paths = "tests/content/media/test/" + resource;
  var split = paths.split("/");
  for(var i = 0; i < split.length; ++i) {
    file.append(split[i]);
  }
  fis.init(file, -1, -1, false);
  dump("file=" + file + "\n");
  bis.setInputStream(fis);
  var bytes = bis.readBytes(bis.available());
  response.setStatusLine(request.httpVersion, 206, "Partial Content");
  response.setHeader("Content-Range", "bytes 0-" + (bytes.length - 1) + "/" + bytes.length);
  response.setHeader("Content-Length", ""+bytes.length, false);
  response.setHeader("Content-Type", "video/ogg", false);
  response.write(bytes, bytes.length);
  bis.close();
}
