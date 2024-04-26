/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const SNIPPET = `
<select id="selectList" size="2">
  <option id="sl1" selected>sl1</option>
  <option id="sl2">sl2</option>
</select>
<select id="selectRequired" size="2" required>
  <option id="sr1">sr1</option>
</select>
<select id="selectMulti" size="2" multiple>
  <option id="sm1" selected>sm1</option>
  <option id="sm2">sm2</option>
  <option id="sm3" selected>sm3</option>
  <option id="sm4">sm4</option>
  <option id="sm5">sm5</option>
  <option id="sm6">sm6</option>
</select>
<select id="selectCombo" size="1">
  <option>sc1</option>
</select>
<div id="ariaListbox" role="listbox">
  <div id="al1" role="option" aria-selected="true">al1</div>
  <div id="al2" role="option">al2</div>
</div>
<div id="tablist" role="tablist">
  <div id="t1" role="tab">t1</div>
  <div id="t2" role="tab" aria-selected="true">t2</div>
</div>
<table id="grid" role="grid" aria-multiselectable="true">
  <tr>
    <td id="g1">g1</td>
    <td id="g2" role="gridcell" aria-selected="true">g2</td>
  </tr>
  <tr>
    <td id="g3">g3</td>
    <td id="g4" role="gridcell" aria-selected="true">g4</td>
  </tr>
</table>
<div id="radiogroup" role="radiogroup">
  <label><input id="r1" type="radio" name="r" checked>r1</label>
  <label><input id="r2" type="radio" name="r">r2</label>
</div>
<div id="menu" role="menu">
  <div id="m1" role="menuitem">m1</div>
  <div id="m2" role="menuitemradio">m2</div>
  <div id="m3" role="menuitemradio" aria-checked="true">m3</div>
</div>
<button id="button">button</button>
`;

async function testSelectionProps(id, selection, multiple, required) {
  await assignPyVarToUiaWithId(id);
  await definePyVar("pattern", `getUiaPattern(${id}, "Selection")`);
  ok(await runPython(`bool(pattern)`), `${id} has Selection pattern`);
  await isUiaElementArray(
    `pattern.GetCurrentSelection()`,
    selection,
    `${id} has correct Selection`
  );
  is(
    !!(await runPython(`pattern.CurrentCanSelectMultiple`)),
    multiple,
    `${id} has correct CanSelectMultiple`
  );
  // The IA2 -> UIA proxy doesn't reflect the required state correctly.
  if (gIsUiaEnabled) {
    is(
      !!(await runPython(`pattern.CurrentIsSelectionRequired`)),
      required,
      `${id} has correct IsSelectionRequired`
    );
  }
}

async function testSelectionItemProps(id, selected, container) {
  await assignPyVarToUiaWithId(id);
  await definePyVar("pattern", `getUiaPattern(${id}, "SelectionItem")`);
  ok(await runPython(`bool(pattern)`), `${id} has SelectionItem pattern`);
  is(
    !!(await runPython(`pattern.CurrentIsSelected`)),
    selected,
    `${id} has correct IsSelected`
  );
  if (container) {
    is(
      await runPython(`pattern.CurrentSelectionContainer.CurrentAutomationId`),
      container,
      `${id} has correct SelectionContainer`
    );
  } else {
    ok(
      !(await runPython(`bool(pattern.CurrentSelectionContainer)`)),
      `${id} has no SelectionContainer`
    );
  }
}

/**
 * Test the Selection pattern.
 */
addUiaTask(SNIPPET, async function testSelection(browser) {
  await definePyVar("doc", `getDocUia()`);
  await testSelectionProps("selectList", ["sl1"], false, false);
  await testSelectionProps("selectRequired", [], false, true);

  await testSelectionProps("selectMulti", ["sm1", "sm3"], true, false);
  // The Selection pattern only has an event for a complete invalidation of the
  // container's selection, which only happens when there are too many selection
  // events. Smaller selection changes fire events in the SelectionItem pattern.
  info("Changing selectMulti selection");
  await setUpWaitForUiaEvent("Selection_Invalidated", "selectMulti");
  await invokeContentTask(browser, [], () => {
    const multi = content.document.getElementById("selectMulti");
    multi[0].selected = false;
    multi[1].selected = true;
    multi[2].selected = false;
    multi[3].selected = true;
    multi[4].selected = true;
    multi[5].selected = true;
  });
  await waitForUiaEvent();
  ok(true, "select got Invalidated event");
  await testSelectionProps(
    "selectMulti",
    ["sm2", "sm4", "sm5", "sm6"],
    true,
    false
  );

  await testPatternAbsent("selectCombo", "Selection");

  await testSelectionProps("ariaListbox", ["al1"], false, false);
  await testSelectionProps("tablist", ["t2"], false, false);
  // The IA2 -> UIA proxy doesn't expose the Selection pattern on grids.
  if (gIsUiaEnabled) {
    await testSelectionProps("grid", ["g2", "g4"], true, false);
  }

  // radio gets the SelectionItem pattern, but radiogroup doesn't get the
  // Selection pattern for now. Same for menu/menuitemradio.
  await testPatternAbsent("radiogroup", "Selection");
  await testPatternAbsent("menu", "Selection");

  await testPatternAbsent("button", "Selection");
});

/**
 * Test the SelectionItem pattern.
 */
addUiaTask(SNIPPET, async function testSelection() {
  await definePyVar("doc", `getDocUia()`);
  await testPatternAbsent("selectList", "SelectionItem");
  await testSelectionItemProps("sl1", true, "selectList");
  await testSelectionItemProps("sl2", false, "selectList");
  info("Calling Select on sl2");
  await setUpWaitForUiaEvent("SelectionItem_ElementSelected", "sl2");
  await runPython(`pattern.Select()`);
  await waitForUiaEvent();
  ok(true, "sl2 got ElementSelected event");
  await testSelectionItemProps("sl1", false, "selectList");
  await testSelectionItemProps("sl2", true, "selectList");

  await testSelectionItemProps("sr1", false, "selectRequired");

  await testSelectionItemProps("sm1", true, "selectMulti");
  await testSelectionItemProps("sm2", false, "selectMulti");
  info("Calling AddToSelection on sm2");
  await setUpWaitForUiaEvent("SelectionItem_ElementAddedToSelection", "sm2");
  await runPython(`pattern.AddToSelection()`);
  await waitForUiaEvent();
  ok(true, "sm2 got ElementAddedToSelection event");
  await testSelectionItemProps("sm2", true, "selectMulti");
  await testSelectionItemProps("sm3", true, "selectMulti");
  info("Calling RemoveFromSelection on sm3");
  await setUpWaitForUiaEvent(
    "SelectionItem_ElementRemovedFromSelection",
    "sm3"
  );
  await runPython(`pattern.RemoveFromSelection()`);
  await waitForUiaEvent();
  ok(true, "sm3 got ElementRemovedFromSelection event");
  await testSelectionItemProps("sm3", false, "selectMulti");

  await testSelectionItemProps("t1", false, "tablist");
  await testSelectionItemProps("t2", true, "tablist");

  // The IA2 -> UIA proxy doesn't expose the SelectionItem pattern on grid
  // cells.
  if (gIsUiaEnabled) {
    await testSelectionItemProps("g1", false, "grid");
    await testSelectionItemProps("g2", true, "grid");
  }

  await testSelectionItemProps("r1", true, null);
  await testSelectionItemProps("r2", false, null);
  // The IA2 -> UIA proxy doesn't fire correct events for radio buttons.
  if (gIsUiaEnabled) {
    info("Calling Select on r2");
    await setUpWaitForUiaEvent("SelectionItem_ElementSelected", "r2");
    await runPython(`pattern.Select()`);
    await waitForUiaEvent();
    ok(true, "r2 got ElementSelected event");
    await testSelectionItemProps("r1", false, null);
    await testSelectionItemProps("r2", true, null);
    info("Calling RemoveFromSelection on r2");
    await testPythonRaises(
      `pattern.RemoveFromSelection()`,
      "RemoveFromSelection failed on r2"
    );
  }

  await testPatternAbsent("m1", "SelectionItem");
  // The IA2 -> UIA proxy doesn't expose the SelectionItem pattern for radio
  // menu items.
  if (gIsUiaEnabled) {
    await testSelectionItemProps("m2", false, null);
    await testSelectionItemProps("m3", true, null);
  }

  await testPatternAbsent("button", "SelectionItem");
});
