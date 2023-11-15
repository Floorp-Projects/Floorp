const PAGE = `
<!doctype html>
<select id="select">
  <option>ABC</option>
  <option>DEFG</option>
</select>
<button id="invoker" invoketarget="select" invokeaction="showpicker">invoke</button>
`;

add_task(async function test_invoke() {
  const url = "data:text/html," + encodeURI(PAGE);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async function (browser) {
      let popupShownPromise = BrowserTestUtils.waitForSelectPopupShown(window);

      EventUtils.synthesizeMouseAtCenter(
        content.document.getElementById("invoker"),
        {}
      );

      let selectPopup = await popupShownPromise;
      is(
        selectPopup.state,
        "open",
        "select popup is open after invoking with showPicker"
      );
    }
  );
});

add_task(async function test_invoke_alreadyOpen() {
  const url = "data:text/html," + encodeURI(PAGE);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async function (browser) {
      let selectPopup = await openSelectPopup("click");

      EventUtils.synthesizeMouseAtCenter(
        content.document.getElementById("invoker"),
        {}
      );

      // Wait some time for potential (unwanted) closing.
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      await new Promise(resolve => setTimeout(resolve, 100));

      is(
        selectPopup.state,
        "open",
        "select popup is still open after invoking with showPicker"
      );
    }
  );
});
