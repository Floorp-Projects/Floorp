add_task(function* () {
  const url = "data:text/html,<html><head></head><body>" +
    "<a id=\"target\" href=\"about:blank\" title=\"This is tooltip text\" " +
            "style=\"display:block;height:20px;margin:10px;\" " +
            "onclick=\"return false;\">here is an anchor element</a></body></html>";

  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  let browser = gBrowser.selectedBrowser;

  yield new Promise(resolve => {
    SpecialPowers.pushPrefEnv({"set": [["ui.tooltipDelay", 0]]}, resolve);
  });

  // Send a mousemove at a known position to start the test.
  yield BrowserTestUtils.synthesizeMouse("#target", -5, -5, { type: "mousemove" }, browser);

  // show tooltip by mousemove into target.
  let popupShownPromise = BrowserTestUtils.waitForEvent(document, "popupshown");
  yield BrowserTestUtils.synthesizeMouse("#target", 5, 15, { type: "mousemove" }, browser);
  yield popupShownPromise;

  // hide tooltip by mousemove to outside.
  let popupHiddenPromise = BrowserTestUtils.waitForEvent(document, "popuphidden");
  yield BrowserTestUtils.synthesizeMouse("#target", -5, 15, { type: "mousemove" }, browser);
  yield popupHiddenPromise;

  // mousemove into the target and start drag by emulation via nsIDragService.
  // Note that on some platforms, we cannot actually start the drag by
  // synthesized events.  E.g., Windows waits an actual mousemove event after
  // dragstart.

  // Emulate a buggy mousemove event.  widget might dispatch mousemove event
  // during drag.

  function tooltipNotExpected()
  {
    ok(false, "tooltip is shown during drag");
  }
  addEventListener("popupshown", tooltipNotExpected, true);

  let dragService = Components.classes["@mozilla.org/widget/dragservice;1"].
                      getService(Components.interfaces.nsIDragService);
  dragService.startDragSession();
  yield BrowserTestUtils.synthesizeMouse("#target", 5, 15, { type: "mousemove" }, browser);

  yield new Promise(resolve => setTimeout(resolve, 100));
  removeEventListener("popupshown", tooltipNotExpected, true);
  dragService.endDragSession(true);

  yield BrowserTestUtils.synthesizeMouse("#target", -5, -5, { type: "mousemove" }, browser);

  // If tooltip listener used a flag for managing D&D state, we would need
  // to test if the tooltip is shown after drag.

  // show tooltip by mousemove into target.
  popupShownPromise = BrowserTestUtils.waitForEvent(document, "popupshown");
  yield BrowserTestUtils.synthesizeMouse("#target", 5, 15, { type: "mousemove" }, browser);
  yield popupShownPromise;

  // hide tooltip by mousemove to outside.
  popupHiddenPromise = BrowserTestUtils.waitForEvent(document, "popuphidden");
  yield BrowserTestUtils.synthesizeMouse("#target", -5, 15, { type: "mousemove" }, browser);
  yield popupHiddenPromise;

  // Show tooltip after mousedown
  popupShownPromise = BrowserTestUtils.waitForEvent(document, "popupshown");
  yield BrowserTestUtils.synthesizeMouse("#target", 5, 15, { type: "mousemove" }, browser);
  yield popupShownPromise;

  popupHiddenPromise = BrowserTestUtils.waitForEvent(document, "popuphidden");
  yield BrowserTestUtils.synthesizeMouse("#target", 5, 15, { type: "mousedown" }, browser);
  yield popupHiddenPromise;

  yield BrowserTestUtils.synthesizeMouse("#target", 5, 15, { type: "mouseup" }, browser);
  yield BrowserTestUtils.synthesizeMouse("#target", -5, 15, { type: "mousemove" }, browser);

  ok(true, "tooltips appear properly");

  gBrowser.removeCurrentTab();
});




