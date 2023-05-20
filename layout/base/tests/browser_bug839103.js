const gTestRoot = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "http://127.0.0.1:8888/"
);

add_task(async function test() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: gTestRoot + "file_bug839103.html" },
    async function (browser) {
      await SpecialPowers.spawn(browser, [gTestRoot], testBody);
    }
  );
});

// This function runs entirely in the content process. It doesn't have access
// any free variables in this file.
async function testBody(testRoot) {
  const gStyleSheet = "bug839103.css";

  function unexpectedContentEvent(event) {
    ok(false, "Received a " + event.type + " event on content");
  }

  // We've seen the original stylesheet in the document.
  // Now add a stylesheet on the fly and make sure we see it.
  let doc = content.document;
  doc.styleSheetChangeEventsEnabled = true;
  doc.addEventListener(
    "StyleSheetApplicableStateChanged",
    unexpectedContentEvent
  );
  doc.defaultView.addEventListener(
    "StyleSheetApplicableStateChanged",
    unexpectedContentEvent
  );

  let link = doc.createElement("link");
  link.setAttribute("rel", "stylesheet");
  link.setAttribute("type", "text/css");
  link.setAttribute("href", testRoot + gStyleSheet);

  let stateChanged = ContentTaskUtils.waitForEvent(
    docShell.chromeEventHandler,
    "StyleSheetApplicableStateChanged",
    true
  );
  doc.body.appendChild(link);

  info("waiting for applicable state change event");
  let evt = await stateChanged;
  info("received dynamic style sheet applicable state change event");
  is(
    evt.type,
    "StyleSheetApplicableStateChanged",
    "evt.type has expected value"
  );
  is(evt.target, doc, "event targets correct document");
  is(evt.stylesheet, link.sheet, "evt.stylesheet has the right value");
  is(evt.applicable, true, "evt.applicable has the right value");

  stateChanged = ContentTaskUtils.waitForEvent(
    docShell.chromeEventHandler,
    "StyleSheetApplicableStateChanged",
    true
  );
  link.sheet.disabled = true;

  evt = await stateChanged;
  is(
    evt.type,
    "StyleSheetApplicableStateChanged",
    "evt.type has expected value"
  );
  info(
    'received dynamic style sheet applicable state change event after media="" changed'
  );
  is(evt.target, doc, "event targets correct document");
  is(evt.stylesheet, link.sheet, "evt.stylesheet has the right value");
  is(evt.applicable, false, "evt.applicable has the right value");

  doc.body.removeChild(link);
}
