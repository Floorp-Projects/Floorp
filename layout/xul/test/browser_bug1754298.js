add_task(async function() {
  const PAGE = `
<!doctype html>
<select>
<option value="1">AA Option</option>
<option value="2">BB Option</option>
<option value="3">&nbsp;CC Option</option>
<option value="4">&nbsp;&nbsp;DD Option</option>
<option value="5">&nbsp;&nbsp;&nbsp;EE Option</option>
</select>`;
  const url = "data:text/html," + encodeURI(PAGE);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async function(browser) {
      let popupShownPromise = BrowserTestUtils.waitForSelectPopupShown(window);
      await BrowserTestUtils.synthesizeMouseAtCenter("select", {}, browser);
      let popup = await popupShownPromise;
      EventUtils.sendString("C", window);
      EventUtils.sendKey("RETURN", window);
      ok(
        await TestUtils.waitForCondition(() => {
          return SpecialPowers.spawn(
            browser,
            [],
            () => content.document.querySelector("select").value
          ).then(value => value == 3);
        }),
        "Unexpected value for select element (expected 3)!"
      );
    }
  );
});
