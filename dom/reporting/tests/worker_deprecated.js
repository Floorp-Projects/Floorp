/* eslint-disable no-undef */

// eslint-disable-next-line no-unused-vars
function ok(a, msg) {
  postMessage({ type: "check", check: !!a, msg });
}

// eslint-disable-next-line no-unused-vars
function is(a, b, msg) {
  ok(a === b, msg);
}

// eslint-disable-next-line no-unused-vars
function info(msg) {
  postMessage({ type: "info", msg });
}

function finish() {
  postMessage({ type: "finish" });
}

importScripts("common_deprecated.js");

test_deprecatedInterface()
  .then(() => test_deprecatedMethod())
  .then(() => test_deprecatedAttribute())
  .then(() => test_takeRecords())
  .then(() => finish());
