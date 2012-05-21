/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-bug-632347-iterators-generators.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", tabLoaded, true);
}

function tabLoaded() {
  browser.removeEventListener("load", tabLoaded, true);

  let tmp = {};
  Cu.import("resource:///modules/WebConsoleUtils.jsm", tmp);
  let WCU = tmp.WebConsoleUtils;

  openConsole();

  let hudId = HUDService.getHudIdByWindow(content);
  let HUD = HUDService.hudReferences[hudId];
  let jsterm = HUD.jsterm;

  let win = content.wrappedJSObject;

  // Make sure autocomplete does not walk through iterators and generators.
  let result = win.gen1.next();
  let completion = jsterm.propertyProvider(win, "gen1.");
  is(completion, null, "no matchees for gen1");
  ok(!WCU.isObjectInspectable(win.gen1),
     "gen1 is not inspectable");

  is(result+1, win.gen1.next(), "gen1.next() did not execute");

  result = win.gen2.next();

  completion = jsterm.propertyProvider(win, "gen2.");
  is(completion, null, "no matchees for gen2");
  ok(!WCU.isObjectInspectable(win.gen2),
     "gen2 is not inspectable");

  is((result/2+1)*2, win.gen2.next(),
     "gen2.next() did not execute");

  result = win.iter1.next();
  is(result[0], "foo", "iter1.next() [0] is correct");
  is(result[1], "bar", "iter1.next() [1] is correct");

  completion = jsterm.propertyProvider(win, "iter1.");
  is(completion, null, "no matchees for iter1");
  ok(!WCU.isObjectInspectable(win.iter1),
     "iter1 is not inspectable");

  result = win.iter1.next();
  is(result[0], "baz", "iter1.next() [0] is correct");
  is(result[1], "baaz", "iter1.next() [1] is correct");

  completion = jsterm.propertyProvider(content, "iter2.");
  is(completion, null, "no matchees for iter2");
  ok(!WCU.isObjectInspectable(win.iter2),
     "iter2 is not inspectable");

  completion = jsterm.propertyProvider(win, "window.");
  ok(completion, "matches available for window");
  ok(completion.matches.length, "matches available for window (length)");
  ok(WCU.isObjectInspectable(win),
     "window is inspectable");

  let panel = jsterm.openPropertyPanel("Test", win);
  ok(panel, "opened the Property Panel");
  let rows = panel.treeView._rows;
  ok(rows.length, "Property Panel rows are available");

  let find = function(display, children) {
    return rows.some(function(row) {
      return row.display == display &&
             row.children == children;
    });
  };

  ok(find("gen1: Generator", false),
     "gen1 is correctly displayed in the Property Panel");

  ok(find("gen2: Generator", false),
     "gen2 is correctly displayed in the Property Panel");

  ok(find("iter1: Iterator", false),
     "iter1 is correctly displayed in the Property Panel");

  ok(find("iter2: Iterator", false),
     "iter2 is correctly displayed in the Property Panel");

  /*
   * - disabled, see bug 632347, c#9
   * ok(find("parent: Window", true),
   *   "window.parent is correctly displayed in the Property Panel");
   */

  panel.destroy();

  finishTest();
}
