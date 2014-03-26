/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the filter button UI logic works correctly.

const TEST_URI = "http://example.com/";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, testFilterButtons);
  }, true);
}

function testFilterButtons(aHud) {
  hud = aHud;
  hudId = hud.hudId;
  hudBox = hud.ui.rootElement;

  testMenuFilterButton("net");
  testMenuFilterButton("css");
  testMenuFilterButton("js");
  testMenuFilterButton("logging");
  testMenuFilterButton("security");

  testIsolateFilterButton("net");
  testIsolateFilterButton("css");
  testIsolateFilterButton("js");
  testIsolateFilterButton("logging");
  testIsolateFilterButton("security");

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
    if (menuItem.hasAttribute("prefKey")) {
      prefKey = menuItem.getAttribute("prefKey");
      chooseMenuItem(menuItem);
      ok(isChecked(menuItem), "menu item " + prefKey + " for category " +
         aCategory + " is checked after clicking it");
      ok(hud.ui.filterPrefs[prefKey], prefKey + " messages are " +
         "on after clicking the appropriate menu item");
    }
    menuItem = menuItem.nextSibling;
  }
  ok(isChecked(button), "the button for category " + aCategory + " is " +
     "checked after turning on all its menu items");

  // Turn one filter off; make sure the button is still checked.
  prefKey = firstMenuItem.getAttribute("prefKey");
  chooseMenuItem(firstMenuItem);
  ok(!isChecked(firstMenuItem), "the first menu item for category " +
     aCategory + " is no longer checked after clicking it");
  ok(!hud.ui.filterPrefs[prefKey], prefKey + " messages are " +
     "turned off after clicking the appropriate menu item");
  ok(isChecked(button), "the button for category " + aCategory + " is still " +
     "checked after turning off its first menu item");

  // Turn all the filters off by clicking the main part of the button.
  let subbutton = getMainButton(button);
  ok(subbutton, "we have the subbutton for category " + aCategory);

  clickButton(subbutton);
  ok(!isChecked(button), "the button for category " + aCategory + " is " +
     "no longer checked after clicking its main part");

  menuItem = firstMenuItem;
  while (menuItem) {
    let prefKey = menuItem.getAttribute("prefKey");
    ok(!isChecked(menuItem), "menu item " + prefKey + " for category " +
       aCategory + " is no longer checked after clicking the button");
    ok(!hud.ui.filterPrefs[prefKey], prefKey + " messages are " +
       "off after clicking the button");
    menuItem = menuItem.nextSibling;
  }

  // Turn all the filters on by clicking the main part of the button.
  clickButton(subbutton);

  ok(isChecked(button), "the button for category " + aCategory + " is " +
     "checked after clicking its main part");

  menuItem = firstMenuItem;
  while (menuItem) {
    if (menuItem.hasAttribute("prefKey")) {
      let prefKey = menuItem.getAttribute("prefKey");
      // The CSS/Log menu item should not be checked. See bug 971798.
      if (aCategory == "css" && prefKey == "csslog") {
        ok(!isChecked(menuItem), "menu item " + prefKey + " for category " +
           aCategory + " should not be checked after clicking the button");
        ok(!hud.ui.filterPrefs[prefKey], prefKey + " messages are " +
           "off after clicking the button");
      } else {
        ok(isChecked(menuItem), "menu item " + prefKey + " for category " +
           aCategory + " is checked after clicking the button");
        ok(hud.ui.filterPrefs[prefKey], prefKey + " messages are " +
           "on after clicking the button");
      }
    }
    menuItem = menuItem.nextSibling;
  }

  // Uncheck the main button by unchecking all the filters
  menuItem = firstMenuItem;
  while (menuItem) {
    // The csslog menu item is already unchecked at this point.
    // Make sure it is not selected. See bug 971798.
    if (menuItem.getAttribute("prefKey") != "csslog") {
      chooseMenuItem(menuItem);
    }
    menuItem = menuItem.nextSibling;
  }

  ok(!isChecked(button), "the button for category " + aCategory + " is " +
     "unchecked after unchecking all its filters");

  // Turn all the filters on again by clicking the button.
  clickButton(subbutton);
}

function testIsolateFilterButton(aCategory) {
  let selector = ".webconsole-filter-button[category=\"" + aCategory + "\"]";
  let targetButton = hudBox.querySelector(selector);
  ok(targetButton, "we have the \"" + aCategory + "\" button");

  // Get the main part of the filter button.
  let subbutton = getMainButton(targetButton);
  ok(subbutton, "we have the subbutton for category " + aCategory);

  // Turn on all the filters by alt clicking the main part of the button.
  altClickButton(subbutton);
  ok(isChecked(targetButton), "the button for category " + aCategory +
     " is checked after isolating for filter");

  // Check if all the filters for the target button are on.
  let menuItems = targetButton.querySelectorAll("menuitem");
  Array.forEach(menuItems, (item) => {
    let prefKey = item.getAttribute("prefKey");
    // The CSS/Log filter should not be checked. See bug 971798.
    if (aCategory == "css" && prefKey == "csslog") {
      ok(!isChecked(item), "menu item " + prefKey + " for category " +
        aCategory + " should not be checked after isolating for " + aCategory);
      ok(!hud.ui.filterPrefs[prefKey], prefKey + " messages should be " +
        "turned off after isolating for " + aCategory);
    } else {
      ok(isChecked(item), "menu item " + prefKey + " for category " +
        aCategory + " is checked after isolating for " + aCategory);
      ok(hud.ui.filterPrefs[prefKey], prefKey + " messages are " +
        "turned on after isolating for " + aCategory);
    }
  });

  // Ensure all other filter buttons are toggled off and their
  // associated filters are turned off
  let buttons = hudBox.querySelectorAll(".webconsole-filter-button[category]");
  Array.forEach(buttons, (filterButton) => {
    if (filterButton !== targetButton) {
      let category = filterButton.getAttribute("category");
      ok(!isChecked(filterButton), "the button for category " +
        category + " is unchecked after isolating for " + aCategory);

      menuItems = filterButton.querySelectorAll("menuitem");
      Array.forEach(menuItems, (item) => {
        let prefKey = item.getAttribute("prefKey");
        ok(!isChecked(item), "menu item " + prefKey + " for category " +
          aCategory + " is unchecked after isolating for " + aCategory);
        ok(!hud.ui.filterPrefs[prefKey], prefKey + " messages are " +
          "turned off after isolating for " + aCategory);
      });

      // Turn all the filters on again by clicking the button.
      let mainButton = getMainButton(filterButton);
      clickButton(mainButton);
    }
  });
}

/**
 * Return the main part of the target filter button.
 */
function getMainButton(aTargetButton) {
  let anonymousNodes = hud.ui.document.getAnonymousNodes(aTargetButton);
  let subbutton;

  for (let i = 0; i < anonymousNodes.length; i++) {
    let node = anonymousNodes[i];
    if (node.classList.contains("toolbarbutton-menubutton-button")) {
      subbutton = node;
      break;
    }
  }

  return subbutton;
}

function clickButton(aNode) {
  EventUtils.sendMouseEvent({ type: "click" }, aNode);
}

function altClickButton(aNode) {
  EventUtils.sendMouseEvent({ type: "click", altKey: true }, aNode);
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

