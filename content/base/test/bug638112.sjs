function getInputStream(path)
{
  var file = Components.classes["@mozilla.org/file/directory_service;1"].
                        getService(Components.interfaces.nsIProperties).
                        get("CurWorkD", Components.interfaces.nsILocalFile);
  var fis  = Components.classes['@mozilla.org/network/file-input-stream;1'].
                        createInstance(Components.interfaces.nsIFileInputStream);
  var split = path.split("/");
  for(var i = 0; i < split.length; ++i) {
    file.append(split[i]);
  }
  fis.init(file, -1, -1, false);
  return fis;
}



function handleRequest(request, response)
{
  var inputStream = getInputStream("tests/content/base/test/bug638112-response.txt")
  response.seizePower();
  response.bodyOutputStream.writeFrom(inputStream,
                                      inputStream.available());
  response.finish();
  inputStream.close();
}
