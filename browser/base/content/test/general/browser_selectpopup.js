/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test checks that a <select> with an <optgroup> opens and can be navigated
// in a child process. This is different than single-process as a <menulist> is used
// to implement the dropdown list.

const PAGECONTENT =
  "<html><body onload='gChangeEvents = 0; document.body.firstChild.focus()'><select onchange='gChangeEvents++'>" +
  "  <optgroup label='First Group'>" +
  "    <option value=One>One" +
  "    <option value=Two>Two" +
  "  </optgroup>" +
  "  <option value=Three>Three" +
  "  <optgroup label='Second Group' disabled='true'>" +
  "    <option value=Four>Four" +
  "    <option value=Five>Five" +
  "  </optgroup>" +
  "  <option value=Six disabled='true'>Six" +
  "  <optgroup label='Third Group'>" +
  "    <option value=Seven>Seven" +
  "    <option value=Eight>Eight" +
  "  </optgroup></select><input>" +
  "</body></html>";

function openSelectPopup(selectPopup, withMouse)
{
  let popupShownPromise = BrowserTestUtils.waitForEvent(selectPopup, "popupshown");

  if (withMouse) {
    return Promise.all([popupShownPromise,
                        BrowserTestUtils.synthesizeMouseAtCenter("select", { }, gBrowser.selectedBrowser)]);
  }

  setTimeout(() => EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true, code: "ArrowDown" }), 1500);
  return popupShownPromise;
}

function hideSelectPopup(selectPopup, withEscape)
{
  let popupShownPromise = BrowserTestUtils.waitForEvent(selectPopup, "popuphidden");

  if (withEscape) {
    EventUtils.synthesizeKey("KEY_Escape", { code: "Escape" });
  }
  else {
    EventUtils.synthesizeKey("KEY_Enter", { code: "Enter" });
  }

  return popupShownPromise;
}

function getChangeEvents()
{
  return ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
    return content.wrappedJSObject.gChangeEvents;
  });
}

add_task(function*() {
  let tab = gBrowser.selectedTab = gBrowser.addTab();
  let browser = gBrowser.getBrowserForTab(tab);
  yield promiseTabLoadEvent(tab, "data:text/html," + escape(PAGECONTENT));

  yield SimpleTest.promiseFocus(browser.contentWindow);

  let menulist = document.getElementById("ContentSelectDropdown");
  let selectPopup = menulist.menupopup;

  yield openSelectPopup(selectPopup);

  let isWindows = navigator.platform.indexOf("Win") >= 0;

  is(menulist.selectedIndex, 1, "Initial selection");
  is(selectPopup.firstChild.localName, "menucaption", "optgroup is caption");
  is(selectPopup.firstChild.getAttribute("label"), "First Group", "optgroup label");
  is(selectPopup.childNodes[1].localName, "menuitem", "option is menuitem");
  is(selectPopup.childNodes[1].getAttribute("label"), "One", "option label");

  EventUtils.synthesizeKey("KEY_ArrowDown", { code: "ArrowDown" });
  is(menulist.menuBoxObject.activeChild, menulist.getItemAtIndex(2), "Select item 2");
  is(menulist.selectedIndex, isWindows ? 2 : 1, "Select item 2 selectedIndex");

  EventUtils.synthesizeKey("KEY_ArrowDown", { code: "ArrowDown" });
  is(menulist.menuBoxObject.activeChild, menulist.getItemAtIndex(3), "Select item 3");
  is(menulist.selectedIndex, isWindows ? 3 : 1, "Select item 3 selectedIndex");

  EventUtils.synthesizeKey("KEY_ArrowDown", { code: "ArrowDown" });

  // On Windows, one can navigate on disabled menuitems
  let expectedIndex = isWindows ? 5 : 9;
     
  is(menulist.menuBoxObject.activeChild, menulist.getItemAtIndex(expectedIndex),
     "Skip optgroup header and disabled items select item 7");
  is(menulist.selectedIndex, isWindows ? 5 : 1, "Select or skip disabled item selectedIndex");

  for (let i = 0; i < 10; i++) {
    is(menulist.getItemAtIndex(i).disabled, i >= 4 && i <= 7, "item " + i + " disabled")
  }

  EventUtils.synthesizeKey("KEY_ArrowUp", { code: "ArrowUp" });
  is(menulist.menuBoxObject.activeChild, menulist.getItemAtIndex(3), "Select item 3 again");
  is(menulist.selectedIndex, isWindows ? 3 : 1, "Select item 3 selectedIndex");

  is((yield getChangeEvents()), 0, "Before closed - number of change events");

  yield hideSelectPopup(selectPopup);

  is(menulist.selectedIndex, 3, "Item 3 still selected");
  is((yield getChangeEvents()), 1, "After closed - number of change events");

  // Opening and closing the popup without changing the value should not fire a change event.
  yield openSelectPopup(selectPopup, true);
  yield hideSelectPopup(selectPopup, true);
  is((yield getChangeEvents()), 1, "Open and close with no change - number of change events");
  EventUtils.synthesizeKey("VK_TAB", { });
  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true });
  is((yield getChangeEvents()), 1, "Tab away from select with no change - number of change events");

  yield openSelectPopup(selectPopup, true);
  EventUtils.synthesizeKey("KEY_ArrowDown", { code: "ArrowDown" });
  yield hideSelectPopup(selectPopup, true);
  is((yield getChangeEvents()), isWindows ? 2 : 1, "Open and close with change - number of change events");
  EventUtils.synthesizeKey("VK_TAB", { });
  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true });
  is((yield getChangeEvents()), isWindows ? 2 : 1, "Tab away from select with change - number of change events");

  gBrowser.removeCurrentTab();
});

