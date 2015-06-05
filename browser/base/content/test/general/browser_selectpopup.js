/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test checks that a <select> with an <optgroup> opens and can be navigated
// in a child process. This is different than single-process as a <menulist> is used
// to implement the dropdown list.

const PAGECONTENT =
  "<html><body onload='document.body.firstChild.focus()'><select>" +
  "  <optgroup label='First Group'>" +
  "    <option value=One>One" +
  "    <option value=Two>Two" +
  "  </optgroup>" +
  "  <option value=Three>Three" +
  "  <optgroup label='Second Group'>" +
  "    <option value=Four>Four" +
  "    <option value=Five>Five" +
  "  </optgroup>" +
  "</body></html>";

function openSelectPopup(selectPopup)
{
  return new Promise((resolve, reject) => {
    selectPopup.addEventListener("popupshown", function popupListener(event) {
      selectPopup.removeEventListener("popupshown", popupListener, false)
      resolve();
    }, false);
    setTimeout(() => EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true, code: "ArrowDown" }), 1500);
  });
}

function hideSelectPopup(selectPopup)
{
  return new Promise((resolve, reject) => {
    selectPopup.addEventListener("popuphidden", function popupListener(event) {
      selectPopup.removeEventListener("popuphidden", popupListener, false)
      resolve();
    }, false);
    EventUtils.synthesizeKey("KEY_Enter", { code: "Enter" });
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

  is(menulist.selectedIndex, 1, "Initial selection");
  is(selectPopup.firstChild.localName, "menucaption", "optgroup is caption");
  is(selectPopup.firstChild.getAttribute("label"), "First Group", "optgroup label");
  is(selectPopup.childNodes[1].localName, "menuitem", "option is menuitem");
  is(selectPopup.childNodes[1].getAttribute("label"), "One", "option label");

  EventUtils.synthesizeKey("KEY_ArrowDown", { code: "ArrowDown" });
  is(menulist.menuBoxObject.activeChild, menulist.getItemAtIndex(2), "Select item 2");

  EventUtils.synthesizeKey("KEY_ArrowDown", { code: "ArrowDown" });
  is(menulist.menuBoxObject.activeChild, menulist.getItemAtIndex(3), "Select item 3");

  EventUtils.synthesizeKey("KEY_ArrowDown", { code: "ArrowDown" });
  is(menulist.menuBoxObject.activeChild, menulist.getItemAtIndex(5), "Skip optgroup header and select item 4");

  yield hideSelectPopup(selectPopup);

  is(menulist.selectedIndex, 5, "Item 4 still selected selectedIndex");

  gBrowser.removeCurrentTab();
});

