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
    gDebugger = gPane.contentWindow;
    gWatch = gDebugger.DebuggerView.WatchExpressions;
    gVars = gDebugger.DebuggerView.Variables;

    gDebugger.DebuggerView.togglePanes({ visible: true, animated: false });
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
    gWatch.addExpression("[1, 2, 3]");
    gWatch.addExpression("x = [1, 2, 3]");
    gWatch.addExpression("y = [1, 2, 3]; y.test = 4");
    gWatch.addExpression("z = [1, 2, 3]; z.test = 4; z");
    gWatch.addExpression("t = [1, 2, 3]; t.test = 4; !t");
    gWatch.addExpression("encodeURI(\"\\\")");
    gWatch.addExpression("decodeURI(\"\\\")");
  }

  function performTest()
  {
    is(gWatch._container._parent.querySelectorAll(".dbg-expression[hidden=true]").length, 0,
      "There should be 0 hidden nodes in the watch expressions container");
    is(gWatch._container._parent.querySelectorAll(".dbg-expression:not([hidden=true])").length, 13,
      "There should be 13 visible nodes in the watch expressions container");

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
    is(gWatch._container._parent.querySelectorAll(".dbg-expression[hidden=true]").length, 0,
      "There should be 0 hidden nodes in the watch expressions container");
    is(gWatch._container._parent.querySelectorAll(".dbg-expression:not([hidden=true])").length, 12,
      "There should be 12 visible nodes in the watch expressions container");

    closeDebuggerAndFinish();
  }

  function test1(callback) {
    waitForWatchExpressions(function() {
      info("Performing test1");
      checkWatchExpressions("ReferenceError: a is not defined");
      callback();
    });
    executeSoon(function() {
      gDebuggee.ermahgerd(); // ermahgerd!!
    });
  }

  function test2(callback) {
    waitForWatchExpressions(function() {
      info("Performing test2");
      checkWatchExpressions(undefined);
      callback();
    });
    EventUtils.sendMouseEvent({ type: "mousedown" },
      gDebugger.document.getElementById("resume"),
      gDebugger);
  }

  function test3(callback) {
    waitForWatchExpressions(function() {
      info("Performing test3");
      checkWatchExpressions({ type: "object", class: "Object" });
      callback();
    });
    EventUtils.sendMouseEvent({ type: "mousedown" },
      gDebugger.document.getElementById("resume"),
      gDebugger);
  }

  function test4(callback) {
    waitForWatchExpressions(function() {
      info("Performing test4");
      checkWatchExpressions(5, 12);
      callback();
    });
    executeSoon(function() {
      gWatch.addExpression("a = 5");
      EventUtils.sendKey("RETURN");
    });
  }

  function test5(callback) {
    waitForWatchExpressions(function() {
      info("Performing test5");
      checkWatchExpressions(5, 12);
      callback();
    });
    executeSoon(function() {
      gWatch.addExpression("encodeURI(\"\\\")");
      EventUtils.sendKey("RETURN");
    });
  }

  function test6(callback) {
    waitForWatchExpressions(function() {
      info("Performing test6");
      checkWatchExpressions(5, 12);
      callback();
    })
    executeSoon(function() {
      gWatch.addExpression("decodeURI(\"\\\")");
      EventUtils.sendKey("RETURN");
    });
  }

  function test7(callback) {
    waitForWatchExpressions(function() {
      info("Performing test7");
      checkWatchExpressions(5, 12);
      callback();
    });
    executeSoon(function() {
      gWatch.addExpression("?");
      EventUtils.sendKey("RETURN");
    });
  }

  function test8(callback) {
    waitForWatchExpressions(function() {
      info("Performing test8");
      checkWatchExpressions(5, 12);
      callback();
    });
    executeSoon(function() {
      gWatch.addExpression("a");
      EventUtils.sendKey("RETURN");
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

  function checkWatchExpressions(expected, total = 11) {
    is(gWatch._container._parent.querySelectorAll(".dbg-expression[hidden=true]").length, total,
      "There should be " + total + " hidden nodes in the watch expressions container");
    is(gWatch._container._parent.querySelectorAll(".dbg-expression:not([hidden=true])").length, 0,
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
    let w7 = scope.get("x = [1, 2, 3]");
    let w8 = scope.get("y = [1, 2, 3]; y.test = 4");
    let w9 = scope.get("z = [1, 2, 3]; z.test = 4; z");
    let w10 = scope.get("t = [1, 2, 3]; t.test = 4; !t");
    let w11 = scope.get("encodeURI(\"\\\")");
    let w12 = scope.get("decodeURI(\"\\\")");

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
    ok(!w11, "The eleventh watch expression should not be present in the scope");
    ok(!w12, "The twelveth watch expression should not be present in the scope");

    is(w1.value, "a", "The first value is correct");
    is(w2.value, "a", "The second value is correct");
    is(w3.value, "a\"\"", "The third value is correct");
    is(w4.value, "a''", "The fourth value is correct");
    is(w5.value, "SyntaxError: syntax error", "The fifth value is correct");

    if (typeof expected == "object") {
      is(w6.value.type, expected.type, "The sixth value type is correct");
      is(w6.value.class, expected.class, "The sixth value class is correct");
    } else {
      is(w6.value, expected, "The sixth value is correct");
    }

    is(w7.value.type, "object", "The seventh value type is correct");
    is(w7.value.class, "Array", "The seventh value class is correct");

    is(w8.value, "4", "The eight value is correct");

    is(w9.value.type, "object", "The ninth value type is correct");
    is(w9.value.class, "Array", "The ninth value class is correct");

    is(w10.value, false, "The tenth value is correct");
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
