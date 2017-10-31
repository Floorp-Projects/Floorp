/**
 * Make sure that the form validation error message shows even if the form is in an iframe.
 */
add_task(async function test_iframe() {
  let uri = "data:text/html;charset=utf-8," + escape("<iframe src=\"data:text/html,<iframe name='t'></iframe><form target='t' action='data:text/html,'><input required id='i'><input id='s' type='submit'></form>\" height=\"600\"></iframe>");

  var gInvalidFormPopup = document.getElementById("invalid-form-popup");
  ok(gInvalidFormPopup,
     "The browser should have a popup to show when a form is invalid");

  await BrowserTestUtils.withNewTab(uri, async function checkTab(browser) {
    let popupShownPromise = BrowserTestUtils.waitForEvent(gInvalidFormPopup, "popupshown");

    await ContentTask.spawn(browser, {}, async function() {
      content.document.getElementsByTagName("iframe")[0]
        .contentDocument.getElementById("s").click();
    });
    await popupShownPromise;

    await ContentTask.spawn(browser, {}, async function() {
      let childdoc = content.document.getElementsByTagName("iframe")[0].contentDocument;
      Assert.equal(childdoc.activeElement, childdoc.getElementById("i"),
                   "First invalid element should be focused");
    });

    ok(gInvalidFormPopup.state == "showing" || gInvalidFormPopup.state == "open",
       "The invalid form popup should be shown");
  });
});
