/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// Tests that keyboard navigation in the search panel works as designed.

const isMac = ("nsILocalFileMac" in Ci);

const searchbar = document.getElementById("searchbar");
const textbox = searchbar._textbox;
const searchPopup = document.getElementById("PopupSearchAutoComplete");
const searchIcon = document.getAnonymousElementByAttribute(searchbar, "anonid",
                                                           "searchbar-search-button");

const oneOffsContainer =
  document.getAnonymousElementByAttribute(searchPopup, "anonid",
                                          "search-one-off-buttons");
const searchSettings =
  document.getAnonymousElementByAttribute(oneOffsContainer, "anonid",
                                          "search-settings");
var header =
  document.getAnonymousElementByAttribute(oneOffsContainer, "anonid",
                                          "search-panel-one-offs-header");
function getHeaderText() {
  let headerChild = header.selectedPanel;
  while (headerChild.hasChildNodes()) {
    headerChild = headerChild.firstChild;
  }
  let headerStrings = [];
  for (let label = headerChild; label; label = label.nextSibling) {
    headerStrings.push(label.value);
  }
  return headerStrings.join("");
}

const msg = isMac ? 5 : 1;
const utils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIDOMWindowUtils);
const scale = utils.screenPixelsPerCSSPixel;
function* synthesizeNativeMouseMove(aElement) {
  let rect = aElement.getBoundingClientRect();
  let win = aElement.ownerGlobal;
  let x = win.mozInnerScreenX + (rect.left + rect.right) / 2;
  let y = win.mozInnerScreenY + (rect.top + rect.bottom) / 2;

  // Wait for the mouseup event to occur before continuing.
  return new Promise((resolve, reject) => {
    function eventOccurred(e)
    {
      aElement.removeEventListener("mouseover", eventOccurred, true);
      resolve();
    }

    aElement.addEventListener("mouseover", eventOccurred, true);

    utils.sendNativeMouseEvent(x * scale, y * scale, msg, 0, null);
  });
}


add_task(function* init() {
  yield promiseNewEngine("testEngine.xml");
});

add_task(function* test_notext() {
  let promise = promiseEvent(searchPopup, "popupshown");
  info("Opening search panel");
  EventUtils.synthesizeMouseAtCenter(searchIcon, {});
  yield promise;

  is(header.getAttribute("selectedIndex"), 0,
     "Header has the correct index selected with no search terms.");

  is(getHeaderText(), "Search with:",
     "Search header string is correct when no search terms have been entered");

  yield synthesizeNativeMouseMove(searchSettings);
  is(header.getAttribute("selectedIndex"), 0,
     "Header has the correct index when no search terms have been entered and the Change Search Settings button is selected.");
  is(getHeaderText(), "Search with:",
     "Header has the correct text when no search terms have been entered and the Change Search Settings button is selected.");

  let buttons = getOneOffs();
  yield synthesizeNativeMouseMove(buttons[0]);
  is(header.getAttribute("selectedIndex"), 2,
     "Header has the correct index selected when a search engine has been selected");
  is(getHeaderText(), "Search " + buttons[0].engine.name,
     "Is the header text correct when a search engine is selected and no terms have been entered.");

  promise = promiseEvent(searchPopup, "popuphidden");
  info("Closing search panel");
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  yield promise;
});

add_task(function* test_text() {
  textbox.value = "foo";
  registerCleanupFunction(() => {
    textbox.value = "";
  });

  let promise = promiseEvent(searchPopup, "popupshown");
  info("Opening search panel");
  SimpleTest.executeSoon(() => {
    EventUtils.synthesizeMouseAtCenter(searchIcon, {});
  });
  yield promise;

  is(header.getAttribute("selectedIndex"), 1,
     "Header has the correct index selected with a search term.");
  is(getHeaderText(), "Search for foo with:",
     "Search header string is correct when a search term has been entered");

  let buttons = getOneOffs();
  yield synthesizeNativeMouseMove(buttons[0]);
  is(header.getAttribute("selectedIndex"), 2,
     "Header has the correct index selected when a search engine has been selected");
  is(getHeaderText(), "Search " + buttons[0].engine.name,
     "Is the header text correct when search terms are entered after a search engine has been selected.");

  yield synthesizeNativeMouseMove(searchSettings);
  is(header.getAttribute("selectedIndex"), 1,
     "Header has the correct index selected when search terms have been entered and the Change Search Settings button is selected.");
  is(getHeaderText(), "Search for foo with:",
     "Header has the correct text when search terms have been entered and the Change Search Settings button is selected.");

  promise = promiseEvent(searchPopup, "popuphidden");
  info("Closing search panel");
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  yield promise;

  // Move the cursor out of the panel area to avoid messing with other tests.
  yield synthesizeNativeMouseMove(searchbar);
});
