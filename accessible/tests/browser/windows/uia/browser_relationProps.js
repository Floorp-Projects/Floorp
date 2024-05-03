/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function testUiaRelationArray(id, prop, targets) {
  return isUiaElementArray(
    `findUiaByDomId(doc, "${id}").Current${prop}`,
    targets,
    `${id} has correct ${prop} targets`
  );
}

/**
 * Test the ControllerFor property.
 */
addUiaTask(
  `
<input id="controls" aria-controls="t1 t2">
<input id="error" aria-errormessage="t3 t4" aria-invalid="true">
<input id="controlsError" aria-controls="t1 t2" aria-errormessage="t3 t4" aria-invalid="true">
<div id="t1">t1</div>
<div id="t2">t2</div>
<div id="t3">t3</div>
<div id="t4">t4</div>
<button id="none">none</button>
  `,
  async function testControllerFor() {
    await definePyVar("doc", `getDocUia()`);
    await testUiaRelationArray("controls", "ControllerFor", ["t1", "t2"]);
    // The IA2 -> UIA proxy doesn't support IA2_RELATION_ERROR.
    if (gIsUiaEnabled) {
      await testUiaRelationArray("error", "ControllerFor", ["t3", "t4"]);
      await testUiaRelationArray("controlsError", "ControllerFor", [
        "t1",
        "t2",
        "t3",
        "t4",
      ]);
    }
    await testUiaRelationArray("none", "ControllerFor", []);
  }
);

/**
 * Test the DescribedBy property.
 */
addUiaTask(
  `
<input id="describedby" aria-describedby="t1 t2">
<input id="details" aria-details="t3 t4">
<input id="describedbyDetails" aria-describedby="t1 t2" aria-details="t3 t4" aria-invalid="true">
<div id="t1">t1</div>
<div id="t2">t2</div>
<div id="t3">t3</div>
<div id="t4">t4</div>
<button id="none">none</button>
  `,
  async function testDescribedBy() {
    await definePyVar("doc", `getDocUia()`);
    await testUiaRelationArray("describedby", "DescribedBy", ["t1", "t2"]);
    // The IA2 -> UIA proxy doesn't support IA2_RELATION_DETAILS.
    if (gIsUiaEnabled) {
      await testUiaRelationArray("details", "DescribedBy", ["t3", "t4"]);
      await testUiaRelationArray("describedbyDetails", "DescribedBy", [
        "t1",
        "t2",
        "t3",
        "t4",
      ]);
    }
    await testUiaRelationArray("none", "DescribedBy", []);
  }
);

/**
 * Test the FlowsFrom and FlowsTo properties.
 */
addUiaTask(
  `
<div id="t1" aria-flowto="t2">t1</div>
<div id="t2">t2</div>
<button id="none">none</button>
  `,
  async function testFlows() {
    await definePyVar("doc", `getDocUia()`);
    await testUiaRelationArray("t1", "FlowsTo", ["t2"]);
    await testUiaRelationArray("t2", "FlowsFrom", ["t1"]);
    await testUiaRelationArray("none", "FlowsFrom", []);
    await testUiaRelationArray("none", "FlowsTo", []);
  }
);
