Components.utils.import("resource://gre/modules/Services.jsm");
function startup() {
  Services.obs.notifyObservers(null, "test-devtools", null);
}
function shutdown() {}
function install() {}
function uninstall() {}
