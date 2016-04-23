/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test checks that a <select> with an <optgroup> opens and can be navigated
// in a child process. This is different than single-process as a <menulist> is used
// to implement the dropdown list.

const XHTML_DTD = '<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN" "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">';

const PAGECONTENT =
  "<html xmlns='http://www.w3.org/1999/xhtml'>" + 
  "<body onload='gChangeEvents = 0; document.body.firstChild.focus()'><select onchange='gChangeEvents++'>" +
  "  <optgroup label='First Group'>" +
  "    <option value='One'>One</option>" +
  "    <option value='Two'>Two</option>" +
  "  </optgroup>" +
  "  <option value='Three'>Three</option>" +
  "  <optgroup label='Second Group' disabled='true'>" +
  "    <option value='Four'>Four</option>" +
  "    <option value='Five'>Five</option>" +
  "  </optgroup>" +
  "  <option value='Six' disabled='true'>Six</option>" +
  "  <optgroup label='Third Group'>" +
  "    <option value='Seven'>   Seven  </option>" +
  "    <option value='Eight'>&nbsp;&nbsp;Eight&nbsp;&nbsp;</option>" +
  "  </optgroup></select><input />Text" +
  "</body></html>";

const PAGECONTENT_SMALL =
  "<html>" +
  "<body><select id='one'>" +
  "  <option value='One'>One</option>" +
  "  <option value='Two'>Two</option>" +
  "</select><select id='two'>" +
  "  <option value='Three'>Three</option>" +
  "  <option value='Four'>Four</option>" +
  "</select><select id='three'>" +
  "  <option value='Five'>Five</option>" +
  "  <option value='Six'>Six</option>" +
  "</select></body></html>";

function openSelectPopup(selectPopup, withMouse, selector = "select")
{
  let popupShownPromise = BrowserTestUtils.waitForEvent(selectPopup, "popupshown");

  if (withMouse) {
    return Promise.all([popupShownPromise,
                        BrowserTestUtils.synthesizeMouseAtCenter(selector, { }, gBrowser.selectedBrowser)]);
  }

  setTimeout(() => EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true, code: "ArrowDown" }), 1500);
  return popupShownPromise;
}

function hideSelectPopup(selectPopup, withEscape)
{
  let popupHiddenPromise = BrowserTestUtils.waitForEvent(selectPopup, "popuphidden");

  if (withEscape) {
    EventUtils.synthesizeKey("KEY_Escape", { code: "Escape" });
  }
  else {
    EventUtils.synthesizeKey("KEY_Enter", { code: "Enter" });
  }

  return popupHiddenPromise;
}

function getChangeEvents()
{
  return ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
    return content.wrappedJSObject.gChangeEvents;
  });
}

function doSelectTests(contentType, dtd)
{
  const pageUrl = "data:" + contentType + "," + escape(dtd + "\n" + PAGECONTENT);
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);

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

  EventUtils.synthesizeKey("a", { accelKey: true });
  yield ContentTask.spawn(gBrowser.selectedBrowser, { isWindows }, function(args) {
    Assert.equal(String(content.getSelection()), args.isWindows ? "Text" : "",
      "Select all while popup is open");
  });

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

  is(selectPopup.lastChild.previousSibling.label, "Seven", "Spaces collapsed");
  is(selectPopup.lastChild.label, "\xA0\xA0Eight\xA0\xA0", "Non-breaking spaces not collapsed");

  yield BrowserTestUtils.removeTab(tab);
}

add_task(function*() {
  yield doSelectTests("text/html", "");
});

add_task(function*() {
  yield doSelectTests("application/xhtml+xml", XHTML_DTD);
});

// This test opens a select popup and removes the content node of a popup while
// The popup should close if its node is removed.
add_task(function*() {
  const pageUrl = "data:text/html," + escape(PAGECONTENT_SMALL);
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);

  let menulist = document.getElementById("ContentSelectDropdown");
  let selectPopup = menulist.menupopup;

  // First, try it when a different <select> element than the one that is open is removed
  yield openSelectPopup(selectPopup, true, "#one");

  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
    content.document.body.removeChild(content.document.getElementById("two"));
  });

  // Wait a bit just to make sure the popup won't close.
  yield new Promise(resolve => setTimeout(resolve, 1000));
  
  is(selectPopup.state, "open", "Different popup did not affect open popup");

  yield hideSelectPopup(selectPopup);

  // Next, try it when the same <select> element than the one that is open is removed
  yield openSelectPopup(selectPopup, true, "#three");

  let popupHiddenPromise = BrowserTestUtils.waitForEvent(selectPopup, "popuphidden");
  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
    content.document.body.removeChild(content.document.getElementById("three"));
  });
  yield popupHiddenPromise;

  ok(true, "Popup hidden when select is removed");

  // Finally, try it when the tab is closed while the select popup is open.
  yield openSelectPopup(selectPopup, true, "#one");

  popupHiddenPromise = BrowserTestUtils.waitForEvent(selectPopup, "popuphidden");
  yield BrowserTestUtils.removeTab(tab);
  yield popupHiddenPromise;

  ok(true, "Popup hidden when tab is closed");
});
