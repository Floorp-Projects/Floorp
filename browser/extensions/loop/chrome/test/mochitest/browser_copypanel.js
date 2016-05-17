/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gConstants;
LoopAPI.inspect()[1].GetAllConstants({}, constants => (gConstants = constants));

let { goDoCommand, gURLBar } = window;

function cleanUp() {
  cleanupPanel();
  LoopUI.removeCopyPanel();
}
registerCleanupFunction(cleanUp);

/**
 * Cleanup function to allow the loop panel to finish opening then close it.
 */
function waitForLoopPanelShowHide() {
  return new Promise(resolve => {
    let panel = document.getElementById("loop-notification-panel");
    panel.addEventListener("popupshown", function onShow() {
      panel.removeEventListener("popupshown", onShow);
      panel.hidePopup();
    });
    panel.addEventListener("popuphidden", function onHidden() {
      panel.removeEventListener("popuphidden", onHidden);
      resolve();
    });
  });
}

// Even with max threshold, no panel if already shown.
add_task(function* test_init_copy_panel_already_shown() {
  cleanUp();
  Services.prefs.setIntPref("loop.copy.ticket", -1);
  Services.prefs.setBoolPref("loop.copy.shown", true);

  yield LoopUI.maybeAddCopyPanel();

  Assert.equal(document.getElementById("loop-copy-notification-panel"), null, "copy panel doesn't exist for already shown");
  Assert.equal(Services.prefs.getIntPref("loop.copy.ticket"), -1, "ticket should be unchanged");
});

// Even with max threshold, no panel if private browsing.
add_task(function* test_init_copy_panel_private() {
  cleanUp();
  Services.prefs.setIntPref("loop.copy.ticket", -1);

  let win = yield BrowserTestUtils.openNewBrowserWindow({ private: true });
  yield win.LoopUI.maybeAddCopyPanel();

  Assert.equal(win.document.getElementById("loop-copy-notification-panel"), null, "copy panel doesn't exist for private browsing");
  Assert.equal(Services.prefs.getIntPref("loop.copy.ticket"), -1, "ticket should be unchanged");

  yield BrowserTestUtils.closeWindow(win);
});

/**
 * Helper function for testing clicks on the copy panel.
 * @param {String} domain Text to put in the urlbar to assist in debugging.
 * @param {Function} onIframe Callback to interact with iframe contents.
 */
function testClick(domain, onIframe) {
  let histogram = Services.telemetry.getHistogramById("LOOP_COPY_PANEL_ACTIONS");
  histogram.clear();
  cleanUp();
  gURLBar.value = "http://" + domain + "/";

  return new Promise(resolve => {
    // Continue testing when the click has been handled.
    LoopUI.addCopyPanel(detail => resolve([histogram, detail]));
    gURLBar.focus();
    gURLBar.select();
    goDoCommand("cmd_copy");

    // Wait for the panel to completely finish opening.
    let iframe = document.getElementById("loop-copy-panel-iframe");
    iframe.parentNode.addEventListener("popupshown", () => {
      // Continue immediately if the page loaded before the panel opened.
      if (iframe.contentDocument.readyState === "complete") {
        onIframe(iframe);
      }
      // Allow synchronous on-load code to run before we continue.
      else {
        iframe.addEventListener("DOMContentLoaded", () => {
          setTimeout(() => onIframe(iframe));
        });
      }
    });
  });
}

// Show the copy panel on location bar copy.
add_task(function* test_copy_panel_shown() {
  Services.prefs.setIntPref("loop.copy.showLimit", 2);
  let [histogram, detail] = yield testClick("some.site", iframe => {
    iframe.contentWindow.dispatchEvent(new CustomEvent("CopyPanelClick", {
      detail: { test: true }
    }));
  });

  Assert.notEqual(document.getElementById("loop-copy-notification-panel"), null, "copy panel exists on copy");
  Assert.strictEqual(histogram.snapshot().counts[gConstants.COPY_PANEL.SHOWN], 1, "triggered telemetry count for showing");
  Assert.ok(detail.test, "got the special event for testing");
});

// Click the accept button without checkbox.
add_task(function* test_click_yes_again() {
  Services.prefs.setIntPref("loop.copy.showLimit", 2);
  let [histogram] = yield testClick("yes.again", iframe => {
    iframe.contentDocument.querySelector(".copy-button:last-child").click();
  });

  Assert.notEqual(document.getElementById("loop-copy-notification-panel"), null, "copy panel still exists");
  Assert.strictEqual(histogram.snapshot().counts[gConstants.COPY_PANEL.YES_AGAIN], 1, "triggered telemetry count for yes again");
  yield waitForLoopPanelShowHide();
});

// Click the accept button with checkbox.
add_task(function* test_click_yes_never() {
  Services.prefs.setIntPref("loop.copy.showLimit", 2);
  let [histogram] = yield testClick("yes.never", iframe => {
    iframe.contentDocument.querySelector(".copy-toggle-label").click();
    iframe.contentDocument.querySelector(".copy-button:last-child").click();
  });

  Assert.equal(document.getElementById("loop-copy-notification-panel"), null, "copy panel removed");
  Assert.strictEqual(histogram.snapshot().counts[gConstants.COPY_PANEL.YES_NEVER], 1, "triggered telemetry count for yes never");
  yield waitForLoopPanelShowHide();
});

// Click the cancel button without checkbox.
add_task(function* test_click_no_again() {
  Services.prefs.setIntPref("loop.copy.showLimit", 2);
  let [histogram] = yield testClick("no.again", iframe => {
    iframe.contentDocument.querySelector(".copy-button").click();
  });

  Assert.notEqual(document.getElementById("loop-copy-notification-panel"), null, "copy panel still exists");
  Assert.strictEqual(histogram.snapshot().counts[gConstants.COPY_PANEL.NO_AGAIN], 1, "triggered telemetry count for no again");
});

// Click the cancel button with checkbox.
add_task(function* test_click_no_never() {
  Services.prefs.setIntPref("loop.copy.showLimit", 2);
  let [histogram] = yield testClick("no.never", iframe => {
    iframe.contentDocument.querySelector(".copy-toggle-label").click();
    iframe.contentDocument.querySelector(".copy-button").click();
  });

  Assert.equal(document.getElementById("loop-copy-notification-panel"), null, "copy panel removed");
  Assert.strictEqual(histogram.snapshot().counts[gConstants.COPY_PANEL.NO_NEVER], 1, "triggered telemetry count for no never");
});

// Try to trigger copy panel after saying no.
add_task(function* test_click_no_never_retry() {
  Services.prefs.setIntPref("loop.copy.showLimit", 2);
  yield testClick("no.never", iframe => {
    iframe.contentDocument.querySelector(".copy-toggle-label").click();
    iframe.contentDocument.querySelector(".copy-button").click();
  });
  yield LoopUI.maybeAddCopyPanel();

  Assert.equal(document.getElementById("loop-copy-notification-panel"), null, "copy panel stays removed");
});

// Only show the copy panel some number of times.
add_task(function* test_click_no_several() {
  Services.prefs.setIntPref("loop.copy.showLimit", 2);
  let [histogram] = yield testClick("no.several.0", iframe => {
    iframe.contentDocument.querySelector(".copy-button").click();
  });

  Assert.notEqual(document.getElementById("loop-copy-notification-panel"), null, "copy panel still exists");
  Assert.equal(Services.prefs.getIntPref("loop.copy.showLimit"), 1, "decremented show limit");
  Assert.strictEqual(histogram.snapshot().counts[gConstants.COPY_PANEL.SHOWN], 1, "triggered telemetry count for showing");

  yield testClick("no.several.1", iframe => {
    iframe.contentDocument.querySelector(".copy-button").click();
  });

  Assert.notEqual(document.getElementById("loop-copy-notification-panel"), null, "copy panel still exists");
  Assert.equal(Services.prefs.getIntPref("loop.copy.showLimit"), 0, "decremented show limit again");
  Assert.strictEqual(histogram.snapshot().counts[gConstants.COPY_PANEL.SHOWN], 1, "triggered telemetry count for showing again after resetting");

  gURLBar.value = "http://no.several.2/";
  gURLBar.focus();
  gURLBar.select();
  goDoCommand("cmd_copy");

  Assert.equal(document.getElementById("loop-copy-notification-panel"), null, "copy panel removed");
  Assert.equal(Services.prefs.getIntPref("loop.copy.showLimit"), 0, "show limit unchanged");
  Assert.strictEqual(histogram.snapshot().counts[gConstants.COPY_PANEL.SHOWN], 1, "telemetry count for showing unchanged");
});

// Make sure there's no panel if already sharing.
add_task(function* test_already_sharing_no_copy() {
  let histogram = Services.telemetry.getHistogramById("LOOP_COPY_PANEL_ACTIONS");
  histogram.clear();
  cleanUp();
  gURLBar.value = "http://already.sharing/";
  MozLoopService.setScreenShareState("1", true);

  // Continue testing when the click has been handled.
  LoopUI.addCopyPanel();
  gURLBar.focus();
  gURLBar.select();
  goDoCommand("cmd_copy");

  Assert.strictEqual(histogram.snapshot().counts[gConstants.COPY_PANEL.SHOWN], 0, "no triggered telemetry count for not showing");
  MozLoopService.setScreenShareState("1", false);
});
