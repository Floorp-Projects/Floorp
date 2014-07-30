'use strict';

function runDebuggerStatement () {
  window.document.body.setAttribute('style', 'background-color: red');
  debugger;
  window.document.body.setAttribute('style', 'background-color: green');
}

exportFunction(
  runDebuggerStatement,
  document.defaultView,
  { defineAs: "runDebuggerStatement" }
);
