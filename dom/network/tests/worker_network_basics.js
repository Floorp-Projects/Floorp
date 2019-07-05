function ok(a, msg) {
  postMessage({ type: "status", status: !!a, msg: msg });
}

function is(a, b, msg) {
  ok(a === b, msg);
}

function finish() {
  postMessage({ type: "finish" });
}

ok("connection" in navigator, "navigator.connection should exist");

ok(navigator.connection, "navigator.connection returns an object");

ok(
  navigator.connection instanceof EventTarget,
  "navigator.connection is a EventTarget object"
);

ok("type" in navigator.connection, "type should be a Connection attribute");
is(
  navigator.connection.type,
  "none",
  "By default connection.type equals to none"
);
finish();
