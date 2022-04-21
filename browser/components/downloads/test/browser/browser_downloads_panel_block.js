/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

add_task(async function mainTest() {
  await task_resetState();

  let verdicts = [
    Downloads.Error.BLOCK_VERDICT_UNCOMMON,
    Downloads.Error.BLOCK_VERDICT_MALWARE,
    Downloads.Error.BLOCK_VERDICT_POTENTIALLY_UNWANTED,
    Downloads.Error.BLOCK_VERDICT_INSECURE,
  ];
  await task_addDownloads(verdicts.map(v => makeDownload(v)));

  // Check that the richlistitem for each download is correct.
  for (let i = 0; i < verdicts.length; i++) {
    await openPanel();

    // Handle items backwards, using lastElementChild, to ensure there's no
    // code wrongly resetting the selection to the first item during the process.
    let item = DownloadsView.richListBox.lastElementChild;

    info("Open the panel and click the item to show the subview.");
    let viewPromise = promiseViewShown(DownloadsBlockedSubview.subview);
    EventUtils.synthesizeMouseAtCenter(item, {});
    await viewPromise;

    // Items are listed in newest-to-oldest order, so e.g. the first item's
    // verdict is the last element in the verdicts array.
    Assert.ok(
      DownloadsBlockedSubview.subview.getAttribute("verdict"),
      verdicts[verdicts.count - i - 1]
    );

    info("Go back to the main view.");
    viewPromise = promiseViewShown(DownloadsBlockedSubview.mainView);
    DownloadsBlockedSubview.panelMultiView.goBack();
    await viewPromise;

    info("Show the subview again.");
    viewPromise = promiseViewShown(DownloadsBlockedSubview.subview);
    EventUtils.synthesizeMouseAtCenter(item, {});
    await viewPromise;

    info("Click the Open button.");
    // The download should be unblocked and then opened,
    // i.e., unblockAndOpenDownload() should be called on the item.  The panel
    // should also be closed as a result, so wait for that too.
    let unblockPromise = promiseUnblockAndSaveCalled(item);
    let hidePromise = promisePanelHidden();
    // Simulate a mousemove to ensure it's not wrongly being handled by the
    // panel as the user changing download selection.
    EventUtils.synthesizeMouseAtCenter(
      DownloadsBlockedSubview.elements.unblockButton,
      { type: "mousemove" }
    );
    EventUtils.synthesizeMouseAtCenter(
      DownloadsBlockedSubview.elements.unblockButton,
      {}
    );
    info("waiting for unblockOpen");
    await unblockPromise;
    info("waiting for hide panel");
    await hidePromise;

    window.focus();
    await SimpleTest.promiseFocus(window);

    info("Reopen the panel and show the subview again.");
    await openPanel();
    viewPromise = promiseViewShown(DownloadsBlockedSubview.subview);
    EventUtils.synthesizeMouseAtCenter(item, {});
    await viewPromise;

    info("Click the Remove button.");
    // The panel should close and the item should be removed from it.
    hidePromise = promisePanelHidden();
    EventUtils.synthesizeMouseAtCenter(
      DownloadsBlockedSubview.elements.deleteButton,
      {}
    );
    info("Waiting for hide panel");
    await hidePromise;

    info("Open the panel again and check the item is gone.");
    await openPanel();
    Assert.ok(!item.parentNode);

    hidePromise = promisePanelHidden();
    DownloadsPanel.hidePanel();
    await hidePromise;
  }

  await task_resetState();
});

async function openPanel() {
  // This function is insane but something intermittently causes the panel to be
  // closed as soon as it's opening on Linux ASAN.  Maybe it would also happen
  // on other build machines if the test ran often enough.  Not only is the
  // panel closed, it's closed while it's opening, leaving DownloadsPanel._state
  // such that when you try to open the panel again, it thinks it's already
  // open, but it's not.  The result is that the test times out.
  //
  // What this does is call DownloadsPanel.showPanel over and over again until
  // the panel is really open.  There are a few wrinkles:
  //
  // (1) When panel.state is "open", check four more times (for a total of five)
  // before returning to make the panel stays open.
  // (2) If the panel is not open, check the _state.  It should be either
  // kStateUninitialized or kStateHidden.  If it's not, then the panel is in the
  // process of opening -- or maybe it's stuck in that process -- so reset the
  // _state to kStateHidden.
  // (3) If the _state is not kStateUninitialized or kStateHidden, then it may
  // actually be properly opening and not stuck at all.  To avoid always closing
  // the panel while it's properly opening, use an exponential backoff mechanism
  // for retries.
  //
  // If all that fails, then the test will time out, but it would have timed out
  // anyway.

  await promiseFocus();
  await new Promise(resolve => {
    let verifyCount = 5;
    let backoff = 0;
    let iBackoff = 0;
    let interval = setInterval(() => {
      if (DownloadsPanel.panel && DownloadsPanel.panel.state == "open") {
        if (verifyCount > 0) {
          verifyCount--;
        } else {
          clearInterval(interval);
          resolve();
        }
      } else if (iBackoff < backoff) {
        // Keep backing off before trying again.
        iBackoff++;
      } else {
        // Try (or retry) opening the panel.
        verifyCount = 5;
        backoff = Math.max(1, 2 * backoff);
        iBackoff = 0;
        if (DownloadsPanel._state != DownloadsPanel.kStateUninitialized) {
          DownloadsPanel._state = DownloadsPanel.kStateHidden;
        }
        DownloadsPanel.showPanel();
      }
    }, 100);
  });
}

function promisePanelHidden() {
  return BrowserTestUtils.waitForEvent(DownloadsPanel.panel, "popuphidden");
}

function makeDownload(verdict) {
  return {
    state: DownloadsCommon.DOWNLOAD_DIRTY,
    hasBlockedData: true,
    errorObj: {
      result: Cr.NS_ERROR_FAILURE,
      message: "Download blocked.",
      becauseBlocked: true,
      becauseBlockedByReputationCheck: true,
      reputationCheckVerdict: verdict,
    },
  };
}

function promiseViewShown(view) {
  return BrowserTestUtils.waitForEvent(view, "ViewShown");
}

function promiseUnblockAndSaveCalled(item) {
  return new Promise(resolve => {
    let realFn = item._shell.unblockAndSave;
    item._shell.unblockAndSave = async () => {
      item._shell.unblockAndSave = realFn;
      resolve();
    };
  });
}
