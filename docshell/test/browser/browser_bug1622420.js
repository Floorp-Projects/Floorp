const ACTOR = "Bug1622420";

add_task(async function test() {
  let base = getRootDirectory(gTestPath).slice(0, -1);
  ChromeUtils.registerWindowActor(ACTOR, {
    allFrames: true,
    child: {
      moduleURI: `${base}/Bug1622420Child.jsm`,
    },
  });

  registerCleanupFunction(async () => {
    gBrowser.removeTab(tab);

    ChromeUtils.unregisterWindowActor(ACTOR);
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.org/browser/docshell/test/browser/file_bug1622420.html"
  );
  let childBC = tab.linkedBrowser.browsingContext.children[0];
  let success = await childBC.currentWindowGlobal
    .getActor(ACTOR)
    .sendQuery("hasWindowContextForTopBC");
  ok(
    success,
    "Should have a WindowContext for the top BrowsingContext in the process of a child BrowsingContext"
  );
});
