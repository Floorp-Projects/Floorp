/* Any copyright is dedicated to the Public Domain.
    http://creativecommons.org/publicdomain/zero/1.0/ */

const URL =
  "data:text/html," +
  "<style> a:hover {background-color: black}</style>" +
  "<body style='width:100px;height:100px'>" +
  "<a href='http://www.example.com'>Click Me</a>" +
  "</body>";

function isAnchorHovered(win) {
  return SpecialPowers.spawn(
    win.gBrowser.selectedBrowser,
    [],
    async function () {
      const a = content.document.querySelector("a");
      return a.matches(":hover");
    }
  );
}

add_task(async function test() {
  let newWin = await BrowserTestUtils.openNewBrowserWindow();

  // This bug is only reproducible if the cursor is out of the viewport, so
  // we resize the window to ensure the cursor is out of the viewport.

  // SynthesizeMouse isn't sufficient because it only synthesizes
  // mouse events without actually moving the cursor permanently to a
  // new location.
  newWin.resizeTo(50, 50);

  BrowserTestUtils.startLoadingURIString(newWin.gBrowser.selectedBrowser, URL);
  await BrowserTestUtils.browserLoaded(newWin.gBrowser.selectedBrowser);

  await SpecialPowers.spawn(
    newWin.gBrowser.selectedBrowser,
    [],
    async function () {
      const a = content.document.querySelector("a");
      await EventUtils.synthesizeMouseAtCenter(
        a,
        { type: "mousemove" },
        content
      );
    }
  );

  // We've hovered the anchor element.
  let anchorHovered = await isAnchorHovered(newWin);
  ok(anchorHovered, "Anchor should be hovered");

  let locationChange = BrowserTestUtils.waitForLocationChange(newWin.gBrowser);

  // Click the anchor to navigate away
  await SpecialPowers.spawn(
    newWin.gBrowser.selectedBrowser,
    [],
    async function () {
      const a = content.document.querySelector("a");
      await EventUtils.synthesizeMouseAtCenter(
        a,
        { type: "mousedown" },
        content
      );
      await EventUtils.synthesizeMouseAtCenter(a, { type: "mouseup" }, content);
    }
  );
  await locationChange;

  // Navigate back to the previous page which has the anchor
  locationChange = BrowserTestUtils.waitForLocationChange(newWin.gBrowser);
  newWin.gBrowser.selectedBrowser.goBack();
  await locationChange;

  // Hover state should be cleared upon page caching.
  anchorHovered = await isAnchorHovered(newWin);
  ok(!anchorHovered, "Anchor should not be hovered");

  BrowserTestUtils.closeWindow(newWin);
});
