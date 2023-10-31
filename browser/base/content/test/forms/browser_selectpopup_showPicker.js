const PAGE = `
<!doctype html>
<select>
  <option>ABC</option>
  <option>DEFG</option>
</select>
`;

add_task(async function test_showPicker() {
  const url = "data:text/html," + encodeURI(PAGE);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async function (browser) {
      let popupShownPromise = BrowserTestUtils.waitForSelectPopupShown(window);

      await SpecialPowers.spawn(browser, [], async function () {
        content.document.notifyUserGestureActivation();
        content.document.querySelector("select").showPicker();
      });

      let selectPopup = await popupShownPromise;
      is(
        selectPopup.state,
        "open",
        "select popup is open after calling showPicker"
      );
    }
  );
});

add_task(async function test_showPicker_alreadyOpen() {
  const url = "data:text/html," + encodeURI(PAGE);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async function (browser) {
      let selectPopup = await openSelectPopup("click");

      await SpecialPowers.spawn(browser, [], async function () {
        content.document.notifyUserGestureActivation();
        content.document.querySelector("select").showPicker();
      });

      // Wait some time for potential (unwanted) closing.
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      await new Promise(resolve => setTimeout(resolve, 100));

      is(
        selectPopup.state,
        "open",
        "select popup is still open after calling showPicker"
      );
    }
  );
});
