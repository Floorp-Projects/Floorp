// Make sure that we can resolve the ascii charset alias to us-ascii
// (bug 383018)

function run_test() {
  var ccm = Components.classes["@mozilla.org/charset-converter-manager;1"];
  var ccms = ccm.getService(Components.interfaces.nsICharsetConverterManager);

  var alias = ccms.getCharsetAlias("ascii");
  // ascii should be an alias for us-ascii
  do_check_eq(alias, "us-ascii");
}
