let SELECT =
  "<html><body><select id='one'>";
for (let i = 0; i < 75; i++) {
  SELECT +=
    `  <option>${i}${i}${i}${i}${i}</option>`;
}
SELECT +=
    '  <option selected="true">{"end": "true"}</option>' +
  "</select></body></html>";

add_task(function* setup() {
  yield SpecialPowers.pushPrefEnv({
    "set": [
      ["dom.select_popup_in_parent.enabled", true],
      ["dom.forms.selectSearch", true]
    ]
  });
});

add_task(function* test_focus_on_search_shouldnt_close_popup() {
  const pageUrl = "data:text/html," + escape(SELECT);
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);

  let menulist = document.getElementById("ContentSelectDropdown");
  let selectPopup = menulist.menupopup;

  let popupShownPromise = BrowserTestUtils.waitForEvent(selectPopup, "popupshown");
  yield BrowserTestUtils.synthesizeMouseAtCenter("#one", { type: "mousedown" }, gBrowser.selectedBrowser);
  yield popupShownPromise;

  let searchInput = selectPopup.querySelector("textbox[type='search']");
  searchInput.scrollIntoView();
  let searchFocused = BrowserTestUtils.waitForEvent(searchInput, "focus");
  yield EventUtils.synthesizeMouseAtCenter(searchInput, {}, window);
  yield searchFocused;

  is(selectPopup.state, "open", "select popup should still be open after clicking on the search field");

  yield hideSelectPopup(selectPopup, "escape");
  yield BrowserTestUtils.removeTab(tab);
});

