/* Any copyright is dedicated to the Public Domain.
* http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests for the sub-dialog infrastructure, not for actual sub-dialog functionality.
 */

const gDialogURL = getRootDirectory(gTestPath) + "subdialog.xul";
const gDialogURL2 = getRootDirectory(gTestPath) + "subdialog2.xul";

function* open_subdialog_and_test_generic_start_state(browser, domcontentloadedFn, url = gDialogURL) {
  let domcontentloadedFnStr = domcontentloadedFn ?
    "(" + domcontentloadedFn.toString() + ")()" :
    "";
  return ContentTask.spawn(browser, {url, domcontentloadedFnStr}, function*(args) {
    let {url, domcontentloadedFnStr} = args;
    let rv = { acceptCount: 0 };
    let win = content.window;
    let subdialog = win.gSubDialog;
    subdialog.open(url, null, rv);

    info("waiting for subdialog DOMFrameContentLoaded");
    yield ContentTaskUtils.waitForEvent(win, "DOMFrameContentLoaded", true);
    let result;
    if (domcontentloadedFnStr) {
      result = eval(domcontentloadedFnStr);
    }

    info("waiting for subdialog load");
    yield ContentTaskUtils.waitForEvent(subdialog._frame, "load");
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
    Assert.equal(win.getComputedStyle(subdialog._overlay, "").visibility, "visible",
      "Overlay should be visible");
    Assert.equal(expectedStyleSheetURLs.length, 0,
      "No stylesheets that were expected are missing");
    return result;
  });
}

function* close_subdialog_and_test_generic_end_state(browser, closingFn, closingButton, acceptCount, options) {
  let dialogclosingPromise = ContentTask.spawn(browser, {closingButton, acceptCount}, function*(expectations) {
    let win = content.window;
    let subdialog = win.gSubDialog;
    let frame = subdialog._frame;
    info("waiting for dialogclosing");
    let closingEvent =
      yield ContentTaskUtils.waitForEvent(frame.contentWindow, "dialogclosing");
    let closingButton = closingEvent.detail.button;
    let actualAcceptCount = frame.contentWindow.arguments &&
                            frame.contentWindow.arguments[0].acceptCount;

    info("waiting for about:blank load");
    yield ContentTaskUtils.waitForEvent(frame, "load");

    Assert.notEqual(win.getComputedStyle(subdialog._overlay, "").visibility, "visible",
      "overlay is not visible");
    Assert.equal(frame.getAttribute("style"), "", "inline styles should be cleared");
    Assert.equal(frame.contentWindow.location.href.toString(), "about:blank",
      "sub-dialog should be unloaded");
    Assert.equal(closingButton, expectations.closingButton,
      "closing event should indicate button was '" + expectations.closingButton + "'");
    Assert.equal(actualAcceptCount, expectations.acceptCount,
      "should be 1 if accepted, 0 if canceled, undefined if closed w/out button");
  });

  if (options && options.runClosingFnOutsideOfContentTask) {
    yield closingFn();
  } else {
    ContentTask.spawn(browser, null, closingFn);
  }

  yield dialogclosingPromise;
}

let tab;

add_task(function* test_initialize() {
  tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:preferences");
});

add_task(function* check_titlebar_focus_returnval_titlechanges_accepting() {
  yield open_subdialog_and_test_generic_start_state(tab.linkedBrowser);

  let domtitlechangedPromise = BrowserTestUtils.waitForEvent(tab.linkedBrowser, "DOMTitleChanged");
  yield ContentTask.spawn(tab.linkedBrowser, null, function*() {
    let dialog = content.window.gSubDialog._frame.contentWindow;
    let dialogTitleElement = content.document.getElementById("dialogTitle");
    Assert.equal(dialogTitleElement.textContent, "Sample sub-dialog",
       "Title should be correct initially");
    Assert.equal(dialog.document.activeElement.value, "Default text",
       "Textbox with correct text is focused");
    dialog.document.title = "Updated title";
  });

  info("waiting for DOMTitleChanged event");
  yield domtitlechangedPromise;

  ContentTask.spawn(tab.linkedBrowser, null, function*() {
    let dialogTitleElement = content.document.getElementById("dialogTitle");
    Assert.equal(dialogTitleElement.textContent, "Updated title",
      "subdialog should have updated title");
  });

  // Accept the dialog
  yield close_subdialog_and_test_generic_end_state(tab.linkedBrowser,
    function() { content.window.gSubDialog._frame.contentDocument.documentElement.acceptDialog(); },
    "accept", 1);
});

add_task(function* check_canceling_dialog() {
  yield open_subdialog_and_test_generic_start_state(tab.linkedBrowser);

  info("canceling the dialog");
  yield close_subdialog_and_test_generic_end_state(tab.linkedBrowser,
    function() { content.window.gSubDialog._frame.contentDocument.documentElement.cancelDialog(); },
    "cancel", 0);
});

add_task(function* check_reopening_dialog() {
  yield open_subdialog_and_test_generic_start_state(tab.linkedBrowser);
  info("opening another dialog which will close the first");
  yield open_subdialog_and_test_generic_start_state(tab.linkedBrowser, "", gDialogURL2);
  info("closing as normal");
  yield close_subdialog_and_test_generic_end_state(tab.linkedBrowser,
    function() { content.window.gSubDialog._frame.contentDocument.documentElement.acceptDialog(); },
    "accept", 1);
});

add_task(function* check_opening_while_closing() {
  yield open_subdialog_and_test_generic_start_state(tab.linkedBrowser);
  info("closing");
  content.window.gSubDialog.close();
  info("reopening immediately after calling .close()");
  yield open_subdialog_and_test_generic_start_state(tab.linkedBrowser);
  yield close_subdialog_and_test_generic_end_state(tab.linkedBrowser,
    function() { content.window.gSubDialog._frame.contentDocument.documentElement.acceptDialog(); },
    "accept", 1);

});

add_task(function* window_close_on_dialog() {
  yield open_subdialog_and_test_generic_start_state(tab.linkedBrowser);

  info("canceling the dialog");
  yield close_subdialog_and_test_generic_end_state(tab.linkedBrowser,
    function() { content.window.gSubDialog._frame.contentWindow.window.close(); },
    null, 0);
});

add_task(function* click_close_button_on_dialog() {
  yield open_subdialog_and_test_generic_start_state(tab.linkedBrowser);

  info("canceling the dialog");
  yield close_subdialog_and_test_generic_end_state(tab.linkedBrowser,
    function() { return BrowserTestUtils.synthesizeMouseAtCenter("#dialogClose", {}, tab.linkedBrowser); },
    null, 0, {runClosingFnOutsideOfContentTask: true});
});

add_task(function* back_navigation_on_subdialog_should_close_dialog() {
  yield open_subdialog_and_test_generic_start_state(tab.linkedBrowser);

  info("canceling the dialog");
  yield close_subdialog_and_test_generic_end_state(tab.linkedBrowser,
    function() { content.window.gSubDialog._frame.goBack(); },
    null, undefined);
});

add_task(function* back_navigation_on_browser_tab_should_close_dialog() {
  yield open_subdialog_and_test_generic_start_state(tab.linkedBrowser);

  info("canceling the dialog");
  yield close_subdialog_and_test_generic_end_state(tab.linkedBrowser,
    function() { tab.linkedBrowser.goBack(); },
    null, undefined, {runClosingFnOutsideOfContentTask: true});
});

add_task(function* escape_should_close_dialog() {
  yield open_subdialog_and_test_generic_start_state(tab.linkedBrowser);

  info("canceling the dialog");
  yield close_subdialog_and_test_generic_end_state(tab.linkedBrowser,
    function() { return BrowserTestUtils.synthesizeKey("VK_ESCAPE", {}, tab.linkedBrowser); },
    "cancel", 0, {runClosingFnOutsideOfContentTask: true});
});

add_task(function* correct_width_and_height_should_be_used_for_dialog() {
  yield open_subdialog_and_test_generic_start_state(tab.linkedBrowser);

  yield ContentTask.spawn(tab.linkedBrowser, null, function*() {
    let frameStyle = content.window.gSubDialog._frame.style;
    Assert.equal(frameStyle.width, "32em",
      "Width should be set on the frame from the dialog");
    Assert.equal(frameStyle.height, "5em",
      "Height should be set on the frame from the dialog");
  });

  yield close_subdialog_and_test_generic_end_state(tab.linkedBrowser,
    function() { content.window.gSubDialog._frame.contentWindow.window.close(); },
    null, 0);
});

add_task(function* wrapped_text_in_dialog_should_have_expected_scrollHeight() {
  let oldHeight = yield open_subdialog_and_test_generic_start_state(tab.linkedBrowser, function domcontentloadedFn() {
    let frame = content.window.gSubDialog._frame;
    let doc = frame.contentDocument;
    let oldHeight = doc.documentElement.scrollHeight;
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
      voluptatem sequi nesciunt.`
    return oldHeight;
  });

  yield ContentTask.spawn(tab.linkedBrowser, oldHeight, function*(oldHeight) {
    let frame = content.window.gSubDialog._frame;
    let docEl = frame.contentDocument.documentElement;
    Assert.equal(frame.style.width, "32em",
      "Width should be set on the frame from the dialog");
    Assert.ok(docEl.scrollHeight > oldHeight,
      "Content height increased (from " + oldHeight + " to " + docEl.scrollHeight + ").");
    Assert.equal(frame.style.height, docEl.scrollHeight + "px",
      "Height on the frame should be higher now");
  });

  yield close_subdialog_and_test_generic_end_state(tab.linkedBrowser,
    function() { content.window.gSubDialog._frame.contentWindow.window.close(); },
    null, 0);
});

add_task(function* dialog_too_tall_should_get_reduced_in_height() {
  yield open_subdialog_and_test_generic_start_state(tab.linkedBrowser, function domcontentloadedFn() {
    let frame = content.window.gSubDialog._frame;
    frame.contentDocument.documentElement.style.height = '100000px';
  });

  yield ContentTask.spawn(tab.linkedBrowser, null, function*() {
    let frame = content.window.gSubDialog._frame;
    Assert.equal(frame.style.width, "32em", "Width should be set on the frame from the dialog");
    Assert.ok(parseInt(frame.style.height, 10) < content.window.innerHeight,
       "Height on the frame should be smaller than window's innerHeight");
  });

  yield close_subdialog_and_test_generic_end_state(tab.linkedBrowser,
    function() { content.window.gSubDialog._frame.contentWindow.window.close(); },
    null, 0);
});

add_task(function* scrollWidth_and_scrollHeight_from_subdialog_should_size_the_browser() {
  yield open_subdialog_and_test_generic_start_state(tab.linkedBrowser, function domcontentloadedFn() {
    let frame = content.window.gSubDialog._frame;
    frame.contentDocument.documentElement.style.removeProperty("height");
    frame.contentDocument.documentElement.style.removeProperty("width");
  });

  yield ContentTask.spawn(tab.linkedBrowser, null, function*() {
    let frame = content.window.gSubDialog._frame;
    Assert.ok(frame.style.width.endsWith("px"),
       "Width (" + frame.style.width + ") should be set to a px value of the scrollWidth from the dialog");
    Assert.ok(frame.style.height.endsWith("px"),
       "Height (" + frame.style.height + ") should be set to a px value of the scrollHeight from the dialog");
  });

  yield close_subdialog_and_test_generic_end_state(tab.linkedBrowser,
    function() { content.window.gSubDialog._frame.contentWindow.window.close(); },
    null, 0);
});

add_task(function* test_shutdown() {
  gBrowser.removeTab(tab);
});
