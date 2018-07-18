/* Any copyright is dedicated to the Public Domain.
* http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests for the sub-dialog infrastructure, not for actual sub-dialog functionality.
 */

const gDialogURL = getRootDirectory(gTestPath) + "subdialog.xul";
const gDialogURL2 = getRootDirectory(gTestPath) + "subdialog2.xul";

function open_subdialog_and_test_generic_start_state(browser, domcontentloadedFn, url = gDialogURL) {
  let domcontentloadedFnStr = domcontentloadedFn ?
    "(" + domcontentloadedFn.toString() + ")()" :
    "";
  return ContentTask.spawn(browser, {url, domcontentloadedFnStr}, async function(args) {
    let rv = { acceptCount: 0 };
    let win = content.window;
    content.gSubDialog.open(args.url, null, rv);
    let subdialog = content.gSubDialog._topDialog;

    info("waiting for subdialog DOMFrameContentLoaded");
    let dialogOpenPromise;
    await new Promise(resolve => {
      win.addEventListener("DOMFrameContentLoaded", () => {
        dialogOpenPromise = ContentTaskUtils.waitForEvent(subdialog._overlay, "dialogopen");
        resolve();
      }, { once: true, capture: true });
    });
    let result;
    if (args.domcontentloadedFnStr) {
      // eslint-disable-next-line no-eval
      result = eval(args.domcontentloadedFnStr);
    }

    info("waiting for subdialog load");
    await dialogOpenPromise;
    info("subdialog window is loaded");

    let expectedStyleSheetURLs = subdialog._injectedStyleSheets.slice(0);
    for (let styleSheet of subdialog._frame.contentDocument.styleSheets) {
      let index = expectedStyleSheetURLs.indexOf(styleSheet.href);
      if (index >= 0) {
        expectedStyleSheetURLs.splice(index, 1);
      }
    }

    Assert.ok(!!subdialog._frame.contentWindow, "The dialog should be non-null");
    Assert.notEqual(subdialog._frame.contentWindow.location.toString(), "about:blank",
      "Subdialog URL should not be about:blank");
    Assert.equal(win.getComputedStyle(subdialog._overlay).visibility, "visible",
      "Overlay should be visible");
    Assert.equal(expectedStyleSheetURLs.length, 0,
      "No stylesheets that were expected are missing");
    return result;
  });
}

async function close_subdialog_and_test_generic_end_state(browser, closingFn, closingButton, acceptCount, options) {
  let getDialogsCount = () => {
    return ContentTask.spawn(browser, null, () =>
      content.window.gSubDialog._dialogs.length);
  };
  let getStackChildrenCount = () => {
    return ContentTask.spawn(browser, null, () =>
      content.window.gSubDialog._dialogStack.children.length);
  };
  let dialogclosingPromise = ContentTask.spawn(browser, {closingButton, acceptCount}, async function(expectations) {
    let win = content.window;
    let subdialog = win.gSubDialog._topDialog;
    let frame = subdialog._frame;
    info("waiting for dialogclosing");
    let closingEvent =
      await ContentTaskUtils.waitForEvent(frame.contentWindow, "dialogclosing");
    let contentClosingButton = closingEvent.detail.button;
    let actualAcceptCount = frame.contentWindow.arguments &&
                            frame.contentWindow.arguments[0].acceptCount;

    info("waiting for about:blank load");
    await ContentTaskUtils.waitForEvent(frame, "load", false, () => {
      return frame.contentWindow.location.href == "about:blank";
    });

    Assert.notEqual(win.getComputedStyle(subdialog._overlay).visibility, "visible",
      "overlay is not visible");
    Assert.equal(frame.getAttribute("style"), "", "inline styles should be cleared");
    Assert.equal(frame.contentWindow.location.href.toString(), "about:blank",
      "sub-dialog should be unloaded");
    Assert.equal(contentClosingButton, expectations.closingButton,
      "closing event should indicate button was '" + expectations.closingButton + "'");
    Assert.equal(actualAcceptCount, expectations.acceptCount,
      "should be 1 if accepted, 0 if canceled, undefined if closed w/out button");
  });
  let initialDialogsCount = await getDialogsCount();
  let initialStackChildrenCount = await getStackChildrenCount();
  if (options && options.runClosingFnOutsideOfContentTask) {
    await closingFn();
  } else {
    ContentTask.spawn(browser, null, closingFn);
  }

  await dialogclosingPromise;
  let endDialogsCount = await getDialogsCount();
  let endStackChildrenCount = await getStackChildrenCount();
  Assert.equal(initialDialogsCount - 1, endDialogsCount,
    "dialog count should decrease by 1");
  Assert.equal(initialStackChildrenCount - 1, endStackChildrenCount,
    "stack children count should decrease by 1");
}

let tab;

add_task(async function test_initialize() {
  tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:preferences");
});

add_task(async function check_titlebar_focus_returnval_titlechanges_accepting() {
  await open_subdialog_and_test_generic_start_state(tab.linkedBrowser);

  let domtitlechangedPromise = BrowserTestUtils.waitForEvent(tab.linkedBrowser, "DOMTitleChanged");
  await ContentTask.spawn(tab.linkedBrowser, null, async function() {
    let dialog = content.window.gSubDialog._topDialog;
    let dialogWin = dialog._frame.contentWindow;
    let dialogTitleElement = dialog._titleElement;
    Assert.equal(dialogTitleElement.textContent, "Sample sub-dialog",
       "Title should be correct initially");
    Assert.equal(dialogWin.document.activeElement.value, "Default text",
       "Textbox with correct text is focused");
    dialogWin.document.title = "Updated title";
  });

  info("waiting for DOMTitleChanged event");
  await domtitlechangedPromise;

  ContentTask.spawn(tab.linkedBrowser, null, async function() {
    let dialogTitleElement = content.window.gSubDialog._topDialog._titleElement;
    Assert.equal(dialogTitleElement.textContent, "Updated title",
      "subdialog should have updated title");
  });

  // Accept the dialog
  await close_subdialog_and_test_generic_end_state(tab.linkedBrowser,
    function() { content.window.gSubDialog._topDialog._frame.contentDocument.documentElement.acceptDialog(); },
    "accept", 1);
});

add_task(async function check_canceling_dialog() {
  await open_subdialog_and_test_generic_start_state(tab.linkedBrowser);

  info("canceling the dialog");
  await close_subdialog_and_test_generic_end_state(tab.linkedBrowser,
    function() { content.window.gSubDialog._topDialog._frame.contentDocument.documentElement.cancelDialog(); },
    "cancel", 0);
});

add_task(async function check_reopening_dialog() {
  await open_subdialog_and_test_generic_start_state(tab.linkedBrowser);
  info("opening another dialog which will close the first");
  await open_subdialog_and_test_generic_start_state(tab.linkedBrowser, "", gDialogURL2);

  ContentTask.spawn(tab.linkedBrowser, null, async function() {
    let win = content.window;
    let dialogs = win.gSubDialog._dialogs;
    let lowerDialog = dialogs[0];
    let topDialog = dialogs[1];
    Assert.equal(dialogs.length, 2, "There should be two visible dialogs");
    Assert.equal(win.getComputedStyle(topDialog._overlay).visibility, "visible",
      "The top dialog should be visible");
    Assert.equal(win.getComputedStyle(lowerDialog._overlay).visibility, "visible",
      "The lower dialog should be visible");
    Assert.equal(win.getComputedStyle(topDialog._overlay).backgroundColor, "rgba(0, 0, 0, 0.5)",
      "The top dialog should have a semi-transparent overlay");
    Assert.equal(win.getComputedStyle(lowerDialog._overlay).backgroundColor, "rgba(0, 0, 0, 0)",
      "The lower dialog should not have an overlay");
  });

  info("closing two dialogs");
  await close_subdialog_and_test_generic_end_state(tab.linkedBrowser,
    function() { content.window.gSubDialog._topDialog._frame.contentDocument.documentElement.acceptDialog(); },
    "accept", 1);
  await close_subdialog_and_test_generic_end_state(tab.linkedBrowser,
    function() { content.window.gSubDialog._topDialog._frame.contentDocument.documentElement.acceptDialog(); },
    "accept", 1);
});

add_task(async function check_opening_while_closing() {
  await open_subdialog_and_test_generic_start_state(tab.linkedBrowser);
  info("closing");
  content.window.gSubDialog._topDialog.close();
  info("reopening immediately after calling .close()");
  await open_subdialog_and_test_generic_start_state(tab.linkedBrowser);
  await close_subdialog_and_test_generic_end_state(tab.linkedBrowser,
    function() { content.window.gSubDialog._topDialog._frame.contentDocument.documentElement.acceptDialog(); },
    "accept", 1);

});

add_task(async function window_close_on_dialog() {
  await open_subdialog_and_test_generic_start_state(tab.linkedBrowser);

  info("canceling the dialog");
  await close_subdialog_and_test_generic_end_state(tab.linkedBrowser,
    function() { content.window.gSubDialog._topDialog._frame.contentWindow.window.close(); },
    null, 0);
});

add_task(async function click_close_button_on_dialog() {
  await open_subdialog_and_test_generic_start_state(tab.linkedBrowser);

  info("canceling the dialog");
  await close_subdialog_and_test_generic_end_state(tab.linkedBrowser,
    function() { return BrowserTestUtils.synthesizeMouseAtCenter(".dialogClose", {}, tab.linkedBrowser); },
    null, 0, {runClosingFnOutsideOfContentTask: true});
});

add_task(async function background_click_should_close_dialog() {
  await open_subdialog_and_test_generic_start_state(tab.linkedBrowser);

  // Clicking on an inactive part of dialog itself should not close the dialog.
  // Click the dialog title bar here to make sure nothing happens.
  info("clicking the dialog title bar");
  BrowserTestUtils.synthesizeMouseAtCenter(".dialogTitle", {}, tab.linkedBrowser);

  // Close the dialog by clicking on the overlay background. Simulate a click
  // at point (2,2) instead of (0,0) so we are sure we're clicking on the
  // overlay background instead of some boundary condition that a real user
  // would never click.
  info("clicking the overlay background");
  await close_subdialog_and_test_generic_end_state(tab.linkedBrowser,
    function() { return BrowserTestUtils.synthesizeMouseAtPoint(2, 2, {}, tab.linkedBrowser); },
    null, 0, {runClosingFnOutsideOfContentTask: true});
});

add_task(async function back_navigation_on_subdialog_should_close_dialog() {
  await open_subdialog_and_test_generic_start_state(tab.linkedBrowser);

  info("canceling the dialog");
  await close_subdialog_and_test_generic_end_state(tab.linkedBrowser,
    function() { content.window.gSubDialog._topDialog._frame.goBack(); },
    null, undefined);
});

add_task(async function back_navigation_on_browser_tab_should_close_dialog() {
  await open_subdialog_and_test_generic_start_state(tab.linkedBrowser);

  info("canceling the dialog");
  await close_subdialog_and_test_generic_end_state(tab.linkedBrowser,
    function() { tab.linkedBrowser.goBack(); },
    null, undefined, {runClosingFnOutsideOfContentTask: true});
});

add_task(async function escape_should_close_dialog() {
  await open_subdialog_and_test_generic_start_state(tab.linkedBrowser);

  info("canceling the dialog");
  await close_subdialog_and_test_generic_end_state(tab.linkedBrowser,
    function() { return BrowserTestUtils.synthesizeKey("VK_ESCAPE", {}, tab.linkedBrowser); },
    "cancel", 0, {runClosingFnOutsideOfContentTask: true});
});

add_task(async function correct_width_and_height_should_be_used_for_dialog() {
  await open_subdialog_and_test_generic_start_state(tab.linkedBrowser);

  await ContentTask.spawn(tab.linkedBrowser, null, async function() {
    let frameStyle = content.window.gSubDialog._topDialog._frame.style;
    Assert.equal(frameStyle.width, "32em",
      "Width should be set on the frame from the dialog");
    Assert.equal(frameStyle.height, "5em",
      "Height should be set on the frame from the dialog");
  });

  await close_subdialog_and_test_generic_end_state(tab.linkedBrowser,
    function() { content.window.gSubDialog._topDialog._frame.contentWindow.window.close(); },
    null, 0);
});

add_task(async function wrapped_text_in_dialog_should_have_expected_scrollHeight() {
  let oldHeight = await open_subdialog_and_test_generic_start_state(tab.linkedBrowser, function domcontentloadedFn() {
    let frame = content.window.gSubDialog._topDialog._frame;
    let doc = frame.contentDocument;
    let scrollHeight = doc.documentElement.scrollHeight;
    doc.documentElement.style.removeProperty("height");
    doc.getElementById("desc").textContent = `
      Sed ut perspiciatis unde omnis iste natus error sit voluptatem accusantium doloremque
      laudantium, totam rem aperiam, eaque ipsa quae ab illo inventore veritatis et quasi
      architecto beatae vitae dicta sunt explicabo. Nemo enim ipsam voluptatem quia voluptas
      sit aspernatur aut odit aut fugit, sed quia consequuntur magni dolores eos qui ratione
      laudantium, totam rem aperiam, eaque ipsa quae ab illo inventore veritatis et quasi
      architecto beatae vitae dicta sunt explicabo. Nemo enim ipsam voluptatem quia voluptas
      sit aspernatur aut odit aut fugit, sed quia consequuntur magni dolores eos qui ratione
      laudantium, totam rem aperiam, eaque ipsa quae ab illo inventore veritatis et quasi
      architecto beatae vitae dicta sunt explicabo. Nemo enim ipsam voluptatem quia voluptas
      sit aspernatur aut odit aut fugit, sed quia consequuntur magni dolores eos qui ratione
      voluptatem sequi nesciunt.`;
    return scrollHeight;
  });

  await ContentTask.spawn(tab.linkedBrowser, oldHeight, async function(contentOldHeight) {
    let frame = content.window.gSubDialog._topDialog._frame;
    let docEl = frame.contentDocument.documentElement;
    Assert.equal(frame.style.width, "32em",
      "Width should be set on the frame from the dialog");
    Assert.ok(docEl.scrollHeight > contentOldHeight,
      "Content height increased (from " + contentOldHeight + " to " + docEl.scrollHeight + ").");
    Assert.equal(frame.style.height, docEl.scrollHeight + "px",
      "Height on the frame should be higher now. " +
      "This test may fail on certain screen resoluition. " +
      "See bug 1420576 and bug 1205717.");
  });

  await close_subdialog_and_test_generic_end_state(tab.linkedBrowser,
    function() { content.window.gSubDialog._topDialog._frame.contentWindow.window.close(); },
    null, 0);
});

add_task(async function dialog_too_tall_should_get_reduced_in_height() {
  await open_subdialog_and_test_generic_start_state(tab.linkedBrowser, function domcontentloadedFn() {
    let frame = content.window.gSubDialog._topDialog._frame;
    frame.contentDocument.documentElement.style.height = "100000px";
  });

  await ContentTask.spawn(tab.linkedBrowser, null, async function() {
    let frame = content.window.gSubDialog._topDialog._frame;
    Assert.equal(frame.style.width, "32em", "Width should be set on the frame from the dialog");
    Assert.ok(parseInt(frame.style.height, 10) < content.window.innerHeight,
       "Height on the frame should be smaller than window's innerHeight");
  });

  await close_subdialog_and_test_generic_end_state(tab.linkedBrowser,
    function() { content.window.gSubDialog._topDialog._frame.contentWindow.window.close(); },
    null, 0);
});

add_task(async function scrollWidth_and_scrollHeight_from_subdialog_should_size_the_browser() {
  await open_subdialog_and_test_generic_start_state(tab.linkedBrowser, function domcontentloadedFn() {
    let frame = content.window.gSubDialog._topDialog._frame;
    frame.contentDocument.documentElement.style.removeProperty("height");
    frame.contentDocument.documentElement.style.removeProperty("width");
  });

  await ContentTask.spawn(tab.linkedBrowser, null, async function() {
    let frame = content.window.gSubDialog._topDialog._frame;
    Assert.ok(frame.style.width.endsWith("px"),
       "Width (" + frame.style.width + ") should be set to a px value of the scrollWidth from the dialog");
    Assert.ok(frame.style.height.endsWith("px"),
       "Height (" + frame.style.height + ") should be set to a px value of the scrollHeight from the dialog");
  });

  await close_subdialog_and_test_generic_end_state(tab.linkedBrowser,
    function() { content.window.gSubDialog._topDialog._frame.contentWindow.window.close(); },
    null, 0);
});

add_task(async function test_shutdown() {
  gBrowser.removeTab(tab);
});
