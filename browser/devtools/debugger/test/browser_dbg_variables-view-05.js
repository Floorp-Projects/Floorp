/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that grips are correctly applied to variables and properties.
 */

const TAB_URL = EXAMPLE_URL + "doc_recursion-stack.html";

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    let variables = aPanel.panelWin.DebuggerView.Variables;

    let globalScope = variables.addScope("Test-Global");
    let localScope = variables.addScope("Test-Local");

    ok(globalScope, "The globalScope hasn't been created correctly.");
    ok(localScope, "The localScope hasn't been created correctly.");

    is(globalScope.target.querySelector(".separator"), null,
      "No separator string should be created for scopes (1).");
    is(localScope.target.querySelector(".separator"), null,
      "No separator string should be created for scopes (2).");

    let windowVar = globalScope.addItem("window");
    let documentVar = globalScope.addItem("document");

    ok(windowVar, "The windowVar hasn't been created correctly.");
    ok(documentVar, "The documentVar hasn't been created correctly.");

    ok(windowVar.target.querySelector(".separator").hidden,
      "No separator string should be shown for variables without a grip (1).");
    ok(documentVar.target.querySelector(".separator").hidden,
      "No separator string should be shown for variables without a grip (2).");

    windowVar.setGrip({ type: "object", class: "Window" });
    documentVar.setGrip({ type: "object", class: "HTMLDocument" });

    is(windowVar.target.querySelector(".separator").hidden, false,
      "A separator string should now be shown after setting the grip (1).");
    is(documentVar.target.querySelector(".separator").hidden, false,
      "A separator string should now be shown after setting the grip (2).");

    is(windowVar.target.querySelector(".separator").getAttribute("value"), ": ",
      "The separator string label is correct (1).");
    is(documentVar.target.querySelector(".separator").getAttribute("value"), ": ",
      "The separator string label is correct (2).");

    let localVar0 = localScope.addItem("localVar0");
    let localVar1 = localScope.addItem("localVar1");
    let localVar2 = localScope.addItem("localVar2");
    let localVar3 = localScope.addItem("localVar3");
    let localVar4 = localScope.addItem("localVar4");
    let localVar5 = localScope.addItem("localVar5");

    let localVar6 = localScope.addItem("localVar6");
    let localVar7 = localScope.addItem("localVar7");
    let localVar8 = localScope.addItem("localVar8");
    let localVar9 = localScope.addItem("localVar9");

    ok(localVar0, "The localVar0 hasn't been created correctly.");
    ok(localVar1, "The localVar1 hasn't been created correctly.");
    ok(localVar2, "The localVar2 hasn't been created correctly.");
    ok(localVar3, "The localVar3 hasn't been created correctly.");
    ok(localVar4, "The localVar4 hasn't been created correctly.");
    ok(localVar5, "The localVar5 hasn't been created correctly.");
    ok(localVar6, "The localVar6 hasn't been created correctly.");
    ok(localVar7, "The localVar7 hasn't been created correctly.");
    ok(localVar8, "The localVar8 hasn't been created correctly.");
    ok(localVar9, "The localVar9 hasn't been created correctly.");

    localVar0.setGrip(42);
    localVar1.setGrip(true);
    localVar2.setGrip("nasu");

    localVar3.setGrip({ type: "undefined" });
    localVar4.setGrip({ type: "null" });
    localVar5.setGrip({ type: "object", class: "Object" });
    localVar6.setGrip({ type: "Infinity" });
    localVar7.setGrip({ type: "-Infinity" });
    localVar8.setGrip({ type: "NaN" });
    localVar9.setGrip({ type: "-0" });

    localVar5.addItems({
      someProp0: { value: 42, enumerable: true },
      someProp1: { value: true, enumerable: true },
      someProp2: { value: "nasu", enumerable: true },
      someProp3: { value: { type: "undefined" }, enumerable: true },
      someProp4: { value: { type: "null" }, enumerable: true },
      someProp5: { value: { type: "object", class: "Object" }, enumerable: true },
      someProp6: { value: { type: "Infinity" }, enumerable: true },
      someProp7: { value: { type: "-Infinity" }, enumerable: true },
      someProp8: { value: { type: "NaN" }, enumerable: true },
      someProp9: { value: { type: "-0" }, enumerable: true },
      someUndefined: {
        get: { type: "undefined" },
        set: { type: "undefined" },
        enumerable: true
      },
      someAccessor: {
        get: { type: "object", class: "Function" },
        set: { type: "undefined" },
        enumerable: true
      }
    });

    localVar5.get("someProp5").addItems({
      someProp0: { value: 42, enumerable: true },
      someProp1: { value: true, enumerable: true },
      someProp2: { value: "nasu", enumerable: true },
      someProp3: { value: { type: "undefined" }, enumerable: true },
      someProp4: { value: { type: "null" }, enumerable: true },
      someProp5: { value: { type: "object", class: "Object" }, enumerable: true },
      someProp6: { value: { type: "Infinity" }, enumerable: true },
      someProp7: { value: { type: "-Infinity" }, enumerable: true },
      someProp8: { value: { type: "NaN" }, enumerable: true },
      someProp9: { value: { type: "-0" }, enumerable: true },
      someUndefined: {
        get: { type: "undefined" },
        set: { type: "undefined" },
        enumerable: true
      },
      someAccessor: {
        get: { type: "object", class: "Function" },
        set: { type: "undefined" },
        enumerable: true
      }
    });

    is(globalScope.target.querySelector(".enum").childNodes.length, 0,
      "The globalScope doesn't contain all the created enumerable variable elements.");
    is(globalScope.target.querySelector(".nonenum").childNodes.length, 2,
      "The globalScope doesn't contain all the created non-enumerable variable elements.");

    is(localScope.target.querySelector(".enum").childNodes.length, 0,
      "The localScope doesn't contain all the created enumerable variable elements.");
    is(localScope.target.querySelector(".nonenum").childNodes.length, 10,
      "The localScope doesn't contain all the created non-enumerable variable elements.");

    is(localVar5.target.querySelector(".enum").childNodes.length, 12,
      "The localVar5 doesn't contain all the created enumerable properties.");
    is(localVar5.target.querySelector(".nonenum").childNodes.length, 0,
      "The localVar5 doesn't contain all the created non-enumerable properties.");

    is(localVar5.get("someProp5").target.querySelector(".enum").childNodes.length, 12,
      "The localVar5.someProp5 doesn't contain all the created enumerable properties.");
    is(localVar5.get("someProp5").target.querySelector(".nonenum").childNodes.length, 0,
      "The localVar5.someProp5 doesn't contain all the created non-enumerable properties.");

    is(windowVar.target.querySelector(".value").getAttribute("value"), "Window",
      "The grip information for the windowVar wasn't set correctly.");
    is(documentVar.target.querySelector(".value").getAttribute("value"), "HTMLDocument",
      "The grip information for the documentVar wasn't set correctly.");

    is(localVar0.target.querySelector(".value").getAttribute("value"), "42",
      "The grip information for the localVar0 wasn't set correctly.");
    is(localVar1.target.querySelector(".value").getAttribute("value"), "true",
      "The grip information for the localVar1 wasn't set correctly.");
    is(localVar2.target.querySelector(".value").getAttribute("value"), "\"nasu\"",
      "The grip information for the localVar2 wasn't set correctly.");
    is(localVar3.target.querySelector(".value").getAttribute("value"), "undefined",
      "The grip information for the localVar3 wasn't set correctly.");
    is(localVar4.target.querySelector(".value").getAttribute("value"), "null",
      "The grip information for the localVar4 wasn't set correctly.");
    is(localVar5.target.querySelector(".value").getAttribute("value"), "Object",
      "The grip information for the localVar5 wasn't set correctly.");
    is(localVar6.target.querySelector(".value").getAttribute("value"), "Infinity",
      "The grip information for the localVar6 wasn't set correctly.");
    is(localVar7.target.querySelector(".value").getAttribute("value"), "-Infinity",
      "The grip information for the localVar7 wasn't set correctly.");
    is(localVar8.target.querySelector(".value").getAttribute("value"), "NaN",
      "The grip information for the localVar8 wasn't set correctly.");
    is(localVar9.target.querySelector(".value").getAttribute("value"), "-0",
      "The grip information for the localVar9 wasn't set correctly.");

    is(localVar5.get("someProp0").target.querySelector(".value").getAttribute("value"), "42",
      "The grip information for the someProp0 wasn't set correctly.");
    is(localVar5.get("someProp1").target.querySelector(".value").getAttribute("value"), "true",
      "The grip information for the someProp1 wasn't set correctly.");
    is(localVar5.get("someProp2").target.querySelector(".value").getAttribute("value"), "\"nasu\"",
      "The grip information for the someProp2 wasn't set correctly.");
    is(localVar5.get("someProp3").target.querySelector(".value").getAttribute("value"), "undefined",
      "The grip information for the someProp3 wasn't set correctly.");
    is(localVar5.get("someProp4").target.querySelector(".value").getAttribute("value"), "null",
      "The grip information for the someProp4 wasn't set correctly.");
    is(localVar5.get("someProp5").target.querySelector(".value").getAttribute("value"), "Object",
      "The grip information for the someProp5 wasn't set correctly.");
    is(localVar5.get("someProp6").target.querySelector(".value").getAttribute("value"), "Infinity",
      "The grip information for the someProp6 wasn't set correctly.");
    is(localVar5.get("someProp7").target.querySelector(".value").getAttribute("value"), "-Infinity",
      "The grip information for the someProp7 wasn't set correctly.");
    is(localVar5.get("someProp8").target.querySelector(".value").getAttribute("value"), "NaN",
      "The grip information for the someProp8 wasn't set correctly.");
    is(localVar5.get("someProp9").target.querySelector(".value").getAttribute("value"), "-0",
      "The grip information for the someProp9 wasn't set correctly.");
    is(localVar5.get("someUndefined").target.querySelector(".value").getAttribute("value"), "",
      "The grip information for the someUndefined wasn't set correctly.");
    is(localVar5.get("someAccessor").target.querySelector(".value").getAttribute("value"), "",
      "The grip information for the someAccessor wasn't set correctly.");

    is(localVar5.get("someProp5").get("someProp0").target.querySelector(".value").getAttribute("value"), "42",
      "The grip information for the sub-someProp0 wasn't set correctly.");
    is(localVar5.get("someProp5").get("someProp1").target.querySelector(".value").getAttribute("value"), "true",
      "The grip information for the sub-someProp1 wasn't set correctly.");
    is(localVar5.get("someProp5").get("someProp2").target.querySelector(".value").getAttribute("value"), "\"nasu\"",
      "The grip information for the sub-someProp2 wasn't set correctly.");
    is(localVar5.get("someProp5").get("someProp3").target.querySelector(".value").getAttribute("value"), "undefined",
      "The grip information for the sub-someProp3 wasn't set correctly.");
    is(localVar5.get("someProp5").get("someProp4").target.querySelector(".value").getAttribute("value"), "null",
      "The grip information for the sub-someProp4 wasn't set correctly.");
    is(localVar5.get("someProp5").get("someProp5").target.querySelector(".value").getAttribute("value"), "Object",
      "The grip information for the sub-someProp5 wasn't set correctly.");
    is(localVar5.get("someProp5").get("someProp6").target.querySelector(".value").getAttribute("value"), "Infinity",
      "The grip information for the sub-someProp6 wasn't set correctly.");
    is(localVar5.get("someProp5").get("someProp7").target.querySelector(".value").getAttribute("value"), "-Infinity",
      "The grip information for the sub-someProp7 wasn't set correctly.");
    is(localVar5.get("someProp5").get("someProp8").target.querySelector(".value").getAttribute("value"), "NaN",
      "The grip information for the sub-someProp8 wasn't set correctly.");
    is(localVar5.get("someProp5").get("someProp9").target.querySelector(".value").getAttribute("value"), "-0",
      "The grip information for the sub-someProp9 wasn't set correctly.");
    is(localVar5.get("someProp5").get("someUndefined").target.querySelector(".value").getAttribute("value"), "",
      "The grip information for the sub-someUndefined wasn't set correctly.");
    is(localVar5.get("someProp5").get("someAccessor").target.querySelector(".value").getAttribute("value"), "",
      "The grip information for the sub-someAccessor wasn't set correctly.");

    closeDebuggerAndFinish(aPanel);
  });
}
