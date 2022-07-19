let SELECT = "<html><body><select id='one'>";
for (let i = 0; i < 75; i++) {
  SELECT += `  <option>${i}${i}${i}${i}${i}</option>`;
}
SELECT +=
  '  <option selected="true">{"end": "true"}</option>' +
  "</select></body></html>";

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.forms.selectSearch", true]],
  });
});

add_task(async function test_focus_on_search_shouldnt_close_popup() {
  const pageUrl = "data:text/html," + escape(SELECT);
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);

  let popupShownPromise = BrowserTestUtils.waitForSelectPopupShown(window);

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#one",
    { type: "mousedown" },
    gBrowser.selectedBrowser
  );
  let selectPopup = await popupShownPromise;

  let searchInput = selectPopup.querySelector(
    ".contentSelectDropdown-searchbox"
  );
  searchInput.scrollIntoView();
  let searchFocused = BrowserTestUtils.waitForEvent(searchInput, "focus", true);
  await EventUtils.synthesizeMouseAtCenter(searchInput, {}, window);
  await searchFocused;

  is(
    selectPopup.state,
    "open",
    "select popup should still be open after clicking on the search field"
  );

  await hideSelectPopup("escape");
  BrowserTestUtils.removeTab(tab);
});
