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

  // setTimeout(String) test  -- should pass
  try {
    setTimeout('onevalexecuted(true, "setTimeout(String)", "setTimeout with a string was enabled.");', 10);
  } catch (e) {
    onevalblocked(true, "setTimeout(String)",
                  "setTimeout with a string was blocked");
  }

  // setTimeout(function) test  -- should pass
  try {
    setTimeout(function() {
          onevalexecuted(true, "setTimeout(function)",
                        "setTimeout with a function was enabled.")
        }, 10);
  } catch (e) {
    onevalblocked(true, "setTimeout(function)",
                  "setTimeout with a function was blocked");
  }

  // eval() test
  try {
    eval('onevalexecuted(true, "eval(String)", "eval() was enabled.");');
  } catch (e) {
    onevalblocked(true, "eval(String)",
                  "eval() was blocked");
  }

  // eval(foo,bar) test
  try {
    eval('onevalexecuted(true, "eval(String,scope)", "eval() was enabled.");',1);
  } catch (e) {
    onevalblocked(true, "eval(String,object)",
                  "eval() with scope was blocked");
  }

  // [foo,bar].sort(eval) test
  try {
    ['onevalexecuted(true, "[String, obj].sort(eval)", "eval() was enabled.");',1].sort(eval);
  } catch (e) {
    onevalblocked(true, "[String, obj].sort(eval)",
                  "eval() with scope via sort was blocked");
  }

  // [].sort.call([foo,bar], eval) test
  try {
    [].sort.call(['onevalexecuted(true, "[String, obj].sort(eval)", "eval() was enabled.");',1], eval);
  } catch (e) {
    onevalblocked(true, "[].sort.call([String, obj], eval)",
                  "eval() with scope via sort/call was blocked");
  }

  // new Function() test
  try {
    var fcn = new Function('onevalexecuted(true, "new Function(String)", "new Function(String) was enabled.");');
    fcn();
  } catch (e) {
    onevalblocked(true, "new Function(String)",
                  "new Function(String) was blocked.");
  }

  function checkResult() {
    //alert(bar);
    if (bar) {
      onevalexecuted(true, "setTimeout(eval, 0, str)",
                      "setTimeout(eval, 0, string) was enabled.");
    } else {
      onevalblocked(true, "setTimeout(eval, 0, str)",
                          "setTimeout(eval, 0, str) was blocked.");
    }
  }

  var bar = false;

  function foo() {
    bar = true;
  }

  window.foo = foo;

  // setTimeout(eval, 0, str)

  // error is not catchable here

  setTimeout(eval, 0, 'window.foo();');

  setTimeout(checkResult.bind(this), 0);

}, false);



