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

      let globalScope = gDebugger.DebuggerView.Properties.globalScope;
      let localScope = gDebugger.DebuggerView.Properties.localScope;
      globalScope.empty();
      localScope.empty();

      let windowVar = globalScope.addVar("window");
      let documentVar = globalScope.addVar("document");
      let localVar0 = localScope.addVar("localVariable");
      let localVar1 = localScope.addVar("localVar1");
      let localVar2 = localScope.addVar("localVar2");
      let localVar3 = localScope.addVar("localVar3");
      let localVar4 = localScope.addVar("localVar4");
      let localVar5 = localScope.addVar("localVar5");

      localVar0.setGrip(42);
      localVar1.setGrip(true);
      localVar2.setGrip("nasu");

      localVar3.setGrip({ "type": "undefined" });
      localVar4.setGrip({ "type": "null" });
      localVar5.setGrip({ "type": "object", "class": "Object" });

      localVar5.addProperties({ "someProp0": { "value": 42 },
                                "someProp1": { "value": true },
                                "someProp2": { "value": "nasu" },
                                "someProp3": { "value": { "type": "undefined" } },
                                "someProp4": { "value": { "type": "null" } },
                                "someProp5": { "value": { "type": "object", "class": "Object" } } });

      localVar5.someProp5.addProperties({ "someProp0": { "value": 42 },
                                          "someProp1": { "value": true },
                                          "someProp2": { "value": "nasu" },
                                          "someProp3": { "value": { "type": "undefined" } },
                                          "someProp4": { "value": { "type": "null" } },
                                          "someAccessor": { "get": { "type": "object", "class": "Function" },
                                                            "set": { "type": "undefined" } } });

      windowVar.setGrip({ "type": "object", "class": "Window" });
      windowVar.addProperties({ "helloWorld": { "value": "hello world" } });

      documentVar.setGrip({ "type": "object", "class": "HTMLDocument" });
      documentVar.addProperties({ "onload": { "value": { "type": "null" } },
                                  "onunload": { "value": { "type": "null" } },
                                  "onfocus": { "value": { "type": "null" } },
                                  "onblur": { "value": { "type": "null" } },
                                  "onclick": { "value": { "type": "null" } },
                                  "onkeypress": { "value": { "type": "null" } } });


      ok(windowVar, "The windowVar hasn't been created correctly.");
      ok(documentVar, "The documentVar hasn't been created correctly.");
      ok(localVar0, "The localVar0 hasn't been created correctly.");
      ok(localVar1, "The localVar1 hasn't been created correctly.");
      ok(localVar2, "The localVar2 hasn't been created correctly.");
      ok(localVar3, "The localVar3 hasn't been created correctly.");
      ok(localVar4, "The localVar4 hasn't been created correctly.");
      ok(localVar5, "The localVar5 hasn't been created correctly.");


      for each (let elt in globalScope.querySelector(".details").childNodes) {
        info("globalScope :: " + {
          id: elt.id, className: elt.className }.toSource());
      }
      is(globalScope.querySelector(".details").childNodes.length, 2,
        "The globalScope doesn't contain all the created variable elements.");

      for each (let elt in localScope.querySelector(".details").childNodes) {
        info("localScope :: " + {
          id: elt.id, className: elt.className }.toSource());
      }
      is(localScope.querySelector(".details").childNodes.length, 6,
        "The localScope doesn't contain all the created variable elements.");


      is(localVar5.querySelector(".details").childNodes.length, 6,
        "The localVar5 doesn't contain all the created properties.");

      is(localVar5.someProp5.querySelector(".details").childNodes.length, 6,
        "The localVar5.someProp5 doesn't contain all the created properties.");


      is(windowVar.querySelector(".value").textContent, "[object Window]",
        "The grip information for the windowVar wasn't set correctly.");

      is(documentVar.querySelector(".value").textContent, "[object HTMLDocument]",
        "The grip information for the documentVar wasn't set correctly.");

      is(localVar0.querySelector(".value").textContent, "42",
        "The grip information for the localVar0 wasn't set correctly.");

      is(localVar1.querySelector(".value").textContent, "true",
        "The grip information for the localVar1 wasn't set correctly.");

      is(localVar2.querySelector(".value").textContent, "\"nasu\"",
        "The grip information for the localVar2 wasn't set correctly.");

      is(localVar3.querySelector(".value").textContent, "undefined",
        "The grip information for the localVar3 wasn't set correctly.");

      is(localVar4.querySelector(".value").textContent, "null",
        "The grip information for the localVar4 wasn't set correctly.");

      is(localVar5.querySelector(".value").textContent, "[object Object]",
        "The grip information for the localVar5 wasn't set correctly.");

      gDebugger.DebuggerController.activeThread.resume(function() {
        closeDebuggerAndFinish(gTab);
      });
    }}, 0);
  });

  gDebuggee.simpleCall();
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebuggee = null;
  gDebugger = null;
});
