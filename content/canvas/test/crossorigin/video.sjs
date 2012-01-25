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

function handleRequest(request, response) {
  var name = parseQuery(request, "name");
  var type = parseQuery(request, "type");
  var cors = parseQuery(request, "cors");
  var file = Components.classes["@mozilla.org/file/directory_service;1"].
                        getService(Components.interfaces.nsIProperties).
                        get("CurWorkD", Components.interfaces.nsILocalFile);
  var fis = Components.classes['@mozilla.org/network/file-input-stream;1'].
                        createInstance(Components.interfaces.nsIFileInputStream);
  var bis = Components.classes["@mozilla.org/binaryinputstream;1"].
                        createInstance(Components.interfaces.nsIBinaryInputStream);
  var split = name.split("/");
  for(var i = 0; i < split.length; ++i) {
    file.append(split[i]);
  }
  fis.init(file, -1, -1, false);
  bis.setInputStream(fis);
  var bytes = bis.readBytes(bis.available());
  response.setHeader("Content-Length", ""+bytes.length, false);
  response.setHeader("Content-Type", type, false);
  if (cors == "anonymous") {
    response.setHeader("Access-Control-Allow-Origin", "*", false);
  } else if (cors == "use-credentials") {
    response.setHeader("Access-Control-Allow-Origin", "http://mochi.test:8888", false);
    response.setHeader("Access-Control-Allow-Credentials", "true", false);
  }
  response.write(bytes, bytes.length);
  bis.close();
}
