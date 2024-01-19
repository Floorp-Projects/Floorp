let ourTab;

async function test() {
  waitForExplicitFinish();

  BrowserTestUtils.openNewForegroundTab(gBrowser, "about:home", true).then(
    function (tab) {
      ourTab = tab;
      ok(
        !document.querySelector(".printPreviewBrowser"),
        "Should NOT be in print preview mode at starting this tests"
      );
      testClosePrintPreviewWithEscKey();
    }
  );
}

function tidyUp() {
  BrowserTestUtils.removeTab(ourTab);
  finish();
}

async function testClosePrintPreviewWithEscKey() {
  await openPrintPreview();
  EventUtils.synthesizeKey("KEY_Escape");
  await checkPrintPreviewClosed();
  ok(true, "print preview mode should be finished by Esc key press");
  tidyUp();
}

async function openPrintPreview() {
  document.getElementById("cmd_print").doCommand();
  await BrowserTestUtils.waitForCondition(() => {
    let preview = document.querySelector(".printPreviewBrowser");
    return preview && BrowserTestUtils.isVisible(preview);
  });
}

async function checkPrintPreviewClosed() {
  await BrowserTestUtils.waitForCondition(
    () => !document.querySelector(".printPreviewBrowser")
  );
}
