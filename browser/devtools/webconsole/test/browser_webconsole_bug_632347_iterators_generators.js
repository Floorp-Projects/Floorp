/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Web Console test suite.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mihai Sucan <mihai.sucan@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-bug-632347-iterators-generators.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", tabLoaded, true);
}

function tabLoaded() {
  browser.removeEventListener("load", tabLoaded, true);
  openConsole();

  let hudId = HUDService.getHudIdByWindow(content);
  let HUD = HUDService.hudReferences[hudId];
  let jsterm = HUD.jsterm;

  let win = content.wrappedJSObject;

  // Make sure autocomplete does not walk through iterators and generators.
  let result = win.gen1.next();
  let completion = jsterm.propertyProvider(win, "gen1.");
  is(completion, null, "no matchees for gen1");
  ok(!jsterm.isResultInspectable(win.gen1),
     "gen1 is not inspectable");

  is(result+1, win.gen1.next(), "gen1.next() did not execute");

  result = win.gen2.next();

  completion = jsterm.propertyProvider(win, "gen2.");
  is(completion, null, "no matchees for gen2");
  ok(!jsterm.isResultInspectable(win.gen2),
     "gen2 is not inspectable");

  is((result/2+1)*2, win.gen2.next(),
     "gen2.next() did not execute");

  result = win.iter1.next();
  is(result[0], "foo", "iter1.next() [0] is correct");
  is(result[1], "bar", "iter1.next() [1] is correct");

  completion = jsterm.propertyProvider(win, "iter1.");
  is(completion, null, "no matchees for iter1");
  ok(!jsterm.isResultInspectable(win.iter1),
     "iter1 is not inspectable");

  result = win.iter1.next();
  is(result[0], "baz", "iter1.next() [0] is correct");
  is(result[1], "baaz", "iter1.next() [1] is correct");

  completion = jsterm.propertyProvider(content, "iter2.");
  is(completion, null, "no matchees for iter2");
  ok(!jsterm.isResultInspectable(win.iter2),
     "iter2 is not inspectable");

  completion = jsterm.propertyProvider(win, "window.");
  ok(completion, "matches available for window");
  ok(completion.matches.length, "matches available for window (length)");
  ok(jsterm.isResultInspectable(win),
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
