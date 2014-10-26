function handleRequest(request, response)
{
  response.setHeader("Content-Type", "text/plain; charset=ISO-8859-1", false);
  const body = [0xC1];
  var bos = Components.classes["@mozilla.org/binaryoutputstream;1"]
      .createInstance(Components.interfaces.nsIBinaryOutputStream);
  bos.setOutputStream(response.bodyOutputStream);

  bos.writeByteArray(body, body.length);
}

