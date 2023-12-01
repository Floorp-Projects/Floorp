/*
  Test for https://bugzilla.mozilla.org/show_bug.cgi?id=1683776
*/

const EVENTUTILS_URL =
  "chrome://mochikit/content/tests/SimpleTest/EventUtils.js";
var EventUtils = {};

const NATIVEKEYCODES_URL =
  "chrome://mochikit/content/tests/SimpleTest/NativeKeyCodes.js";

const APZNATIVEEVENTUTILS_URL =
  "chrome://mochitests/content/browser/gfx/layers/apz/test/mochitest/apz_test_native_event_utils.js";

Services.scriptloader.loadSubScript(EVENTUTILS_URL, EventUtils);
Services.scriptloader.loadSubScript(NATIVEKEYCODES_URL, this);
Services.scriptloader.loadSubScript(APZNATIVEEVENTUTILS_URL, this);

add_task(async function test_dragging_tabs_event_counts() {
  function httpURL(filename) {
    let chromeURL = getRootDirectory(gTestPath) + filename;
    return chromeURL.replace(
      "chrome://mochitests/content/",
      "http://mochi.test:8888/"
    );
  }

  async function sendKeyEvent(key) {
    await new Promise(resolve => {
      EventUtils.synthesizeNativeKey(
        EventUtils.KEYBOARD_LAYOUT_EN_US,
        key,
        {},
        "",
        "",
        resolve
      );
    });
  }

  const pageUrl = httpURL("helper_test_tab_drag_event_counts.html");

  // Open two windows:
  // window 1 with 1 tab
  // window 2 with 2 tabs
  let win1 = await BrowserTestUtils.openNewBrowserWindow();
  let win2 = await BrowserTestUtils.openNewBrowserWindow();

  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    win1.gBrowser,
    pageUrl
  );
  let tab2 = await BrowserTestUtils.openNewForegroundTab(
    win2.gBrowser,
    pageUrl
  );
  let tab3 = await BrowserTestUtils.openNewForegroundTab(
    win2.gBrowser,
    pageUrl
  );

  await SpecialPowers.spawn(tab1.linkedBrowser, [], async () => {
    await content.wrappedJSObject.waitUntilApzStable();
  });
  await SpecialPowers.spawn(tab2.linkedBrowser, [], async () => {
    await content.wrappedJSObject.waitUntilApzStable();
  });
  await SpecialPowers.spawn(tab3.linkedBrowser, [], async () => {
    await content.wrappedJSObject.waitUntilApzStable();
  });

  // send numerous key events
  for (let i = 0; i < 100; i++) {
    await sendKeyEvent(nativeArrowUpKey());
    await sendKeyEvent(nativeArrowDownKey());
  }

  // drag and drop tab 3 from window 2 to window 1
  let dropPromise = BrowserTestUtils.waitForEvent(
    win1.gBrowser.tabContainer,
    "drop"
  );
  let effect = EventUtils.synthesizeDrop(
    tab3,
    tab1,
    [[{ type: TAB_DROP_TYPE, data: tab3 }]],
    null,
    win2,
    win1
  );
  is(effect, "move", "Tab should be moved from win2 to win1.");
  await dropPromise;

  // focus window 1
  await SimpleTest.promiseFocus(win1);

  // send another key event
  // when the bug occurs, an assertion is triggered when processing this event
  await sendKeyEvent(nativeArrowUpKey());

  await BrowserTestUtils.closeWindow(win1);
  await BrowserTestUtils.closeWindow(win2);

  ok(true);
});
