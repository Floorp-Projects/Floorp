var Cc = Components.classes;
var Ci = Components.interfaces;

// eslint-disable-next-line mozilla/use-services
const runtime = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime);

function callback(result) {
  Assert.equal(result, Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT);
  do_test_finished();
}

function run_test() {
  do_test_pending();

  Assert.equal(runtime.processType, Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT);

  sendCommand("load('test_ipcshell_child.js');");

  sendCommand("runtime.processType;", callback);

  [ [ "C", "D" ], [ "D", "C" ], [ "\u010C", "D" ], [ "D", "\u010C" ] ].forEach(
    function(pair) {
      do_test_pending();
      var cmp = pair[0].localeCompare(pair[1]);
      sendCommand(
          "'" + pair[0] + "'.localeCompare('" + pair[1] + "');",
          function(result) {
              Assert.equal(cmp, result);
              do_test_finished();
          });
    });
}
