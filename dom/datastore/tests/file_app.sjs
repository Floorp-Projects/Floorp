var gBasePath = "tests/dom/datastore/tests/";
var gAppTemplatePath = "tests/dom/datastore/tests/file_app.template.webapp";

function handleRequest(request, response) {
  var template = gBasePath + 'file_app.template.webapp';
  response.setHeader("Content-Type", "application/x-web-app-manifest+json", false);
  response.write(readTemplate(template));
}

// Copy-pasted incantations. There ought to be a better way to synchronously read
// a file into a string, but I guess we're trying to discourage that.
function readTemplate(path) {
  var file = Components.classes["@mozilla.org/file/directory_service;1"].
                        getService(Components.interfaces.nsIProperties).
                        get("CurWorkD", Components.interfaces.nsILocalFile);
  var fis  = Components.classes['@mozilla.org/network/file-input-stream;1'].
                        createInstance(Components.interfaces.nsIFileInputStream);
  var cis = Components.classes["@mozilla.org/intl/converter-input-stream;1"].
                       createInstance(Components.interfaces.nsIConverterInputStream);
  var split = path.split("/");
  for(var i = 0; i < split.length; ++i) {
    file.append(split[i]);
  }
  fis.init(file, -1, -1, false);
  cis.init(fis, "UTF-8", 0, 0);

  var data = "";
  let (str = {}) {
    let read = 0;
    do {
      read = cis.readString(0xffffffff, str); // read as much as we can and put it in str.value
      data += str.value;
    } while (read != 0);
  }
  cis.close();
  return data;
}
