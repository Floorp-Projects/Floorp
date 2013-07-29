Components.utils.import("resource://gre/modules/Services.jsm");

function run_test() {
  Services.prefs.setBoolPref('dom.mozTCPSocket.enabled', true);
  run_test_in_child("../unit/test_tcpserversocket.js", function() {
    Services.prefs.clearUserPref('dom.mozTCPSocket.enabled');
    do_test_finished();
  });
}
