// SJS file for HSTS mochitests

Components.utils.import("resource://gre/modules/NetUtil.jsm");
Components.utils.importGlobalProperties(["URLSearchParams"]);

function loadFromFile(path) {
  // Load the HTML to return in the response from file.
  // Since it's relative to the cwd of the test runner, we start there and
  // append to get to the actual path of the file.
  var testFile =
    Components.classes["@mozilla.org/file/directory_service;1"].
    getService(Components.interfaces.nsIProperties).
    get("CurWorkD", Components.interfaces.nsILocalFile);
  var dirs = path.split("/");
  for (var i = 0; i < dirs.length; i++) {
    testFile.append(dirs[i]);
  }
  var testFileStream =
    Components.classes["@mozilla.org/network/file-input-stream;1"].
    createInstance(Components.interfaces.nsIFileInputStream);
  testFileStream.init(testFile, -1, 0, 0);
  var test = NetUtil.readInputStreamToString(testFileStream, testFileStream.available());
  return test;
}

function handleRequest(request, response)
{
  const query = new URLSearchParams(request.queryString);

  redir = query.get('redir');
  if (redir == 'same') {
    query.delete("redir");
    response.setStatus(302);
    let newURI = request.uri;
    newURI.queryString = query.serialize();
    response.setHeader("Location", newURI.spec)
  }

  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  // if we have a priming header, check for required behavior
  // and set header appropriately
  if (request.hasHeader('Upgrade-Insecure-Requests')) {
    var expected = query.get('primer');
    if (expected == 'prime-hsts') {
      // set it for 5 minutes
      response.setHeader("Strict-Transport-Security", "max-age="+(60*5), false);
    } else if (expected == 'reject-upgrade') {
      response.setHeader("Strict-Transport-Security", "max-age=0", false);
    }
    response.write('');
    return;
  }

  var file = query.get('file');
  if (file) {
    var mimetype = unescape(query.get('mimetype'));
    response.setHeader("Content-Type", mimetype, false);
    response.write(loadFromFile(unescape(file)));
    return;
  }

  response.setHeader("Content-Type", "application/json", false);
  response.write('{}');
}
