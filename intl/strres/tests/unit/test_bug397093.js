/* Tests getting properties from string bundles with incorrect encoding.
 * The string bundle contains one ascii property, one UTF-8 and one Latin-1.
 * Expected behaviour is that the whole string bundle should be rejected and
 * all GetStringFromName calls should fail.
 */

const name_ascii = "asciiProperty";
const value_ascii = "";

const name_utf8 = "utf8Property";
const value_utf8 = "";

const name_latin1 = "latin1";
const value_latin1 = "";


function run_test() {
    var StringBundle = 
	Components.classes["@mozilla.org/intl/stringbundle;1"]
	 .getService(Components.interfaces.nsIStringBundleService);
    var ios = Components.classes["@mozilla.org/network/io-service;1"]
	 .getService(Components.interfaces.nsIIOService);
    var bundleURI = ios.newFileURI(do_get_file("397093.properties"));

    var bundle = StringBundle.createBundle(bundleURI.spec);
    
    var bundle_ascii="", bundle_utf8="", bundle_latin1="";
    try {
	bundle_ascii = bundle.GetStringFromName(name_ascii);
    } catch(e) {}
    do_check_eq(bundle_ascii, value_ascii);

    try {
	bundle_utf8 = bundle.GetStringFromName(name_utf8);
    } catch(e) {}
    do_check_eq(bundle_utf8, value_utf8);

    try {
	bundle_latin1 = bundle.GetStringFromName(name_latin1);
    } catch(e) {}
    do_check_eq(bundle_latin1, value_latin1);
}
    
