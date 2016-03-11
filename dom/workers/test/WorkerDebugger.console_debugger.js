"use strict"

function ok(a, msg) {
  postMessage(JSON.stringify({ type: 'status', what: !!a, msg: msg }));
}

function is(a, b, msg) {
  ok(a === b, msg);
}

function finish() {
  postMessage(JSON.stringify({ type: 'finish' }));
}

function magic() {
  console.log("Hello from the debugger script!");

  var foo = retrieveConsoleEvents();
  ok(Array.isArray(foo), "We received an array.");
  ok(foo.length >= 2, "At least 2 messages.");

  is(foo[0].arguments[0], "Can you see this console message?", "First message ok.");
  is(foo[1].arguments[0], "Can you see this second console message?", "Second message ok.");

  setConsoleEventHandler(function(consoleData) {
    is(consoleData.arguments[0], "Random message.", "Random message ok!");
    finish();
  });
}

this.onmessage = function (event) {
  switch (event.data) {
  case "do magic":
    magic();
    break;
  }
};
