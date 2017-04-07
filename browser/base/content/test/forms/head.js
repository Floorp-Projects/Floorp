function hideSelectPopup(selectPopup, mode = "enter", win = window) {
  let browser = win.gBrowser.selectedBrowser;
  let selectClosedPromise = ContentTask.spawn(browser, null, function*() {
    Cu.import("resource://gre/modules/SelectContentHelper.jsm");
    return ContentTaskUtils.waitForCondition(() => !SelectContentHelper.open);
  });

  if (mode == "escape") {
    EventUtils.synthesizeKey("KEY_Escape", { code: "Escape" }, win);
  } else if (mode == "enter") {
    EventUtils.synthesizeKey("KEY_Enter", { code: "Enter" }, win);
  } else if (mode == "click") {
    EventUtils.synthesizeMouseAtCenter(selectPopup.lastChild, { }, win);
  }

  return selectClosedPromise;
}
