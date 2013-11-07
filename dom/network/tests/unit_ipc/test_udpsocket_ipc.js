Components.utils.import("resource://gre/modules/Services.jsm");

function run_test() {
  Services.prefs.setBoolPref('media.peerconnection.ipc.enabled', true);
  run_test_in_child("/udpsocket_child.js", function() {
    Services.prefs.clearUserPref('media.peerconnection.ipc.enabled');
    do_test_finished();
  });
}
