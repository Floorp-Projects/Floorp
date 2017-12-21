registerManifests([do_get_file("data/test_crlf.manifest")]);

function run_test() {
  let cr = Cc["@mozilla.org/chrome/chrome-registry;1"].
    getService(Ci.nsIChromeRegistry);

  let sourceURI = Services.io.newURI("chrome://test_crlf/content/");
  // this throws for packages that are not registered
  let file = cr.convertChromeURL(sourceURI).QueryInterface(Ci.nsIFileURL).file;

  Assert.ok(file.equals(do_get_file("data/test_crlf.xul", true)));
}
