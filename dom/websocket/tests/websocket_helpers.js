var current_test = 0;

function shouldNotOpen(e) {
  var ws = e.target;
  ok(false, "onopen shouldn't be called on test " + ws._testNumber + "!");
}

function shouldCloseCleanly(e) {
  var ws = e.target;
  ok(
    e.wasClean,
    "the ws connection in test " + ws._testNumber + " should be closed cleanly"
  );
}

function shouldCloseNotCleanly(e) {
  var ws = e.target;
  ok(
    !e.wasClean,
    "the ws connection in test " +
      ws._testNumber +
      " shouldn't be closed cleanly"
  );
}

function ignoreError(e) {}

function CreateTestWS(ws_location, ws_protocol) {
  var ws;

  if (ws_protocol == undefined) {
    ws = new WebSocket(ws_location);
  } else {
    ws = new WebSocket(ws_location, ws_protocol);
  }

  ws._testNumber = current_test;
  ok(true, "Created websocket for test " + ws._testNumber + "\n");

  ws.onerror = function(e) {
    ok(false, "onerror called on test " + e.target._testNumber + "!");
  };

  return ws;
}

function forcegc() {
  SpecialPowers.forceGC();
  SpecialPowers.gc();
}

function feedback() {
  $("feedback").innerHTML =
    "executing test: " + (current_test + 1) + " of " + tests.length + " tests.";
}

function finish() {
  SimpleTest.finish();
}

function doTest() {
  if (current_test >= tests.length) {
    finish();
    return;
  }

  feedback();
  tests[current_test++]().then(doTest);
}
