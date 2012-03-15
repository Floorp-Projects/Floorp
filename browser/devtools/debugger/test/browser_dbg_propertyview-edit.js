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

      let localVar0 = testScope.addVar("aNumber");
      localVar0.setGrip(1.618);

      let localVar1 = testScope.addVar("aBoolean");
      localVar1.setGrip(false);

      let localVar2 = testScope.addVar("aString");
      localVar2.setGrip("hello world");

      let localVar3 = testScope.addVar("aUndefined");
      localVar3.setGrip({ type: "undefined" });

      let localVar4 = testScope.addVar("aNull");
      localVar4.setGrip({ type: "null" });

      testVar0(localVar0, function() {
        testVar1(localVar1, function() {
          testVar2(localVar2, function() {
            testVar2_bis(localVar2, function() {
              testVar3(localVar3, function() {
                testVar4_switch(localVar4, localVar0, function() {
                  resumeAndFinish();
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

function testVar0(aVar, aCallback) {
  var changeTo = "true";

  function makeChangesAndExitInputMode() {
    EventUtils.sendString(changeTo);
    EventUtils.sendKey("RETURN");
  }

  ok(!aVar.querySelector(".element-input"),
    "There should be no input elements for this variable yet.");

  ok(!aVar.arrowVisible,
    "The arrow shouldn't be visible for number variables.'");
  ok(aVar.visible,
    "The variable should be visible before entering 'input-mode'.");
  ok(!aVar.expanded,
    "Simple number variables shouldn't be expanded.");

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

    makeChangesAndExitInputMode();

    executeSoon(function() {
      ok(!aVar.querySelector(".element-input"),
        "There should be no input elements after exiting 'input-mode'.");

      is(aVar.querySelector(".value").textContent, changeTo,
        "The grip information for the variable wasn't set correctly.");

      ok(!aVar.arrowVisible,
        "The arrow shouldn't be visible for boolean variables after exiting 'input-mode'.");
      ok(aVar.visible,
        "The variable should be visible after exiting 'input-mode'.");
      ok(!aVar.expanded,
        "Simple number elements shouldn't be expanded.");

      executeSoon(function() {
        aCallback();
      });
    });
  });
}

function testVar1(aVar, aCallback) {
  var changeTo = "\"nasu\"";

  function makeChangesAndExitInputMode() {
    EventUtils.sendString(changeTo);
    EventUtils.sendKey("ENTER");
  }

  ok(!aVar.querySelector(".element-input"),
    "There should be no input elements for this variable yet.");

  ok(!aVar.arrowVisible,
    "The arrow shouldn't be visible for boolean variables.'");
  ok(aVar.visible,
    "The variable should be visible before entering 'input-mode'.");
  ok(!aVar.expanded,
    "Simple boolean variables shouldn't be expanded.");

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

    makeChangesAndExitInputMode();

    executeSoon(function() {
      ok(!aVar.querySelector(".element-input"),
        "There should be no input elements after exiting 'input-mode'.");

      is(aVar.querySelector(".value").textContent, changeTo,
        "The grip information for the variable wasn't set correctly.");

      ok(!aVar.arrowVisible,
        "The arrow shouldn't be visible for string variables after exiting 'input-mode'.");
      ok(aVar.visible,
        "The variable should be visible after exiting 'input-mode'.");
      ok(!aVar.expanded,
        "Simple string elements shouldn't be expanded.");

      executeSoon(function() {
        aCallback();
      });
    });
  });
}

function testVar2(aVar, aCallback) {
  var changeTo = "1234.5678";

  function makeChangesAndExitInputMode() {
    EventUtils.sendString(changeTo);
    gDebugger.editor.focus();
  }

  ok(!aVar.querySelector(".element-input"),
    "There should be no input elements for this variable yet.");

  ok(!aVar.arrowVisible,
    "The arrow shouldn't be visible for string variables.'");
  ok(aVar.visible,
    "The variable should be visible before entering 'input-mode'.");
  ok(!aVar.expanded,
    "Simple string variables shouldn't be expanded.");

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

    makeChangesAndExitInputMode();

    executeSoon(function() {
      ok(!aVar.querySelector(".element-input"),
        "There should be no input elements after exiting 'input-mode'.");

      is(aVar.querySelector(".value").textContent, "\"" + changeTo + "\"",
        "The grip information for the variable wasn't set correctly.");
      // when changing a string, its contents are automatically selected
      // so that the result is another string; for example, if the value was
      // "hello", then writing 42 would automatically produce "42"

      // see more details in _activateElementInputMode from DebuggerView

      ok(!aVar.arrowVisible,
        "The arrow shouldn't be visible for string variables after exiting 'input-mode'.");
      ok(aVar.visible,
        "The variable should be visible after exiting 'input-mode'.");
      ok(!aVar.expanded,
        "Simple string elements shouldn't be expanded.");

      executeSoon(function() {
        aCallback();
      });
    });
  });
}

function testVar2_bis(aVar, aCallback) {
  var changeTo = "42";

  function makeChangesAndExitInputMode() {
    EventUtils.sendString(changeTo);
    gDebugger.editor.focus();
  }

  ok(!aVar.querySelector(".element-input"),
    "There should be no input elements for this variable yet.");

  ok(!aVar.arrowVisible,
    "The arrow shouldn't be visible for string variables.'");
  ok(aVar.visible,
    "The variable should be visible before entering 'input-mode'.");
  ok(!aVar.expanded,
    "Simple string variables shouldn't be expanded.");

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

    aVar.querySelector(".element-input").select();
    makeChangesAndExitInputMode();

    executeSoon(function() {
      ok(!aVar.querySelector(".element-input"),
        "There should be no input elements after exiting 'input-mode'.");

      is(aVar.querySelector(".value").textContent, changeTo,
        "The grip information for the variable wasn't set correctly.");

      ok(!aVar.arrowVisible,
        "The arrow shouldn't be visible for number variables after exiting 'input-mode'.");
      ok(aVar.visible,
        "The variable should be visible after exiting 'input-mode'.");
      ok(!aVar.expanded,
        "Simple number elements shouldn't be expanded.");

      executeSoon(function() {
        aCallback();
      });
    });
  });
}

function testVar3(aVar, aCallback) {
  var changeTo = "\"this will be ignored\"";

  function makeChangesAndExitInputMode() {
    EventUtils.sendString(changeTo);
    EventUtils.sendKey("ESCAPE");
  }

  ok(!aVar.querySelector(".element-input"),
    "There should be no input elements for this variable yet.");

  ok(!aVar.arrowVisible,
    "The arrow shouldn't be visible for undefined variables.'");
  ok(aVar.visible,
    "The variable should be visible before entering 'input-mode'.");
  ok(!aVar.expanded,
    "Simple undefined variables shouldn't be expanded.");

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

    makeChangesAndExitInputMode();

    executeSoon(function() {
      ok(!aVar.querySelector(".element-input"),
        "There should be no input elements after exiting 'input-mode'.");

      is(aVar.querySelector(".value").textContent, "undefined",
        "The grip information for the variable wasn't reverted correctly.");
      isnot(aVar.querySelector(".value").textContent, changeTo,
        "The grip information for the variable wasn't reverted correctly.");

      ok(!aVar.arrowVisible,
        "The arrow shouldn't be visible for undefined variables after exiting 'input-mode'.");
      ok(aVar.visible,
        "The variable should be visible after exiting 'input-mode'.");
      ok(!aVar.expanded,
        "Simple undefined elements shouldn't be expanded.");

      executeSoon(function() {
        aCallback();
      });
    });
  });
}

function testVar4_switch(aVar, aVarSwitch, aCallback) {
  var changeTo = "\"this will not be ignored\"";

  function makeChangesAndExitInputMode() {
    EventUtils.sendString(changeTo);
    EventUtils.sendMouseEvent({ type: "click" },
      aVarSwitch.querySelector(".value"),
      gDebugger);
  }

  ok(!aVar.querySelector(".element-input"),
    "There should be no input elements for this variable yet.");

  ok(!aVar.arrowVisible,
    "The arrow shouldn't be visible for null variables.'");
  ok(aVar.visible,
    "The variable should be visible before entering 'input-mode'.");
  ok(!aVar.expanded,
    "Simple null variables shouldn't be expanded.");

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

    makeChangesAndExitInputMode();

    executeSoon(function() {
      ok(!aVar.querySelector(".element-input"),
        "There should be no input elements after exiting 'input-mode'.");

      is(aVar.querySelector(".value").textContent, changeTo,
        "The grip information for the variable set correctly after the input element lost focus.");

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
