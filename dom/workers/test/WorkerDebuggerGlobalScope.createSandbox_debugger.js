"use strict";

const SANDBOX_URL = "WorkerDebuggerGlobalScope.createSandbox_sandbox.js";

var prototype = {
  self: this,
};
var sandbox = createSandbox(SANDBOX_URL, prototype);
loadSubScript(SANDBOX_URL, sandbox);
