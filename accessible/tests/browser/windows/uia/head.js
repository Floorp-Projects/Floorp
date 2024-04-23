/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported gIsUiaEnabled, addUiaTask, definePyVar, assignPyVarToUiaWithId, setUpWaitForUiaEvent, setUpWaitForUiaPropEvent, waitForUiaEvent, testPatternAbsent, testPythonRaises, isUiaElementArray */

// Load the shared-head file first.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/accessible/tests/browser/shared-head.js",
  this
);

// Loading and common.js from accessible/tests/mochitest/ for all tests, as
// well as promisified-events.js.
loadScripts(
  { name: "common.js", dir: MOCHITESTS_DIR },
  { name: "promisified-events.js", dir: MOCHITESTS_DIR }
);

let gIsUiaEnabled = false;

/**
 * This is like addAccessibleTask, but takes two additional boolean options:
 * - uiaEnabled: Whether to run a variation of this test with Gecko UIA enabled.
 * - uiaDisabled: Whether to run a variation of this test with UIA disabled. In
 *   this case, UIA will rely entirely on the IA2 -> UIA proxy.
 * If both are set, the test will be run twice with different configurations.
 * You can determine which variant is currently running using the gIsUiaEnabled
 * variable. This is useful for conditional tests; e.g. if Gecko UIA supports
 * something that the IA2 -> UIA proxy doesn't support.
 */
function addUiaTask(doc, task, options = {}) {
  const { uiaEnabled = true, uiaDisabled = true } = options;

  function addTask(shouldEnable) {
    async function uiaTask(browser, docAcc, topDocAcc) {
      await SpecialPowers.pushPrefEnv({
        set: [["accessibility.uia.enable", shouldEnable]],
      });
      gIsUiaEnabled = shouldEnable;
      info(shouldEnable ? "Gecko UIA enabled" : "Gecko UIA disabled");
      await task(browser, docAcc, topDocAcc);
    }
    addAccessibleTask(doc, uiaTask, options);
  }

  if (uiaEnabled) {
    addTask(true);
  }
  if (uiaDisabled) {
    addTask(false);
  }
}

/**
 * Define a global Python variable and assign it to a given Python expression.
 */
function definePyVar(varName, expression) {
  return runPython(`
    global ${varName}
    ${varName} = ${expression}
  `);
}

/**
 * Get the UIA element with the given id and assign it to a global Python
 * variable using the id as the variable name.
 */
function assignPyVarToUiaWithId(id) {
  return definePyVar(id, `findUiaByDomId(doc, "${id}")`);
}

/**
 * Set up to wait for a UIA event. You must await this before performing the
 * action which fires the event.
 */
function setUpWaitForUiaEvent(eventName, id) {
  return definePyVar(
    "onEvent",
    `WaitForUiaEvent(eventId=UIA_${eventName}EventId, match="${id}")`
  );
}

/**
 * Set up to wait for a UIA property change event. You must await this before
 * performing the action which fires the event.
 */
function setUpWaitForUiaPropEvent(propName, id) {
  return definePyVar(
    "onEvent",
    `WaitForUiaEvent(property=UIA_${propName}PropertyId, match="${id}")`
  );
}

/**
 * Wait for the event requested in setUpWaitForUia*Event.
 */
function waitForUiaEvent() {
  return runPython(`
    onEvent.wait()
  `);
}

/**
 * Verify that a UIA element does *not* support the given control pattern.
 */
async function testPatternAbsent(id, patternName) {
  const hasPattern = await runPython(`
    el = findUiaByDomId(doc, "${id}")
    return bool(getUiaPattern(el, "${patternName}"))
  `);
  ok(!hasPattern, `${id} doesn't have ${patternName} pattern`);
}

/**
 * Verify that a Python expression raises an exception.
 */
async function testPythonRaises(expression, message) {
  let failed = false;
  try {
    await runPython(expression);
  } catch {
    failed = true;
  }
  ok(failed, message);
}

/**
 * Verify that an array of UIA elements contains (only) elements with the given
 * DOM ids.
 */
async function isUiaElementArray(pyExpr, ids, message) {
  const result = await runPython(`
    uias = (${pyExpr})
    return [uias.GetElement(i).CurrentAutomationId for i in range(uias.Length)]
  `);
  SimpleTest.isDeeply(result, ids, message);
}
