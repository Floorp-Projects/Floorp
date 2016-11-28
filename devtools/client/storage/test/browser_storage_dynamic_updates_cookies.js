/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test dynamic updates in the storage inspector for cookies.

add_task(function* () {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-updates.html");

  gUI.tree.expandAll();

  ok(gUI.sidebar.hidden, "Sidebar is initially hidden");
  let c1id = getCookieId("c1", "test1.example.org", "/browser");
  yield selectTableItem(c1id);

  // test that value is something initially
  let initialValue = [[
    {name: "c1", value: "1.2.3.4.5.6.7"},
    {name: "c1.Path", value: "/browser"}
  ], [
    {name: "c1", value: "Array"},
    {name: "c1.0", value: "1"},
    {name: "c1.6", value: "7"}
  ]];

  // test that value is something initially
  let finalValue = [[
    {name: "c1", value: '{"foo": 4,"bar":6}'},
    {name: "c1.Path", value: "/browser"}
  ], [
    {name: "c1", value: "Object"},
    {name: "c1.foo", value: "4"},
    {name: "c1.bar", value: "6"}
  ]];

  // Check that sidebar shows correct initial value
  yield findVariableViewProperties(initialValue[0], false);

  yield findVariableViewProperties(initialValue[1], true);
  // Check if table shows correct initial value

  yield checkState([
    [
      ["cookies", "test1.example.org"],
      [
        getCookieId("c1", "test1.example.org", "/browser"),
        getCookieId("c2", "test1.example.org", "/browser")
      ]
    ],
  ]);
  checkCell(c1id, "value", "1.2.3.4.5.6.7");

  gWindow.addCookie("c1", '{"foo": 4,"bar":6}', "/browser");
  yield gUI.once("sidebar-updated");

  yield findVariableViewProperties(finalValue[0], false);
  yield findVariableViewProperties(finalValue[1], true);

  yield checkState([
    [
      ["cookies", "test1.example.org"],
      [
        getCookieId("c1", "test1.example.org", "/browser"),
        getCookieId("c2", "test1.example.org", "/browser")
      ]
    ],
  ]);
  checkCell(c1id, "value", '{"foo": 4,"bar":6}');

  // Add a new entry
  gWindow.addCookie("c3", "booyeah");

  // Wait once for update and another time for value fetching
  yield gUI.once("store-objects-updated");
  yield gUI.once("store-objects-updated");

  yield checkState([
    [
      ["cookies", "test1.example.org"],
      [
        getCookieId("c1", "test1.example.org", "/browser"),
        getCookieId("c2", "test1.example.org", "/browser"),
        getCookieId("c3", "test1.example.org",
                    "/browser/devtools/client/storage/test/")
      ]
    ],
  ]);
  let c3id = getCookieId("c3", "test1.example.org",
                         "/browser/devtools/client/storage/test/");
  checkCell(c3id, "value", "booyeah");

  // Add another
  gWindow.addCookie("c4", "booyeah");

  // Wait once for update and another time for value fetching
  yield gUI.once("store-objects-updated");
  yield gUI.once("store-objects-updated");

  yield checkState([
    [
      ["cookies", "test1.example.org"],
      [
        getCookieId("c1", "test1.example.org", "/browser"),
        getCookieId("c2", "test1.example.org", "/browser"),
        getCookieId("c3", "test1.example.org",
                    "/browser/devtools/client/storage/test/"),
        getCookieId("c4", "test1.example.org",
                    "/browser/devtools/client/storage/test/")
      ]
    ],
  ]);
  let c4id = getCookieId("c4", "test1.example.org",
                         "/browser/devtools/client/storage/test/");
  checkCell(c4id, "value", "booyeah");

  // Removing cookies
  gWindow.removeCookie("c1", "/browser");

  yield gUI.once("sidebar-updated");

  yield checkState([
    [
      ["cookies", "test1.example.org"],
      [
        getCookieId("c2", "test1.example.org", "/browser"),
        getCookieId("c3", "test1.example.org",
                    "/browser/devtools/client/storage/test/"),
        getCookieId("c4", "test1.example.org",
                    "/browser/devtools/client/storage/test/")
      ]
    ],
  ]);

  ok(!gUI.sidebar.hidden, "Sidebar still visible for next row");

  // Check if next element's value is visible in sidebar
  yield findVariableViewProperties([{name: "c2", value: "foobar"}]);

  // Keep deleting till no rows
  gWindow.removeCookie("c3");

  yield gUI.once("store-objects-updated");

  yield checkState([
    [
      ["cookies", "test1.example.org"],
      [
        getCookieId("c2", "test1.example.org", "/browser"),
        getCookieId("c4", "test1.example.org",
                    "/browser/devtools/client/storage/test/")
      ]
    ],
  ]);

  // Check if next element's value is visible in sidebar
  yield findVariableViewProperties([{name: "c2", value: "foobar"}]);

  gWindow.removeCookie("c2", "/browser");

  yield gUI.once("sidebar-updated");

  yield checkState([
    [
      ["cookies", "test1.example.org"],
      [
        getCookieId("c4", "test1.example.org",
                    "/browser/devtools/client/storage/test/")
      ]
    ],
  ]);

  // Check if next element's value is visible in sidebar
  yield findVariableViewProperties([{name: "c4", value: "booyeah"}]);

  gWindow.removeCookie("c4");

  yield gUI.once("store-objects-updated");

  yield checkState([
    [["cookies", "test1.example.org"], [ ]],
  ]);

  ok(gUI.sidebar.hidden, "Sidebar is hidden when no rows");

  yield finishTests();
});
