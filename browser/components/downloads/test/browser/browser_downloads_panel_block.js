"use strict";

add_task(function* mainTest() {
  yield task_resetState();

  let verdicts = [
    Downloads.Error.BLOCK_VERDICT_UNCOMMON,
    Downloads.Error.BLOCK_VERDICT_MALWARE,
    Downloads.Error.BLOCK_VERDICT_POTENTIALLY_UNWANTED,
  ];
  yield task_addDownloads(verdicts.map(v => makeDownload(v)));

  // Check that the richlistitem for each download is correct.
  for (let i = 0; i < verdicts.length; i++) {
    yield openPanel();

    // The current item is always the first one in the listbox since each
    // iteration of this loop removes the item at the end.
    let item = DownloadsView.richListBox.firstChild;

    // Open the panel and click the item to show the subview.
    EventUtils.sendMouseEvent({ type: "click" }, item);
    yield promiseSubviewShown(true);

    // Items are listed in newest-to-oldest order, so e.g. the first item's
    // verdict is the last element in the verdicts array.
    Assert.ok(DownloadsBlockedSubview.subview.getAttribute("verdict"),
              verdicts[verdicts.count - i - 1]);

    // Click the sliver of the main view that's still showing on the left to go
    // back to it.
    EventUtils.synthesizeMouse(DownloadsPanel.panel, 10, 10, {}, window);
    yield promiseSubviewShown(false);

    // Show the subview again.
    EventUtils.sendMouseEvent({ type: "click" }, item);
    yield promiseSubviewShown(true);

    // Click the Open button.  The alert blocked-download dialog should be
    // shown.
    let dialogPromise = promiseAlertDialogOpen("cancel");
    DownloadsBlockedSubview.elements.openButton.click();
    yield dialogPromise;

    window.focus();
    yield SimpleTest.promiseFocus(window);

    // Reopen the panel and show the subview again.
    yield openPanel();

    EventUtils.sendMouseEvent({ type: "click" }, item);
    yield promiseSubviewShown(true);

    // Click the Remove button.  The panel should close and the item should be
    // removed from it.
    DownloadsBlockedSubview.elements.deleteButton.click();
    yield promisePanelHidden();
    yield openPanel();

    Assert.ok(!item.parentNode);
    DownloadsPanel.hidePanel();
    yield promisePanelHidden();
  }

  yield task_resetState();
});

function* openPanel() {
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

  yield promiseFocus();
  yield new Promise(resolve => {
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
      } else {
        if (iBackoff < backoff) {
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
      }
    }, 100);
  });
}

function promisePanelHidden() {
  return new Promise(resolve => {
    if (!DownloadsPanel.panel || DownloadsPanel.panel.state == "closed") {
      resolve();
      return;
    }
    DownloadsPanel.panel.addEventListener("popuphidden", function onHidden() {
      DownloadsPanel.panel.removeEventListener("popuphidden", onHidden);
      setTimeout(resolve, 0);
    });
  });
}

function makeDownload(verdict) {
  return {
    state: nsIDM.DOWNLOAD_DIRTY,
    hasBlockedData: true,
    errorObj: {
      result: Components.results.NS_ERROR_FAILURE,
      message: "Download blocked.",
      becauseBlocked: true,
      becauseBlockedByReputationCheck: true,
      reputationCheckVerdict:  verdict,
    },
  };
}

function promiseSubviewShown(shown) {
  return new Promise(resolve => {
    if (shown == DownloadsBlockedSubview.view.showingSubView) {
      resolve();
      return;
    }
    let event = shown ? "ViewShowing" : "ViewHiding";
    let subview = DownloadsBlockedSubview.subview;
    subview.addEventListener(event, function showing() {
      subview.removeEventListener(event, showing);
      resolve();
    });
  });
}
