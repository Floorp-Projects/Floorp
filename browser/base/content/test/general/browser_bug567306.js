/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var HasFindClipboard = Services.clipboard.supportsFindClipboard();

add_task(async function() {
  let newwindow = await BrowserTestUtils.openNewBrowserWindow();

  let selectedBrowser = newwindow.gBrowser.selectedBrowser;
  await new Promise((resolve, reject) => {
    BrowserTestUtils.waitForContentEvent(
      selectedBrowser,
      "pageshow",
      true,
      event => {
        return event.target.location != "about:blank";
      }
    ).then(function pageshowListener() {
      ok(
        true,
        "pageshow listener called: " + newwindow.gBrowser.currentURI.spec
      );
      resolve();
    });
    selectedBrowser.loadURI("data:text/html,<h1 id='h1'>Select Me</h1>", {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
  });

  await SimpleTest.promiseFocus(newwindow);

  ok(!newwindow.gFindBarInitialized, "find bar is not yet initialized");
  let findBar = await newwindow.gFindBarPromise;

  await SpecialPowers.spawn(selectedBrowser, [], async function() {
    let elt = content.document.getElementById("h1");
    let selection = content.getSelection();
    let range = content.document.createRange();
    range.setStart(elt, 0);
    range.setEnd(elt, 1);
    selection.removeAllRanges();
    selection.addRange(range);
  });

  await findBar.onFindCommand();

  // When the OS supports the Find Clipboard (OSX), the find field value is
  // persisted across Fx sessions, thus not useful to test.
  if (!HasFindClipboard) {
    is(
      findBar._findField.value,
      "Select Me",
      "Findbar is initialized with selection"
    );
  }
  findBar.close();
  await promiseWindowClosed(newwindow);
});
