/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/attributes.js */
loadScripts({ name: "attributes.js", dir: MOCHITESTS_DIR });

addAccessibleTask(
  `<table role="grid" id="grid">
    <tr role="row" id="grid_row1">
      <td role="gridcell" id="grid_cell1">cell1</td>
      <td role="gridcell" id="grid_cell2">cell2</td>
    </tr>
    <tr role="row" id="grid_row2">
      <td role="gridcell" id="grid_cell3">cell3</td>
      <td role="gridcell" id="grid_cell4">cell4</td>
    </tr>
  </table>`,
  async function (browser, accDoc) {
    let getAcc = id => findAccessibleChildByID(accDoc, id);

    // ////////////////////////////////////////////////////////////////////////
    // ARIA grid
    testGroupAttrs(getAcc("grid_row1"), 1, 2);
    testAbsentAttrs(getAcc("grid_cell1"), { posinset: "", setsize: "" });
    testAbsentAttrs(getAcc("grid_cell2"), { posinset: "", setsize: "" });

    testGroupAttrs(getAcc("grid_row2"), 2, 2);
    testAbsentAttrs(getAcc("grid_cell3"), { posinset: "", setsize: "" });
    testAbsentAttrs(getAcc("grid_cell4"), { posinset: "", setsize: "" });
    testGroupParentAttrs(getAcc("grid"), 2, false, false);
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    chrome: true,
  }
);

addAccessibleTask(
  `<div role="treegrid" id="treegrid" aria-colcount="4">
    <div role="row" aria-level="1" id="treegrid_row1">
      <div role="gridcell" id="treegrid_cell1">cell1</div>
      <div role="gridcell" id="treegrid_cell2">cell2</div>
    </div>
    <div role="row" aria-level="2" id="treegrid_row2">
      <div role="gridcell" id="treegrid_cell3">cell1</div>
      <div role="gridcell" id="treegrid_cell4">cell2</div>
    </div>
    <div role="row" id="treegrid_row3">
      <div role="gridcell" id="treegrid_cell5">cell1</div>
      <div role="gridcell" id="treegrid_cell6">cell2</div>
    </div>
  </div>`,
  async function (browser, accDoc) {
    let getAcc = id => findAccessibleChildByID(accDoc, id);

    // ////////////////////////////////////////////////////////////////////////
    // ARIA treegrid
    testGroupAttrs(getAcc("treegrid_row1"), 1, 2, 1);
    testAbsentAttrs(getAcc("treegrid_cell1"), { posinset: "", setsize: "" });
    testAbsentAttrs(getAcc("treegrid_cell2"), { posinset: "", setsize: "" });

    testGroupAttrs(getAcc("treegrid_row2"), 1, 1, 2);
    testAbsentAttrs(getAcc("treegrid_cell3"), { posinset: "", setsize: "" });
    testAbsentAttrs(getAcc("treegrid_cell4"), { posinset: "", setsize: "" });

    testGroupAttrs(getAcc("treegrid_row3"), 2, 2, 1);
    testAbsentAttrs(getAcc("treegrid_cell5"), { posinset: "", setsize: "" });
    testAbsentAttrs(getAcc("treegrid_cell6"), { posinset: "", setsize: "" });

    testGroupParentAttrs(getAcc("treegrid"), 2, true);
    // row child item count provided by parent grid's aria-colcount
    testGroupParentAttrs(getAcc("treegrid_row1"), 4, false);
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    chrome: true,
  }
);

addAccessibleTask(
  `<div id="headings">
    <h1 id="h1">heading1</h1>
    <h2 id="h2">heading2</h2>
    <h3 id="h3">heading3</h3>
    <h4 id="h4">heading4</h4>
    <h5 id="h5">heading5</h5>
    <h6 id="h6">heading6</h6>
    <div id="ariaHeadingNoLevel" role="heading">ariaHeadingNoLevel</div>
  </div>`,
  async function (browser, accDoc) {
    let getAcc = id => findAccessibleChildByID(accDoc, id);

    // ////////////////////////////////////////////////////////////////////////
    // HTML headings
    testGroupAttrs(getAcc("h1"), 0, 0, 1);
    testGroupAttrs(getAcc("h2"), 0, 0, 2);
    testGroupAttrs(getAcc("h3"), 0, 0, 3);
    testGroupAttrs(getAcc("h4"), 0, 0, 4);
    testGroupAttrs(getAcc("h5"), 0, 0, 5);
    testGroupAttrs(getAcc("h6"), 0, 0, 6);
    testGroupAttrs(getAcc("ariaHeadingNoLevel"), 0, 0, 2);
    // No child item counts or "tree" flag for parent of headings
    testAbsentAttrs(getAcc("headings"), { "child-item-count": "", tree: "" });
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    chrome: true,
  }
);

addAccessibleTask(
  `<ul id="combo1" role="combobox">Password
    <li id="combo1_opt1" role="option">Xyzzy</li>
    <li id="combo1_opt2" role="option">Plughs</li>
    <li id="combo1_opt3" role="option">Shazaam</li>
    <li id="combo1_opt4" role="option">JoeSentMe</li>
  </ul>`,
  async function (browser, accDoc) {
    let getAcc = id => findAccessibleChildByID(accDoc, id);

    // ////////////////////////////////////////////////////////////////////////
    // ARIA combobox
    testGroupAttrs(getAcc("combo1_opt1"), 1, 4);
    testGroupAttrs(getAcc("combo1_opt2"), 2, 4);
    testGroupAttrs(getAcc("combo1_opt3"), 3, 4);
    testGroupAttrs(getAcc("combo1_opt4"), 4, 4);
    testGroupParentAttrs(getAcc("combo1"), 4, false);
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    chrome: true,
  }
);

addAccessibleTask(
  `<div role="table" aria-colcount="4" aria-rowcount="2" id="table">
    <div role="row" id="table_row" aria-rowindex="2">
      <div role="cell" id="table_cell" aria-colindex="3">cell</div>
    </div>
  </div>`,
  async function (browser, accDoc) {
    let getAcc = id => findAccessibleChildByID(accDoc, id);

    // ////////////////////////////////////////////////////////////////////////
    // ARIA table
    testGroupAttrs(getAcc("table_cell"), 3, 4);
    testGroupAttrs(getAcc("table_row"), 2, 2);

    // grid child item count provided by aria-rowcount
    testGroupParentAttrs(getAcc("table"), 2, false);
    // row child item count provided by parent grid's aria-colcount
    testGroupParentAttrs(getAcc("table_row"), 4, false);
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    chrome: true,
  }
);

addAccessibleTask(
  `<div role="grid" aria-readonly="true">
    <div tabindex="-1">
      <div role="row" id="wrapped_row_1">
        <div role="gridcell">cell content</div>
      </div>
    </div>
    <div tabindex="-1">
      <div role="row" id="wrapped_row_2">
        <div role="gridcell">cell content</div>
      </div>
    </div>
  </div>`,
  async function (browser, accDoc) {
    let getAcc = id => findAccessibleChildByID(accDoc, id);

    // Attributes calculated even when row is wrapped in a div.
    testGroupAttrs(getAcc("wrapped_row_1"), 1, 2, null);
    testGroupAttrs(getAcc("wrapped_row_2"), 2, 2, null);
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    chrome: true,
  }
);

addAccessibleTask(
  `<div role="list" aria-owns="t1_li1 t1_li2 t1_li3" id="aria-list_4">
    <div role="listitem" id="t1_li2">Apples</div>
    <div role="listitem" id="t1_li1">Oranges</div>
  </div>
  <div role="listitem" id="t1_li3">Bananas</div>`,
  async function (browser, accDoc) {
    let getAcc = id => findAccessibleChildByID(accDoc, id);

    // ////////////////////////////////////////////////////////////////////////
    // ARIA list constructed by ARIA owns
    testGroupAttrs(getAcc("t1_li1"), 1, 3);
    testGroupAttrs(getAcc("t1_li2"), 2, 3);
    testGroupAttrs(getAcc("t1_li3"), 3, 3);
    testGroupParentAttrs(getAcc("aria-list_4"), 3, false);
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    chrome: true,
  }
);

addAccessibleTask(
  `<!-- ARIA comments, 1 level, group pos and size calculation -->
  <article>
    <p id="comm_single_1" role="comment">Comment 1</p>
    <p id="comm_single_2" role="comment">Comment 2</p>
  </article>

  <!-- Nested comments -->
  <article>
    <div id="comm_nested_1" role="comment"><p>Comment 1 level 1</p>
      <div id="comm_nested_1_1" role="comment"><p>Comment 1 level 2</p></div>
      <div id="comm_nested_1_2" role="comment"><p>Comment 2 level 2</p></div>
    </div>
    <div id="comm_nested_2" role="comment"><p>Comment 2 level 1</p>
      <div id="comm_nested_2_1" role="comment"><p>Comment 3 level 2</p>
        <div id="comm_nested_2_1_1" role="comment"><p>Comment 1 level 3</p></div>
      </div>
    </div>
    <div id="comm_nested_3" role="comment"><p>Comment 3 level 1</p></div>
  </article>`,
  async function (browser, accDoc) {
    let getAcc = id => findAccessibleChildByID(accDoc, id);

    // Test group attributes of ARIA comments
    testGroupAttrs(getAcc("comm_single_1"), 1, 2, 1);
    testGroupAttrs(getAcc("comm_single_2"), 2, 2, 1);
    testGroupAttrs(getAcc("comm_nested_1"), 1, 3, 1);
    testGroupAttrs(getAcc("comm_nested_1_1"), 1, 2, 2);
    testGroupAttrs(getAcc("comm_nested_1_2"), 2, 2, 2);
    testGroupAttrs(getAcc("comm_nested_2"), 2, 3, 1);
    testGroupAttrs(getAcc("comm_nested_2_1"), 1, 1, 2);
    testGroupAttrs(getAcc("comm_nested_2_1_1"), 1, 1, 3);
    testGroupAttrs(getAcc("comm_nested_3"), 3, 3, 1);
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    chrome: true,
  }
);

addAccessibleTask(
  `<div role="tree" id="tree4"><div role="treeitem"
  id="tree4_ti1">Item 1</div><div role="treeitem"
  id="tree4_ti2">Item 2</div></div>`,
  async function (browser, accDoc) {
    let getAcc = id => findAccessibleChildByID(accDoc, id);

    // Test that group position information updates after deleting node.
    testGroupAttrs(getAcc("tree4_ti1"), 1, 2, 1);
    testGroupAttrs(getAcc("tree4_ti2"), 2, 2, 1);
    testGroupParentAttrs(getAcc("tree4"), 2, true);

    let p = waitForEvent(EVENT_REORDER, "tree4");
    invokeContentTask(browser, [], () => {
      content.document.getElementById("tree4_ti1").remove();
    });

    await p;
    testGroupAttrs(getAcc("tree4_ti2"), 1, 1, 1);
    testGroupParentAttrs(getAcc("tree4"), 1, true);
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    chrome: true,
  }
);

// Verify that intervening SECTION accs in ARIA compound widgets do not split
// up the group info for descendant owned elements. Test various types of
// widgets that should all be treated the same.
addAccessibleTask(
  `<div role="tree" id="tree">
    <div tabindex="0">
      <div role="treeitem" id="ti1">treeitem 1</div>
    </div>
    <div tabindex="0">
      <div role="treeitem" id="ti2">treeitem 2</div>
    </div>
  </div>
  <div role="listbox" id="listbox">
    <div tabindex="0">
      <div role="option" id="opt1">option 1</div>
    </div>
    <div tabindex="0">
      <div role="option" id="opt2">option 2</div>
    </div>
  </div>
  <div role="list" id="list">
    <div tabindex="0">
      <div role="listitem" id="li1">listitem 1</div>
    </div>
    <div tabindex="0">
      <div role="listitem" id="li2">listitem 2</div>
    </div>
  </div>
  <div role="menu" id="menu">
    <div tabindex="0">
      <div role="menuitem" id="mi1">menuitem 1</div>
    </div>
    <div tabindex="0">
      <div role="menuitem" id="mi2">menuitem 2</div>
    </div>
  </div>
  <div role="radiogroup" id="radiogroup">
    <div tabindex="0">
      <div role="radio" id="r1">radio 1</div>
    </div>
    <div tabindex="0">
      <div role="radio" id="r2">radio 2</div>
    </div>
  </div>
`,
  async function (browser, accDoc) {
    let getAcc = id => findAccessibleChildByID(accDoc, id);

    testGroupAttrs(getAcc("ti1"), 1, 2, 1);
    testGroupAttrs(getAcc("ti2"), 2, 2, 1);

    testGroupAttrs(getAcc("opt1"), 1, 2, 0);
    testGroupAttrs(getAcc("opt2"), 2, 2, 0);

    testGroupAttrs(getAcc("li1"), 1, 2, 0);
    testGroupAttrs(getAcc("li2"), 2, 2, 0);

    testGroupAttrs(getAcc("mi1"), 1, 2, 0);
    testGroupAttrs(getAcc("mi2"), 2, 2, 0);

    testGroupAttrs(getAcc("r1"), 1, 2, 0);
    testGroupAttrs(getAcc("r2"), 2, 2, 0);
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    chrome: true,
  }
);

// Verify that non-generic accessibles (like buttons) correctly split the group
// info of descendant owned elements.
addAccessibleTask(
  `<div role="tree" id="tree">
    <div role="button">
      <div role="treeitem" id="ti1">first</div>
    </div>
    <div tabindex="0">
      <div role="treeitem" id="ti2">second</div>
    </div>
  </div>`,
  async function (browser, accDoc) {
    let getAcc = id => findAccessibleChildByID(accDoc, id);

    testGroupAttrs(getAcc("ti1"), 1, 1, 1);
    testGroupAttrs(getAcc("ti2"), 1, 1, 1);
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    chrome: true,
  }
);
