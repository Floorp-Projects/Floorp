/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/selectable.js */

// ////////////////////////////////////////////////////////////////////////
// role="tablist" role="listbox" role="grid" role="tree" role="treegrid"
addAccessibleTask(
  `<div role="tablist" id="tablist">
      <div role="tab">tab1</div>
      <div role="tab">tab2</div>
    </div>
    <div role="listbox" id="listbox">
      <div role="option">item1</div>
      <div role="option">item2</div>
    </div>
    <div role="grid" id="grid">
    <div role="row">
      <span role="gridcell">cell</span>
      <span role="gridcell">cell</span>
    </div>
    <div role="row">
      <span role="gridcell">cell</span>
      <span role="gridcell">cell</span>
    </div>
    </div>
    <div role="tree" id="tree">
      <div role="treeitem">
        item1
        <div role="group">
          <div role="treeitem">item1.1</div>
        </div>
      </div>
      <div>item2</div>
    </div>
    <div role="treegrid" id="treegrid">
      <div role="row" aria-level="1">
        <span role="gridcell">cell</span>
        <span role="gridcell">cell</span>
      </div>
      <div role="row" aria-level="2">
        <span role="gridcell">cell</span>
        <span role="gridcell">cell</span>
      </div>
      <div role="row" aria-level="1">
        <span role="gridcell">cell</span>
        <span role="gridcell">cell</span>
      </div>
    </div>`,
  async function (browser, docAcc) {
    info(
      'role="tablist" role="listbox" role="grid" role="tree" role="treegrid"'
    );
    testSelectableSelection(findAccessibleChildByID(docAcc, "tablist"), []);
    testSelectableSelection(findAccessibleChildByID(docAcc, "listbox"), []);
    testSelectableSelection(findAccessibleChildByID(docAcc, "grid"), []);
    testSelectableSelection(findAccessibleChildByID(docAcc, "tree"), []);
    testSelectableSelection(findAccessibleChildByID(docAcc, "treegrid"), []);
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
    remoteIframe: true,
  }
);

// ////////////////////////////////////////////////////////////////////////
// role="tablist" aria-multiselectable
addAccessibleTask(
  `<div role="tablist" id="tablist" aria-multiselectable="true">
     <div role="tab" id="tab_multi1" aria-selected="true">tab1</div>
     <div role="tab" id="tab_multi2">tab2</div>
     <div role="tab" id="tab_multi3" aria-selected="true">tab3</div>
   </div>`,
  async function (browser, docAcc) {
    info('role="tablist" aria-multiselectable');
    let tablist = findAccessibleChildByID(docAcc, "tablist", [
      nsIAccessibleSelectable,
    ]);
    testSelectableSelection(tablist, ["tab_multi1", "tab_multi3"]);
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
    remoteIframe: true,
  }
);

// ////////////////////////////////////////////////////////////////////////
// role="listbox" aria-multiselectable
addAccessibleTask(
  `<div role="listbox" id="listbox" aria-multiselectable="true">
     <div role="option" id="listbox2_item1" aria-selected="true">item1</div>
     <div role="option" id="listbox2_item2">item2</div>
     <div role="option" id="listbox2_item3" aria-selected="true">item2</div>
   </div>`,
  async function (browser, docAcc) {
    info('role="listbox" aria-multiselectable');
    let listbox = findAccessibleChildByID(docAcc, "listbox", [
      nsIAccessibleSelectable,
    ]);
    testSelectableSelection(listbox, ["listbox2_item1", "listbox2_item3"]);
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
    remoteIframe: true,
  }
);

// ////////////////////////////////////////////////////////////////////////
// role="grid" aria-multiselectable, selectable children in subtree
addAccessibleTask(
  `
  <table tabindex="0" border="2" cellspacing="0" id="grid" role="grid"
      aria-multiselectable="true">
    <thead>
      <tr>
        <th tabindex="-1" role="columnheader" id="grid_colhead1"
            style="width:6em" aria-selected="true">Entry #</th>
        <th tabindex="-1" role="columnheader" id="grid_colhead2"
            style="width:10em">Date</th>
        <th tabindex="-1" role="columnheader" id="grid_colhead3"
            style="width:20em">Expense</th>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td tabindex="-1" role="rowheader" id="grid_rowhead"
            aria-readonly="true">1</td>
        <td tabindex="-1" id="grid_cell1"
            aria-selected="true">03/14/05</td>
        <td tabindex="-1" id="grid_cell2"
            aria-selected="false">Conference Fee</td>
      </tr>
    </tbody>
  </table>
  <table id="table">
    <tr><th>a</th><td id="tableB" aria-selected="true">b</td></tr>
  </table>
  `,
  async function (browser, docAcc) {
    info('role="grid" aria-multiselectable, selectable children in subtree');
    const grid = findAccessibleChildByID(docAcc, "grid", [
      nsIAccessibleSelectable,
    ]);
    // grid_cell1 is a <td> with an implicit role of gridcell.
    testSelectableSelection(grid, ["grid_colhead1", "grid_cell1"]);
    info("Verify aria-selected doesn't apply to <td> that isn't gridcell");
    // We can't use testSelectableSelection here because table (rightly) isn't a
    // selectable container.
    const tableB = findAccessibleChildByID(docAcc, "tableB");
    testStates(tableB, 0, 0, STATE_SELECTED, 0);
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
    remoteIframe: true,
  }
);
