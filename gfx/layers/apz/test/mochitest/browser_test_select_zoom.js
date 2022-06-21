/* This test is a a mash up of
     https://searchfox.org/mozilla-central/rev/559b25eb41c1cbffcb90a34e008b8288312fcd25/gfx/layers/apz/test/mochitest/browser_test_group_fission.js
     https://searchfox.org/mozilla-central/rev/559b25eb41c1cbffcb90a34e008b8288312fcd25/gfx/layers/apz/test/mochitest/helper_basic_zoom.html
     https://searchfox.org/mozilla-central/rev/559b25eb41c1cbffcb90a34e008b8288312fcd25/browser/base/content/test/forms/browser_selectpopup.js
*/

/* import-globals-from helper_browser_test_utils.js */
Services.scriptloader.loadSubScript(
  new URL("helper_browser_test_utils.js", gTestPath).href,
  this
);

add_task(async function setup_pref() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Dropping the touch slop to 0 makes the tests easier to write because
      // we can just do a one-pixel drag to get over the pan threshold rather
      // than having to hard-code some larger value.
      ["apz.touch_start_tolerance", "0.0"],
      // The subtests in this test do touch-drags to pan the page, but we don't
      // want those pans to turn into fling animations, so we increase the
      // fling-min threshold velocity to an arbitrarily large value.
      ["apz.fling_min_velocity_threshold", "10000"],
    ],
  });
});

// This test opens a select popup after pinch (apz) zooming has happened.
add_task(async function() {
  function httpURL(filename) {
    let chromeURL = getRootDirectory(gTestPath) + filename;
    //return chromeURL;
    return chromeURL.replace(
      "chrome://mochitests/content/",
      "http://mochi.test:8888/"
    );
  }

  const pageUrl = httpURL("helper_test_select_zoom.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    const input = content.document.getElementById("select");
    const focusPromise = new Promise(resolve => {
      input.addEventListener("focus", resolve, { once: true });
    });
    input.focus();
    await focusPromise;
  });

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    await content.wrappedJSObject.waitUntilApzStable();
  });

  const initial_resolution = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    () => {
      return content.window.windowUtils.getResolution();
    }
  );

  const initial_rect = await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    return content.wrappedJSObject.getSelectRect();
  });

  ok(
    initial_resolution > 0,
    "The initial_resolution is " +
      initial_resolution +
      ", which is some sane value"
  );

  // First, get the position of the select popup when no translations have been applied.
  const selectPopup = await openSelectPopup();

  let popup_initial_rect = selectPopup.getBoundingClientRect();
  let popupInitialX = popup_initial_rect.left;
  let popupInitialY = popup_initial_rect.top;

  await hideSelectPopup();

  ok(popupInitialX > 0, "select position before zooming (x) " + popupInitialX);
  ok(popupInitialY > 0, "select position before zooming (y) " + popupInitialY);

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    await content.wrappedJSObject.pinchZoomInWithTouch(150, 300);
  });

  // Flush state and get the resolution we're at now
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    await content.wrappedJSObject.promiseApzFlushedRepaints();
  });

  const final_resolution = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    () => {
      return content.window.windowUtils.getResolution();
    }
  );

  ok(
    final_resolution > initial_resolution,
    "The final resolution (" +
      final_resolution +
      ") is greater after zooming in"
  );

  const final_rect = await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    return content.wrappedJSObject.getSelectRect();
  });

  await openSelectPopup();

  let popupRect = selectPopup.getBoundingClientRect();
  ok(
    Math.abs(popupRect.left - popupInitialX) > 1,
    "popup should have moved by more than one pixel (x) " +
      popupRect.left +
      " " +
      popupInitialX
  );
  ok(
    Math.abs(popupRect.top - popupInitialY) > 1,
    "popup should have moved by more than one pixel (y) " +
      popupRect.top +
      " " +
      popupInitialY
  );

  ok(
    Math.abs(
      final_rect.left - initial_rect.left - (popupRect.left - popupInitialX)
    ) < 1,
    "popup should have moved approximately the same as the element (x)"
  );
  let tolerance = navigator.platform.includes("Linux") ? final_rect.height : 1;
  ok(
    Math.abs(
      final_rect.top - initial_rect.top - (popupRect.top - popupInitialY)
    ) < tolerance,
    "popup should have moved approximately the same as the element (y)"
  );

  ok(
    true,
    "initial " +
      initial_rect.left +
      " " +
      initial_rect.top +
      " " +
      initial_rect.width +
      " " +
      initial_rect.height
  );
  ok(
    true,
    "final " +
      final_rect.left +
      " " +
      final_rect.top +
      " " +
      final_rect.width +
      " " +
      final_rect.height
  );

  ok(
    true,
    "initial popup " +
      popup_initial_rect.left +
      " " +
      popup_initial_rect.top +
      " " +
      popup_initial_rect.width +
      " " +
      popup_initial_rect.height
  );
  ok(
    true,
    "final popup " +
      popupRect.left +
      " " +
      popupRect.top +
      " " +
      popupRect.width +
      " " +
      popupRect.height
  );

  await hideSelectPopup();

  BrowserTestUtils.removeTab(tab);
});
