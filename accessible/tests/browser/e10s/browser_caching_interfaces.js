/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test caching of accessible interfaces
 */
addAccessibleTask(
  `
  <img id="img" src="http://example.com/a11y/accessible/tests/mochitest/moz.png">
  <select id="select" multiple></select>
  <input id="number-input" type="number">
  <table id="table">
    <tr><td id="cell"><a id="link" href="#">hello</a></td></tr>
  </table>
  `,
  async function (browser, accDoc) {
    ok(
      accDoc instanceof nsIAccessibleDocument,
      "Document has Document interface"
    );
    ok(
      accDoc instanceof nsIAccessibleHyperText,
      "Document has HyperText interface"
    );
    ok(
      findAccessibleChildByID(accDoc, "img") instanceof nsIAccessibleImage,
      "img has Image interface"
    );
    ok(
      findAccessibleChildByID(accDoc, "select") instanceof
        nsIAccessibleSelectable,
      "select has Selectable interface"
    );
    ok(
      findAccessibleChildByID(accDoc, "number-input") instanceof
        nsIAccessibleValue,
      "number-input has Value interface"
    );
    ok(
      findAccessibleChildByID(accDoc, "table") instanceof nsIAccessibleTable,
      "table has Table interface"
    );
    ok(
      findAccessibleChildByID(accDoc, "cell") instanceof nsIAccessibleTableCell,
      "cell has TableCell interface"
    );
    ok(
      findAccessibleChildByID(accDoc, "link") instanceof nsIAccessibleHyperLink,
      "link has HyperLink interface"
    );
    ok(
      findAccessibleChildByID(accDoc, "link") instanceof nsIAccessibleHyperText,
      "link has HyperText interface"
    );
  }
);
