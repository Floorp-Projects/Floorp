/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const PAGECONTENT_SMALL = `
  <!doctype html>
  <html>
  <body><select id='one'>
    <option value='One'>One</option>
    <option value='Two'>Two</option>
  </select><select id='two'>
    <option value='Three'>Three</option>
    <option value='Four'>Four</option>
  </select><select id='three'>
    <option value='Five'>Five</option>
    <option value='Six'>Six</option>
  </select></body></html>
`;

async function performLargePopupTests(win) {
  let browser = win.gBrowser.selectedBrowser;

  await SpecialPowers.spawn(browser, [], async function () {
    let doc = content.document;
    let select = doc.getElementById("one");
    for (var i = 0; i < 180; i++) {
      select.add(new content.Option("Test" + i));
    }

    select.options[60].selected = true;
    select.focus();
  });

  // Check if a drag-select works and scrolls the list.
  const selectPopup = await openSelectPopup("mousedown", "select", win);
  const browserRect = browser.getBoundingClientRect();

  let getScrollPos = () => selectPopup.scrollBox.scrollbox.scrollTop;
  let scrollPos = getScrollPos();
  let popupRect = selectPopup.getBoundingClientRect();

  // First, check that scrolling does not occur when the mouse is moved over the
  // anchor button but not the popup yet.
  EventUtils.synthesizeMouseAtPoint(
    popupRect.left + 5,
    popupRect.top - 10,
    {
      type: "mousemove",
      buttons: 1,
    },
    win
  );
  is(
    getScrollPos(),
    scrollPos,
    "scroll position after mousemove over button should not change"
  );

  EventUtils.synthesizeMouseAtPoint(
    popupRect.left + 20,
    popupRect.top + 10,
    {
      type: "mousemove",
      buttons: 1,
    },
    win
  );

  // Dragging above the popup scrolls it up.
  let scrolledPromise = BrowserTestUtils.waitForEvent(
    selectPopup,
    "scroll",
    false,
    () => getScrollPos() < scrollPos - 5
  );
  EventUtils.synthesizeMouseAtPoint(
    popupRect.left + 20,
    popupRect.top - 20,
    {
      type: "mousemove",
      buttons: 1,
    },
    win
  );
  await scrolledPromise;
  ok(true, "scroll position at drag up");

  // Dragging below the popup scrolls it down.
  scrollPos = getScrollPos();
  scrolledPromise = BrowserTestUtils.waitForEvent(
    selectPopup,
    "scroll",
    false,
    () => getScrollPos() > scrollPos + 5
  );
  EventUtils.synthesizeMouseAtPoint(
    popupRect.left + 20,
    popupRect.bottom + 20,
    {
      type: "mousemove",
      buttons: 1,
    },
    win
  );
  await scrolledPromise;
  ok(true, "scroll position at drag down");

  // Releasing the mouse button and moving the mouse does not change the scroll position.
  scrollPos = getScrollPos();
  EventUtils.synthesizeMouseAtPoint(
    popupRect.left + 20,
    popupRect.bottom + 25,
    { type: "mouseup" },
    win
  );
  is(getScrollPos(), scrollPos, "scroll position at mouseup should not change");

  EventUtils.synthesizeMouseAtPoint(
    popupRect.left + 20,
    popupRect.bottom + 20,
    { type: "mousemove" },
    win
  );
  is(
    getScrollPos(),
    scrollPos,
    "scroll position at mousemove after mouseup should not change"
  );

  // Now check dragging with a mousedown on an item
  let menuRect = selectPopup.children[51].getBoundingClientRect();
  EventUtils.synthesizeMouseAtPoint(
    menuRect.left + 5,
    menuRect.top + 5,
    { type: "mousedown" },
    win
  );

  // Dragging below the popup scrolls it down.
  scrolledPromise = BrowserTestUtils.waitForEvent(
    selectPopup,
    "scroll",
    false,
    () => getScrollPos() > scrollPos + 5
  );
  EventUtils.synthesizeMouseAtPoint(
    popupRect.left + 20,
    popupRect.bottom + 20,
    {
      type: "mousemove",
      buttons: 1,
    },
    win
  );
  await scrolledPromise;
  ok(true, "scroll position at drag down from option");

  // Dragging above the popup scrolls it up.
  scrolledPromise = BrowserTestUtils.waitForEvent(
    selectPopup,
    "scroll",
    false,
    () => getScrollPos() < scrollPos - 5
  );
  EventUtils.synthesizeMouseAtPoint(
    popupRect.left + 20,
    popupRect.top - 20,
    {
      type: "mousemove",
      buttons: 1,
    },
    win
  );
  await scrolledPromise;
  ok(true, "scroll position at drag up from option");

  scrollPos = getScrollPos();
  // We intentionally turn off this a11y check, because the following click
  // is sent on an arbitrary web content that is not expected to be tested
  // by itself with the browser mochitests, therefore this rule check shall
  // be ignored by a11y-checks suite.
  AccessibilityUtils.setEnv({ labelRule: false });
  EventUtils.synthesizeMouseAtPoint(
    popupRect.left + 20,
    popupRect.bottom + 25,
    { type: "mouseup" },
    win
  );
  AccessibilityUtils.resetEnv();
  is(
    getScrollPos(),
    scrollPos,
    "scroll position at mouseup from option should not change"
  );

  EventUtils.synthesizeMouseAtPoint(
    popupRect.left + 20,
    popupRect.bottom + 20,
    { type: "mousemove" },
    win
  );
  is(
    getScrollPos(),
    scrollPos,
    "scroll position at mousemove after mouseup should not change"
  );

  await hideSelectPopup("escape", win);

  let positions = [
    "margin-top: 300px;",
    "position: fixed; bottom: 200px;",
    "width: 100%; height: 9999px;",
  ];

  let position;
  while (positions.length) {
    await openSelectPopup("key", "select", win);

    let rect = selectPopup.getBoundingClientRect();
    let marginBottom = parseFloat(getComputedStyle(selectPopup).marginBottom);
    let marginTop = parseFloat(getComputedStyle(selectPopup).marginTop);
    ok(
      rect.top - marginTop >= browserRect.top,
      "Popup top position in within browser area"
    );
    ok(
      rect.bottom + marginBottom <= browserRect.bottom,
      "Popup bottom position in within browser area"
    );

    let cs = win.getComputedStyle(selectPopup);
    let csArrow = win.getComputedStyle(selectPopup.scrollBox);
    let bpBottom =
      parseFloat(cs.paddingBottom) +
      parseFloat(cs.borderBottomWidth) +
      parseFloat(csArrow.paddingBottom) +
      parseFloat(csArrow.borderBottomWidth);
    let selectedOption = 60;

    if (Services.prefs.getBoolPref("dom.forms.selectSearch")) {
      // Use option 61 instead of 60, as the 60th option element is actually the
      // 61st child, since the first child is now the search input field.
      selectedOption = 61;
    }
    // Some of the styles applied to the menuitems are percentages, meaning
    // that the final layout calculations returned by getBoundingClientRect()
    // might return floating point values. We don't care about sub-pixel
    // accuracy, and only care about the final pixel value, so we add a
    // fuzz-factor of 1.
    const fuzzFactor = 1;
    SimpleTest.isfuzzy(
      selectPopup.children[selectedOption].getBoundingClientRect().bottom,
      selectPopup.getBoundingClientRect().bottom - bpBottom + marginBottom,
      fuzzFactor,
      "Popup scroll at correct position " + bpBottom
    );

    await hideSelectPopup("enter", win);

    position = positions.shift();

    let contentPainted = BrowserTestUtils.waitForContentEvent(
      browser,
      "MozAfterPaint"
    );
    await SpecialPowers.spawn(
      browser,
      [position],
      async function (contentPosition) {
        let select = content.document.getElementById("one");
        select.setAttribute("style", contentPosition || "");
        select.getBoundingClientRect();
      }
    );
    await contentPainted;
  }
}

// This test checks select elements with a large number of options to ensure that
// the popup appears within the browser area.
add_task(async function test_large_popup() {
  const pageUrl = "data:text/html," + escape(PAGECONTENT_SMALL);
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);

  await performLargePopupTests(window);

  BrowserTestUtils.removeTab(tab);
});

// This test checks the same as the previous test but in a new, vertically smaller window.
add_task(async function test_large_popup_in_small_window() {
  let newWin = await BrowserTestUtils.openNewBrowserWindow();

  let resizePromise = BrowserTestUtils.waitForEvent(
    newWin,
    "resize",
    false,
    e => {
      info(`Got resize event (innerHeight: ${newWin.innerHeight})`);
      return newWin.innerHeight <= 450;
    }
  );
  newWin.resizeTo(600, 450);
  await resizePromise;

  const pageUrl = "data:text/html," + escape(PAGECONTENT_SMALL);
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    newWin.gBrowser.selectedBrowser
  );
  BrowserTestUtils.startLoadingURIString(
    newWin.gBrowser.selectedBrowser,
    pageUrl
  );
  await browserLoadedPromise;

  newWin.gBrowser.selectedBrowser.focus();

  await performLargePopupTests(newWin);

  await BrowserTestUtils.closeWindow(newWin);
});
