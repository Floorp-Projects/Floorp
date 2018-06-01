/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test dynamic updates in the storage inspector for cookies.

add_task(async function() {
  await openTabAndSetupStorage(MAIN_DOMAIN + "storage-updates.html");

  gUI.tree.expandAll();

  ok(gUI.sidebar.hidden, "Sidebar is initially hidden");
  const c1id = getCookieId("c1", "test1.example.org", "/browser");
  await selectTableItem(c1id);

  // test that value is something initially
  const initialValue = [[
    {name: "c1", value: "1.2.3.4.5.6.7"},
    {name: "c1.Path", value: "/browser"}
  ], [
    {name: "c1", value: "Array"},
    {name: "c1.0", value: "1"},
    {name: "c1.6", value: "7"}
  ]];

  // test that value is something initially
  const finalValue = [[
    {name: "c1", value: '{"foo": 4,"bar":6}'},
    {name: "c1.Path", value: "/browser"}
  ], [
    {name: "c1", value: "Object"},
    {name: "c1.foo", value: "4"},
    {name: "c1.bar", value: "6"}
  ]];

  // Check that sidebar shows correct initial value
  await findVariableViewProperties(initialValue[0], false);

  await findVariableViewProperties(initialValue[1], true);

  // Check if table shows correct initial value
  await checkState([
    [
      ["cookies", "http://test1.example.org"],
      [
        getCookieId("c1", "test1.example.org", "/browser"),
        getCookieId("c2", "test1.example.org", "/browser")
      ]
    ],
  ]);
  checkCell(c1id, "value", "1.2.3.4.5.6.7");

  await addCookie("c1", '{"foo": 4,"bar":6}', "/browser");
  await gUI.once("store-objects-edit");

  await findVariableViewProperties(finalValue[0], false);
  await findVariableViewProperties(finalValue[1], true);

  await checkState([
    [
      ["cookies", "http://test1.example.org"],
      [
        getCookieId("c1", "test1.example.org", "/browser"),
        getCookieId("c2", "test1.example.org", "/browser")
      ]
    ],
  ]);
  checkCell(c1id, "value", '{"foo": 4,"bar":6}');

  // Add a new entry
  await addCookie("c3", "booyeah");

  await gUI.once("store-objects-edit");

  await checkState([
    [
      ["cookies", "http://test1.example.org"],
      [
        getCookieId("c1", "test1.example.org", "/browser"),
        getCookieId("c2", "test1.example.org", "/browser"),
        getCookieId("c3", "test1.example.org",
                    "/browser/devtools/client/storage/test/")
      ]
    ],
  ]);
  const c3id = getCookieId("c3", "test1.example.org",
                         "/browser/devtools/client/storage/test/");
  checkCell(c3id, "value", "booyeah");

  // Add another
  await addCookie("c4", "booyeah");

  await gUI.once("store-objects-edit");

  await checkState([
    [
      ["cookies", "http://test1.example.org"],
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
  const c4id = getCookieId("c4", "test1.example.org",
                         "/browser/devtools/client/storage/test/");
  checkCell(c4id, "value", "booyeah");

  // Removing cookies
  await removeCookie("c1", "/browser");

  await gUI.once("store-objects-edit");

  await checkState([
    [
      ["cookies", "http://test1.example.org"],
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
  await findVariableViewProperties([{name: "c2", value: "foobar"}]);

  // Keep deleting till no rows
  await removeCookie("c3");

  await gUI.once("store-objects-edit");

  await checkState([
    [
      ["cookies", "http://test1.example.org"],
      [
        getCookieId("c2", "test1.example.org", "/browser"),
        getCookieId("c4", "test1.example.org",
                    "/browser/devtools/client/storage/test/")
      ]
    ],
  ]);

  // Check if next element's value is visible in sidebar
  await findVariableViewProperties([{name: "c2", value: "foobar"}]);

  await removeCookie("c2", "/browser");

  await gUI.once("store-objects-edit");

  await checkState([
    [
      ["cookies", "http://test1.example.org"],
      [
        getCookieId("c4", "test1.example.org",
                    "/browser/devtools/client/storage/test/")
      ]
    ],
  ]);

  // Check if next element's value is visible in sidebar
  await findVariableViewProperties([{name: "c4", value: "booyeah"}]);

  await removeCookie("c4");

  await gUI.once("store-objects-edit");

  await checkState([
    [["cookies", "http://test1.example.org"], [ ]],
  ]);

  ok(gUI.sidebar.hidden, "Sidebar is hidden when no rows");

  await finishTests();
});

async function addCookie(name, value, path) {
  await ContentTask.spawn(gBrowser.selectedBrowser, [name, value, path],
    ([nam, valu, pat]) => {
      content.wrappedJSObject.addCookie(nam, valu, pat);
    }
  );
}

async function removeCookie(name, path) {
  await ContentTask.spawn(gBrowser.selectedBrowser, [name, path],
    ([nam, pat]) => {
      content.wrappedJSObject.removeCookie(nam, pat);
    }
  );
}
