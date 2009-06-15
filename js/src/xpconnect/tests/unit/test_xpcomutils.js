Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

var x = {
    QueryInterface: XPCOMUtils.generateQI([Components.interfaces.nsIClassInfo,
					   "nsIDOMNode"])
};

function run_test() {
    try {
	x.QueryInterface(Components.interfaces.nsIClassInfo);
    } catch(e) {
	do_throw("Should QI to nsIClassInfo");
    }
    try {
	x.QueryInterface(Components.interfaces.nsIDOMNode);
    } catch(e) {
	do_throw("Should QI to nsIDOMNode");
    }
    try {
	x.QueryInterface(Components.interfaces.nsIDOMDocument);
	do_throw("QI should not have succeeded!");
    } catch(e) {}
}
