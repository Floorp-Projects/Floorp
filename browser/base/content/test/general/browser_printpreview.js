let ourTab;

function test() {
  waitForExplicitFinish();

  BrowserTestUtils.openNewForegroundTab(gBrowser, "about:home", true).then(function(tab) {
    ourTab = tab;
    ok(!gInPrintPreviewMode,
       "Should NOT be in print preview mode at starting this tests");
    // Skip access key test on platforms which don't support access key.
    if (!/Win|Linux/.test(navigator.platform)) {
      openPrintPreview(testClosePrintPreviewWithEscKey);
    } else {
      openPrintPreview(testClosePrintPreviewWithAccessKey);
    }
  });
}

function tidyUp() {
  BrowserTestUtils.removeTab(ourTab);
  finish();
}

async function testClosePrintPreviewWithAccessKey() {
  let closeButton = document.getElementById("print-preview-toolbar-close-button");
  await TestUtils.waitForCondition(() => closeButton.hasAttribute("accesskey"));
  EventUtils.synthesizeKey("c", {altKey: true});
  checkPrintPreviewClosed(function(aSucceeded) {
    ok(aSucceeded,
       "print preview mode should be finished by access key");
    openPrintPreview(testClosePrintPreviewWithEscKey);
  });
}

function testClosePrintPreviewWithEscKey() {
  EventUtils.synthesizeKey("KEY_Escape");
  checkPrintPreviewClosed(function(aSucceeded) {
    ok(aSucceeded,
       "print preview mode should be finished by Esc key press");
    openPrintPreview(testClosePrintPreviewWithClosingWindowShortcutKey);
  });
}

function testClosePrintPreviewWithClosingWindowShortcutKey() {
  EventUtils.synthesizeKey("w", {accelKey: true});
  checkPrintPreviewClosed(function(aSucceeded) {
    ok(aSucceeded,
       "print preview mode should be finished by closing window shortcut key");
    tidyUp();
  });
}

function openPrintPreview(aCallback) {
  document.getElementById("cmd_printPreview").doCommand();
  executeSoon(function waitForPrintPreview() {
    if (gInPrintPreviewMode) {
      executeSoon(aCallback);
      return;
    }
    executeSoon(waitForPrintPreview);
  });
}

function checkPrintPreviewClosed(aCallback) {
  let count = 0;
  executeSoon(function waitForPrintPreviewClosed() {
    if (!gInPrintPreviewMode) {
      executeSoon(function() { aCallback(count < 1000); });
      return;
    }
    if (++count == 1000) {
      // The test might fail.
      PrintUtils.exitPrintPreview();
    }
    executeSoon(waitForPrintPreviewClosed);
  });
}
