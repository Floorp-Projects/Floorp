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
 * The Original Code is DevTools test code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Michael Ratcliffe <mratcliffe@mozilla.com>
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

// Tests that inspectstyle(node) works properly

const TEST_URI = "http://example.com/browser/browser/devtools/styleinspector/test/browser/browser_styleinspector_webconsole.htm";

let doc;
let jsterm;
let hudBox;
let stylePanels = [];

function test() {
  addTab(TEST_URI);
  browser.addEventListener("DOMContentLoaded", prepConsole, false);
}

function prepConsole() {
  browser.removeEventListener("DOMContentLoaded", prepConsole, false);
  doc = content.document;
  openConsole();
 
  ok(window.StyleInspector, "StyleInspector exists");
 
  let hud = HUDService.getHudByWindow(content);
  ok(hud, "we have a console");
 
  hudBox = hud.HUDBox;
  ok(hudBox, "we have the console display");
 
  jsterm = hud.jsterm;
  ok(jsterm, "we have a jsterm");
 
  openStyleInspector1();
}

function openStyleInspector1() {
  info("opening style inspector instance 1");
  Services.obs.addObserver(openStyleInspector2, "StyleInspector-opened", false);
  jsterm.execute("inspectstyle($('text'))");
}

function openStyleInspector2() {
  Services.obs.removeObserver(openStyleInspector2, "StyleInspector-opened", false);
  info("opening style inspector instance 2");
  Services.obs.addObserver(openStyleInspector3, "StyleInspector-opened", false);
  jsterm.execute("inspectstyle($('text2'))");
}

function openStyleInspector3() {
  Services.obs.removeObserver(openStyleInspector3, "StyleInspector-opened", false);
  info("opening style inspector instance 3");
  Services.obs.addObserver(teststylePanels, "StyleInspector-opened", false);
  jsterm.execute("inspectstyle($('container'))");
}

function teststylePanels() {
  Services.obs.removeObserver(teststylePanels, "StyleInspector-opened", false);

  info("adding style inspector instances to stylePanels array");
  let popupSet = document.getElementById("mainPopupSet");
  let len = popupSet.childNodes.length - 3;
  stylePanels.push(popupSet.childNodes[len++]);
  stylePanels.push(popupSet.childNodes[len++]);
  stylePanels.push(popupSet.childNodes[len++]);

  let eltArray = [
    doc.getElementById("text"),
    doc.getElementById("text2"),
    doc.getElementById("container")
  ];

  // We have 3 style inspector instances, each with an element selected:
  // 1. #text
  // 2. #text2
  // 3. #container
  //
  // We will loop through each instance and check that the correct node is
  // selected and that the correct css selector has been selected as active
  info("looping through array to check initialization");
  for (let i = 0, max = stylePanels.length; i < max; i++) {
    ok(stylePanels[i], "style inspector instance " + i +
       " correctly initialized");
    ok(stylePanels[i].isOpen(), "style inspector " + i + " is open");

    let htmlTree = stylePanels[i].cssHtmlTree;
    let cssLogic = htmlTree.cssLogic;
    let elt = eltArray[i];
    let eltId = elt.id;

    // Check that the correct node is selected
    is(elt, htmlTree.viewedElement,
      "style inspector node matches the selected node (id=" + eltId + ")");
    is(htmlTree.viewedElement, stylePanels[i].cssLogic.viewedElement,
      "cssLogic node matches the cssHtmlTree node (id=" + eltId + ")");

    // Check that the correct css selector has been selected as active
    let matchedSelectors = cssLogic.getPropertyInfo("font-family").matchedSelectors;
    let sel = matchedSelectors[0];
    let selector = sel.selector.text;
    let value = sel.value;

    // Because we know which selectors should be the best match and what their
    // values should be we can check them
    switch(eltId) {
      case "text":
        is(selector, "#container > .text", "correct best match for #text");
        is(value, "cursive", "correct css property value for #" + eltId);
        break;
      case "text2":
        is(selector, "#container > span", "correct best match for #text2");
        is(value, "cursive", "correct css property value for #" + eltId);
        break;
      case "container":
        is(selector, "#container", "correct best match for #container");
        is(value, "fantasy", "correct css property value for #" + eltId);
    }
  }

  info("hiding stylePanels[1]");
  Services.obs.addObserver(styleInspectorClosedByHide,
                           "StyleInspector-closed", false);
  stylePanels[1].hidePopup();
}

function styleInspectorClosedByHide()
{
  Services.obs.removeObserver(styleInspectorClosedByHide, "StyleInspector-closed", false);
  ok(stylePanels[0].isOpen(), "instance stylePanels[0] is still open");
  ok(!stylePanels[1].isOpen(), "instance stylePanels[1] is hidden");
  ok(stylePanels[2].isOpen(), "instance stylePanels[2] is still open");

  info("closing web console");
  Services.obs.addObserver(styleInspectorClosedFromConsole1,
                           "StyleInspector-closed", false);
  closeConsole();
}

function styleInspectorClosedFromConsole1()
{
  Services.obs.removeObserver(styleInspectorClosedFromConsole1,
                              "StyleInspector-closed", false);
  info("Style Inspector 1 closed");
  Services.obs.addObserver(styleInspectorClosedFromConsole2,
                           "StyleInspector-closed", false);
}

function styleInspectorClosedFromConsole2()
{
  Services.obs.removeObserver(styleInspectorClosedFromConsole2,
                              "StyleInspector-closed", false);
  info("Style Inspector 2 closed");
  executeSoon(cleanUp);
}

function cleanUp()
{
  let popupSet = document.getElementById("mainPopupSet");
  ok(!popupSet.lastChild.hasAttribute("hudToolId"),
     "all style inspector panels are now detached and ready for garbage collection");

  info("cleaning up");
  doc = hudBox = stylePanels = jsterm = null;
  finishTest();
}
