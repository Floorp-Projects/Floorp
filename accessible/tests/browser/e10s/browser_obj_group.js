/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/attributes.js */
loadScripts({ name: "attributes.js", dir: MOCHITESTS_DIR });

/**
 * select elements
 */
addAccessibleTask(
  `<select>
    <option id="opt1-nosize">option1</option>
    <option id="opt2-nosize">option2</option>
    <option id="opt3-nosize">option3</option>
    <option id="opt4-nosize">option4</option>
  </select>

  <select size="4">
    <option id="opt1">option1</option>
    <option id="opt2">option2</option>
  </select>

  <select size="4">
    <optgroup id="select2_optgroup" label="group">
      <option id="select2_opt1">option1</option>
      <option id="select2_opt2">option2</option>
    </optgroup>
    <option id="select2_opt3">option3</option>
    <option id="select2_opt4">option4</option>
  </select>`,
  async function(browser, accDoc) {
    let getAcc = id => findAccessibleChildByID(accDoc, id);

    // ////////////////////////////////////////////////////////////////////////
    // HTML select with no size attribute.
    testGroupAttrs(getAcc("opt1-nosize"), 1, 4);
    testGroupAttrs(getAcc("opt2-nosize"), 2, 4);
    testGroupAttrs(getAcc("opt3-nosize"), 3, 4);
    testGroupAttrs(getAcc("opt4-nosize"), 4, 4);

    // Container should have item count and not hierarchical
    testGroupParentAttrs(getAcc("opt1-nosize").parent, 4, false);

    // ////////////////////////////////////////////////////////////////////////
    // HTML select
    testGroupAttrs(getAcc("opt1"), 1, 2);
    testGroupAttrs(getAcc("opt2"), 2, 2);

    // ////////////////////////////////////////////////////////////////////////
    // HTML select with optgroup
    testGroupAttrs(getAcc("select2_opt3"), 1, 2, 1);
    testGroupAttrs(getAcc("select2_opt4"), 2, 2, 1);
    testGroupAttrs(getAcc("select2_opt1"), 1, 2, 2);
    testGroupAttrs(getAcc("select2_opt2"), 2, 2, 2);
  }
);

/**
 * radios
 */
addAccessibleTask(
  `<form>
    <input type="radio" id="radio1" name="group1"/>
    <input type="radio" id="radio2" name="group1"/>
  </form>

  <input type="radio" id="radio3" name="group2"/>
  <input type="radio" id="radio4" name="group2"/>

  <form>
    <input type="radio" style="display: none;" name="group3">
    <input type="radio" id="radio5" name="group3">
  </form>`,
  async function(browser, accDoc) {
    let getAcc = id => findAccessibleChildByID(accDoc, id);

    // ////////////////////////////////////////////////////////////////////////
    // HTML input@type="radio" within form
    testGroupAttrs(getAcc("radio1"), 1, 2);
    testGroupAttrs(getAcc("radio2"), 2, 2);

    // ////////////////////////////////////////////////////////////////////////
    // HTML input@type="radio" within document
    testGroupAttrs(getAcc("radio3"), 1, 2);
    testGroupAttrs(getAcc("radio4"), 2, 2);

    // ////////////////////////////////////////////////////////////////////////
    // Hidden HTML input@type="radio"
    testGroupAttrs(getAcc("radio5"), 1, 1);
  }
);

/**
 * lists
 */
addAccessibleTask(
  `<ul id="ul">
    <li id="li1">Oranges</li>
    <li id="li2">Apples</li>
    <li id="li3">Bananas</li>
  </ul>

  <ol id="ol">
    <li id="li4">Oranges</li>
    <li id="li5">Apples</li>
    <li id="li6">Bananas
      <ul id="ol_nested">
        <li id="n_li4">Oranges</li>
        <li id="n_li5">Apples</li>
        <li id="n_li6">Bananas</li>
      </ul>
    </li>
  </ol>

  <span role="list" id="aria-list_1">
    <span role="listitem" id="li7">Oranges</span>
    <span role="listitem" id="li8">Apples</span>
    <span role="listitem" id="li9">Bananas</span>
  </span>

  <span role="list" id="aria-list_2">
    <span role="listitem" id="li10">Oranges</span>
    <span role="listitem" id="li11">Apples</span>
    <span role="listitem" id="li12">Bananas
      <span role="list" id="aria-list_2_1">
        <span role="listitem" id="n_li10">Oranges</span>
        <span role="listitem" id="n_li11">Apples</span>
        <span role="listitem" id="n_li12">Bananas</span>
      </span>
    </span>
  </span>

  <div role="list" id="aria-list_3">
    <div role="listitem" id="lgt_li1">Item 1
      <div role="group">
        <div role="listitem" id="lgt_li1_nli1">Item 1A</div>
        <div role="listitem" id="lgt_li1_nli2">Item 1B</div>
      </div>
    </div>
    <div role="listitem" id="lgt_li2">Item 2
      <div role="group">
        <div role="listitem" id="lgt_li2_nli1">Item 2A</div>
        <div role="listitem" id="lgt_li2_nli2">Item 2B</div>
      </div>
    </div>
  </div>`,
  async function(browser, accDoc) {
    let getAcc = id => findAccessibleChildByID(accDoc, id);

    // ////////////////////////////////////////////////////////////////////////
    // HTML ul/ol
    testGroupAttrs(getAcc("li1"), 1, 3);
    testGroupAttrs(getAcc("li2"), 2, 3);
    testGroupAttrs(getAcc("li3"), 3, 3);

    // ul should have item count and not hierarchical
    testGroupParentAttrs(getAcc("ul"), 3, false);

    // ////////////////////////////////////////////////////////////////////////
    // HTML ul/ol (nested lists)

    testGroupAttrs(getAcc("li4"), 1, 3, 1);
    testGroupAttrs(getAcc("li5"), 2, 3, 1);
    testGroupAttrs(getAcc("li6"), 3, 3, 1);
    // ol with nested list should have 1st level item count and be hierarchical
    testGroupParentAttrs(getAcc("ol"), 3, true);

    testGroupAttrs(getAcc("n_li4"), 1, 3, 2);
    testGroupAttrs(getAcc("n_li5"), 2, 3, 2);
    testGroupAttrs(getAcc("n_li6"), 3, 3, 2);
    // nested ol should have item count and be hierarchical
    testGroupParentAttrs(getAcc("ol_nested"), 3, true);

    // ////////////////////////////////////////////////////////////////////////
    // ARIA list
    testGroupAttrs(getAcc("li7"), 1, 3);
    testGroupAttrs(getAcc("li8"), 2, 3);
    testGroupAttrs(getAcc("li9"), 3, 3);
    // simple flat aria list
    testGroupParentAttrs(getAcc("aria-list_1"), 3, false);

    // ////////////////////////////////////////////////////////////////////////
    // ARIA list (nested lists: list -> listitem -> list -> listitem)
    testGroupAttrs(getAcc("li10"), 1, 3, 1);
    testGroupAttrs(getAcc("li11"), 2, 3, 1);
    testGroupAttrs(getAcc("li12"), 3, 3, 1);
    // aria list with nested list
    testGroupParentAttrs(getAcc("aria-list_2"), 3, true);

    testGroupAttrs(getAcc("n_li10"), 1, 3, 2);
    testGroupAttrs(getAcc("n_li11"), 2, 3, 2);
    testGroupAttrs(getAcc("n_li12"), 3, 3, 2);
    // nested aria list.
    testGroupParentAttrs(getAcc("aria-list_2_1"), 3, true);

    // ////////////////////////////////////////////////////////////////////////
    // ARIA list (nested lists: list -> listitem -> group -> listitem)
    testGroupAttrs(getAcc("lgt_li1"), 1, 2, 1);
    testGroupAttrs(getAcc("lgt_li1_nli1"), 1, 2, 2);
    testGroupAttrs(getAcc("lgt_li1_nli2"), 2, 2, 2);
    testGroupAttrs(getAcc("lgt_li2"), 2, 2, 1);
    testGroupAttrs(getAcc("lgt_li2_nli1"), 1, 2, 2);
    testGroupAttrs(getAcc("lgt_li2_nli2"), 2, 2, 2);
    // aria list with nested list
    testGroupParentAttrs(getAcc("aria-list_3"), 2, true);
  }
);

addAccessibleTask(
  `<ul role="menubar" id="menubar">
    <li role="menuitem" aria-haspopup="true" id="menu_item1">File
      <ul role="menu" id="menu">
        <li role="menuitem" id="menu_item1.1">New</li>
        <li role="menuitem" id="menu_item1.2">Open…</li>
        <li role="separator">-----</li>
        <li role="menuitem" id="menu_item1.3">Item</li>
        <li role="menuitemradio" id="menu_item1.4">Radio</li>
        <li role="menuitemcheckbox" id="menu_item1.5">Checkbox</li>
      </ul>
    </li>
    <li role="menuitem" aria-haspopup="false" id="menu_item2">Help</li>
  </ul>`,
  async function(browser, accDoc) {
    let getAcc = id => findAccessibleChildByID(accDoc, id);

    // ////////////////////////////////////////////////////////////////////////
    // ARIA menu (menuitem, separator, menuitemradio and menuitemcheckbox)
    testGroupAttrs(getAcc("menu_item1"), 1, 2);
    testGroupAttrs(getAcc("menu_item2"), 2, 2);
    testGroupAttrs(getAcc("menu_item1.1"), 1, 2);
    testGroupAttrs(getAcc("menu_item1.2"), 2, 2);
    testGroupAttrs(getAcc("menu_item1.3"), 1, 3);
    testGroupAttrs(getAcc("menu_item1.4"), 2, 3);
    testGroupAttrs(getAcc("menu_item1.5"), 3, 3);
    // menu bar item count
    testGroupParentAttrs(getAcc("menubar"), 2, false);
    // Bug 1492529. Menu should have total number of items 5 from both sets,
    // but only has the first 2 item set.
    todoAttr(getAcc("menu"), "child-item-count", "5");
  }
);

addAccessibleTask(
  `<ul id="tablist_1" role="tablist">
    <li id="tab_1" role="tab">Crust</li>
    <li id="tab_2" role="tab">Veges</li>
    <li id="tab_3" role="tab">Carnivore</li>
  </ul>`,
  async function(browser, accDoc) {
    let getAcc = id => findAccessibleChildByID(accDoc, id);

    // ////////////////////////////////////////////////////////////////////////
    // ARIA tab
    testGroupAttrs(getAcc("tab_1"), 1, 3);
    testGroupAttrs(getAcc("tab_2"), 2, 3);
    testGroupAttrs(getAcc("tab_3"), 3, 3);
    // tab list tab count
    testGroupParentAttrs(getAcc("tablist_1"), 3, false);
  }
);

addAccessibleTask(
  `<ul id="rg1" role="radiogroup">
    <li id="r1" role="radio" aria-checked="false">Thai</li>
    <li id="r2" role="radio" aria-checked="false">Subway</li>
    <li id="r3" role="radio" aria-checked="false">Jimmy Johns</li>
  </ul>`,
  async function(browser, accDoc) {
    let getAcc = id => findAccessibleChildByID(accDoc, id);

    // ////////////////////////////////////////////////////////////////////////
    // ARIA radio
    testGroupAttrs(getAcc("r1"), 1, 3);
    testGroupAttrs(getAcc("r2"), 2, 3);
    testGroupAttrs(getAcc("r3"), 3, 3);
    // explicit aria radio group
    testGroupParentAttrs(getAcc("rg1"), 3, false);
  }
);

addAccessibleTask(
  `<table role="tree" id="tree_1">
    <tr role="presentation">
      <td role="treeitem" aria-expanded="true" aria-level="1"
          id="ti1">vegetables</td>
    </tr>
    <tr role="presentation">
      <td role="treeitem" aria-level="2" id="ti2">cucumber</td>
    </tr>
    <tr role="presentation">
      <td role="treeitem" aria-level="2" id="ti3">carrot</td>
    </tr>
    <tr role="presentation">
      <td role="treeitem" aria-expanded="false" aria-level="1"
          id="ti4">cars</td>
    </tr>
    <tr role="presentation">
      <td role="treeitem" aria-level="2" id="ti5">mercedes</td>
    </tr>
    <tr role="presentation">
      <td role="treeitem" aria-level="2" id="ti6">BMW</td>
    </tr>
    <tr role="presentation">
      <td role="treeitem" aria-level="2" id="ti7">Audi</td>
    </tr>
    <tr role="presentation">
      <td role="treeitem" aria-level="1" id="ti8">people</td>
    </tr>
  </table>

  <ul role="tree" id="tree_2">
    <li role="treeitem" id="tree2_ti1">Item 1
      <ul role="group">
        <li role="treeitem" id="tree2_ti1a">Item 1A</li>
        <li role="treeitem" id="tree2_ti1b">Item 1B</li>
      </ul>
    </li>
    <li role="treeitem" id="tree2_ti2">Item 2
      <ul role="group">
        <li role="treeitem" id="tree2_ti2a">Item 2A</li>
        <li role="treeitem" id="tree2_ti2b">Item 2B</li>
      </ul>
    </li>
  </div>

  <div role="tree" id="tree_3">
    <div role="treeitem" id="tree3_ti1">Item 1</div>
    <div role="group">
      <li role="treeitem" id="tree3_ti1a">Item 1A</li>
      <li role="treeitem" id="tree3_ti1b">Item 1B</li>
    </div>
    <div role="treeitem" id="tree3_ti2">Item 2</div>
    <div role="group">
      <div role="treeitem" id="tree3_ti2a">Item 2A</div>
      <div role="treeitem" id="tree3_ti2b">Item 2B</div>
    </div>
  </div>`,
  async function(browser, accDoc) {
    let getAcc = id => findAccessibleChildByID(accDoc, id);

    // ////////////////////////////////////////////////////////////////////////
    // ARIA tree
    testGroupAttrs(getAcc("ti1"), 1, 3, 1);
    testGroupAttrs(getAcc("ti2"), 1, 2, 2);
    testGroupAttrs(getAcc("ti3"), 2, 2, 2);
    testGroupAttrs(getAcc("ti4"), 2, 3, 1);
    testGroupAttrs(getAcc("ti5"), 1, 3, 2);
    testGroupAttrs(getAcc("ti6"), 2, 3, 2);
    testGroupAttrs(getAcc("ti7"), 3, 3, 2);
    testGroupAttrs(getAcc("ti8"), 3, 3, 1);
    testGroupParentAttrs(getAcc("tree_1"), 3, true);

    // ////////////////////////////////////////////////////////////////////////
    // ARIA tree (tree -> treeitem -> group -> treeitem)
    testGroupAttrs(getAcc("tree2_ti1"), 1, 2, 1);
    testGroupAttrs(getAcc("tree2_ti1a"), 1, 2, 2);
    testGroupAttrs(getAcc("tree2_ti1b"), 2, 2, 2);
    testGroupAttrs(getAcc("tree2_ti2"), 2, 2, 1);
    testGroupAttrs(getAcc("tree2_ti2a"), 1, 2, 2);
    testGroupAttrs(getAcc("tree2_ti2b"), 2, 2, 2);
    testGroupParentAttrs(getAcc("tree_2"), 2, true);

    // ////////////////////////////////////////////////////////////////////////
    // ARIA tree (tree -> treeitem, group -> treeitem)
    testGroupAttrs(getAcc("tree3_ti1"), 1, 2, 1);
    testGroupAttrs(getAcc("tree3_ti1a"), 1, 2, 2);
    testGroupAttrs(getAcc("tree3_ti1b"), 2, 2, 2);
    testGroupAttrs(getAcc("tree3_ti2"), 2, 2, 1);
    testGroupAttrs(getAcc("tree3_ti2a"), 1, 2, 2);
    testGroupAttrs(getAcc("tree3_ti2b"), 2, 2, 2);
    testGroupParentAttrs(getAcc("tree_3"), 2, true);
  }
);

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
  async function(browser, accDoc) {
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
  async function(browser, accDoc) {
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
  async function(browser, accDoc) {
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
  }
);

addAccessibleTask(
  `<ul id="combo1" role="combobox">Password
    <li id="combo1_opt1" role="option">Xyzzy</li>
    <li id="combo1_opt2" role="option">Plughs</li>
    <li id="combo1_opt3" role="option">Shazaam</li>
    <li id="combo1_opt4" role="option">JoeSentMe</li>
  </ul>`,
  async function(browser, accDoc) {
    let getAcc = id => findAccessibleChildByID(accDoc, id);

    // ////////////////////////////////////////////////////////////////////////
    // ARIA combobox
    testGroupAttrs(getAcc("combo1_opt1"), 1, 4);
    testGroupAttrs(getAcc("combo1_opt2"), 2, 4);
    testGroupAttrs(getAcc("combo1_opt3"), 3, 4);
    testGroupAttrs(getAcc("combo1_opt4"), 4, 4);
    testGroupParentAttrs(getAcc("combo1"), 4, false);
  }
);

addAccessibleTask(
  `<div role="table" aria-colcount="4" aria-rowcount="2" id="table">
    <div role="row" id="table_row" aria-rowindex="2">
      <div role="cell" id="table_cell" aria-colindex="3">cell</div>
    </div>
  </div>`,
  async function(browser, accDoc) {
    let getAcc = id => findAccessibleChildByID(accDoc, id);

    // ////////////////////////////////////////////////////////////////////////
    // ARIA table
    testGroupAttrs(getAcc("table_cell"), 3, 4);
    testGroupAttrs(getAcc("table_row"), 2, 2);

    // grid child item count provided by aria-rowcount
    testGroupParentAttrs(getAcc("table"), 2, false);
    // row child item count provided by parent grid's aria-colcount
    testGroupParentAttrs(getAcc("table_row"), 4, false);
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
  async function(browser, accDoc) {
    let getAcc = id => findAccessibleChildByID(accDoc, id);

    // Attributes calculated even when row is wrapped in a div.
    testGroupAttrs(getAcc("wrapped_row_1"), 1, 2, null, isCacheEnabled);
    testGroupAttrs(getAcc("wrapped_row_2"), 2, 2, null, isCacheEnabled);
  }
);

addAccessibleTask(
  `<div role="list" aria-owns="t1_li1 t1_li2 t1_li3" id="aria-list_4">
    <div role="listitem" id="t1_li2">Apples</div>
    <div role="listitem" id="t1_li1">Oranges</div>
  </div>
  <div role="listitem" id="t1_li3">Bananas</div>`,
  async function(browser, accDoc) {
    let getAcc = id => findAccessibleChildByID(accDoc, id);

    // ////////////////////////////////////////////////////////////////////////
    // ARIA list constructed by ARIA owns
    testGroupAttrs(getAcc("t1_li1"), 1, 3);
    testGroupAttrs(getAcc("t1_li2"), 2, 3);
    testGroupAttrs(getAcc("t1_li3"), 3, 3);
    testGroupParentAttrs(getAcc("aria-list_4"), 3, false);
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
  async function(browser, accDoc) {
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
  }
);

addAccessibleTask(
  `<div role="tree" id="tree4"><div role="treeitem"
  id="tree4_ti1">Item 1</div><div role="treeitem"
  id="tree4_ti2">Item 2</div></div>`,
  async function(browser, accDoc) {
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
  }
);
