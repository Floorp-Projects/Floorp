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
 * The Mozilla Foundation.
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

const TEST_URI = "data:text/html,<p>bug 585991 - autocomplete popup keyboard usage test";

registerCleanupFunction(function() {
  Services.prefs.clearUserPref("devtools.gcli.enable");
});

function test() {
  Services.prefs.setBoolPref("devtools.gcli.enable", false);
  addTab(TEST_URI);
  browser.addEventListener("load", tabLoaded, true);
}

function tabLoaded() {
  browser.removeEventListener("load", tabLoaded, true);
  openConsole();

  content.wrappedJSObject.foobarBug585991 = {
    "item0": "value0",
    "item1": "value1",
    "item2": "value2",
    "item3": "value3",
  };

  let hudId = HUDService.getHudIdByWindow(content);
  HUD = HUDService.hudReferences[hudId];
  let jsterm = HUD.jsterm;
  let popup = jsterm.autocompletePopup;
  let completeNode = jsterm.completeNode;

  ok(!popup.isOpen, "popup is not open");

  popup._panel.addEventListener("popupshown", function() {
    popup._panel.removeEventListener("popupshown", arguments.callee, false);

    ok(popup.isOpen, "popup is open");

    is(popup.itemCount, 4, "popup.itemCount is correct");

    let sameItems = popup.getItems();
    is(sameItems.every(function(aItem, aIndex) {
      return aItem.label == "item" + aIndex;
    }), true, "getItems returns back the same items");

    is(popup.selectedIndex, -1, "no index is selected");
    EventUtils.synthesizeKey("VK_DOWN", {});
      
    let prefix = jsterm.inputNode.value.replace(/[\S]/g, " ");

    is(popup.selectedIndex, 0, "index 0 is selected");
    is(popup.selectedItem.label, "item0", "item0 is selected");
    is(completeNode.value, prefix + "item0", "completeNode.value holds item0");

    EventUtils.synthesizeKey("VK_DOWN", {});

    is(popup.selectedIndex, 1, "index 1 is selected");
    is(popup.selectedItem.label, "item1", "item1 is selected");
    is(completeNode.value, prefix + "item1", "completeNode.value holds item1");

    EventUtils.synthesizeKey("VK_UP", {});

    is(popup.selectedIndex, 0, "index 0 is selected");
    is(popup.selectedItem.label, "item0", "item0 is selected");
    is(completeNode.value, prefix + "item0", "completeNode.value holds item0");

    popup._panel.addEventListener("popuphidden", autocompletePopupHidden, false);

    EventUtils.synthesizeKey("VK_TAB", {});
  }, false);

  jsterm.setInputValue("window.foobarBug585991");
  EventUtils.synthesizeKey(".", {});
}

function autocompletePopupHidden()
{
  let jsterm = HUD.jsterm;
  let popup = jsterm.autocompletePopup;
  let completeNode = jsterm.completeNode;
  let inputNode = jsterm.inputNode;

  popup._panel.removeEventListener("popuphidden", arguments.callee, false);

  ok(!popup.isOpen, "popup is not open");

  is(inputNode.value, "window.foobarBug585991.item0",
     "completion was successful after VK_TAB");

  ok(!completeNode.value, "completeNode is empty");

  popup._panel.addEventListener("popupshown", function() {
    popup._panel.removeEventListener("popupshown", arguments.callee, false);

    ok(popup.isOpen, "popup is open");

    is(popup.itemCount, 4, "popup.itemCount is correct");

    is(popup.selectedIndex, -1, "no index is selected");
    EventUtils.synthesizeKey("VK_DOWN", {});
    
    let prefix = jsterm.inputNode.value.replace(/[\S]/g, " ");

    is(popup.selectedIndex, 0, "index 0 is selected");
    is(popup.selectedItem.label, "item0", "item0 is selected");
    is(completeNode.value, prefix + "item0", "completeNode.value holds item0");

    popup._panel.addEventListener("popuphidden", function() {
      popup._panel.removeEventListener("popuphidden", arguments.callee, false);

      ok(!popup.isOpen, "popup is not open after VK_ESCAPE");

      is(inputNode.value, "window.foobarBug585991.",
         "completion was cancelled");

      ok(!completeNode.value, "completeNode is empty");

      executeSoon(testReturnKey);
    }, false);

    executeSoon(function() {
      EventUtils.synthesizeKey("VK_ESCAPE", {});
    });
  }, false);

  executeSoon(function() {
    jsterm.setInputValue("window.foobarBug585991");
    EventUtils.synthesizeKey(".", {});
  });
}

function testReturnKey()
{
  let jsterm = HUD.jsterm;
  let popup = jsterm.autocompletePopup;
  let completeNode = jsterm.completeNode;
  let inputNode = jsterm.inputNode;

  popup._panel.addEventListener("popupshown", function() {
    popup._panel.removeEventListener("popupshown", arguments.callee, false);

    ok(popup.isOpen, "popup is open");

    is(popup.itemCount, 4, "popup.itemCount is correct");
    
    is(popup.selectedIndex, -1, "no index is selected");
    EventUtils.synthesizeKey("VK_DOWN", {});

    let prefix = jsterm.inputNode.value.replace(/[\S]/g, " ");

    is(popup.selectedIndex, 0, "index 0 is selected");
    is(popup.selectedItem.label, "item0", "item0 is selected");
    is(completeNode.value, prefix + "item0", "completeNode.value holds item0");

    EventUtils.synthesizeKey("VK_DOWN", {});

    is(popup.selectedIndex, 1, "index 1 is selected");
    is(popup.selectedItem.label, "item1", "item1 is selected");
    is(completeNode.value, prefix + "item1", "completeNode.value holds item1");

    popup._panel.addEventListener("popuphidden", function() {
      popup._panel.removeEventListener("popuphidden", arguments.callee, false);

      ok(!popup.isOpen, "popup is not open after VK_RETURN");

      is(inputNode.value, "window.foobarBug585991.item1",
         "completion was successful after VK_RETURN");

      ok(!completeNode.value, "completeNode is empty");

      executeSoon(finishTest);
    }, false);

    EventUtils.synthesizeKey("VK_RETURN", {});
  }, false);

  executeSoon(function() {
    jsterm.setInputValue("window.foobarBug58599");
    EventUtils.synthesizeKey("1", {});
    EventUtils.synthesizeKey(".", {});
  });
}
