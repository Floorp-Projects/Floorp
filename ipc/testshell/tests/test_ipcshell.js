function checkStatus(result) {
  if (result == "Waiting") {
    sendCommand("observer.status;", checkStatus);
    return;
  }

  do_check_eq(result, "Success");
  do_test_finished();
}

function run_test() {
  do_test_pending();
  sendCommand("load('test_ipcshell_child.js'); start();");
  sendCommand("observer.status;", checkStatus);
}
