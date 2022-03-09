/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/selectable.js */
/* import-globals-from ../../mochitest/states.js */

// ////////////////////////////////////////////////////////////////////////
// select@size="1" aka combobox
addAccessibleTask(
  `<select id="combobox">
     <option id="item1">option1</option>
     <option id="item2">option2</option>
   </select>`,
  async function(browser, docAcc) {
    info("select@size='1' aka combobox");
    let combobox = findAccessibleChildByID(docAcc, "combobox");
    let comboboxList = combobox.firstChild;
    ok(
      isAccessible(comboboxList, [nsIAccessibleSelectable]),
      "No selectable accessible for combobox"
    );

    let select = getAccessible(comboboxList, [nsIAccessibleSelectable]);
    testSelectableSelection(select, ["item1"]);

    // select 2nd item
    let promise = Promise.all([
      waitForStateChange("item2", STATE_SELECTED, true),
      waitForStateChange("item1", STATE_SELECTED, false),
    ]);
    select.addItemToSelection(1);
    await promise;
    testSelectableSelection(select, ["item2"], "addItemToSelection(1): ");

    // unselect 2nd item, 1st item gets selected automatically
    promise = Promise.all([
      waitForStateChange("item2", STATE_SELECTED, false),
      waitForStateChange("item1", STATE_SELECTED, true),
    ]);
    select.removeItemFromSelection(1);
    await promise;
    testSelectableSelection(select, ["item1"], "removeItemFromSelection(1): ");

    // doesn't change selection
    is(select.selectAll(), false, "No way to select all items in combobox");
    testSelectableSelection(select, ["item1"], "selectAll: ");

    // doesn't change selection
    select.unselectAll();
    testSelectableSelection(select, ["item1"], "unselectAll: ");
  },
  {
    chrome: true,
    topLevel: !isWinNoCache,
    iframe: !isWinNoCache,
    remoteIframe: !isWinNoCache,
  }
);

// ////////////////////////////////////////////////////////////////////////
// select@size="1" with optgroups
addAccessibleTask(
  `<select id="combobox">
    <option id="item1">option1</option>
    <optgroup>optgroup
      <option id="item2">option2</option>
    </optgroup>
  </select>`,
  async function(browser, docAcc) {
    info("select@size='1' with optgroups");
    let combobox = findAccessibleChildByID(docAcc, "combobox");
    let comboboxList = combobox.firstChild;
    ok(
      isAccessible(comboboxList, [nsIAccessibleSelectable]),
      "No selectable accessible for combobox"
    );

    let select = getAccessible(comboboxList, [nsIAccessibleSelectable]);
    testSelectableSelection(select, ["item1"]);

    let promise = Promise.all([
      waitForStateChange("item2", STATE_SELECTED, true),
      waitForStateChange("item1", STATE_SELECTED, false),
    ]);
    select.addItemToSelection(1);
    await promise;
    testSelectableSelection(select, ["item2"], "addItemToSelection(1): ");

    promise = Promise.all([
      waitForStateChange("item2", STATE_SELECTED, false),
      waitForStateChange("item1", STATE_SELECTED, true),
    ]);
    select.removeItemFromSelection(1);
    await promise;
    testSelectableSelection(select, ["item1"], "removeItemFromSelection(1): ");

    is(select.selectAll(), false, "No way to select all items in combobox");
    testSelectableSelection(select, ["item1"]);

    select.unselectAll();
    testSelectableSelection(select, ["item1"]);
  },
  {
    chrome: true,
    topLevel: !isWinNoCache,
    iframe: !isWinNoCache,
    remoteIframe: !isWinNoCache,
  }
);

// ////////////////////////////////////////////////////////////////////////
// select@size="4" aka single selectable listbox
addAccessibleTask(
  `<select id="listbox" size="4">
    <option id="item1">option1</option>
    <option id="item2">option2</option>
   </select>`,
  async function(browser, docAcc) {
    info("select@size='4' aka single selectable listbox");
    let select = findAccessibleChildByID(docAcc, "listbox", [
      nsIAccessibleSelectable,
    ]);
    testSelectableSelection(select, []);

    // select 2nd item
    let promise = waitForStateChange("item2", STATE_SELECTED, true);
    select.addItemToSelection(1);
    await promise;
    testSelectableSelection(select, ["item2"], "addItemToSelection(1): ");

    // unselect 2nd item, 1st item gets selected automatically
    promise = waitForStateChange("item2", STATE_SELECTED, false);
    select.removeItemFromSelection(1);
    await promise;
    testSelectableSelection(select, [], "removeItemFromSelection(1): ");

    // doesn't change selection
    is(
      select.selectAll(),
      false,
      "No way to select all items in single selectable listbox"
    );
    testSelectableSelection(select, [], "selectAll: ");

    // doesn't change selection
    select.unselectAll();
    testSelectableSelection(select, [], "unselectAll: ");
  },
  {
    chrome: true,
    topLevel: !isWinNoCache,
    iframe: !isWinNoCache,
    remoteIframe: !isWinNoCache,
  }
);

// ////////////////////////////////////////////////////////////////////////
// select@size="4" with optgroups, single selectable
addAccessibleTask(
  `<select id="listbox" size="4">
    <option id="item1">option1</option>
    <optgroup>optgroup>
      <option id="item2">option2</option>
    </optgroup>
   </select>`,
  async function(browser, docAcc) {
    info("select@size='4' with optgroups, single selectable");
    let select = findAccessibleChildByID(docAcc, "listbox", [
      nsIAccessibleSelectable,
    ]);
    testSelectableSelection(select, []);

    let promise = waitForStateChange("item2", STATE_SELECTED, true);
    select.addItemToSelection(1);
    await promise;
    testSelectableSelection(select, ["item2"]);

    promise = waitForStateChange("item2", STATE_SELECTED, false);
    select.removeItemFromSelection(1);
    await promise;
    testSelectableSelection(select, []);

    is(
      select.selectAll(),
      false,
      "No way to select all items in single selectable listbox"
    );
    testSelectableSelection(select, []);

    select.unselectAll();
    testSelectableSelection(select, []);
  },
  {
    chrome: true,
    topLevel: !isWinNoCache,
    iframe: !isWinNoCache,
    remoteIframe: !isWinNoCache,
  }
);

// ////////////////////////////////////////////////////////////////////////
// select@size="4" multiselect aka listbox
addAccessibleTask(
  `<select id="listbox" size="4" multiple="true">
    <option id="item1">option1</option>
    <option id="item2">option2</option>
   </select>`,
  async function(browser, docAcc) {
    info("select@size='4' multiselect aka listbox");
    let select = findAccessibleChildByID(docAcc, "listbox", [
      nsIAccessibleSelectable,
    ]);
    await testMultiSelectable(
      select,
      ["item1", "item2"],
      "select@size='4' multiselect aka listbox "
    );
  },
  {
    chrome: true,
    topLevel: !isWinNoCache,
    iframe: !isWinNoCache,
    remoteIframe: !isWinNoCache,
  }
);

// ////////////////////////////////////////////////////////////////////////
// select@size="4" multiselect with optgroups
addAccessibleTask(
  `<select id="listbox" size="4" multiple="true">
    <option id="item1">option1</option>
    <optgroup>optgroup>
      <option id="item2">option2</option>
    </optgroup>
   </select>`,
  async function(browser, docAcc) {
    info("select@size='4' multiselect with optgroups");
    let select = findAccessibleChildByID(docAcc, "listbox", [
      nsIAccessibleSelectable,
    ]);
    await testMultiSelectable(
      select,
      ["item1", "item2"],
      "select@size='4' multiselect aka listbox "
    );
  },
  {
    chrome: true,
    topLevel: !isWinNoCache,
    iframe: !isWinNoCache,
    remoteIframe: !isWinNoCache,
  }
);
