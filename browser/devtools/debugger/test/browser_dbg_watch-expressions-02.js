/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 727429: Test the debugger watch expressions.
 */

const TAB_URL = EXAMPLE_URL + "doc_watch-expressions.html";

function test() {
  // Debug test slaves are a bit slow at this test.
  requestLongerTimeout(2);

  let gTab, gPanel, gDebugger;
  let gWatch, gVariables;

  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gWatch = gDebugger.DebuggerView.WatchExpressions;
    gVariables = gDebugger.DebuggerView.Variables;

    gDebugger.DebuggerView.toggleInstrumentsPane({ visible: true, animated: false });

    waitForSourceShown(gPanel, ".html", 1)
      .then(addExpressions)
      .then(performTest)
      .then(finishTest)
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });

  function addExpressions() {
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

  function performTest() {
    let deferred = promise.defer();

    is(gDebugger.document.querySelectorAll(".dbg-expression[hidden=true]").length, 0,
      "There should be 0 hidden nodes in the watch expressions container");
    is(gDebugger.document.querySelectorAll(".dbg-expression:not([hidden=true])").length, 27,
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
                      deferred.resolve();
                    });
                  });
                });
              });
            });
          });
        });
      });
    });

    return deferred.promise;
  }

  function finishTest() {
    is(gDebugger.document.querySelectorAll(".dbg-expression[hidden=true]").length, 0,
      "There should be 0 hidden nodes in the watch expressions container");
    is(gDebugger.document.querySelectorAll(".dbg-expression:not([hidden=true])").length, 27,
      "There should be 27 visible nodes in the watch expressions container");
  }

  function test1(aCallback) {
    gDebugger.once(gDebugger.EVENTS.FETCHED_WATCH_EXPRESSIONS, () => {
      checkWatchExpressions(26, {
        a: "ReferenceError: a is not defined",
        this: { type: "object", class: "Object" },
        prop: { type: "object", class: "String" },
        args: { type: "undefined" }
      });
      aCallback();
    });

    callInTab(gTab, "test");
  }

  function test2(aCallback) {
    gDebugger.once(gDebugger.EVENTS.FETCHED_WATCH_EXPRESSIONS, () => {
      checkWatchExpressions(26, {
        a: { type: "undefined" },
        this: { type: "object", class: "Window" },
        prop: { type: "undefined" },
        args: "sensational"
      });
      aCallback();
    });

    EventUtils.sendMouseEvent({ type: "mousedown" },
      gDebugger.document.getElementById("resume"),
      gDebugger);
  }

  function test3(aCallback) {
    gDebugger.once(gDebugger.EVENTS.FETCHED_WATCH_EXPRESSIONS, () => {
      checkWatchExpressions(26, {
        a: { type: "object", class: "Object" },
        this: { type: "object", class: "Window" },
        prop: { type: "undefined" },
        args: "sensational"
      });
      aCallback();
    });

    EventUtils.sendMouseEvent({ type: "mousedown" },
      gDebugger.document.getElementById("resume"),
      gDebugger);
  }

  function test4(aCallback) {
    gDebugger.once(gDebugger.EVENTS.FETCHED_WATCH_EXPRESSIONS, () => {
      checkWatchExpressions(27, {
        a: 5,
        this: { type: "object", class: "Window" },
        prop: { type: "undefined" },
        args: "sensational"
      });
      aCallback();
    });

    gWatch.addExpression("a = 5");
    EventUtils.sendKey("RETURN", gDebugger);
  }

  function test5(aCallback) {
    gDebugger.once(gDebugger.EVENTS.FETCHED_WATCH_EXPRESSIONS, () => {
      checkWatchExpressions(27, {
        a: 5,
        this: { type: "object", class: "Window" },
        prop: { type: "undefined" },
        args: "sensational"
      });
      aCallback();
    });

    gWatch.addExpression("encodeURI(\"\\\")");
    EventUtils.sendKey("RETURN", gDebugger);
  }

  function test6(aCallback) {
    gDebugger.once(gDebugger.EVENTS.FETCHED_WATCH_EXPRESSIONS, () => {
      checkWatchExpressions(27, {
        a: 5,
        this: { type: "object", class: "Window" },
        prop: { type: "undefined" },
        args: "sensational"
      });
      aCallback();
    })

    gWatch.addExpression("decodeURI(\"\\\")");
    EventUtils.sendKey("RETURN", gDebugger);
  }

  function test7(aCallback) {
    gDebugger.once(gDebugger.EVENTS.FETCHED_WATCH_EXPRESSIONS, () => {
      checkWatchExpressions(27, {
        a: 5,
        this: { type: "object", class: "Window" },
        prop: { type: "undefined" },
        args: "sensational"
      });
      aCallback();
    });

    gWatch.addExpression("?");
    EventUtils.sendKey("RETURN", gDebugger);
  }

  function test8(aCallback) {
    gDebugger.once(gDebugger.EVENTS.FETCHED_WATCH_EXPRESSIONS, () => {
      checkWatchExpressions(27, {
        a: 5,
        this: { type: "object", class: "Window" },
        prop: { type: "undefined" },
        args: "sensational"
      });
      aCallback();
    });

    gWatch.addExpression("a");
    EventUtils.sendKey("RETURN", gDebugger);
  }

  function test9(aCallback) {
    gDebugger.once(gDebugger.EVENTS.AFTER_FRAMES_CLEARED, () => {
      aCallback();
    });

    EventUtils.sendMouseEvent({ type: "mousedown" },
      gDebugger.document.getElementById("resume"),
      gDebugger);
  }

  function checkWatchExpressions(aTotal, aExpectedExpressions) {
    let {
      a: expected_a,
      this: expected_this,
      prop: expected_prop,
      args: expected_args
    } = aExpectedExpressions;

    is(gDebugger.document.querySelectorAll(".dbg-expression[hidden=true]").length, aTotal,
      "There should be " + aTotal + " hidden nodes in the watch expressions container.");
    is(gDebugger.document.querySelectorAll(".dbg-expression:not([hidden=true])").length, 0,
      "There should be 0 visible nodes in the watch expressions container.");

    let label = gDebugger.L10N.getStr("watchExpressionsScopeLabel");
    let scope = gVariables._currHierarchy.get(label);

    ok(scope, "There should be a wach expressions scope in the variables view.");
    is(scope._store.size, aTotal, "There should be " + aTotal + " evaluations availalble.");

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

    ok(w1, "The first watch expression should be present in the scope.");
    ok(w2, "The second watch expression should be present in the scope.");
    ok(w3, "The third watch expression should be present in the scope.");
    ok(w4, "The fourth watch expression should be present in the scope.");
    ok(w5, "The fifth watch expression should be present in the scope.");
    ok(w6, "The sixth watch expression should be present in the scope.");
    ok(w7, "The seventh watch expression should be present in the scope.");
    ok(w8, "The eight watch expression should be present in the scope.");
    ok(w9, "The ninth watch expression should be present in the scope.");
    ok(w10, "The tenth watch expression should be present in the scope.");
    ok(w11, "The eleventh watch expression should be present in the scope.");
    ok(w12, "The twelfth watch expression should be present in the scope.");
    ok(w13, "The 13th watch expression should be present in the scope.");
    ok(w14, "The 14th watch expression should be present in the scope.");
    ok(w15, "The 15th watch expression should be present in the scope.");
    ok(w16, "The 16th watch expression should be present in the scope.");
    ok(w17, "The 17th watch expression should be present in the scope.");
    ok(w18, "The 18th watch expression should be present in the scope.");
    ok(w19, "The 19th watch expression should be present in the scope.");
    ok(w20, "The 20th watch expression should be present in the scope.");
    ok(w21, "The 21st watch expression should be present in the scope.");
    ok(w22, "The 22nd watch expression should be present in the scope.");
    ok(w23, "The 23nd watch expression should be present in the scope.");
    ok(w24, "The 24th watch expression should be present in the scope.");
    ok(w25, "The 25th watch expression should be present in the scope.");
    ok(w26, "The 26th watch expression should be present in the scope.");
    ok(!w27, "The 27th watch expression should not be present in the scope.");

    is(w1.value, "a", "The first value is correct.");
    is(w2.value, "a", "The second value is correct.");
    is(w3.value, "a\"\"", "The third value is correct.");
    is(w4.value, "a''", "The fourth value is correct.");
    is(w5.value, "SyntaxError: expected expression, got '?'", "The fifth value is correct.");

    if (typeof expected_a == "object") {
      is(w6.value.type, expected_a.type, "The sixth value type is correct.");
      is(w6.value.class, expected_a.class, "The sixth value class is correct.");
    } else {
      is(w6.value, expected_a, "The sixth value is correct.");
    }

    if (typeof expected_this == "object") {
      is(w7.value.type, expected_this.type, "The seventh value type is correct.");
      is(w7.value.class, expected_this.class, "The seventh value class is correct.");
    } else {
      is(w7.value, expected_this, "The seventh value is correct.");
    }

    if (typeof expected_prop == "object") {
      is(w8.value.type, expected_prop.type, "The eighth value type is correct.");
      is(w8.value.class, expected_prop.class, "The eighth value class is correct.");
    } else {
      is(w8.value, expected_prop, "The eighth value is correct.");
    }

    is(w9.value.type, "object", "The ninth value type is correct.");
    is(w9.value.class, "Array", "The ninth value class is correct.");
    is(w10.value.type, "object", "The tenth value type is correct.");
    is(w10.value.class, "Array", "The tenth value class is correct.");
    is(w11.value, "4", "The eleventh value is correct.");
    is(w12.value.type, "object", "The eleventh value type is correct.");
    is(w12.value.class, "Array", "The twelfth value class is correct.");
    is(w13.value, false, "The 13th value is correct.");

    if (typeof expected_args == "object") {
      is(w14.value.type, expected_args.type, "The 14th value type is correct.");
      is(w14.value.class, expected_args.class, "The 14th value class is correct.");
    } else {
      is(w14.value, expected_args, "The 14th value is correct.");
    }

    is(w15.value, "SyntaxError: unterminated string literal", "The 15th value is correct.");
    is(w16.value, "SyntaxError: unterminated string literal", "The 16th value is correct.");
    is(w17.value, "URIError: malformed URI sequence", "The 17th value is correct.");

    is(w18.value.type, "undefined", "The 18th value type is correct.");
    is(w18.value.class, undefined, "The 18th value class is correct.");

    is(w19.value.type, "undefined", "The 19th value type is correct.");
    is(w19.value.class, undefined, "The 19th value class is correct.");

    is(w20.value, "SyntaxError: expected expression, got '.'", "The 20th value is correct.");
    is(w21.value, "SyntaxError: expected expression, got '.'", "The 21th value is correct.");
    is(w22.value, "TypeError: (intermediate value).foo is not a function", "The 22th value is correct.");
    is(w23.value, "RangeError: invalid array length", "The 23th value is correct.");
    is(w24.value, "RangeError: precision -4 out of range", "The 24th value is correct.");
    is(w25.value, "Error: bazinga", "The 25th value is correct.");
    is(w26.value, "Error: bazinga", "The 26th value is correct.");
  }
}
