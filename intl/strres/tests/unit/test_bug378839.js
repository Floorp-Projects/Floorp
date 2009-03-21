/* Tests getting properties from string bundles
 */

const name_file = "file";
const value_file = "File";

const name_loyal = "loyal";
const value_loyal = "\u5fe0\u5fc3"; // tests escaped Unicode

const name_trout = "trout";
const value_trout = "\u9cdf\u9b5a"; // tests UTF-8

const name_edit = "edit";
const value_edit = "Edit"; // tests literal leading spaces are stripped

const name_view = "view";
const value_view = "View"; // tests literal trailing spaces are stripped

const name_go = "go";
const value_go = " Go"; // tests escaped leading spaces are not stripped

const name_message = "message";
const value_message = "Message "; // tests escaped trailing spaces are not stripped

const name_hello = "hello";
const var_hello = "World";
const value_hello = "Hello World"; // tests formatStringFromName with parameter


function run_test() {
    var StringBundle = 
	Components.classes["@mozilla.org/intl/stringbundle;1"]
	 .getService(Components.interfaces.nsIStringBundleService);
    var ios = Components.classes["@mozilla.org/network/io-service;1"]
	 .getService(Components.interfaces.nsIIOService);
    var bundleURI = ios.newFileURI(do_get_file("strres.properties"));

    var bundle = StringBundle.createBundle(bundleURI.spec);

    var bundle_file = bundle.GetStringFromName(name_file);
    do_check_eq(bundle_file, value_file);

    var bundle_loyal = bundle.GetStringFromName(name_loyal);
    do_check_eq(bundle_loyal, value_loyal);

    var bundle_trout = bundle.GetStringFromName(name_trout);
    do_check_eq(bundle_trout, value_trout);

    var bundle_edit = bundle.GetStringFromName(name_edit);
    do_check_eq(bundle_edit, value_edit);

    var bundle_view = bundle.GetStringFromName(name_view);
    do_check_eq(bundle_view, value_view);

    var bundle_go = bundle.GetStringFromName(name_go);
    do_check_eq(bundle_go, value_go);

    var bundle_message = bundle.GetStringFromName(name_message);
    do_check_eq(bundle_message, value_message);

    var bundle_hello = bundle.formatStringFromName(name_hello, [var_hello], 1);
    do_check_eq(bundle_hello, value_hello);
}
    
