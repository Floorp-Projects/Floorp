/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
var gPane = null;
var gTab = null;
var gDebuggee = null;
var gDebugger = null;

function test() {
  debug_tab_pane(STACK_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.contentWindow;

    testSimpleCall();
  });
}

function testSimpleCall() {
  gDebugger.DebuggerController.activeThread.addOneTimeListener("framesadded", function() {
    Services.tm.currentThread.dispatch({ run: function() {

      let testScope = gDebugger.DebuggerView.Properties._addScope("test").expand();

      let v = testScope.addVar("aObject");
      v.setGrip({ type: "object", "class": "Object" }).expand();
      v.addProperties({ "someProp0": { "value": 42 },
                        "someProp1": { "value": true },
                        "someProp2": { "value": "nasu" },
                        "someProp3": { "value": { "type": "undefined" } },
                        "someProp4": { "value": { "type": "null" } },
                        "someProp5": { "value": { "type": "object", "class": "Object" } } });

      testPropContainer(v, function() {
        testProp0(v.someProp0, function() {
          testProp1(v.someProp1, function() {
            testProp2(v.someProp2, function() {
              testProp2_bis(v.someProp2, function() {
                testProp3(v.someProp3, function() {
                  testProp4_switch(v.someProp4, v.someProp0, function() {
                    resumeAndFinish();
                  });
                });
              });
            });
          });
        });
      });
    }}, 0);
  });

  gDebuggee.simpleCall();
}

function testPropContainer(aVar, aCallback) {
  ok(!aVar.querySelector(".element-input"),
    "There should be no input elements for this variable yet.");

  ok(aVar.arrowVisible,
    "The arrow should be visible for variable containers.'");
  ok(aVar.visible,
    "The variable should be visible before entering 'input-mode'.");
  ok(aVar.expanded,
    "This variable container should have been previously expanded.");

  EventUtils.sendMouseEvent({ type: "click" },
    aVar.querySelector(".value"),
    gDebugger);

  executeSoon(function() {
    ok(aVar.querySelector(".element-input"),
      "There should be an input element created.");

    ok(!aVar.arrowVisible,
      "The arrow shouldn't be visible while in 'input-mode.'");
    ok(aVar.visible,
      "The variable should be visible while in 'input-mode'.");
    ok(!aVar.expanded,
      "The variable shouldn't be expanded while in 'input-mode'.");

    gDebugger.editor.focus();

    executeSoon(function() {
      ok(!aVar.querySelector(".element-input"),
        "There should be no input elements after exiting 'input-mode'.");

      is(aVar.querySelector(".value").textContent, "[object Object]",
        "The grip information for the variable wasn't reset correctly.");

      ok(aVar.arrowVisible,
        "The arrow should be visible for variable containers after exiting 'input-mode'.'");
      ok(aVar.visible,
        "The variable should be visible before entering 'input-mode'.");
      ok(aVar.expanded,
        "This variable container should have been previously expanded.");

      executeSoon(function() {
        aCallback();
      });
    });
  });
}

function testProp0(aVar, aCallback) {
  var changeTo = "true";

  function makeChangesAndExitInputMode() {
    EventUtils.sendString(changeTo);
    EventUtils.sendKey("RETURN");
  }

  ok(!aVar.querySelector(".element-input"),
    "There should be no input elements for this property yet.");

  ok(!aVar.arrowVisible,
    "The arrow shouldn't be visible for number properties.'");
  ok(aVar.visible,
    "The property should be visible before entering 'input-mode'.");
  ok(!aVar.expanded,
    "Simple number properties shouldn't be expanded.");

  EventUtils.sendMouseEvent({ type: "click" },
    aVar.querySelector(".value"),
    gDebugger);

  executeSoon(function() {
    ok(aVar.querySelector(".element-input"),
      "There should be an input element created.");

    ok(!aVar.arrowVisible,
      "The arrow shouldn't be visible while in 'input-mode.'");
    ok(aVar.visible,
      "The property should be visible while in 'input-mode'.");
    ok(!aVar.expanded,
      "The property shouldn't be expanded while in 'input-mode'.");

    makeChangesAndExitInputMode();

    executeSoon(function() {
      ok(!aVar.querySelector(".element-input"),
        "There should be no input elements after exiting 'input-mode'.");

      is(aVar.querySelector(".value").textContent, changeTo,
        "The grip information for the property wasn't set correctly.");

      ok(!aVar.arrowVisible,
        "The arrow shouldn't be visible for boolean properties after exiting 'input-mode'.");
      ok(aVar.visible,
        "The property should be visible after exiting 'input-mode'.");
      ok(!aVar.expanded,
        "Simple number elements shouldn't be expanded.");

      executeSoon(function() {
        aCallback();
      });
    });
  });
}

function testProp1(aVar, aCallback) {
  var changeTo = "\"nasu\"";

  function makeChangesAndExitInputMode() {
    EventUtils.sendString(changeTo);
    EventUtils.sendKey("ENTER");
  }

  ok(!aVar.querySelector(".element-input"),
    "There should be no input elements for this property yet.");

  ok(!aVar.arrowVisible,
    "The arrow shouldn't be visible for boolean properties.'");
  ok(aVar.visible,
    "The property should be visible before entering 'input-mode'.");
  ok(!aVar.expanded,
    "Simple boolean properties shouldn't be expanded.");

  EventUtils.sendMouseEvent({ type: "click" },
    aVar.querySelector(".value"),
    gDebugger);

  executeSoon(function() {
    ok(aVar.querySelector(".element-input"),
      "There should be an input element created.");

    ok(!aVar.arrowVisible,
      "The arrow shouldn't be visible while in 'input-mode.'");
    ok(aVar.visible,
      "The property should be visible while in 'input-mode'.");
    ok(!aVar.expanded,
      "The property shouldn't be expanded while in 'input-mode'.");

    makeChangesAndExitInputMode();

    executeSoon(function() {
      ok(!aVar.querySelector(".element-input"),
        "There should be no input elements after exiting 'input-mode'.");

      is(aVar.querySelector(".value").textContent, changeTo,
        "The grip information for the property wasn't set correctly.");

      ok(!aVar.arrowVisible,
        "The arrow shouldn't be visible for string properties after exiting 'input-mode'.");
      ok(aVar.visible,
        "The property should be visible after exiting 'input-mode'.");
      ok(!aVar.expanded,
        "Simple string elements shouldn't be expanded.");

      executeSoon(function() {
        aCallback();
      });
    });
  });
}

function testProp2(aVar, aCallback) {
  var changeTo = "1234.5678";

  function makeChangesAndExitInputMode() {
    EventUtils.sendString(changeTo);
    gDebugger.editor.focus();
  }

  ok(!aVar.querySelector(".element-input"),
    "There should be no input elements for this property yet.");

  ok(!aVar.arrowVisible,
    "The arrow shouldn't be visible for string properties.'");
  ok(aVar.visible,
    "The property should be visible before entering 'input-mode'.");
  ok(!aVar.expanded,
    "Simple string properties shouldn't be expanded.");

  EventUtils.sendMouseEvent({ type: "click" },
    aVar.querySelector(".value"),
    gDebugger);

  executeSoon(function() {
    ok(aVar.querySelector(".element-input"),
      "There should be an input element created.");

    ok(!aVar.arrowVisible,
      "The arrow shouldn't be visible while in 'input-mode.'");
    ok(aVar.visible,
      "The property should be visible while in 'input-mode'.");
    ok(!aVar.expanded,
      "The property shouldn't be expanded while in 'input-mode'.");

    makeChangesAndExitInputMode();

    executeSoon(function() {
      ok(!aVar.querySelector(".element-input"),
        "There should be no input elements after exiting 'input-mode'.");

      is(aVar.querySelector(".value").textContent, "\"" + changeTo + "\"",
        "The grip information for the property wasn't set correctly.");
      // when changing a string, its contents are automatically selected
      // so that the result is another string; for example, if the value was
      // "hello", then writing 42 would automatically produce "42"

      // see more details in _activateElementInputMode from DebuggerView

      ok(!aVar.arrowVisible,
        "The arrow shouldn't be visible for string properties after exiting 'input-mode'.");
      ok(aVar.visible,
        "The property should be visible after exiting 'input-mode'.");
      ok(!aVar.expanded,
        "Simple string elements shouldn't be expanded.");

      executeSoon(function() {
        aCallback();
      });
    });
  });
}

function testProp2_bis(aVar, aCallback) {
  var changeTo = "42";

  function makeChangesAndExitInputMode() {
    EventUtils.sendString(changeTo);
    gDebugger.editor.focus();
  }

  ok(!aVar.querySelector(".element-input"),
    "There should be no input elements for this property yet.");

  ok(!aVar.arrowVisible,
    "The arrow shouldn't be visible for string properties.'");
  ok(aVar.visible,
    "The property should be visible before entering 'input-mode'.");
  ok(!aVar.expanded,
    "Simple string properties shouldn't be expanded.");

  EventUtils.sendMouseEvent({ type: "click" },
    aVar.querySelector(".value"),
    gDebugger);

  executeSoon(function() {
    ok(aVar.querySelector(".element-input"),
      "There should be an input element created.");

    ok(!aVar.arrowVisible,
      "The arrow shouldn't be visible while in 'input-mode.'");
    ok(aVar.visible,
      "The property should be visible while in 'input-mode'.");
    ok(!aVar.expanded,
      "The property shouldn't be expanded while in 'input-mode'.");

    aVar.querySelector(".element-input").select();
    makeChangesAndExitInputMode();

    executeSoon(function() {
      ok(!aVar.querySelector(".element-input"),
        "There should be no input elements after exiting 'input-mode'.");

      is(aVar.querySelector(".value").textContent, changeTo,
        "The grip information for the property wasn't set correctly.");

      ok(!aVar.arrowVisible,
        "The arrow shouldn't be visible for number properties after exiting 'input-mode'.");
      ok(aVar.visible,
        "The property should be visible after exiting 'input-mode'.");
      ok(!aVar.expanded,
        "Simple number elements shouldn't be expanded.");

      executeSoon(function() {
        aCallback();
      });
    });
  });
}

function testProp3(aVar, aCallback) {
  var changeTo = "\"this will be ignored\"";

  function makeChangesAndExitInputMode() {
    EventUtils.sendString(changeTo);
    EventUtils.sendKey("ESCAPE");
  }

  ok(!aVar.querySelector(".element-input"),
    "There should be no input elements for this property yet.");

  ok(!aVar.arrowVisible,
    "The arrow shouldn't be visible for undefined properties.'");
  ok(aVar.visible,
    "The property should be visible before entering 'input-mode'.");
  ok(!aVar.expanded,
    "Simple undefined properties shouldn't be expanded.");

  EventUtils.sendMouseEvent({ type: "click" },
    aVar.querySelector(".value"),
    gDebugger);

  executeSoon(function() {
    ok(aVar.querySelector(".element-input"),
      "There should be an input element created.");

    ok(!aVar.arrowVisible,
      "The arrow shouldn't be visible while in 'input-mode.'");
    ok(aVar.visible,
      "The property should be visible while in 'input-mode'.");
    ok(!aVar.expanded,
      "The property shouldn't be expanded while in 'input-mode'.");

    makeChangesAndExitInputMode();

    executeSoon(function() {
      ok(!aVar.querySelector(".element-input"),
        "There should be no input elements after exiting 'input-mode'.");

      is(aVar.querySelector(".value").textContent, "undefined",
        "The grip information for the property wasn't reverted correctly.");
      isnot(aVar.querySelector(".value").textContent, changeTo,
        "The grip information for the property wasn't reverted correctly.");

      ok(!aVar.arrowVisible,
        "The arrow shouldn't be visible for undefined properties after exiting 'input-mode'.");
      ok(aVar.visible,
        "The property should be visible after exiting 'input-mode'.");
      ok(!aVar.expanded,
        "Simple undefined elements shouldn't be expanded.");

      executeSoon(function() {
        aCallback();
      });
    });
  });
}

function testProp4_switch(aVar, aVarSwitch, aCallback) {
  var changeTo = "\"this will not be ignored\"";

  function makeChangesAndExitInputMode() {
    EventUtils.sendString(changeTo);
    EventUtils.sendMouseEvent({ type: "click" },
      aVarSwitch.querySelector(".value"),
      gDebugger);
  }

  ok(!aVar.querySelector(".element-input"),
    "There should be no input elements for this property yet.");

  ok(!aVar.arrowVisible,
    "The arrow shouldn't be visible for null properties.'");
  ok(aVar.visible,
    "The property should be visible before entering 'input-mode'.");
  ok(!aVar.expanded,
    "Simple null properties shouldn't be expanded.");

  EventUtils.sendMouseEvent({ type: "click" },
    aVar.querySelector(".value"),
    gDebugger);

  executeSoon(function() {
    ok(aVar.querySelector(".element-input"),
      "There should be an input element created.");

    ok(!aVar.arrowVisible,
      "The arrow shouldn't be visible while in 'input-mode.'");
    ok(aVar.visible,
      "The property should be visible while in 'input-mode'.");
    ok(!aVar.expanded,
      "The property shouldn't be expanded while in 'input-mode'.");

    makeChangesAndExitInputMode();

    executeSoon(function() {
      ok(!aVar.querySelector(".element-input"),
        "There should be no input elements after exiting 'input-mode'.");

      is(aVar.querySelector(".value").textContent, changeTo,
        "The grip information for the property set correctly after the input element lost focus.");

      executeSoon(function() {
        aCallback();
      });
    });
  });
}

function resumeAndFinish() {
  gDebugger.DebuggerController.activeThread.resume(function() {
    closeDebuggerAndFinish(gTab);
  });
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebuggee = null;
  gDebugger = null;
});
