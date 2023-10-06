function handleRequest(request, response) {
  response.setHeader("Content-Type", "text/plain; charset=ISO-8859-1", false);
  const body = [0xc1];
  var bos = Cc["@mozilla.org/binaryoutputstream;1"].createInstance(
    Ci.nsIBinaryOutputStream
  );
  bos.setOutputStream(response.bodyOutputStream);

  bos.writeByteArray(body);
}
