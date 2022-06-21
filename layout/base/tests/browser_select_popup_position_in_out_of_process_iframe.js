Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/gfx/layers/apz/test/mochitest/apz_test_native_event_utils.js",
  this
);

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/base/content/test/forms/head.js",
  this
);

const PAGECONTENT_TRANSLATED =
  "<html><body>" +
  "<div id='div'>" +
  "<iframe id='frame' width='320' height='295' style='margin: 100px;'" +
  "        src='https://example.com/document-builder.sjs?html=<html><select id=select><option>he he he</option><option>boo boo</option><option>baz baz</option></select></html>'" +
  "</iframe>" +
  "</div></body></html>";

function openSelectPopup(x, y, win) {
  const popupShownPromise = BrowserTestUtils.waitForSelectPopupShown(win);
  EventUtils.synthesizeNativeMouseEvent({
    type: "click",
    target: win.document.documentElement,
    screenX: x,
    screenY: y,
  });
  return popupShownPromise;
}

add_task(async function() {
  const pageUrl = "data:text/html," + escape(PAGECONTENT_TRANSLATED);

  const newWin = await BrowserTestUtils.openNewBrowserWindow({ fission: true });

  const browserLoadedPromise = BrowserTestUtils.browserLoaded(
    newWin.gBrowser.selectedBrowser,
    true /* includeSubFrames */
  );
  BrowserTestUtils.loadURI(newWin.gBrowser.selectedBrowser, pageUrl);
  await browserLoadedPromise;

  newWin.gBrowser.selectedBrowser.focus();

  const tab = newWin.gBrowser.selectedTab;

  // We need to explicitly call Element.focus() since dataURL is treated as
  // cross-origin, thus autofocus doesn't work there.
  const iframe = await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    return content.document.querySelector("iframe").browsingContext;
  });

  await SpecialPowers.spawn(iframe, [], async () => {
    const input = content.document.getElementById("select");
    const focusPromise = new Promise(resolve => {
      input.addEventListener("focus", resolve, { once: true });
    });
    input.focus();
    await focusPromise;
  });

  const [iframeBorderLeft, iframeBorderTop] = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    () => {
      const cs = content.window.getComputedStyle(
        content.document.querySelector("iframe")
      );
      return [parseInt(cs.borderLeftWidth), parseInt(cs.borderTopWidth)];
    }
  );

  const [iframeX, iframeY] = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    () => {
      const rect = content.document
        .querySelector("iframe")
        .getBoundingClientRect();
      const x = content.window.mozInnerScreenX + rect.left;
      const y = content.window.mozInnerScreenY + rect.top;
      return [x, y];
    }
  );

  const selectRect = await SpecialPowers.spawn(iframe, [], () => {
    return content.document.querySelector("select").getBoundingClientRect();
  });

  // Open the select popup.
  const selectPopup = await openSelectPopup(
    iframeX + selectRect.x + selectRect.width / 2,
    iframeY + selectRect.y + selectRect.height / 2,
    newWin
  );

  // Check the coordinates of 'selectPopup'.
  let popupRect = selectPopup.getBoundingClientRect();
  is(
    popupRect.x,
    iframeX +
      iframeBorderLeft +
      selectRect.x -
      newWin.mozInnerScreenX +
      parseFloat(getComputedStyle(selectPopup).marginLeft),
    "x position of the popup"
  );

  let expectedYPosition =
    iframeY +
    selectRect.y +
    iframeBorderTop -
    newWin.mozInnerScreenY +
    parseFloat(getComputedStyle(selectPopup).marginTop);
  // On platforms other than MaxOSX the popup menu is positioned below the
  // option element.
  if (!navigator.platform.includes("Mac")) {
    expectedYPosition += selectRect.height;
  }

  isfuzzy(
    popupRect.y,
    expectedYPosition,
    window.devicePixelRatio,
    "y position of the popup"
  );

  await hideSelectPopup("enter", newWin);

  await BrowserTestUtils.closeWindow(newWin);
});
