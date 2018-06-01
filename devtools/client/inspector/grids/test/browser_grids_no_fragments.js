/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that when grids exist but have no fragments, clicking on the checkbox doesn't
// fail (see bug 1441462).

const TEST_URI = `
  <style type='text/css'>
    #wrapper { display: none; }
    .grid { display: grid; grid-template-columns: 1fr 1fr; }
  </style>
  <div id="wrapper">
    <div class="grid">
      <div>cell1</div>
      <div>cell2</div>
    </div>
    <div class="grid">
      <div>cell1</div>
      <div>cell2</div>
    </div>
  </div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  const { inspector, gridInspector } = await openLayoutView();
  const { document: doc } = gridInspector;
  const { store } = inspector;

  const gridList = doc.querySelector("#grid-list");
  is(gridList.children.length, 2, "There are 2 grids in the list");

  info("Try to click the first grid's checkbox");
  const checkbox1 = gridList.children[0].querySelector("input");
  const onCheckbox1Change = waitUntilState(store, state =>
    state.grids[0] && state.grids[0].highlighted);
  checkbox1.click();
  await onCheckbox1Change;

  info("Try to click the second grid's checkbox");
  const checkbox2 = gridList.children[1].querySelector("input");
  const onCheckbox2Change = waitUntilState(store, state =>
    state.grids[1] && state.grids[1].highlighted);
  checkbox2.click();
  await onCheckbox2Change;
});
