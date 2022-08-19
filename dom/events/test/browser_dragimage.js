const TEST_URI = "dragimage.html";

// This test checks that dragging an image onto the same document
// does not drop it, even when the page cancels the dragover event.
add_task(async function dragimage_remote_tab() {
  var tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://www.example.com/browser/dom/events/test/" + TEST_URI
  );

  let dropHappened = false;
  let oldHandler = tab.linkedBrowser.droppedLinkHandler;
  tab.linkedBrowser.droppedLinkHandler = () => {
    dropHappened = true;
  };

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    let image = content.document.body.firstElementChild;
    let target = content.document.body.lastElementChild;

    const EventUtils = ContentTaskUtils.getEventUtils(content);

    await EventUtils.synthesizePlainDragAndDrop({
      srcElement: image,
      destElement: target,
      srcWindow: content,
      destWindow: content,
      id: content.windowUtils.DEFAULT_MOUSE_POINTER_ID,
    });
  });

  tab.linkedBrowser.droppedLinkHandler = oldHandler;

  ok(!dropHappened, "drop did not occur");

  BrowserTestUtils.removeTab(tab);
});

// This test checks the same but with an in-process page.
add_task(async function dragimage_local_tab() {
  var tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    getRootDirectory(gTestPath) + TEST_URI
  );

  let dropHappened = false;
  let oldHandler = tab.linkedBrowser.droppedLinkHandler;
  tab.linkedBrowser.droppedLinkHandler = () => {
    dropHappened = true;
  };

  let image = tab.linkedBrowser.contentDocument.body.firstElementChild;
  let target = tab.linkedBrowser.contentDocument.body.lastElementChild;

  await EventUtils.synthesizePlainDragAndDrop({
    srcElement: image,
    destElement: target,
    srcWindow: tab.linkedBrowser.contentWindow,
    destWindow: tab.linkedBrowser.contentWindow,
  });

  tab.linkedBrowser.droppedLinkHandler = oldHandler;

  ok(!dropHappened, "drop did not occur");

  BrowserTestUtils.removeTab(tab);
});
