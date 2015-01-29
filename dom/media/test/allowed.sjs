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

var types = {
  js: "text/javascript",
  m4s: "video/mp4",
  mp4: "video/mp4",
  ogg: "video/ogg",
  ogv: "video/ogg",
  oga: "audio/ogg",
  webm: "video/webm",
  wav: "audio/x-wav"
};

// Return file with name as per the query string with access control
// allow headers.
function handleRequest(request, response)
{
  var resource = parseQuery(request, "");

  var file = Components.classes["@mozilla.org/file/directory_service;1"].
                        getService(Components.interfaces.nsIProperties).
                        get("CurWorkD", Components.interfaces.nsILocalFile);
  var fis  = Components.classes['@mozilla.org/network/file-input-stream;1'].
                        createInstance(Components.interfaces.nsIFileInputStream);
  var bis  = Components.classes["@mozilla.org/binaryinputstream;1"].
                        createInstance(Components.interfaces.nsIBinaryInputStream);
  var paths = "tests/dom/media/test/" + resource;
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
  var ext = resource.substring(resource.lastIndexOf(".")+1);
  response.setHeader("Content-Type", types[ext], false);
  response.setHeader("Access-Control-Allow-Origin", "*");
  response.write(bytes, bytes.length);
  bis.close();
}
