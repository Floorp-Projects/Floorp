function handleRequest(request, response)
{
  var self = Components.classes["@mozilla.org/file/local;1"]
                       .createInstance(Components.interfaces.nsILocalFile);
  self.initWithPath(getState("__LOCATION__"));
  var dest = self.parent;
  dest.append("\u263a.ico");
  if (dest.exists())
    dest.remove(false);
  var src = self.parent;
  src.append("bug415761.ico");
  src.copyTo(null, dest.leafName);
  var uri = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService)
                      .newFileURI(dest).spec;
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html");
  response.setHeader("Cache-Control", "no-cache");
  response.write("<link rel=\"stylesheet\" href=\"ImageDocument.css\">");
  response.write("<img src=\"moz-icon:" + uri + "\" width=\"32\" height=\"32\">");
}
