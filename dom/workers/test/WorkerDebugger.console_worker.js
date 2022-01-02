"use strict";

console.log("Can you see this console message?");
console.warn("Can you see this second console message?");

var worker = new Worker("WorkerDebugger.console_childWorker.js");

setInterval(function() {
  console.log("Random message.");
}, 200);
