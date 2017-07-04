const { classes: Cc, interfaces: Ci } = Components;

function getGzippedFileBytes()
{
  var file;
  getObjectState("SERVER_ROOT", function(serverRoot) {
    file = serverRoot.getFile("tests/dom/media/test/short.mp4.gz");
  });
  var fileInputStream =
    Components.classes['@mozilla.org/network/file-input-stream;1']
              .createInstance(Components.interfaces.nsIFileInputStream);
  var binaryInputStream =
    Components.classes["@mozilla.org/binaryinputstream;1"]
              .createInstance(Components.interfaces.nsIBinaryInputStream);
  fileInputStream.init(file, -1, -1, 0);
  binaryInputStream.setInputStream(fileInputStream);
  return binaryInputStream.readBytes(binaryInputStream.available());
}

function handleRequest(request, response)
{
  var bytes = getGzippedFileBytes();
  response.setHeader("Content-Length", String(bytes.length), false);
  response.setHeader("Content-Type", "video/mp4", false);
  response.setHeader("Access-Control-Allow-Origin", "*", false);
  response.setHeader("Content-Encoding", "gzip", false);
  response.setHeader("Cache-Control", "no-cache", false);
  response.write(bytes, bytes.length);
}
