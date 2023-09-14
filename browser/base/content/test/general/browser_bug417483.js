add_task(async function () {
  let loadedPromise = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    true
  );
  const htmlContent =
    "data:text/html, <iframe src='data:text/html,text text'></iframe>";
  BrowserTestUtils.startLoadingURIString(gBrowser, htmlContent);
  await loadedPromise;

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function (arg) {
    let frame = content.frames[0];
    let sel = frame.getSelection();
    let range = frame.document.createRange();
    let tn = frame.document.body.childNodes[0];
    range.setStart(tn, 4);
    range.setEnd(tn, 5);
    sel.addRange(range);
    frame.focus();
  });

  let contentAreaContextMenu = document.getElementById(
    "contentAreaContextMenu"
  );

  let popupShownPromise = BrowserTestUtils.waitForEvent(
    contentAreaContextMenu,
    "popupshown"
  );
  await BrowserTestUtils.synthesizeMouse(
    "frame",
    5,
    5,
    { type: "contextmenu", button: 2 },
    gBrowser.selectedBrowser
  );
  await popupShownPromise;

  ok(
    document.getElementById("frame-sep").hidden,
    "'frame-sep' should be hidden if the selection contains only spaces"
  );

  let popupHiddenPromise = BrowserTestUtils.waitForEvent(
    contentAreaContextMenu,
    "popuphidden"
  );
  contentAreaContextMenu.hidePopup();
  await popupHiddenPromise;
});
