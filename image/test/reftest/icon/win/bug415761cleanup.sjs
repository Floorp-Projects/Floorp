function handleRequest(request, response)
{
  var self = Components.classes["@mozilla.org/file/local;1"]
                       .createInstance(Components.interfaces.nsILocalFile);
  self.initWithPath(getState("__LOCATION__"));
  var dest = self.parent;
  dest.append("\u263a.ico");
  if (dest.exists())
    dest.remove(false);
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html");
}
