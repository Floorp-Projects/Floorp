/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
msg = null;
var errors = 5;

onerror = function(event) {
  ok(true, msg);
  if (!--errors) SimpleTest.finish();
}

msg = "No Eval allowed";
worker = new Worker("csp_worker.js");
worker.postMessage(0);
worker.onmessage = function(event) {
  ok(false, "Eval succeeded!");
}

msg = "No Eval allowed 2";
worker = new Worker("csp_worker.js");
worker.postMessage(4);
worker.onmessage = function(event) {
  ok(false, "Eval succeeded!");
}

msg = "ImportScripts data:";
worker = new Worker("csp_worker.js");
worker.postMessage(-1);
worker.onmessage = function(event) {
  ok(false, "Eval succeeded!");
}

msg = "ImportScripts javascript:";
worker = new Worker("csp_worker.js");
worker.postMessage(-2);
worker.onmessage = function(event) {
  ok(false, "Eval succeeded!");
}

msg = "Loading data:something";
worker = new Worker("data:application/javascript;base64,ZHVtcCgnaGVsbG8gd29ybGQnKQo=");
worker.onerror = function() {
  ok(true, "Threw as expected.");
}

worker = new Worker("javascript:dump(123);");
SimpleTest.waitForExplicitFinish();
