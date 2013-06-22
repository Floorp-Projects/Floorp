/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the property view knows how to edit getters and setters.
 */

const TAB_URL = EXAMPLE_URL + "browser_dbg_frame-parameters.html";

var gPane = null;
var gTab = null;
var gDebugger = null;
var gVars = null;
var gWatch = null;

requestLongerTimeout(2);

function test()
{
  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gPane = aPane;
    gDebugger = gPane.panelWin;
    gVars = gDebugger.DebuggerView.Variables;
    gWatch = gDebugger.DebuggerView.WatchExpressions;

    gVars.switch = function() {};
    gVars.delete = function() {};

    prepareVariablesView();
  });
}

function prepareVariablesView() {
  gDebugger.addEventListener("Debugger:FetchedVariables", function test() {
    gDebugger.removeEventListener("Debugger:FetchedVariables", test, false);
    Services.tm.currentThread.dispatch({ run: function() {

      testVariablesView();

    }}, 0);
  }, false);

  EventUtils.sendMouseEvent({ type: "click" },
    content.document.querySelector("button"),
    content.window);
}

function testVariablesView()
{
  executeSoon(function() {
    addWatchExpressions(function() {
      testEdit("set", "this._prop = value + ' BEER CAN'", function() {
        testEdit("set", "{ this._prop = value + ' BEACON' }", function() {
          testEdit("set", "{ this._prop = value + ' BEACON;'; }", function() {
            testEdit("set", "{ return this._prop = value + ' BEACON;;'; }", function() {
              testEdit("set", "function(value) { this._prop = value + ' BACON' }", function() {
                testEdit("get", "'brelx BEER CAN'", function() {
                  testEdit("get", "{ 'brelx BEACON' }", function() {
                    testEdit("get", "{ 'brelx BEACON;'; }", function() {
                      testEdit("get", "{ return 'brelx BEACON;;'; }", function() {
                        testEdit("get", "function() { return 'brelx BACON'; }", function() {
                          testEdit("get", "bogus", function() {
                            testEdit("set", "sugob", function() {
                              testEdit("get", "", function() {
                                testEdit("set", "", function() {
                                  waitForWatchExpressions(function() {
                                    testEdit("self", "2507", function() {
                                      closeDebuggerAndFinish();
                                    }, {
                                      "myVar.prop": 2507,
                                      "myVar.prop + 42": "250742"
                                    });
                                  })
                                  gWatch.deleteExpression({ name: "myVar.prop = 'xlerb'" });
                                }, {
                                  "myVar.prop": "xlerb",
                                  "myVar.prop + 42": NaN,
                                  "myVar.prop = 'xlerb'": "xlerb"
                                });
                              }, {
                                "myVar.prop": undefined,
                                "myVar.prop + 42": NaN,
                                "myVar.prop = 'xlerb'": "ReferenceError: sugob is not defined"
                              });
                            }, {
                              "myVar.prop": "ReferenceError: bogus is not defined",
                              "myVar.prop + 42": "ReferenceError: bogus is not defined",
                              "myVar.prop = 'xlerb'": "ReferenceError: sugob is not defined"
                            });
                          }, {
                            "myVar.prop": "ReferenceError: bogus is not defined",
                            "myVar.prop + 42": "ReferenceError: bogus is not defined",
                            "myVar.prop = 'xlerb'": "xlerb"
                          });
                        }, {
                          "myVar.prop": "brelx BACON",
                          "myVar.prop + 42": "brelx BACON42",
                          "myVar.prop = 'xlerb'": "xlerb"
                        });
                      }, {
                        "myVar.prop": "brelx BEACON;;",
                        "myVar.prop + 42": "brelx BEACON;;42",
                        "myVar.prop = 'xlerb'": "xlerb"
                      });
                    }, {
                      "myVar.prop": undefined,
                      "myVar.prop + 42": NaN,
                      "myVar.prop = 'xlerb'": "xlerb"
                    });
                  }, {
                    "myVar.prop": undefined,
                    "myVar.prop + 42": NaN,
                    "myVar.prop = 'xlerb'": "xlerb"
                  });
                }, {
                  "myVar.prop": "brelx BEER CAN",
                  "myVar.prop + 42": "brelx BEER CAN42",
                  "myVar.prop = 'xlerb'": "xlerb"
                });
              }, {
                "myVar.prop": "xlerb BACON",
                "myVar.prop + 42": "xlerb BACON42",
                "myVar.prop = 'xlerb'": "xlerb"
              });
            }, {
              "myVar.prop": "xlerb BEACON;;",
              "myVar.prop + 42": "xlerb BEACON;;42",
              "myVar.prop = 'xlerb'": "xlerb"
            });
          }, {
            "myVar.prop": "xlerb BEACON;",
            "myVar.prop + 42": "xlerb BEACON;42",
            "myVar.prop = 'xlerb'": "xlerb"
          });
        }, {
          "myVar.prop": "xlerb BEACON",
          "myVar.prop + 42": "xlerb BEACON42",
          "myVar.prop = 'xlerb'": "xlerb"
        });
      }, {
        "myVar.prop": "xlerb BEER CAN",
        "myVar.prop + 42": "xlerb BEER CAN42",
        "myVar.prop = 'xlerb'": "xlerb"
      });
    });
  });
}

function addWatchExpressions(callback)
{
  waitForWatchExpressions(function() {
    let label = gDebugger.L10N.getStr("watchExpressionsScopeLabel");
    let scope = gVars._currHierarchy.get(label);

    ok(scope, "There should be a wach expressions scope in the variables view");
    is(scope._store.size, 1, "There should be 1 evaluation availalble");

    let w1 = scope.get("myVar.prop");
    let w2 = scope.get("myVar.prop + 42");
    let w3 = scope.get("myVar.prop = 'xlerb'");

    ok(w1, "The first watch expression should be present in the scope");
    ok(!w2, "The second watch expression should not be present in the scope");
    ok(!w3, "The third watch expression should not be present in the scope");

    is(w1.value, 42, "The first value is correct.");


    waitForWatchExpressions(function() {
      let label = gDebugger.L10N.getStr("watchExpressionsScopeLabel");
      let scope = gVars._currHierarchy.get(label);

      ok(scope, "There should be a wach expressions scope in the variables view");
      is(scope._store.size, 2, "There should be 2 evaluations availalble");

      let w1 = scope.get("myVar.prop");
      let w2 = scope.get("myVar.prop + 42");
      let w3 = scope.get("myVar.prop = 'xlerb'");

      ok(w1, "The first watch expression should be present in the scope");
      ok(w2, "The second watch expression should be present in the scope");
      ok(!w3, "The third watch expression should not be present in the scope");

      is(w1.value, "42", "The first expression value is correct.");
      is(w2.value, "84", "The second expression value is correct.");


      waitForWatchExpressions(function() {
        let label = gDebugger.L10N.getStr("watchExpressionsScopeLabel");
        let scope = gVars._currHierarchy.get(label);

        ok(scope, "There should be a wach expressions scope in the variables view");
        is(scope._store.size, 3, "There should be 3 evaluations availalble");

        let w1 = scope.get("myVar.prop");
        let w2 = scope.get("myVar.prop + 42");
        let w3 = scope.get("myVar.prop = 'xlerb'");

        ok(w1, "The first watch expression should be present in the scope");
        ok(w2, "The second watch expression should be present in the scope");
        ok(w3, "The third watch expression should be present in the scope");

        is(w1.value, "xlerb", "The first expression value is correct.");
        is(w2.value, "xlerb42", "The second expression value is correct.");
        is(w3.value, "xlerb", "The third expression value is correct.");

        callback();
      });

      gWatch.addExpression("myVar.prop = 'xlerb'");
      gDebugger.editor.focus();
    });

    gWatch.addExpression("myVar.prop + 42");
    gDebugger.editor.focus();
  });

  gWatch.addExpression("myVar.prop");
  gDebugger.editor.focus();
}

function testEdit(what, string, callback, expected)
{
  let localScope = gDebugger.DebuggerView.Variables._list.querySelectorAll(".variables-view-scope")[1],
      localNodes = localScope.querySelector(".variables-view-element-details").childNodes,
      myVar = gVars.getItemForNode(localNodes[11]);

  waitForProperties(function() {
    let prop = myVar.get("prop");
    let getterOrSetter = (what != "self" ? prop.get(what) : prop);

    EventUtils.sendMouseEvent({ type: "mousedown" },
      getterOrSetter._target.querySelector(".title > .value"),
      gDebugger);

    waitForElement(".element-value-input", true, function() {
      waitForWatchExpressions(function() {
        let label = gDebugger.L10N.getStr("watchExpressionsScopeLabel");
        let scope = gVars._currHierarchy.get(label);

        let w1 = scope.get(Object.keys(expected)[0]);
        let w2 = scope.get(Object.keys(expected)[1]);
        let w3 = scope.get(Object.keys(expected)[2]);

        if (w1) {
          if (isNaN(expected[w1.name]) && typeof expected[w1.name] == "number") {
            ok(isNaN(w1.value),
              "The first expression value is correct after the edit (NaN).");
          } else if (expected[w1.name] === undefined) {
            is(w1.value.type, "undefined",
              "The first expression value is correct after the edit (undefined).");
          } else {
            is(w1.value, expected[w1.name],
              "The first expression value is correct after the edit.");
          }
          info(w1.value + " is equal to " + expected[w1.name]);
        }
        if (w2) {
          if (isNaN(expected[w2.name]) && typeof expected[w2.name] == "number") {
            ok(isNaN(w2.value),
              "The second expression value is correct after the edit (NaN).");
          } else if (expected[w2.name] === undefined) {
            is(w2.value.type, "undefined",
              "The second expression value is correct after the edit (undefined).");
          } else {
            is(w2.value, expected[w2.name],
              "The second expression value is correct after the edit.");
          }
          info(w2.value + " is equal to " + expected[w2.name]);
        }
        if (w3) {
          if (isNaN(expected[w3.name]) && typeof expected[w3.name] == "number") {
            ok(isNaN(w3.value),
              "The third expression value is correct after the edit (NaN).");
          } else if (expected[w3.name] === undefined) {
            is(w3.value.type, "undefined",
              "The third expression value is correct after the edit (undefined).");
          } else {
            is(w3.value, expected[w3.name],
              "The third expression value is correct after the edit.");
          }
          info(w3.value + " is equal to " + expected[w3.name]);
        }

        callback();
      });

      info("Changing the " + what + "ter with '" + string + "'.");

      write(string);
      EventUtils.sendKey("RETURN", gDebugger);
    });
  });

  myVar.expand();
  gVars.clearHierarchy();
}

function waitForWatchExpressions(callback) {
  gDebugger.addEventListener("Debugger:FetchedWatchExpressions", function onFetch() {
    gDebugger.removeEventListener("Debugger:FetchedWatchExpressions", onFetch, false);
    executeSoon(callback);
  }, false);
}

function waitForProperties(callback) {
  gDebugger.addEventListener("Debugger:FetchedProperties", function onFetch() {
    gDebugger.removeEventListener("Debugger:FetchedProperties", onFetch, false);
    executeSoon(callback);
  }, false);
}

function waitForElement(selector, exists, callback)
{
  // Poll every few milliseconds until the element are retrieved.
  let count = 0;
  let intervalID = window.setInterval(function() {
    info("count: " + count + " ");
    if (++count > 50) {
      ok(false, "Timed out while polling for the element.");
      window.clearInterval(intervalID);
      return closeDebuggerAndFinish();
    }
    if (!!gVars._list.querySelector(selector) != exists) {
      return;
    }
    // We got the element, it's safe to callback.
    window.clearInterval(intervalID);
    callback();
  }, 100);
}

function write(text) {
  if (!text) {
    EventUtils.sendKey("BACK_SPACE", gDebugger);
    return;
  }
  for (let i = 0; i < text.length; i++) {
    EventUtils.sendChar(text[i], gDebugger);
  }
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebugger = null;
  gVars = null;
  gWatch = null;
});
