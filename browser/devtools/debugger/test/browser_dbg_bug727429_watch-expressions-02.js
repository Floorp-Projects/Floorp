/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 727429: test the debugger watch expressions.
 */

const TAB_URL = EXAMPLE_URL + "browser_dbg_watch-expressions.html";

let gPane = null;
let gTab = null;
let gDebuggee = null;
let gDebugger = null;
let gWatch = null;
let gVars = null;

function test()
{
  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.panelWin;
    gWatch = gDebugger.DebuggerView.WatchExpressions;
    gVars = gDebugger.DebuggerView.Variables;

    gDebugger.DebuggerView.toggleInstrumentsPane({ visible: true, animated: false });
    addExpressions();
    performTest();
  });

  function addExpressions()
  {
    gWatch.addExpression("'a'");
    gWatch.addExpression("\"a\"");
    gWatch.addExpression("'a\"\"'");
    gWatch.addExpression("\"a''\"");
    gWatch.addExpression("?");
    gWatch.addExpression("a");
    gWatch.addExpression("this");
    gWatch.addExpression("this.canada");
    gWatch.addExpression("[1, 2, 3]");
    gWatch.addExpression("x = [1, 2, 3]");
    gWatch.addExpression("y = [1, 2, 3]; y.test = 4");
    gWatch.addExpression("z = [1, 2, 3]; z.test = 4; z");
    gWatch.addExpression("t = [1, 2, 3]; t.test = 4; !t");
    gWatch.addExpression("arguments[0]");
    gWatch.addExpression("encodeURI(\"\\\")");
    gWatch.addExpression("decodeURI(\"\\\")");
    gWatch.addExpression("decodeURIComponent(\"%\")");
    gWatch.addExpression("//");
    gWatch.addExpression("// 42");
    gWatch.addExpression("{}.foo");
    gWatch.addExpression("{}.foo()");
    gWatch.addExpression("({}).foo()");
    gWatch.addExpression("new Array(-1)");
    gWatch.addExpression("4.2.toExponential(-4.2)");
    gWatch.addExpression("throw new Error(\"bazinga\")");
    gWatch.addExpression("({ get error() { throw new Error(\"bazinga\") } }).error");
    gWatch.addExpression("throw { get name() { throw \"bazinga\" } }");
  }

  function performTest()
  {
    is(gWatch.widget._parent.querySelectorAll(".dbg-expression[hidden=true]").length, 0,
      "There should be 0 hidden nodes in the watch expressions container");
    is(gWatch.widget._parent.querySelectorAll(".dbg-expression:not([hidden=true])").length, 27,
      "There should be 27 visible nodes in the watch expressions container");

    test1(function() {
      test2(function() {
        test3(function() {
          test4(function() {
            test5(function() {
              test6(function() {
                test7(function() {
                  test8(function() {
                    test9(function() {
                      finishTest();
                    });
                  });
                });
              });
            });
          });
        });
      });
    });
  }

  function finishTest()
  {
    is(gWatch.widget._parent.querySelectorAll(".dbg-expression[hidden=true]").length, 0,
      "There should be 0 hidden nodes in the watch expressions container");
    is(gWatch.widget._parent.querySelectorAll(".dbg-expression:not([hidden=true])").length, 27,
      "There should be 27 visible nodes in the watch expressions container");

    closeDebuggerAndFinish();
  }

  function test1(callback) {
    waitForWatchExpressions(function() {
      info("Performing test1");
      checkWatchExpressions("ReferenceError: a is not defined",
                            { type: "object", class: "Object" },
                            { type: "object", class: "String" },
                            undefined,
                            26);
      callback();
    });
    executeSoon(function() {
      gDebuggee.test(); // ermahgerd!!
    });
  }

  function test2(callback) {
    waitForWatchExpressions(function() {
      info("Performing test2");
      checkWatchExpressions(undefined,
                            { type: "object", class: "Window" },
                            undefined,
                            "sensational",
                            26);
      callback();
    });
    EventUtils.sendMouseEvent({ type: "mousedown" },
      gDebugger.document.getElementById("resume"),
      gDebugger);
  }

  function test3(callback) {
    waitForWatchExpressions(function() {
      info("Performing test3");
      checkWatchExpressions({ type: "object", class: "Object" },
                            { type: "object", class: "Window" },
                            undefined,
                            "sensational",
                            26);
      callback();
    });
    EventUtils.sendMouseEvent({ type: "mousedown" },
      gDebugger.document.getElementById("resume"),
      gDebugger);
  }

  function test4(callback) {
    waitForWatchExpressions(function() {
      info("Performing test4");
      checkWatchExpressions(5,
                            { type: "object", class: "Window" },
                            undefined,
                            "sensational",
                            27);
      callback();
    });
    executeSoon(function() {
      gWatch.addExpression("a = 5");
      EventUtils.sendKey("RETURN", gDebugger);
    });
  }

  function test5(callback) {
    waitForWatchExpressions(function() {
      info("Performing test5");
      checkWatchExpressions(5,
                            { type: "object", class: "Window" },
                            undefined,
                            "sensational",
                            27);
      callback();
    });
    executeSoon(function() {
      gWatch.addExpression("encodeURI(\"\\\")");
      EventUtils.sendKey("RETURN", gDebugger);
    });
  }

  function test6(callback) {
    waitForWatchExpressions(function() {
      info("Performing test6");
      checkWatchExpressions(5,
                            { type: "object", class: "Window" },
                            undefined,
                            "sensational",
                            27);
      callback();
    })
    executeSoon(function() {
      gWatch.addExpression("decodeURI(\"\\\")");
      EventUtils.sendKey("RETURN", gDebugger);
    });
  }

  function test7(callback) {
    waitForWatchExpressions(function() {
      info("Performing test7");
      checkWatchExpressions(5,
                            { type: "object", class: "Window" },
                            undefined,
                            "sensational",
                            27);
      callback();
    });
    executeSoon(function() {
      gWatch.addExpression("?");
      EventUtils.sendKey("RETURN", gDebugger);
    });
  }

  function test8(callback) {
    waitForWatchExpressions(function() {
      info("Performing test8");
      checkWatchExpressions(5,
                            { type: "object", class: "Window" },
                            undefined,
                            "sensational",
                            27);
      callback();
    });
    executeSoon(function() {
      gWatch.addExpression("a");
      EventUtils.sendKey("RETURN", gDebugger);
    });
  }

  function test9(callback) {
    waitForAfterFramesCleared(function() {
      info("Performing test9");
      callback();
    });
    EventUtils.sendMouseEvent({ type: "mousedown" },
      gDebugger.document.getElementById("resume"),
      gDebugger);
  }

  function waitForAfterFramesCleared(callback) {
    gDebugger.addEventListener("Debugger:AfterFramesCleared", function onClear() {
      gDebugger.removeEventListener("Debugger:AfterFramesCleared", onClear, false);
      executeSoon(callback);
    }, false);
  }

  function waitForWatchExpressions(callback) {
    gDebugger.addEventListener("Debugger:FetchedWatchExpressions", function onFetch() {
      gDebugger.removeEventListener("Debugger:FetchedWatchExpressions", onFetch, false);
      executeSoon(callback);
    }, false);
  }

  function checkWatchExpressions(expected_a,
                                 expected_this,
                                 expected_prop,
                                 expected_arguments,
                                 total)
  {
    is(gWatch.widget._parent.querySelectorAll(".dbg-expression[hidden=true]").length, total,
      "There should be " + total + " hidden nodes in the watch expressions container");
    is(gWatch.widget._parent.querySelectorAll(".dbg-expression:not([hidden=true])").length, 0,
      "There should be 0 visible nodes in the watch expressions container");

    let label = gDebugger.L10N.getStr("watchExpressionsScopeLabel");
    let scope = gVars._currHierarchy.get(label);

    ok(scope, "There should be a wach expressions scope in the variables view");
    is(scope._store.size, total, "There should be " + total + " evaluations availalble");

    let w1 = scope.get("'a'");
    let w2 = scope.get("\"a\"");
    let w3 = scope.get("'a\"\"'");
    let w4 = scope.get("\"a''\"");
    let w5 = scope.get("?");
    let w6 = scope.get("a");
    let w7 = scope.get("this");
    let w8 = scope.get("this.canada");
    let w9 = scope.get("[1, 2, 3]");
    let w10 = scope.get("x = [1, 2, 3]");
    let w11 = scope.get("y = [1, 2, 3]; y.test = 4");
    let w12 = scope.get("z = [1, 2, 3]; z.test = 4; z");
    let w13 = scope.get("t = [1, 2, 3]; t.test = 4; !t");
    let w14 = scope.get("arguments[0]");
    let w15 = scope.get("encodeURI(\"\\\")");
    let w16 = scope.get("decodeURI(\"\\\")");
    let w17 = scope.get("decodeURIComponent(\"%\")");
    let w18 = scope.get("//");
    let w19 = scope.get("// 42");
    let w20 = scope.get("{}.foo");
    let w21 = scope.get("{}.foo()");
    let w22 = scope.get("({}).foo()");
    let w23 = scope.get("new Array(-1)");
    let w24 = scope.get("4.2.toExponential(-4.2)");
    let w25 = scope.get("throw new Error(\"bazinga\")");
    let w26 = scope.get("({ get error() { throw new Error(\"bazinga\") } }).error");
    let w27 = scope.get("throw { get name() { throw \"bazinga\" } }");

    ok(w1, "The first watch expression should be present in the scope");
    ok(w2, "The second watch expression should be present in the scope");
    ok(w3, "The third watch expression should be present in the scope");
    ok(w4, "The fourth watch expression should be present in the scope");
    ok(w5, "The fifth watch expression should be present in the scope");
    ok(w6, "The sixth watch expression should be present in the scope");
    ok(w7, "The seventh watch expression should be present in the scope");
    ok(w8, "The eight watch expression should be present in the scope");
    ok(w9, "The ninth watch expression should be present in the scope");
    ok(w10, "The tenth watch expression should be present in the scope");
    ok(w11, "The eleventh watch expression should be present in the scope");
    ok(w12, "The twelfth watch expression should be present in the scope");
    ok(w13, "The 13th watch expression should be present in the scope");
    ok(w14, "The 14th watch expression should be present in the scope");
    ok(w15, "The 15th watch expression should be present in the scope");
    ok(w16, "The 16th watch expression should be present in the scope");
    ok(w17, "The 17th watch expression should be present in the scope");
    ok(w18, "The 18th watch expression should be present in the scope");
    ok(w19, "The 19th watch expression should be present in the scope");
    ok(w20, "The 20th watch expression should be present in the scope");
    ok(w21, "The 21st watch expression should be present in the scope");
    ok(w22, "The 22nd watch expression should be present in the scope");
    ok(w23, "The 23nd watch expression should be present in the scope");
    ok(w24, "The 24th watch expression should be present in the scope");
    ok(w25, "The 25th watch expression should be present in the scope");
    ok(w26, "The 26th watch expression should be present in the scope");
    ok(!w27, "The 27th watch expression should not be present in the scope");

    is(w1.value, "a", "The first value is correct");
    is(w2.value, "a", "The second value is correct");
    is(w3.value, "a\"\"", "The third value is correct");
    is(w4.value, "a''", "The fourth value is correct");
    is(w5.value, "SyntaxError: syntax error", "The fifth value is correct");

    if (typeof expected_a == "object") {
      is(w6.value.type, expected_a.type, "The sixth value type is correct");
      is(w6.value.class, expected_a.class, "The sixth value class is correct");
    } else {
      is(w6.value, expected_a, "The sixth value is correct");
    }

    if (typeof expected_this == "object") {
      is(w7.value.type, expected_this.type, "The seventh value type is correct");
      is(w7.value.class, expected_this.class, "The seventh value class is correct");
    } else {
      is(w7.value, expected_this, "The seventh value is correct");
    }

    if (typeof expected_prop == "object") {
      is(w8.value.type, expected_prop.type, "The eighth value type is correct");
      is(w8.value.class, expected_prop.class, "The eighth value class is correct");
    } else {
      is(w8.value, expected_prop, "The eighth value is correct");
    }

    is(w9.value.type, "object", "The ninth value type is correct");
    is(w9.value.class, "Array", "The ninth value class is correct");
    is(w10.value.type, "object", "The tenth value type is correct");
    is(w10.value.class, "Array", "The tenth value class is correct");
    is(w11.value, "4", "The eleventh value is correct");
    is(w12.value.type, "object", "The eleventh value type is correct");
    is(w12.value.class, "Array", "The twelfth value class is correct");
    is(w13.value, false, "The 13th value is correct");
    is(w14.value, expected_arguments, "The 14th value is correct");

    is(w15.value, "SyntaxError: unterminated string literal", "The 15th value is correct");
    is(w16.value, "SyntaxError: unterminated string literal", "The 16th value is correct");
    is(w17.value, "URIError: malformed URI sequence", "The 17th value is correct");
    is(w18.value, undefined, "The 18th value is correct");
    is(w19.value, undefined, "The 19th value is correct");
    is(w20.value, "SyntaxError: syntax error", "The 20th value is correct");
    is(w21.value, "SyntaxError: syntax error", "The 21th value is correct");
    is(w22.value, "TypeError: (intermediate value).foo is not a function", "The 22th value is correct");
    is(w23.value, "RangeError: invalid array length", "The 23th value is correct");
    is(w24.value, "RangeError: precision -4 out of range", "The 24st value is correct");
    is(w25.value, "Error: bazinga", "The 25nd value is correct");
    is(w26.value, "Error: bazinga", "The 26rd value is correct");
  }

  registerCleanupFunction(function() {
    removeTab(gTab);
    gPane = null;
    gTab = null;
    gDebuggee = null;
    gDebugger = null;
    gWatch = null;
    gVars = null;
  });
}
