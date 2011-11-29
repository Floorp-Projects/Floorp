/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the filter button UI logic works correctly.

const TEST_URI = "http://example.com/";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", testFilterButtons, true);
}

function testFilterButtons() {
  browser.removeEventListener("load", testFilterButtons, true);
  openConsole();

  let hud = HUDService.getHudByWindow(content);
  hudId = hud.hudId;
  hudBox = hud.HUDBox;

  testMenuFilterButton("net");
  testMenuFilterButton("css");
  testMenuFilterButton("js");
  testMenuFilterButton("logging");

  finishTest();
}

function testMenuFilterButton(aCategory) {
  let selector = ".webconsole-filter-button[category=\"" + aCategory + "\"]";
  let button = hudBox.querySelector(selector);
  ok(button, "we have the \"" + aCategory + "\" button");

  let firstMenuItem = button.querySelector("menuitem");
  ok(firstMenuItem, "we have the first menu item for the \"" + aCategory +
     "\" button");

  // Turn all the filters off, if they were on.
  let menuItem = firstMenuItem;
  while (menuItem != null) {
    if (isChecked(menuItem)) {
      chooseMenuItem(menuItem);
    }
    menuItem = menuItem.nextSibling;
  }

  // Turn all the filters on; make sure the button gets checked.
  menuItem = firstMenuItem;
  let prefKey;
  while (menuItem) {
    prefKey = menuItem.getAttribute("prefKey");
    chooseMenuItem(menuItem);
    ok(isChecked(menuItem), "menu item " + prefKey + " for category " +
       aCategory + " is checked after clicking it");
    ok(HUDService.filterPrefs[hudId][prefKey], prefKey + " messages are " +
       "on after clicking the appropriate menu item");
    menuItem = menuItem.nextSibling;
  }
  ok(isChecked(button), "the button for category " + aCategory + " is " +
     "checked after turning on all its menu items");

  // Turn one filter off; make sure the button is still checked.
  prefKey = firstMenuItem.getAttribute("prefKey");
  chooseMenuItem(firstMenuItem);
  ok(!isChecked(firstMenuItem), "the first menu item for category " +
     aCategory + " is no longer checked after clicking it");
  ok(!HUDService.filterPrefs[hudId][prefKey], prefKey + " messages are " +
     "turned off after clicking the appropriate menu item");
  ok(isChecked(button), "the button for category " + aCategory + " is still " +
     "checked after turning off its first menu item");

  // Turn all the filters off by clicking the main part of the button.
  let anonymousNodes = document.getAnonymousNodes(button);
  let subbutton;
  for (let i = 0; i < anonymousNodes.length; i++) {
    let node = anonymousNodes[i];
    if (node.classList.contains("toolbarbutton-menubutton-button")) {
      subbutton = node;
      break;
    }
  }
  ok(subbutton, "we have the subbutton for category " + aCategory);

  clickButton(subbutton);
  ok(!isChecked(button), "the button for category " + aCategory + " is " +
     "no longer checked after clicking its main part");

  menuItem = firstMenuItem;
  while (menuItem) {
    let prefKey = menuItem.getAttribute("prefKey");
    ok(!isChecked(menuItem), "menu item " + prefKey + " for category " +
       aCategory + " is no longer checked after clicking the button");
    ok(!HUDService.filterPrefs[hudId][prefKey], prefKey + " messages are " +
       "off after clicking the button");
    menuItem = menuItem.nextSibling;
  }

  // Turn all the filters on by clicking the main part of the button.
  clickButton(subbutton);

  ok(isChecked(button), "the button for category " + aCategory + " is " +
     "checked after clicking its main part");

  menuItem = firstMenuItem;
  while (menuItem) {
    let prefKey = menuItem.getAttribute("prefKey");
    ok(isChecked(menuItem), "menu item " + prefKey + " for category " +
       aCategory + " is checked after clicking the button");
    ok(HUDService.filterPrefs[hudId][prefKey], prefKey + " messages are " +
       "on after clicking the button");
    menuItem = menuItem.nextSibling;
  }

  // Uncheck the main button by unchecking all the filters
  menuItem = firstMenuItem;
  while (menuItem) {
    chooseMenuItem(menuItem);
    menuItem = menuItem.nextSibling;
  }

  ok(!isChecked(button), "the button for category " + aCategory + " is " +
     "unchecked after unchecking all its filters");

  // Turn all the filters on again by clicking the button.
  clickButton(subbutton);
}

function clickButton(aNode) {
  EventUtils.sendMouseEvent({ type: "click" }, aNode);
}

function chooseMenuItem(aNode) {
  let event = document.createEvent("XULCommandEvent");
  event.initCommandEvent("command", true, true, window, 0, false, false, false,
                         false, null);
  aNode.dispatchEvent(event);
}

function isChecked(aNode) {
  return aNode.getAttribute("checked") === "true";
}

