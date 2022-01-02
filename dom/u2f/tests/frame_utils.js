// Utilities to help talk between the frame_ iframe documents and the parent
// tests.

var _parentOrigin = "https://example.com/";

function local_setParentOrigin(aOrigin) {
  _parentOrigin = aOrigin;
}

function handleEventMessage(event) {
  if ("test" in event.data) {
    let summary = event.data.test + ": " + event.data.msg;
    ok(event.data.status, summary);
  } else if ("done" in event.data) {
    SimpleTest.finish();
  } else {
    ok(false, "Unexpected message in the test harness: " + event.data);
  }
}

function local_is(value, expected, message) {
  if (value === expected) {
    local_ok(true, message);
  } else {
    local_ok(false, message + " unexpectedly: " + value + " !== " + expected);
  }
}

function local_isnot(value, expected, message) {
  if (value !== expected) {
    local_ok(true, message);
  } else {
    local_ok(false, message + " unexpectedly: " + value + " === " + expected);
  }
}

function local_ok(expression, message) {
  let body = { test: this.location.pathname, status: expression, msg: message };
  parent.postMessage(body, _parentOrigin);
}

function local_doesThrow(fn, name) {
  let gotException = false;
  try {
    fn();
  } catch (ex) {
    gotException = true;
  }
  local_ok(gotException, name);
}

function local_finished() {
  parent.postMessage({ done: true }, _parentOrigin);
}
