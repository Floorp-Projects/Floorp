/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test to verify that the sidebar parsetree is used for only values it makes sense to
// parse into a tree.

"use strict";

const testCases = [
  {
    row: "ampersand",
    parseTreeVisible: true,
  },
  {
    row: "asterisk",
    parseTreeVisible: true,
  },
  {
    row: "base64",
    parseTreeVisible: false,
  },
  {
    row: "boolean",
    parseTreeVisible: false,
  },
  {
    row: "colon",
    parseTreeVisible: true,
  },
  {
    row: "color",
    parseTreeVisible: false,
  },
  {
    row: "comma",
    parseTreeVisible: true,
  },
  {
    row: "dataURI",
    parseTreeVisible: false,
  },
  {
    row: "date",
    parseTreeVisible: false,
  },
  {
    row: "email",
    parseTreeVisible: false,
  },
  {
    row: "equals",
    parseTreeVisible: true,
  },
  {
    row: "FQDN",
    parseTreeVisible: false,
  },
  {
    row: "hash",
    parseTreeVisible: true,
  },
  {
    row: "IP",
    parseTreeVisible: false,
  },
  {
    row: "MacAddress",
    parseTreeVisible: false,
  },
  {
    row: "maths",
    parseTreeVisible: false,
  },
  {
    row: "numbers",
    parseTreeVisible: false,
  },
  {
    row: "period",
    parseTreeVisible: true,
  },
  {
    row: "SemVer",
    parseTreeVisible: false,
  },
  {
    row: "tilde",
    parseTreeVisible: true,
  },
  {
    row: "URL",
    parseTreeVisible: false,
  },
  {
    row: "URL2",
    parseTreeVisible: false,
  },
];

add_task(async function () {
  await openTabAndSetupStorage(
    MAIN_DOMAIN_SECURED + "storage-sidebar-parsetree.html"
  );

  await selectTreeItem(["localStorage", "https://test1.example.org"]);

  for (const test of testCases) {
    const { parseTreeVisible, row } = test;

    await selectTableItem(row);

    sidebarParseTreeVisible(parseTreeVisible);
  }
});
