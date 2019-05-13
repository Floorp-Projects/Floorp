let SELECT =
  "<html><body><select id='one'>";
for (let i = 0; i < 75; i++) {
  SELECT +=
    `  <option>${i}${i}${i}${i}${i}</option>`;
}
SELECT +=
    '  <option selected="true">{"end": "true"}</option>' +
  "</select></body></html>";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    "set": [
      ["dom.select_popup_in_parent.enabled", true],
      ["dom.forms.selectSearch", true],
    ],
  });
});

add_task(async function test_focus_on_search_shouldnt_close_popup() {
  const pageUrl = "data:text/html," + escape(SELECT);
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);

  let menulist = document.getElementById("ContentSelectDropdown");
  let selectPopup = menulist.menupopup;

  let popupShownPromise = BrowserTestUtils.waitForEvent(selectPopup, "popupshown");
  await BrowserTestUtils.synthesizeMouseAtCenter("#one", { type: "mousedown" }, gBrowser.selectedBrowser);
  await popupShownPromise;

  let searchInput = selectPopup.querySelector(".contentSelectDropdown-searchbox");
  searchInput.scrollIntoView();
  let searchFocused = BrowserTestUtils.waitForEvent(searchInput, "focus", true);
  await EventUtils.synthesizeMouseAtCenter(searchInput, {}, window);
  await searchFocused;

  is(selectPopup.state, "open", "select popup should still be open after clicking on the search field");

  await hideSelectPopup(selectPopup, "escape");
  BrowserTestUtils.removeTab(tab);
});
