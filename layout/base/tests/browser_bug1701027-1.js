/* This test is based on
     https://searchfox.org/mozilla-central/rev/e082df56bbfeaff0f388e7da9da401ff414df18f/gfx/layers/apz/test/mochitest/browser_test_select_zoom.js
*/

// In order for this test to test the original bug we need:
// 1) At least e10s enabled so that apz is enabled so we can create an
//    nsDisplayAsyncZoom item
//    (the insertion of this item without marking the required frame modified
//     is what causes the bug in the retained display list merging)
// 2) a root content document, again so that we can create a nsDisplayAsyncZoom
//    item
// 3) the root content document cannot have a display port to start
//    (if it has a display port then it gets a nsDisplayAsyncZoom, but we need
//     that to be created after the anonymous content we insert into the
//     document)
// Point 3) requires the root content document to be in the parent process,
// since if it is in a content process it will get a displayport for being at
// the root of a process.
// Creating an in-process root content document I think is not possible in
// mochitest-plain. mochitest-chrome does not have e10s enabled. So this has to
// be a mochitest-browser-chrome test.

// Outline of this test:
// Open a new tab with a pretty simple content file, that is not scrollable
// Use the anonymous content api to insert into that content doc
// Send a mouse click over the content doc
// The click hits fixed pos content.
// This sets a displayport on the root scroll frame of the content doc.
// (This is because we call GetAsyncScrollableAncestorFrame in
// PrepareForSetTargetAPZCNotification
// https://searchfox.org/mozilla-central/rev/e082df56bbfeaff0f388e7da9da401ff414df18f/gfx/layers/apz/util/APZCCallbackHelper.cpp#624
// which passes the SCROLLABLE_FIXEDPOS_FINDS_ROOT flag
// https://searchfox.org/mozilla-central/rev/e082df56bbfeaff0f388e7da9da401ff414df18f/layout/base/nsLayoutUtils.cpp#2884
// so starting from fixed pos content means we always find the root scroll
// frame, whereas if we started from non-fixed content we'd walk pass the root
// scroll frame becase it isn't scrollable.)
// Then we have to be careful not to do anything that causes a full display
// list rebuild.
// And finally we change the color of the fixed element which covers the whole
// viewport which causes us to do a partial display list update including the
// anonymous content, which hits the assert we are aiming to test.

add_task(async function () {
  function getChromeURL(filename) {
    let chromeURL = getRootDirectory(gTestPath) + filename;
    return chromeURL;
  }

  // We need this otherwise there is a burst animation on the new tab when it
  // loads and that somehow scrolls a scroll frame, which makes it active,
  // which makes the scrolled frame an AGR, which means we have multiple AGRs
  // (the display port makes the root scroll frame active and an AGR) so we hit
  // this
  // https://searchfox.org/mozilla-central/rev/e082df56bbfeaff0f388e7da9da401ff414df18f/layout/painting/RetainedDisplayListBuilder.cpp#1179
  // and are forced to do a full display list rebuild and that prevents us from
  // testing the original bug.
  await SpecialPowers.pushPrefEnv({
    set: [["ui.prefersReducedMotion", 1]],
  });

  const pageUrl = getChromeURL("helper_bug1701027-1.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);

  const [theX, theY] = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    async () => {
      content.document.body.offsetWidth;

      await new Promise(r => content.window.requestAnimationFrame(r));

      const rect = content.document
        .getElementById("fd")
        .getBoundingClientRect();
      const x = content.window.mozInnerScreenX + rect.left + rect.width / 2;
      const y = content.window.mozInnerScreenY + rect.top + rect.height / 2;

      let doc = SpecialPowers.wrap(content.document);
      var bq = doc.createElement("blockquote");
      bq.textContent = "This blockquote text.";
      var div = doc.createElement("div");
      div.textContent = " This div text.";
      bq.appendChild(div);
      var ac = doc.insertAnonymousContent(bq);
      content.document.body.offsetWidth;

      await new Promise(r => content.window.requestAnimationFrame(r));
      await new Promise(r => content.window.requestAnimationFrame(r));

      return [x, y];
    }
  );

  // We intentionally turn off a11y_checks, because the following click
  // is targeting test content that's not meant to be interactive and
  // is not expected to be accessible:
  AccessibilityUtils.setEnv({
    mustHaveAccessibleRule: false,
  });
  EventUtils.synthesizeNativeMouseEvent({
    type: "click",
    target: window.document.documentElement,
    screenX: theX,
    screenY: theY,
  });

  await new Promise(resolve => setTimeout(resolve, 0));
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    await new Promise(r => content.window.requestAnimationFrame(r));
    await new Promise(r => content.window.requestAnimationFrame(r));
  });

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    content.document.getElementById("fd").style.backgroundColor = "blue";
  });

  await new Promise(resolve => setTimeout(resolve, 0));
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    await new Promise(r => content.window.requestAnimationFrame(r));
    await new Promise(r => content.window.requestAnimationFrame(r));
  });

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    content.document.getElementById("fd").style.backgroundColor = "red";
  });

  await new Promise(resolve => setTimeout(resolve, 0));
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    await new Promise(r => content.window.requestAnimationFrame(r));
    await new Promise(r => content.window.requestAnimationFrame(r));
  });

  BrowserTestUtils.removeTab(tab);

  ok(true, "didn't crash");
});
