function my_ok(v, msg) {
  postMessage({type: "ok", value: v, msg: msg});
}

function finish() {
  postMessage('finish');
}

function expectAsyncResult() {
  postMessage('expect');
}

expectAsyncResult();
try {
  var success = false;
  importScripts("nonexistent_imported_script.js");
} catch(x) {
}

my_ok(success, "worker imported script should be intercepted");
finish();

function check_intercepted_script() {
  success = true;
}

importScripts('fetch_tests.js')

finish(); //corresponds to the gExpected increment before creating this worker
