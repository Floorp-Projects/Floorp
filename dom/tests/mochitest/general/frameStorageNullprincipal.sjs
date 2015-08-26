// This is a sjs file which reads in frameStoragePrevented.html, and writes it out as a data: URI, which this page redirects to.
// This produces a URI with the null principal, which should be unable to access storage.
// We append the #nullprincipal hash to the end of the data: URI to tell the script that it shouldn't try to spawn a webworker,
// as it won't be allowed to, as it has a null principal.

function handleRequest(request, response) {
  // Get the nsIFile for frameStoragePrevented.html
  var file;
  getObjectState("SERVER_ROOT", function(serverRoot) {
    file = serverRoot.getFile("/tests/dom/tests/mochitest/general/frameStoragePrevented.html");
  });

  // Set up the file streams to read in the file as UTF-8
  let fstream = Components.classes["@mozilla.org/network/file-input-stream;1"].
      createInstance(Components.interfaces.nsIFileInputStream);
  fstream.init(file, -1, 0, 0);
  let cstream = Components.classes["@mozilla.org/intl/converter-input-stream;1"].
      createInstance(Components.interfaces.nsIConverterInputStream);
  cstream.init(fstream, "UTF-8", 0, 0);

  // Read in the file, and concatenate it onto the data string
  let data = "";
  let str = {};
  let read = 0;
  do {
    read = cstream.readString(0xffffffff, str);
    data += str.value;
  } while (read != 0);

  // No scheme is provided in data URIs, so explicitly provide one
  data = data.replace('//example.com', 'http://example.com');

  // Write out the file as a data: URI, and redirect to it
  response.setStatusLine('1.1', 302, 'Found');
  response.setHeader('Location', 'data:text/html,' + encodeURIComponent(data) + "#nullprincipal");
}
