const gTestRoot = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "http://127.0.0.1:8888/"
);

add_task(async function test() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: gTestRoot + "file_stylesheet_change_events.html" },
    async function (browser) {
      await SpecialPowers.spawn(
        browser,
        [gTestRoot],
        testApplicableStateChangeEvent
      );
    }
  );
});

// This function runs entirely in the content process. It doesn't have access
// any free variables in this file.
async function testApplicableStateChangeEvent(testRoot) {
  // We've seen the original stylesheet in the document.
  // Now add a stylesheet on the fly and make sure we see it.
  let doc = content.document;
  doc.styleSheetChangeEventsEnabled = true;

  const unexpectedContentEvent = event =>
    ok(false, "Received a " + event.type + " event on content");
  doc.addEventListener(
    "StyleSheetApplicableStateChanged",
    unexpectedContentEvent
  );
  doc.defaultView.addEventListener(
    "StyleSheetApplicableStateChanged",
    unexpectedContentEvent
  );
  doc.addEventListener("StyleSheetRemoved", unexpectedContentEvent);
  doc.defaultView.addEventListener("StyleSheetRemoved", unexpectedContentEvent);

  function shouldIgnoreEvent(e) {
    // accessiblecaret.css might be reported, interfering with the test
    // assertions, so let's ignore it
    return (
      e.stylesheet?.href === "resource://content-accessible/accessiblecaret.css"
    );
  }

  function waitForStyleApplicableStateChanged() {
    return ContentTaskUtils.waitForEvent(
      docShell.chromeEventHandler,
      "StyleSheetApplicableStateChanged",
      true,
      e => !shouldIgnoreEvent(e)
    );
  }

  function waitForStyleSheetRemovedEvent() {
    return ContentTaskUtils.waitForEvent(
      docShell.chromeEventHandler,
      "StyleSheetRemoved",
      true,
      e => !shouldIgnoreEvent(e)
    );
  }

  function checkApplicableStateChangeEvent(event, { applicable, stylesheet }) {
    is(
      event.type,
      "StyleSheetApplicableStateChanged",
      "event.type has expected value"
    );
    is(event.target, doc, "event targets correct document");
    is(event.stylesheet, stylesheet, "event.stylesheet has the expected value");
    is(event.applicable, applicable, "event.applicable has the expected value");
  }

  function checkStyleSheetRemovedEvent(event, { stylesheet }) {
    is(event.type, "StyleSheetRemoved", "event.type has expected value");
    is(event.target, doc, "event targets correct document");
    is(event.stylesheet, stylesheet, "event.stylesheet has the expected value");
  }

  // Updating the text content will actually create a new StyleSheet instance,
  // and so we should get one event for the new instance, and another one for
  // the removal of the "previous"one.
  function waitForTextContentChange() {
    return Promise.all([
      waitForStyleSheetRemovedEvent(),
      waitForStyleApplicableStateChanged(),
    ]);
  }

  let stateChanged, evt;

  {
    const gStyleSheet = "stylesheet_change_events.css";

    info("Add <link> and wait for applicable state change event");
    let linkEl = doc.createElement("link");
    linkEl.setAttribute("rel", "stylesheet");
    linkEl.setAttribute("type", "text/css");
    linkEl.setAttribute("href", testRoot + gStyleSheet);

    stateChanged = waitForStyleApplicableStateChanged();
    doc.body.appendChild(linkEl);
    evt = await stateChanged;

    ok(true, "received dynamic style sheet applicable state change event");
    checkApplicableStateChangeEvent(evt, {
      stylesheet: linkEl.sheet,
      applicable: true,
    });

    stateChanged = waitForStyleApplicableStateChanged();
    linkEl.sheet.disabled = true;
    evt = await stateChanged;

    ok(true, "received dynamic style sheet applicable state change event");
    checkApplicableStateChangeEvent(evt, {
      stylesheet: linkEl.sheet,
      applicable: false,
    });

    info("Remove stylesheet and wait for removed event");
    const removedStylesheet = linkEl.sheet;
    const onStyleSheetRemoved = waitForStyleSheetRemovedEvent();
    doc.body.removeChild(linkEl);
    const removedStyleSheetEvt = await onStyleSheetRemoved;

    ok(true, "received removed sheet event");
    checkStyleSheetRemovedEvent(removedStyleSheetEvt, {
      stylesheet: removedStylesheet,
    });
  }

  {
    info("Add <style> node and wait for applicable state changed event");
    let styleEl = doc.createElement("style");
    styleEl.textContent = `body { background: tomato; }`;

    stateChanged = waitForStyleApplicableStateChanged();
    doc.head.appendChild(styleEl);
    evt = await stateChanged;

    ok(true, "received dynamic style sheet applicable state change event");
    checkApplicableStateChangeEvent(evt, {
      stylesheet: styleEl.sheet,
      applicable: true,
    });

    info("Updating <style> text content");
    stateChanged = waitForTextContentChange();
    const inlineStyleSheetBeforeChange = styleEl.sheet;

    styleEl.textContent = `body { background: gold; }`;
    const [inlineRemovedEvt, inlineAddedEvt] = await stateChanged;

    ok(true, "received expected style sheet events");
    checkStyleSheetRemovedEvent(inlineRemovedEvt, {
      stylesheet: inlineStyleSheetBeforeChange,
    });
    checkApplicableStateChangeEvent(inlineAddedEvt, {
      stylesheet: styleEl.sheet,
      applicable: true,
    });

    info("Remove stylesheet and wait for removed event");
    const onStyleSheetRemoved = waitForStyleSheetRemovedEvent();

    const removedInlineStylesheet = styleEl.sheet;
    styleEl.remove();
    const removedStyleSheetEvt = await onStyleSheetRemoved;

    ok(true, "received removed style sheet event");
    checkStyleSheetRemovedEvent(removedStyleSheetEvt, {
      stylesheet: removedInlineStylesheet,
    });
  }

  {
    info(
      "Create a custom element and check we get an event for its stylesheet"
    );
    stateChanged = waitForStyleApplicableStateChanged();
    const el = doc.createElement("div");
    const shadowRoot = el.attachShadow({ mode: "open" });
    doc.body.appendChild(el);
    shadowRoot.innerHTML = `
        <span>custom</span>
        <style>
          span { color: salmon; }
        </style>`;
    evt = await stateChanged;

    ok(true, "received dynamic style sheet applicable state change event");
    const shadowStyleEl = shadowRoot.querySelector("style");
    checkApplicableStateChangeEvent(evt, {
      stylesheet: shadowStyleEl.sheet,
      applicable: true,
    });

    info("Updating <style> text content");
    stateChanged = waitForTextContentChange();
    const styleSheetBeforeChange = shadowStyleEl.sheet;
    shadowStyleEl.textContent = `span { color: cyan; }`;
    const [removedEvt, addedEvt] = await stateChanged;

    ok(true, "received expected style sheet events");
    checkStyleSheetRemovedEvent(removedEvt, {
      stylesheet: styleSheetBeforeChange,
    });
    checkApplicableStateChangeEvent(addedEvt, {
      stylesheet: shadowStyleEl.sheet,
      applicable: true,
    });

    info("Remove stylesheet and wait for removed event");
    const onStyleSheetRemoved = waitForStyleSheetRemovedEvent();
    const removedShadowStylesheet = shadowStyleEl.sheet;
    shadowStyleEl.remove();
    const removedStyleSheetEvt = await onStyleSheetRemoved;
    ok(true, "received removed style sheet event");
    checkStyleSheetRemovedEvent(removedStyleSheetEvt, {
      stylesheet: removedShadowStylesheet,
    });
  }
}
