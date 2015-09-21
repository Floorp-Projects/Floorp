/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the variables view remains responsive when faced with
 * huge ammounts of data.
 */

const TAB_URL = EXAMPLE_URL + "doc_large-array-buffer.html";

var gTab, gPanel, gDebugger;
var gVariables, gEllipsis;

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gVariables = gDebugger.DebuggerView.Variables;
    gEllipsis = Services.prefs.getComplexValue("intl.ellipsis", Ci.nsIPrefLocalizedString).data;

    waitForSourceAndCaretAndScopes(gPanel, ".html", 23)
      .then(() => initialChecks())
      .then(() => verifyFirstLevel())
      .then(() => verifyNextLevels())
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    generateMouseClickInTab(gTab, "content.document.querySelector('button')");
  });
}

function initialChecks() {
  let localScope = gVariables.getScopeAtIndex(0);
  let bufferVar = localScope.get("buffer");
  let arrayVar = localScope.get("largeArray");
  let objectVar = localScope.get("largeObject");

  ok(bufferVar, "There should be a 'buffer' variable present in the scope.");
  ok(arrayVar, "There should be a 'largeArray' variable present in the scope.");
  ok(objectVar, "There should be a 'largeObject' variable present in the scope.");

  is(bufferVar.target.querySelector(".name").getAttribute("value"), "buffer",
    "Should have the right property name for 'buffer'.");
  is(bufferVar.target.querySelector(".value").getAttribute("value"), "ArrayBuffer",
    "Should have the right property value for 'buffer'.");
  ok(bufferVar.target.querySelector(".value").className.includes("token-other"),
    "Should have the right token class for 'buffer'.");

  is(arrayVar.target.querySelector(".name").getAttribute("value"), "largeArray",
    "Should have the right property name for 'largeArray'.");
  is(arrayVar.target.querySelector(".value").getAttribute("value"), "Int8Array[10000]",
    "Should have the right property value for 'largeArray'.");
  ok(arrayVar.target.querySelector(".value").className.includes("token-other"),
    "Should have the right token class for 'largeArray'.");

  is(objectVar.target.querySelector(".name").getAttribute("value"), "largeObject",
    "Should have the right property name for 'largeObject'.");
  is(objectVar.target.querySelector(".value").getAttribute("value"), "Object[10000]",
    "Should have the right property value for 'largeObject'.");
  ok(objectVar.target.querySelector(".value").className.includes("token-other"),
    "Should have the right token class for 'largeObject'.");

  is(bufferVar.expanded, false,
    "The 'buffer' variable shouldn't be expanded.");
  is(arrayVar.expanded, false,
    "The 'largeArray' variable shouldn't be expanded.");
  is(objectVar.expanded, false,
    "The 'largeObject' variable shouldn't be expanded.");

  return promise.all([arrayVar.expand(),objectVar.expand()]);
}

function verifyFirstLevel() {
  let localScope = gVariables.getScopeAtIndex(0);
  let arrayVar = localScope.get("largeArray");
  let objectVar = localScope.get("largeObject");

  let arrayEnums = arrayVar.target.querySelector(".variables-view-element-details.enum").childNodes;
  let arrayNonEnums = arrayVar.target.querySelector(".variables-view-element-details.nonenum").childNodes;
  is(arrayEnums.length, 0,
    "The 'largeArray' shouldn't contain any enumerable elements.");
  is(arrayNonEnums.length, 9,
    "The 'largeArray' should contain all the created non-enumerable elements.");

  let objectEnums = objectVar.target.querySelector(".variables-view-element-details.enum").childNodes;
  let objectNonEnums = objectVar.target.querySelector(".variables-view-element-details.nonenum").childNodes;
  is(objectEnums.length, 0,
    "The 'largeObject' shouldn't contain any enumerable elements.");
  is(objectNonEnums.length, 5,
    "The 'largeObject' should contain all the created non-enumerable elements.");

  is(arrayVar.target.querySelectorAll(".variables-view-property .name")[0].getAttribute("value"),
    "[0" + gEllipsis + "2499]", "The first page in the 'largeArray' is named correctly.");
  is(arrayVar.target.querySelectorAll(".variables-view-property .value")[0].getAttribute("value"),
    "", "The first page in the 'largeArray' should not have a corresponding value.");
  is(arrayVar.target.querySelectorAll(".variables-view-property .name")[1].getAttribute("value"),
    "[2500" + gEllipsis + "4999]", "The second page in the 'largeArray' is named correctly.");
  is(arrayVar.target.querySelectorAll(".variables-view-property .value")[1].getAttribute("value"),
    "", "The second page in the 'largeArray' should not have a corresponding value.");
  is(arrayVar.target.querySelectorAll(".variables-view-property .name")[2].getAttribute("value"),
    "[5000" + gEllipsis + "7499]", "The third page in the 'largeArray' is named correctly.");
  is(arrayVar.target.querySelectorAll(".variables-view-property .value")[2].getAttribute("value"),
    "", "The third page in the 'largeArray' should not have a corresponding value.");
  is(arrayVar.target.querySelectorAll(".variables-view-property .name")[3].getAttribute("value"),
    "[7500" + gEllipsis + "9999]", "The fourth page in the 'largeArray' is named correctly.");
  is(arrayVar.target.querySelectorAll(".variables-view-property .value")[3].getAttribute("value"),
    "", "The fourth page in the 'largeArray' should not have a corresponding value.");

  is(objectVar.target.querySelectorAll(".variables-view-property .name")[0].getAttribute("value"),
    "[0" + gEllipsis + "2499]", "The first page in the 'largeObject' is named correctly.");
  is(objectVar.target.querySelectorAll(".variables-view-property .value")[0].getAttribute("value"),
    "", "The first page in the 'largeObject' should not have a corresponding value.");
  is(objectVar.target.querySelectorAll(".variables-view-property .name")[1].getAttribute("value"),
    "[2500" + gEllipsis + "4999]", "The second page in the 'largeObject' is named correctly.");
  is(objectVar.target.querySelectorAll(".variables-view-property .value")[1].getAttribute("value"),
    "", "The second page in the 'largeObject' should not have a corresponding value.");
  is(objectVar.target.querySelectorAll(".variables-view-property .name")[2].getAttribute("value"),
    "[5000" + gEllipsis + "7499]", "The thrid page in the 'largeObject' is named correctly.");
  is(objectVar.target.querySelectorAll(".variables-view-property .value")[2].getAttribute("value"),
    "", "The thrid page in the 'largeObject' should not have a corresponding value.");
  is(objectVar.target.querySelectorAll(".variables-view-property .name")[3].getAttribute("value"),
    "[7500" + gEllipsis + "9999]", "The fourth page in the 'largeObject' is named correctly.");
  is(objectVar.target.querySelectorAll(".variables-view-property .value")[3].getAttribute("value"),
    "", "The fourth page in the 'largeObject' should not have a corresponding value.");

  is(arrayVar.target.querySelectorAll(".variables-view-property .name")[4].getAttribute("value"),
    "buffer", "The other properties 'largeArray' are named correctly.");
  is(arrayVar.target.querySelectorAll(".variables-view-property .value")[4].getAttribute("value"),
    "ArrayBuffer", "The other properties 'largeArray' have the correct value.");
  is(arrayVar.target.querySelectorAll(".variables-view-property .name")[5].getAttribute("value"),
    "byteLength", "The other properties 'largeArray' are named correctly.");
  is(arrayVar.target.querySelectorAll(".variables-view-property .value")[5].getAttribute("value"),
    "10000", "The other properties 'largeArray' have the correct value.");
  is(arrayVar.target.querySelectorAll(".variables-view-property .name")[6].getAttribute("value"),
    "byteOffset", "The other properties 'largeArray' are named correctly.");
  is(arrayVar.target.querySelectorAll(".variables-view-property .value")[6].getAttribute("value"),
    "0", "The other properties 'largeArray' have the correct value.");
  is(arrayVar.target.querySelectorAll(".variables-view-property .name")[7].getAttribute("value"),
    "length", "The other properties 'largeArray' are named correctly.");
  is(arrayVar.target.querySelectorAll(".variables-view-property .value")[7].getAttribute("value"),
    "10000", "The other properties 'largeArray' have the correct value.");

  is(arrayVar.target.querySelectorAll(".variables-view-property .name")[8].getAttribute("value"),
    "__proto__", "The other properties 'largeArray' are named correctly.");
  is(arrayVar.target.querySelectorAll(".variables-view-property .value")[8].getAttribute("value"),
    "Int8ArrayPrototype", "The other properties 'largeArray' have the correct value.");

  is(objectVar.target.querySelectorAll(".variables-view-property .name")[4].getAttribute("value"),
    "__proto__", "The other properties 'largeObject' are named correctly.");
  is(objectVar.target.querySelectorAll(".variables-view-property .value")[4].getAttribute("value"),
    "Object", "The other properties 'largeObject' have the correct value.");
}

function verifyNextLevels() {
  let localScope = gVariables.getScopeAtIndex(0);
  let objectVar = localScope.get("largeObject");

  let lastPage1 = objectVar.get("[7500" + gEllipsis + "9999]");
  ok(lastPage1, "The last page in the first level was retrieved successfully.");
  return lastPage1.expand()
                  .then(verifyNextLevels2.bind(null, lastPage1));
}

function verifyNextLevels2(lastPage1) {
  let pageEnums1 = lastPage1.target.querySelector(".variables-view-element-details.enum").childNodes;
  let pageNonEnums1 = lastPage1.target.querySelector(".variables-view-element-details.nonenum").childNodes;
  is(pageEnums1.length, 0,
    "The last page in the first level shouldn't contain any enumerable elements.");
  is(pageNonEnums1.length, 4,
    "The last page in the first level should contain all the created non-enumerable elements.");

  is(lastPage1._nonenum.querySelectorAll(".variables-view-property .name")[0].getAttribute("value"),
    "[7500" + gEllipsis + "8124]", "The first page in this level named correctly (1).");
  is(lastPage1._nonenum.querySelectorAll(".variables-view-property .name")[1].getAttribute("value"),
    "[8125" + gEllipsis + "8749]", "The second page in this level named correctly (1).");
  is(lastPage1._nonenum.querySelectorAll(".variables-view-property .name")[2].getAttribute("value"),
    "[8750" + gEllipsis + "9374]", "The third page in this level named correctly (1).");
  is(lastPage1._nonenum.querySelectorAll(".variables-view-property .name")[3].getAttribute("value"),
    "[9375" + gEllipsis + "9999]", "The fourth page in this level named correctly (1).");

  let lastPage2 = lastPage1.get("[9375" + gEllipsis + "9999]");
  ok(lastPage2, "The last page in the second level was retrieved successfully.");
  return lastPage2.expand()
                  .then(verifyNextLevels3.bind(null, lastPage2));
}

function verifyNextLevels3(lastPage2) {
  let pageEnums2 = lastPage2.target.querySelector(".variables-view-element-details.enum").childNodes;
  let pageNonEnums2 = lastPage2.target.querySelector(".variables-view-element-details.nonenum").childNodes;
  is(pageEnums2.length, 625,
    "The last page in the third level should contain all the created enumerable elements.");
  is(pageNonEnums2.length, 0,
    "The last page in the third level shouldn't contain any non-enumerable elements.");

  is(lastPage2._enum.querySelectorAll(".variables-view-property .name")[0].getAttribute("value"),
    9375, "The properties in this level are named correctly (3).");
  is(lastPage2._enum.querySelectorAll(".variables-view-property .name")[1].getAttribute("value"),
    9376, "The properties in this level are named correctly (3).");
  is(lastPage2._enum.querySelectorAll(".variables-view-property .name")[623].getAttribute("value"),
    9998, "The properties in this level are named correctly (3).");
  is(lastPage2._enum.querySelectorAll(".variables-view-property .name")[624].getAttribute("value"),
    9999, "The properties in this level are named correctly (3).");

  is(lastPage2._enum.querySelectorAll(".variables-view-property .value")[0].getAttribute("value"),
    624, "The properties in this level have the correct value (3).");
  is(lastPage2._enum.querySelectorAll(".variables-view-property .value")[1].getAttribute("value"),
    623, "The properties in this level have the correct value (3).");
  is(lastPage2._enum.querySelectorAll(".variables-view-property .value")[623].getAttribute("value"),
    1, "The properties in this level have the correct value (3).");
  is(lastPage2._enum.querySelectorAll(".variables-view-property .value")[624].getAttribute("value"),
    0, "The properties in this level have the correct value (3).");
}

registerCleanupFunction(function() {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gVariables = null;
  gEllipsis = null;
});
