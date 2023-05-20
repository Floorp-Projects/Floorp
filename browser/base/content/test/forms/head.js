async function openSelectPopup(
  mode = "key",
  selector = "select",
  win = window
) {
  info("Opening select popup");
  let popupShownPromise = BrowserTestUtils.waitForSelectPopupShown(win);
  if (mode == "click" || mode == "mousedown") {
    let mousePromise;
    if (mode == "click") {
      mousePromise = BrowserTestUtils.synthesizeMouseAtCenter(
        selector,
        {},
        win.gBrowser.selectedBrowser
      );
    } else {
      mousePromise = BrowserTestUtils.synthesizeMouse(
        selector,
        5,
        5,
        { type: "mousedown" },
        win.gBrowser.selectedBrowser
      );
    }
    await mousePromise;
  } else {
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true }, win);
  }
  return popupShownPromise;
}

function hideSelectPopup(mode = "enter", win = window) {
  let browser = win.gBrowser.selectedBrowser;
  let selectClosedPromise = SpecialPowers.spawn(browser, [], async function () {
    let { SelectContentHelper } = ChromeUtils.importESModule(
      "resource://gre/actors/SelectChild.sys.mjs"
    );
    return ContentTaskUtils.waitForCondition(() => !SelectContentHelper.open);
  });

  if (mode == "escape") {
    EventUtils.synthesizeKey("KEY_Escape", {}, win);
  } else if (mode == "enter") {
    EventUtils.synthesizeKey("KEY_Enter", {}, win);
  } else if (mode == "click") {
    let popup = win.document.getElementById("ContentSelectDropdown").menupopup;
    EventUtils.synthesizeMouseAtCenter(popup.lastElementChild, {}, win);
  }

  return selectClosedPromise;
}
