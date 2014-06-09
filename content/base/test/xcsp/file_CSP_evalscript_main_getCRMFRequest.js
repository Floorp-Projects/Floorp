// some javascript for the CSP eval() tests

function logResult(str, passed) {
  var elt = document.createElement('div');
  var color = passed ? "#cfc;" : "#fcc";
  elt.setAttribute('style', 'background-color:' + color + '; width:100%; border:1px solid black; padding:3px; margin:4px;');
  elt.innerHTML = str;
  document.body.appendChild(elt);
}

window._testResults = {};

// callback for when stuff is allowed by CSP
var onevalexecuted = (function(window) {
    return function(shouldrun, what, data) {
      window._testResults[what] = "ran";
      window.parent.scriptRan(shouldrun, what, data);
      logResult((shouldrun ? "PASS: " : "FAIL: ") + what + " : " + data, shouldrun);
    };})(window);

// callback for when stuff is blocked
var onevalblocked = (function(window) {
    return function(shouldrun, what, data) {
      window._testResults[what] = "blocked";
      window.parent.scriptBlocked(shouldrun, what, data);
      logResult((shouldrun ? "FAIL: " : "PASS: ") + what + " : " + data, !shouldrun);
    };})(window);


// Defer until document is loaded so that we can write the pretty result boxes
// out.
addEventListener('load', function() {
  // generateCRMFRequest test -- make sure we cannot eval the callback if CSP is in effect
  try {
    var script = 'console.log("dynamic script eval\'d in crypto.generateCRMFRequest should be disallowed")';
    crypto.generateCRMFRequest('CN=0', 0, 0, null, script, 384, null, 'rsa-dual-use');
    onevalexecuted(false, "crypto.generateCRMFRequest()",
                   "crypto.generateCRMFRequest() should not run!");
  } catch (e) {
    onevalblocked(false, "eval(script) inside crypto.generateCRMFRequest",
                  "eval was blocked during crypto.generateCRMFRequest");
  }


}, false);



