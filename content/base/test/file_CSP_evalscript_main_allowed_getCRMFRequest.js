// some javascript for the CSP eval() tests
// all of these evals should succeed, as the document loading this script
// has script-src 'self' 'unsafe-eval'

function logResult(str, passed) {
  var elt = document.createElement('div');
  var color = passed ? "#cfc;" : "#fcc";
  elt.setAttribute('style', 'background-color:' + color + '; width:100%; border:1px solid black; padding:3px; margin:4px;');
  elt.innerHTML = str;
  document.body.appendChild(elt);
}

// callback for when stuff is allowed by CSP
var onevalexecuted = (function(window) {
    return function(shouldrun, what, data) {
      window.parent.scriptRan(shouldrun, what, data);
      logResult((shouldrun ? "PASS: " : "FAIL: ") + what + " : " + data, shouldrun);
    };})(window);

// callback for when stuff is blocked
var onevalblocked = (function(window) {
    return function(shouldrun, what, data) {
      window.parent.scriptBlocked(shouldrun, what, data);
      logResult((shouldrun ? "FAIL: " : "PASS: ") + what + " : " + data, !shouldrun);
    };})(window);


// Defer until document is loaded so that we can write the pretty result boxes
// out.
addEventListener('load', function() {
  // test that allows crypto.generateCRMFRequest eval to run
  try {
      var script =
        'console.log("dynamic script passed to crypto.generateCRMFRequest should execute")';
      crypto.generateCRMFRequest('CN=0', 0, 0, null, script, 384, null, 'rsa-dual-use');
      onevalexecuted(true, "eval(script) inside crypto.generateCRMFRequest",
                     "eval executed during crypto.generateCRMFRequest");
  } catch (e) {
    onevalblocked(true, "eval(script) inside crypto.generateCRMFRequest",
                  "eval was blocked during crypto.generateCRMFRequest");
  }
}, false);
