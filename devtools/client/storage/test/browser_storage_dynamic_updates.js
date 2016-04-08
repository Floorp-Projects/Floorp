/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(function* () {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-updates.html");

  let $ = id => gPanelWindow.document.querySelector(id);
  let $$ = sel => gPanelWindow.document.querySelectorAll(sel);

  gUI.tree.expandAll();

  ok(gUI.sidebar.hidden, "Sidebar is initially hidden");
  yield selectTableItem("c1");

  // test that value is something initially
  let initialValue = [[
    {name: "c1", value: "1.2.3.4.5.6.7"},
    {name: "c1.path", value: "/browser"}
  ], [
    {name: "c1", value: "Array"},
    {name: "c1.0", value: "1"},
    {name: "c1.6", value: "7"}
  ]];

  // test that value is something initially
  let finalValue = [[
    {name: "c1", value: '{"foo": 4,"bar":6}'},
    {name: "c1.path", value: "/browser"}
  ], [
    {name: "c1", value: "Object"},
    {name: "c1.foo", value: "4"},
    {name: "c1.bar", value: "6"}
  ]];
  // Check that sidebar shows correct initial value
  yield findVariableViewProperties(initialValue[0], false);
  yield findVariableViewProperties(initialValue[1], true);
  // Check if table shows correct initial value
  ok($("#value [data-id='c1'].table-widget-cell"), "cell is present");
  is($("#value [data-id='c1'].table-widget-cell").value, "1.2.3.4.5.6.7",
       "correct initial value in table");
  gWindow.addCookie("c1", '{"foo": 4,"bar":6}', "/browser");
  yield gUI.once("sidebar-updated");

  yield findVariableViewProperties(finalValue[0], false);
  yield findVariableViewProperties(finalValue[1], true);
  ok($("#value [data-id='c1'].table-widget-cell"),
     "cell is present after update");
  is($("#value [data-id='c1'].table-widget-cell").value, '{"foo": 4,"bar":6}',
     "correct final value in table");

  // Add a new entry
  is($$("#value .table-widget-cell").length, 2,
     "Correct number of rows before update 0");

  gWindow.addCookie("c3", "booyeah");

  // Wait once for update and another time for value fetching
  yield gUI.once("store-objects-updated");
  yield gUI.once("store-objects-updated");

  is($$("#value .table-widget-cell").length, 3,
     "Correct number of rows after update 1");

  // Add another
  gWindow.addCookie("c4", "booyeah");

  // Wait once for update and another time for value fetching
  yield gUI.once("store-objects-updated");
  yield gUI.once("store-objects-updated");

  is($$("#value .table-widget-cell").length, 4,
     "Correct number of rows after update 2");

  // Removing cookies
  gWindow.removeCookie("c1", "/browser");

  yield gUI.once("sidebar-updated");

  is($$("#value .table-widget-cell").length, 3,
     "Correct number of rows after delete update 3");

  ok(!$("#c1"), "Correct row got deleted");

  ok(!gUI.sidebar.hidden, "Sidebar still visible for next row");

  // Check if next element's value is visible in sidebar
  yield findVariableViewProperties([{name: "c2", value: "foobar"}]);

  // Keep deleting till no rows

  gWindow.removeCookie("c3");

  yield gUI.once("store-objects-updated");

  is($$("#value .table-widget-cell").length, 2,
     "Correct number of rows after delete update 4");

  // Check if next element's value is visible in sidebar
  yield findVariableViewProperties([{name: "c2", value: "foobar"}]);

  gWindow.removeCookie("c2", "/browser");

  yield gUI.once("sidebar-updated");

  yield findVariableViewProperties([{name: "c4", value: "booyeah"}]);

  is($$("#value .table-widget-cell").length, 1,
     "Correct number of rows after delete update 5");

  gWindow.removeCookie("c4");

  yield gUI.once("store-objects-updated");

  is($$("#value .table-widget-cell").length, 0,
     "Correct number of rows after delete update 6");
  ok(gUI.sidebar.hidden, "Sidebar is hidden when no rows");

  // Testing in local storage
  yield selectTreeItem(["localStorage", "http://test1.example.org"]);

  is($$("#value .table-widget-cell").length, 7,
     "Correct number of rows after delete update 7");

  ok($(".table-widget-cell[data-id='ls4']"), "ls4 exists before deleting");

  gWindow.localStorage.removeItem("ls4");

  yield gUI.once("store-objects-updated");

  is($$("#value .table-widget-cell").length, 6,
     "Correct number of rows after delete update 8");
  ok(!$(".table-widget-cell[data-id='ls4']"),
     "ls4 does not exists after deleting");

  gWindow.localStorage.setItem("ls4", "again");

  yield gUI.once("store-objects-updated");
  yield gUI.once("store-objects-updated");

  is($$("#value .table-widget-cell").length, 7,
     "Correct number of rows after delete update 9");
  ok($(".table-widget-cell[data-id='ls4']"),
     "ls4 came back after adding it again");

  // Updating a row
  gWindow.localStorage.setItem("ls2", "ls2-changed");

  yield gUI.once("store-objects-updated");
  yield gUI.once("store-objects-updated");

  is($("#value [data-id='ls2']").value, "ls2-changed",
      "Value got updated for local storage");

  // Testing in session storage
  yield selectTreeItem(["sessionStorage", "http://test1.example.org"]);

  is($$("#value .table-widget-cell").length, 3,
     "Correct number of rows for session storage");

  gWindow.sessionStorage.setItem("ss4", "new-item");

  yield gUI.once("store-objects-updated");
  yield gUI.once("store-objects-updated");

  is($$("#value .table-widget-cell").length, 4,
     "Correct number of rows after session storage update");

  // deleting item

  gWindow.sessionStorage.removeItem("ss3");

  yield gUI.once("store-objects-updated");

  gWindow.sessionStorage.removeItem("ss1");

  yield gUI.once("store-objects-updated");

  is($$("#value .table-widget-cell").length, 2,
     "Correct number of rows after removing items from session storage");

  yield selectTableItem("ss2");

  ok(!gUI.sidebar.hidden, "sidebar is visible");

  // Checking for correct value in sidebar before update
  yield findVariableViewProperties([{name: "ss2", value: "foobar"}]);

  gWindow.sessionStorage.setItem("ss2", "changed=ss2");

  yield gUI.once("sidebar-updated");

  is($("#value [data-id='ss2']").value, "changed=ss2",
      "Value got updated for session storage in the table");

  yield findVariableViewProperties([{name: "ss2", value: "changed=ss2"}]);

  // Clearing items. Bug 1233497 makes it so that we can no longer yield
  // CPOWs from Tasks. We work around this by calling clear via a ContentTask
  // instead.
  yield ContentTask.spawn(gBrowser.selectedBrowser, null, function* () {
    return Task.spawn(content.wrappedJSObject.clear);
  });

  yield gUI.once("store-objects-cleared");

  is($$("#value .table-widget-cell").length, 0,
     "Table should be cleared");

  yield finishTests();
});
