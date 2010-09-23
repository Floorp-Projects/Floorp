const Cc = Components.classes;
const Ci = Components.interfaces;

function run_test() {
  var file = do_get_file("bug596580_versioned.js");
  var ios = Cc["@mozilla.org/network/io-service;1"]
              .getService(Ci.nsIIOService);
  var uri = ios.newFileURI(file);
  var scriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
                       .getService(Ci.mozIJSSubScriptLoader);
  scriptLoader.loadSubScript(uri.spec);
  version(150)
  try {
      scriptLoader.loadSubScript(uri.spec);
      throw new Exception("Subscript should fail to load.");
  } catch (e if e instanceof SyntaxError) {
      // Okay.
  }
}
