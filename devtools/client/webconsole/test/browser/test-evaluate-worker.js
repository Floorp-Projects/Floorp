"use strict";

// Given WorkerGlobalScope.prototype.location and WorkerLocation.prototype.href
// getters are not in eager-evaluation allowlist (see bug 1815381),
// store the value here and use location_href variable in the test.
// eslint-disable-next-line no-unused-vars
var location_href = globalThis.location.href;

self.addEventListener("message", ({ data }) => foo(data));

function foo(data) {
  // eslint-disable-next-line no-debugger
  debugger;
}
