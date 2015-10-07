var Cc = Components.classes;
var Ci = Components.interfaces;

const runtime = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime);

if (typeof(run_test) == "undefined") {
  run_test = function() {
    do_check_eq(runtime.processType, Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT);
  }}
