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

function synthesizeNativeMouseClick(aWin, aScreenX, aScreenY) {
  const utils = SpecialPowers.getDOMWindowUtils(aWin);

  utils.sendNativeMouseEvent(
    aScreenX,
    aScreenY,
    nativeMouseDownEventMsg(),
    0,
    aWin.document.documentElement,
    () => {
      utils.sendNativeMouseEvent(
        aScreenX,
        aScreenY,
        nativeMouseUpEventMsg(),
        0,
        aWin.document.documentElement
      );
    }
  );
}

function openSelectPopup(selectPopup, x, y, win) {
  const popupShownPromise = BrowserTestUtils.waitForEvent(
    selectPopup,
    "popupshown"
  );

  synthesizeNativeMouseClick(win, x, y);

  return popupShownPromise;
}

add_task(async function() {
  const pageUrl = "data:text/html," + escape(PAGECONTENT_TRANSLATED);

  const newWin = await BrowserTestUtils.openNewBrowserWindow({ fission: true });

  const browserLoadedPromise = BrowserTestUtils.browserLoaded(
    newWin.gBrowser.selectedBrowser
  );
  await BrowserTestUtils.loadURI(newWin.gBrowser.selectedBrowser, pageUrl);
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

  const menulist = newWin.document.getElementById("ContentSelectDropdown");
  const selectPopup = menulist.menupopup;

  // Open the select popup.
  await openSelectPopup(
    selectPopup,
    iframeX + selectRect.x + selectRect.width / 2,
    iframeY + selectRect.y + selectRect.height / 2,
    newWin
  );

  // Check the coordinates of 'selectPopup'.
  let popupRect = selectPopup.getBoundingClientRect();
  is(
    popupRect.x,
    iframeX + iframeBorderLeft + selectRect.x - newWin.mozInnerScreenX,
    "x position of the popup"
  );
  is(
    popupRect.y,
    iframeY +
      selectRect.y +
      iframeBorderTop +
      selectRect.height -
      newWin.mozInnerScreenY,
    "y position of the popup"
  );

  await hideSelectPopup(selectPopup, "enter", newWin);

  await BrowserTestUtils.closeWindow(newWin);
});
