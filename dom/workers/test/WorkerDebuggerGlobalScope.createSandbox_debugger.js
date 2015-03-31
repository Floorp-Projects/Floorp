"use strict";

const SANDBOX_URL = "WorkerDebuggerGlobalScope.createSandbox_sandbox.js";

let prototype = {
  self: this,
};
let sandbox = createSandbox(SANDBOX_URL, prototype);
loadSubScript(SANDBOX_URL, sandbox);
