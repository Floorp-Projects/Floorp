/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test tables for both local and remote Accessibles. There is more extensive
 * coverage in ../../mochitest/table. These tests are primarily to ensure that
 * the cache works as expected and that there is consistency between local and
 * remote.
 */

"use strict";

/* import-globals-from ../../mochitest/table.js */
/* import-globals-from ../../mochitest/attributes.js */
loadScripts(
  { name: "table.js", dir: MOCHITESTS_DIR },
  { name: "attributes.js", dir: MOCHITESTS_DIR }
);

/**
 * Test table counts, indexes, extents and implicit headers.
 */
addAccessibleTask(
  `
<table id="table">
  <thead>
    <tr><th id="a">a</th><th id="bc" colspan="2">bc</th><th id="d">d</th></tr>
  </thead>
  <tbody>
    <tr><th id="ei" rowspan="2">ei</th><td id="fj" rowspan="0">fj</td><td id="g">g</td><td id="h">h</td></tr>
    <tr><td id="k">k</td></tr>
  </tbody>
</table>
  `,
  async function (browser, docAcc) {
    const table = findAccessibleChildByID(docAcc, "table", [
      nsIAccessibleTable,
    ]);
    is(table.rowCount, 3, "table rowCount correct");
    is(table.columnCount, 4, "table columnCount correct");
    testTableIndexes(table, [
      [0, 1, 1, 2],
      [3, 4, 5, 6],
      [3, 4, 7, -1],
    ]);
    const cells = {};
    for (const id of ["a", "bc", "d", "ei", "fj", "g", "h", "k"]) {
      cells[id] = findAccessibleChildByID(docAcc, id, [nsIAccessibleTableCell]);
    }
    is(cells.a.rowExtent, 1, "a rowExtent correct");
    is(cells.a.columnExtent, 1, "a columnExtent correct");
    is(cells.bc.rowExtent, 1, "bc rowExtent correct");
    is(cells.bc.columnExtent, 2, "bc columnExtent correct");
    is(cells.ei.rowExtent, 2, "ei rowExtent correct");
    is(cells.fj.rowExtent, 2, "fj rowExtent correct");
    testHeaderCells([
      {
        cell: cells.ei,
        rowHeaderCells: [],
        columnHeaderCells: [cells.a],
      },
      {
        cell: cells.g,
        rowHeaderCells: [cells.ei],
        columnHeaderCells: [cells.bc],
      },
      {
        cell: cells.k,
        rowHeaderCells: [cells.ei],
        columnHeaderCells: [cells.bc],
      },
    ]);
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
    remoteIframe: true,
  }
);

/**
 * Test table explicit headers.
 */
addAccessibleTask(
  `
<table id="table">
  <tr><th id="a">a</th><th id="b">b</th></tr>
  <tr><td id="c" headers="b d">c</td><th scope="row" id="d">d</th></tr>
  <tr><td id="e" headers="c f">e</td><td id="f">f</td></tr>
</table>
  `,
  async function (browser, docAcc) {
    const cells = {};
    for (const id of ["a", "b", "c", "d", "e", "f"]) {
      cells[id] = findAccessibleChildByID(docAcc, id, [nsIAccessibleTableCell]);
    }
    testHeaderCells([
      {
        cell: cells.c,
        rowHeaderCells: [cells.d],
        columnHeaderCells: [cells.b],
      },
      {
        cell: cells.e,
        rowHeaderCells: [cells.f],
        columnHeaderCells: [cells.c],
      },
    ]);
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
    remoteIframe: true,
  }
);

/**
 * Test that an inner table doesn't impact an outer table.
 */
addAccessibleTask(
  `
<table id="outerTable">
  <tr><th id="outerCell">outerCell<table id="innerTable">
    <tr><th id="innerCell">a</th></tr></table>
  </table></th></tr>
</table>
  `,
  async function (browser, docAcc) {
    const outerTable = findAccessibleChildByID(docAcc, "outerTable", [
      nsIAccessibleTable,
    ]);
    is(outerTable.rowCount, 1, "outerTable rowCount correct");
    is(outerTable.columnCount, 1, "outerTable columnCount correct");
    const outerCell = findAccessibleChildByID(docAcc, "outerCell");
    is(
      outerTable.getCellAt(0, 0),
      outerCell,
      "outerTable returns correct cell"
    );
    const innerTable = findAccessibleChildByID(docAcc, "innerTable", [
      nsIAccessibleTable,
    ]);
    is(innerTable.rowCount, 1, "innerTable rowCount correct");
    is(innerTable.columnCount, 1, "innerTable columnCount correct");
    const innerCell = findAccessibleChildByID(docAcc, "innerCell");
    is(
      innerTable.getCellAt(0, 0),
      innerCell,
      "innerTable returns correct cell"
    );
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
    remoteIframe: true,
  }
);

/**
 * Test table caption and summary.
 */
addAccessibleTask(
  `
<table id="t1">
  <caption id="c1">c1</caption>
  <tr><th>a</th></tr>
</table>
<table id="t2" summary="s2">
  <tr><th>a</th></tr>
</table>
<table id="t3" summary="s3">
  <caption id="c3">c3</caption>
  <tr><th>a</th></tr>
</table>
  `,
  async function (browser, docAcc) {
    const t1 = findAccessibleChildByID(docAcc, "t1", [nsIAccessibleTable]);
    const c1 = findAccessibleChildByID(docAcc, "c1");
    is(t1.caption, c1, "t1 caption correct");
    ok(!t1.summary, "t1 no summary");
    const t2 = findAccessibleChildByID(docAcc, "t2", [nsIAccessibleTable]);
    ok(!t2.caption, "t2 caption is null");
    is(t2.summary, "s2", "t2 summary correct");
    const t3 = findAccessibleChildByID(docAcc, "t3", [nsIAccessibleTable]);
    const c3 = findAccessibleChildByID(docAcc, "c3");
    is(t3.caption, c3, "t3 caption correct");
    is(t3.summary, "s3", "t3 summary correct");
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
    remoteIframe: true,
  }
);

/**
 * Test table layout guess.
 */
addAccessibleTask(
  `
<table id="layout"><tr><td>a</td></tr></table>
<table id="data"><tr><th>a</th></tr></table>
<table id="mutate"><tr><td>a</td><td>b</td></tr></table>
<div id="newTableContainer"></div>
  `,
  async function (browser, docAcc) {
    const layout = findAccessibleChildByID(docAcc, "layout");
    testAttrs(layout, { "layout-guess": "true" }, true);
    const data = findAccessibleChildByID(docAcc, "data");
    testAbsentAttrs(data, { "layout-guess": "true" });
    const mutate = findAccessibleChildByID(docAcc, "mutate");
    testAttrs(mutate, { "layout-guess": "true" }, true);

    info("mutate: Adding 5 rows");
    let reordered = waitForEvent(EVENT_REORDER, mutate);
    await invokeContentTask(browser, [], () => {
      const frag = content.document.createDocumentFragment();
      for (let r = 0; r < 6; ++r) {
        const tr = content.document.createElement("tr");
        tr.innerHTML = "<td>a</td><td>b</td>";
        frag.append(tr);
      }
      content.document.getElementById("mutate").tBodies[0].append(frag);
    });
    await reordered;
    testAbsentAttrs(mutate, { "layout-guess": "true" });

    info("mutate: Removing 5 rows");
    reordered = waitForEvent(EVENT_REORDER, mutate);
    await invokeContentTask(browser, [], () => {
      // Pause refresh driver so all the children removals below will
      // be collated into the same tick and only one 'reorder' event will
      // be dispatched.
      content.windowUtils.advanceTimeAndRefresh(100);

      let tBody = content.document.getElementById("mutate").tBodies[0];
      for (let r = 0; r < 6; ++r) {
        tBody.lastChild.remove();
      }

      // Resume refresh driver
      content.windowUtils.restoreNormalRefresh();
    });
    await reordered;
    testAttrs(mutate, { "layout-guess": "true" }, true);

    info("mutate: Adding new table");
    let shown = waitForEvent(EVENT_SHOW, "newTable");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById(
        "newTableContainer"
      ).innerHTML = `<table id="newTable"><tr><th>a</th></tr></table>`;
    });
    let newTable = (await shown).accessible;
    testAbsentAttrs(newTable, { "layout-guess": "true" });
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
    remoteIframe: true,
  }
);

/**
 * Test table layout guess with border styling changes.
 */
addAccessibleTask(
  `
  <table id="layout"><tr><td id="cell">a</td><td>b</td></tr>
  <tr><td>c</td><td>d</td></tr><tr><td>c</td><td>d</td></tr></table>
  `,
  async function (browser, docAcc) {
    const layout = findAccessibleChildByID(docAcc, "layout");
    testAttrs(layout, { "layout-guess": "true" }, true);
    info("changing border style on table cell");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("cell").style.border = "1px solid black";
      content.document.body.offsetTop; // Flush layout.
    });
    await untilCacheOk(() => {
      // manually verify the attribute doesn't exist, since `testAbsentAttrs`
      // has internal calls to ok() which fail if the cache hasn't yet updated
      for (let prop of layout.attributes.enumerate()) {
        if (prop.key == "layout-guess") {
          return false;
        }
      }
      return true;
    }, "Table is a data table");
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
    remoteIframe: true,
  }
);

/**
 * Test ARIA grid.
 */
addAccessibleTask(
  `
<div id="grid" role="grid">
  <div role="rowgroup">
    <div role="row"><div id="a" role="columnheader">a</div><div id="b" role="columnheader">b</div></div>
  </div>
  <div tabindex="-1">
    <div role="row"><div id="c" role="rowheader">c</div><div id="d" role="gridcell">d</div></div>
  </div>
</div>
  `,
  async function (browser, docAcc) {
    const grid = findAccessibleChildByID(docAcc, "grid", [nsIAccessibleTable]);
    is(grid.rowCount, 2, "grid rowCount correct");
    is(grid.columnCount, 2, "grid columnCount correct");
    testTableIndexes(grid, [
      [0, 1],
      [2, 3],
    ]);
    const cells = {};
    for (const id of ["a", "b", "c", "d"]) {
      cells[id] = findAccessibleChildByID(docAcc, id, [nsIAccessibleTableCell]);
    }
    is(cells.a.rowExtent, 1, "a rowExtent correct");
    is(cells.a.columnExtent, 1, "a columnExtent correct");
    testHeaderCells([
      {
        cell: cells.c,
        rowHeaderCells: [],
        columnHeaderCells: [cells.a],
      },
      {
        cell: cells.d,
        rowHeaderCells: [cells.c],
        columnHeaderCells: [cells.b],
      },
    ]);
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
    remoteIframe: true,
  }
);

function setNodeHidden(browser, id, hidden) {
  return invokeContentTask(browser, [id, hidden], (cId, cHidden) => {
    content.document.getElementById(cId).hidden = cHidden;
  });
}

/**
 * Test that the table is updated correctly when it is mutated.
 */
addAccessibleTask(
  `
<table id="table">
  <tr id="r1"><td>a</td><td id="b">b</td></tr>
  <tr id="r2" hidden><td>c</td><td>d</td></tr>
</table>
<div id="owner"></div>
  `,
  async function (browser, docAcc) {
    const table = findAccessibleChildByID(docAcc, "table", [
      nsIAccessibleTable,
    ]);
    is(table.rowCount, 1, "table rowCount correct");
    is(table.columnCount, 2, "table columnCount correct");
    testTableIndexes(table, [[0, 1]]);
    info("Showing r2");
    let reordered = waitForEvent(EVENT_REORDER, table);
    await setNodeHidden(browser, "r2", false);
    await reordered;
    is(table.rowCount, 2, "table rowCount correct");
    testTableIndexes(table, [
      [0, 1],
      [2, 3],
    ]);
    info("Hiding r2");
    reordered = waitForEvent(EVENT_REORDER, table);
    await setNodeHidden(browser, "r2", true);
    await reordered;
    is(table.rowCount, 1, "table rowCount correct");
    testTableIndexes(table, [[0, 1]]);
    info("Hiding b");
    reordered = waitForEvent(EVENT_REORDER, "r1");
    await setNodeHidden(browser, "b", true);
    await reordered;
    is(table.columnCount, 1, "table columnCount correct");
    testTableIndexes(table, [[0]]);
    info("Showing b");
    reordered = waitForEvent(EVENT_REORDER, "r1");
    await setNodeHidden(browser, "b", false);
    await reordered;
    is(table.columnCount, 2, "table columnCount correct");
    info("Moving b out of table using aria-owns");
    reordered = waitForEvent(EVENT_REORDER, "r1");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("owner").setAttribute("aria-owns", "b");
    });
    await reordered;
    is(table.columnCount, 1, "table columnCount correct");
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
    remoteIframe: true,
  }
);

/**
 * Test the handling of ARIA tables with display: contents.
 */
addAccessibleTask(
  `
<div id="table" role="table" style="display: contents;">
  <div role="row"><div role="cell">a</div></div>
</div>
  `,
  async function (browser, docAcc) {
    const table = findAccessibleChildByID(docAcc, "table", [
      nsIAccessibleTable,
    ]);
    is(table.rowCount, 1, "table rowCount correct");
    is(table.columnCount, 1, "table columnCount correct");
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
    remoteIframe: true,
  }
);

/**
 * Test a broken ARIA table with an invalid cell.
 */
addAccessibleTask(
  `
<div id="table" role="table">
  <div role="main">
    <div role="row">
      <div id="cell" role="cell">a</div>
    </div>
  </div>
</div>
  `,
  async function (browser, docAcc) {
    const table = findAccessibleChildByID(docAcc, "table", [
      nsIAccessibleTable,
    ]);
    is(table.rowCount, 0, "table rowCount correct");
    is(table.columnCount, 0, "table columnCount correct");
    const cell = findAccessibleChildByID(docAcc, "cell");
    let queryOk = false;
    try {
      cell.QueryInterface(nsIAccessibleTableCell);
      queryOk = true;
    } catch (e) {}
    ok(!queryOk, "Got nsIAccessibleTableCell on an invalid cell");
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
    remoteIframe: true,
  }
);

/**
 * Test that building the cache for a malformed table with an iframe inside a
 * row doesn't crash (bug 1800780).
 */
addAccessibleTask(
  `<table><tr id="tr"></tr></table>`,
  async function (browser, docAcc) {
    let reordered = waitForEvent(EVENT_REORDER, "tr");
    await invokeContentTask(browser, [], () => {
      const iframe = content.document.createElement("iframe");
      content.document.getElementById("tr").append(iframe);
    });
    await reordered;
  },
  { topLevel: true }
);

/**
 * Verify that table row and column information is correct when there are
 * intervening generics between the table and a rowgroup.
 */
addAccessibleTask(
  `
<div id="table" role="grid">
  <div role="rowgroup">
    <div role="row">
      <div role="columnheader">a</div>
    </div>
  </div>
  <div tabindex="-1" style="height: 1px; overflow: auto;">
    <div role="rowgroup">
      <div role="row">
        <div id="cell" role="gridcell">b</div>
      </div>
    </div>
  </div>
</div>
  `,
  async function (browser, docAcc) {
    const table = findAccessibleChildByID(docAcc, "table", [
      nsIAccessibleTable,
    ]);

    info("Verifying that the table row and column counts are correct.");
    is(table.rowCount, 2, "table rowCount correct");
    is(table.columnCount, 1, "table columnCount correct");

    info("Verifying that the cell row and column extents are correct.");
    const cell = findAccessibleChildByID(docAcc, "cell", [
      nsIAccessibleTableCell,
    ]);
    is(cell.rowExtent, 1, "cell rowExtent correct");
    is(cell.columnExtent, 1, "cell colExtent correct");
    is(cell.rowIndex, 1, "cell rowIndex correct");
    is(cell.columnIndex, 0, "cell columnIndex correct");
  },
  { chrome: true, topLevel: true, iframe: true, remoteIframe: true }
);

/**
 * Verify that table row and column information is correct when there are
 * intervening generics between rows and cells.
 */
addAccessibleTask(
  `
<div id="table" role="grid">
  <div role="rowgroup">
    <div role="row">
      <div role="columnheader">a</div>
    </div>
  </div>
  <div role="rowgroup">
    <div role="row">
      <div tabindex="-1" style="height: 1px; overflow: auto;">
        <div id="cell" role="gridcell">b</div>
      </div>
    </div>
  </div>
</div>
  `,
  async function (browser, docAcc) {
    const table = findAccessibleChildByID(docAcc, "table", [
      nsIAccessibleTable,
    ]);

    info("Verifying that the table row and column counts are correct.");
    is(table.rowCount, 2, "table rowCount correct");
    is(table.columnCount, 1, "table columnCount correct");

    info("Verifying that the cell row and column extents are correct.");
    const cell = findAccessibleChildByID(docAcc, "cell", [
      nsIAccessibleTableCell,
    ]);
    is(cell.rowExtent, 1, "cell rowExtent correct");
    is(cell.columnExtent, 1, "cell colExtent correct");
    is(cell.rowIndex, 1, "cell rowIndex correct");
    is(cell.columnIndex, 0, "cell columnIndex correct");
  },
  { chrome: true, topLevel: true, iframe: true, remoteIframe: true }
);

/**
 * Verify that we don't crash for authoring error like <table role="gridcell">.
 */
addAccessibleTask(
  `<table id="table" role="gridcell">`,
  async function (browser, docAcc) {
    const table = findAccessibleChildByID(docAcc, "table");
    ok(table, "Retrieved table Accessible");
  },
  { chrome: true, topLevel: true }
);

/**
 * Test ARIA tables in SVG.
 */
addAccessibleTask(
  `
<svg id="table" role="table">
  <text id="caption" role="caption">caption</text>
  <g role="row">
    <text id="a" role="columnheader">a</text>
    <text id="b" role="columnheader">b</text>
  </g>
  <g role="row">
    <text id="c" role="cell">c</text>
    <text id="d" role="cell">d</text>
  </g>
</svg>
  `,
  async function (browser, docAcc) {
    const table = findAccessibleChildByID(docAcc, "table", [
      nsIAccessibleTable,
    ]);
    is(table.rowCount, 2, "table rowCount correct");
    is(table.columnCount, 2, "table columnCount correct");
    const caption = findAccessibleChildByID(docAcc, "caption");
    is(table.caption, caption, "table caption correct");
    testTableIndexes(table, [
      [0, 1],
      [2, 3],
    ]);
    const cells = {};
    for (const id of ["a", "b", "c", "d"]) {
      cells[id] = findAccessibleChildByID(docAcc, id, [nsIAccessibleTableCell]);
    }
    testHeaderCells([
      {
        cell: cells.c,
        rowHeaderCells: [],
        columnHeaderCells: [cells.a],
      },
      {
        cell: cells.d,
        rowHeaderCells: [],
        columnHeaderCells: [cells.b],
      },
    ]);
  },
  { chrome: true, topLevel: true, remoteIframe: true }
);

/**
 * Verify that we don't crash for authoring error like <tr role="grid">.
 */
addAccessibleTask(
  `
<table id="table">
  <tr><th>a</th></tr>
  <tr role="grid"><td id="b">b</td></tr>
</table>
  `,
  async function (browser, docAcc) {
    const table = findAccessibleChildByID(docAcc, "table", [
      nsIAccessibleTable,
    ]);
    is(table.rowCount, 1, "table rowCount correct");
    is(table.columnCount, 1, "table columnCount correct");
    const b = findAccessibleChildByID(docAcc, "b");
    let queryOk = false;
    try {
      b.QueryInterface(nsIAccessibleTableCell);
      queryOk = true;
    } catch (e) {}
    ok(!queryOk, "No nsIAccessibleTableCell on invalid cell b");
  }
);
