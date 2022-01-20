/* This test is a a mash up of
     https://searchfox.org/mozilla-central/rev/016925857e2f81a9425de9e03021dcf4251cafcc/gfx/layers/apz/test/mochitest/browser_test_select_zoom.js
     https://searchfox.org/mozilla-central/rev/016925857e2f81a9425de9e03021dcf4251cafcc/browser/base/content/test/general/browser_tab_drag_drop_perwindow.js
*/

const EVENTUTILS_URL =
  "chrome://mochikit/content/tests/SimpleTest/EventUtils.js";
var EventUtils = {};

Services.scriptloader.loadSubScript(EVENTUTILS_URL, EventUtils);

add_task(async function test_dragging_zoom_handling() {
  function httpURL(filename) {
    let chromeURL = getRootDirectory(gTestPath) + filename;
    //return chromeURL;
    return chromeURL.replace(
      "chrome://mochitests/content/",
      "http://mochi.test:8888/"
    );
  }

  const pageUrl = httpURL("helper_test_tab_drag_zoom.html");

  let win1 = await BrowserTestUtils.openNewBrowserWindow();
  let win2 = await BrowserTestUtils.openNewBrowserWindow();

  let tab1 = await BrowserTestUtils.openNewForegroundTab(win1.gBrowser);
  let tab2 = await BrowserTestUtils.openNewForegroundTab(
    win2.gBrowser,
    pageUrl
  );

  await SpecialPowers.spawn(tab2.linkedBrowser, [], async () => {
    await content.wrappedJSObject.waitUntilApzStable();
  });

  const initial_resolution = await SpecialPowers.spawn(
    tab2.linkedBrowser,
    [],
    () => {
      return content.window.windowUtils.getResolution();
    }
  );

  ok(
    initial_resolution > 0,
    "The initial_resolution is " +
      initial_resolution +
      ", which is some sane value"
  );

  let effect = EventUtils.synthesizeDrop(
    tab2,
    tab1,
    [[{ type: TAB_DROP_TYPE, data: tab2 }]],
    null,
    win2,
    win1
  );
  is(effect, "move", "Tab should be moved from win2 to win1.");

  await SpecialPowers.spawn(win1.gBrowser.selectedBrowser, [], async () => {
    await content.wrappedJSObject.waitUntilApzStable();
  });

  let resolution = await SpecialPowers.spawn(
    win1.gBrowser.selectedBrowser,
    [],
    () => {
      return content.window.windowUtils.getResolution();
    }
  );

  ok(
    resolution == initial_resolution,
    "The resolution (" + resolution + ") is the same after tab dragging"
  );

  await SpecialPowers.spawn(win1.gBrowser.selectedBrowser, [], async () => {
    await content.wrappedJSObject.pinchZoomInWithTouch(150, 300);
  });

  // Flush state and get the resolution we're at now
  await SpecialPowers.spawn(win1.gBrowser.selectedBrowser, [], async () => {
    await content.wrappedJSObject.promiseApzFlushedRepaints();
  });

  resolution = await SpecialPowers.spawn(
    win1.gBrowser.selectedBrowser,
    [],
    () => {
      return content.window.windowUtils.getResolution();
    }
  );

  ok(
    resolution > initial_resolution,
    "The resolution (" + resolution + ") is greater after zooming in"
  );

  await BrowserTestUtils.closeWindow(win1);
  await BrowserTestUtils.closeWindow(win2);
});
