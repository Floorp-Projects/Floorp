function callback(result) {
  do_check_eq(result, Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT);
  do_test_finished();
}

function run_test() {
  do_test_pending();

  do_check_eq(runtime.processType, Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT);
  
  sendCommand("load('test_ipcshell_child.js');");
  sendCommand("runtime.processType;", callback);
}
load('test_ipcshell_child.js');

