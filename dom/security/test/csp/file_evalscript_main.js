// some javascript for the CSP eval() tests

function logResult(str, passed) {
  var elt = document.createElement('div');
  var color = passed ? "#cfc;" : "#fcc";
  elt.setAttribute('style', 'background-color:' + color + '; width:100%; border:1px solid black; padding:3px; margin:4px;');
  elt.innerHTML = str;
  document.body.appendChild(elt);
}

window._testResults = {};

// check values for return values from blocked timeout or intervals
var verifyZeroRetVal = (function(window) {
  return function(val, details) {
    logResult((val === 0 ? "PASS: " : "FAIL: ") + "Blocked interval/timeout should have zero return value; " + details, val === 0);
    window.parent.verifyZeroRetVal(val, details);
  };})(window);

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
  // setTimeout(String) test -- mutate something in the window._testResults
  // obj, then check it.
  {
    var str_setTimeoutWithStringRan = 'onevalexecuted(false, "setTimeout(String)", "setTimeout with a string was enabled.");';
    function fcn_setTimeoutWithStringCheck() {
      if (this._testResults["setTimeout(String)"] !== "ran") {
        onevalblocked(false, "setTimeout(String)",
                      "setTimeout with a string was blocked");
      }
    }
    setTimeout(fcn_setTimeoutWithStringCheck.bind(window), 10);
    var res = setTimeout(str_setTimeoutWithStringRan, 10);
    verifyZeroRetVal(res, "setTimeout(String)");
  }

  // setInterval(String) test -- mutate something in the window._testResults
  // obj, then check it.
  {
    var str_setIntervalWithStringRan = 'onevalexecuted(false, "setInterval(String)", "setInterval with a string was enabled.");';
    function fcn_setIntervalWithStringCheck() {
      if (this._testResults["setInterval(String)"] !== "ran") {
        onevalblocked(false, "setInterval(String)",
                      "setInterval with a string was blocked");
      }
    }
    setTimeout(fcn_setIntervalWithStringCheck.bind(window), 10);
    var res = setInterval(str_setIntervalWithStringRan, 10);
    verifyZeroRetVal(res, "setInterval(String)");

    // emergency cleanup, just in case.
    if (res != 0) {
      setTimeout(function () { clearInterval(res); }, 15);
    }
  }

  // setTimeout(function) test -- mutate something in the window._testResults
  // obj, then check it.
  {
    function fcn_setTimeoutWithFunctionRan() {
      onevalexecuted(true, "setTimeout(function)",
                    "setTimeout with a function was enabled.")
    }
    function fcn_setTimeoutWithFunctionCheck() {
      if (this._testResults["setTimeout(function)"] !== "ran") {
        onevalblocked(true, "setTimeout(function)",
                      "setTimeout with a function was blocked");
      }
    }
    setTimeout(fcn_setTimeoutWithFunctionRan.bind(window), 10);
    setTimeout(fcn_setTimeoutWithFunctionCheck.bind(window), 10);
  }

  // eval() test -- should throw exception as per spec
  try {
    eval('onevalexecuted(false, "eval(String)", "eval() was enabled.");');
  } catch (e) {
    onevalblocked(false, "eval(String)",
                  "eval() was blocked");
  }

  // eval(foo,bar) test -- should throw exception as per spec
  try {
    eval('onevalexecuted(false, "eval(String,scope)", "eval() was enabled.");',1);
  } catch (e) {
    onevalblocked(false, "eval(String,object)",
                  "eval() with scope was blocked");
  }

  // [foo,bar].sort(eval) test -- should throw exception as per spec
  try {
    ['onevalexecuted(false, "[String, obj].sort(eval)", "eval() was enabled.");',1].sort(eval);
  } catch (e) {
    onevalblocked(false, "[String, obj].sort(eval)",
                  "eval() with scope via sort was blocked");
  }

  // [].sort.call([foo,bar], eval) test -- should throw exception as per spec
  try {
    [].sort.call(['onevalexecuted(false, "[String, obj].sort(eval)", "eval() was enabled.");',1], eval);
  } catch (e) {
    onevalblocked(false, "[].sort.call([String, obj], eval)",
                  "eval() with scope via sort/call was blocked");
  }

  // new Function() test -- should throw exception as per spec
  try {
    var fcn = new Function('onevalexecuted(false, "new Function(String)", "new Function(String) was enabled.");');
    fcn();
  } catch (e) {
    onevalblocked(false, "new Function(String)",
                  "new Function(String) was blocked.");
  }

  // setTimeout(eval, 0, str)
  {
    // error is not catchable here, instead, we're going to side-effect
    // 'worked'.
    var worked = false;

    setTimeout(eval, 0, 'worked = true');
    setTimeout(function(worked) {
                  if (worked) {
                    onevalexecuted(false, "setTimeout(eval, 0, str)",
                                    "setTimeout(eval, 0, string) was enabled.");
                  } else {
                    onevalblocked(false, "setTimeout(eval, 0, str)",
                                        "setTimeout(eval, 0, str) was blocked.");
                  }
                }, 0, worked);
  }

}, false);



