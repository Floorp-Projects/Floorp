'use strict';

unsafeWindow.runDebuggerStatement = function() {
  window.document.body.setAttribute('style', 'background-color: red');
  debugger;
  window.document.body.setAttribute('style', 'background-color: green');
}
