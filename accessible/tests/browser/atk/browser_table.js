/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test getRowColumnSpan.
 */
addAccessibleTask(
  `
<table>
  <tr>
    <th id="ab" colspan="2">ab</th>
    <td id="cf" rowspan="2">cf</td>
  </tr>
  <tr>
    <td id="d">d</td>
    <td>e</td>
  </tr>
</table>
  `,
  async function (browser, docAcc) {
    let result = await runPython(`
      global doc
      doc = getDoc()
      ab = findByDomId(doc, "ab")
      return str(ab.queryTableCell().getRowColumnSpan())
    `);
    is(
      result,
      "(row=0, column=0, row_span=1, column_span=2)",
      "ab getColumnRowSpan correct"
    );
    result = await runPython(`
      cf = findByDomId(doc, "cf")
      return str(cf.queryTableCell().getRowColumnSpan())
    `);
    is(
      result,
      "(row=0, column=2, row_span=2, column_span=1)",
      "cf getColumnRowSpan correct"
    );
    result = await runPython(`
      d = findByDomId(doc, "d")
      return str(d.queryTableCell().getRowColumnSpan())
    `);
    is(
      result,
      "(row=1, column=0, row_span=1, column_span=1)",
      "d getColumnRowSpan correct"
    );
  }
);
