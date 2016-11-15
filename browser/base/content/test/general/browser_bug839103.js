const gTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");

add_task(function* test() {
  yield BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank" },
    function* (browser) {
      yield ContentTask.spawn(browser, gTestRoot, testBody);
    });
});

// This function runs entirely in the content process. It doesn't have access
// any free variables in this file.
function* testBody(testRoot) {
  const gStyleSheet = "bug839103.css";

  let loaded = ContentTaskUtils.waitForEvent(this, "load", true);
  content.location = testRoot + "test_bug839103.html";

  yield loaded;
  function unexpectedContentEvent(event) {
    ok(false, "Received a " + event.type + " event on content");
  }

  // We've seen the original stylesheet in the document.
  // Now add a stylesheet on the fly and make sure we see it.
  let doc = content.document;
  doc.styleSheetChangeEventsEnabled = true;
  doc.addEventListener("StyleSheetAdded", unexpectedContentEvent);
  doc.addEventListener("StyleSheetRemoved", unexpectedContentEvent);
  doc.addEventListener("StyleSheetApplicableStateChanged", unexpectedContentEvent);
  doc.defaultView.addEventListener("StyleSheetAdded", unexpectedContentEvent);
  doc.defaultView.addEventListener("StyleSheetRemoved", unexpectedContentEvent);
  doc.defaultView.addEventListener("StyleSheetApplicableStateChanged", unexpectedContentEvent);

  let link = doc.createElement("link");
  link.setAttribute("rel", "stylesheet");
  link.setAttribute("type", "text/css");
  link.setAttribute("href", testRoot + gStyleSheet);

  let sheetAdded =
    ContentTaskUtils.waitForEvent(this, "StyleSheetAdded", true);
  let stateChanged =
    ContentTaskUtils.waitForEvent(this, "StyleSheetApplicableStateChanged", true);
  doc.body.appendChild(link);

  let evt = yield sheetAdded;
  info("received dynamic style sheet event");
  is(evt.type, "StyleSheetAdded", "evt.type has expected value");
  is(evt.target, doc, "event targets correct document");
  ok(evt.stylesheet, "evt.stylesheet is defined");
  ok(evt.stylesheet.toString().includes("CSSStyleSheet"), "evt.stylesheet is a stylesheet");
  ok(evt.documentSheet, "style sheet is a document sheet");

  evt = yield stateChanged;
  info("received dynamic style sheet applicable state change event");
  is(evt.type, "StyleSheetApplicableStateChanged", "evt.type has expected value");
  is(evt.target, doc, "event targets correct document");
  is(evt.stylesheet, link.sheet, "evt.stylesheet has the right value");
  is(evt.applicable, true, "evt.applicable has the right value");

  stateChanged =
    ContentTaskUtils.waitForEvent(this, "StyleSheetApplicableStateChanged", true);
  link.disabled = true;

  evt = yield stateChanged;
  is(evt.type, "StyleSheetApplicableStateChanged", "evt.type has expected value");
  info("received dynamic style sheet applicable state change event after media=\"\" changed");
  is(evt.target, doc, "event targets correct document");
  is(evt.stylesheet, link.sheet, "evt.stylesheet has the right value");
  is(evt.applicable, false, "evt.applicable has the right value");

  let sheetRemoved =
    ContentTaskUtils.waitForEvent(this, "StyleSheetRemoved", true);
  doc.body.removeChild(link);

  evt = yield sheetRemoved;
  info("received dynamic style sheet removal");
  is(evt.type, "StyleSheetRemoved", "evt.type has expected value");
  is(evt.target, doc, "event targets correct document");
  ok(evt.stylesheet, "evt.stylesheet is defined");
  ok(evt.stylesheet.toString().includes("CSSStyleSheet"), "evt.stylesheet is a stylesheet");
  ok(evt.stylesheet.href.includes(gStyleSheet), "evt.stylesheet is the removed stylesheet");

  let ruleAdded =
    ContentTaskUtils.waitForEvent(this, "StyleRuleAdded", true);
  doc.querySelector("style").sheet.insertRule("*{color:black}", 0);

  evt = yield ruleAdded;
  info("received style rule added event");
  is(evt.type, "StyleRuleAdded", "evt.type has expected value");
  is(evt.target, doc, "event targets correct document");
  ok(evt.stylesheet, "evt.stylesheet is defined");
  ok(evt.stylesheet.toString().includes("CSSStyleSheet"), "evt.stylesheet is a stylesheet");
  ok(evt.rule, "evt.rule is defined");
  is(evt.rule.cssText, "* { color: black; }", "evt.rule.cssText has expected value");

  let ruleChanged =
    ContentTaskUtils.waitForEvent(this, "StyleRuleChanged", true);
  evt.rule.style.cssText = "color:green";

  evt = yield ruleChanged;
  ok(true, "received style rule changed event");
  is(evt.type, "StyleRuleChanged", "evt.type has expected value");
  is(evt.target, doc, "event targets correct document");
  ok(evt.stylesheet, "evt.stylesheet is defined");
  ok(evt.stylesheet.toString().includes("CSSStyleSheet"), "evt.stylesheet is a stylesheet");
  ok(evt.rule, "evt.rule is defined");
  is(evt.rule.cssText, "* { color: green; }", "evt.rule.cssText has expected value");

  let ruleRemoved =
    ContentTaskUtils.waitForEvent(this, "StyleRuleRemoved", true);
  evt.stylesheet.deleteRule(0);

  evt = yield ruleRemoved;
  info("received style rule removed event");
  is(evt.type, "StyleRuleRemoved", "evt.type has expected value");
  is(evt.target, doc, "event targets correct document");
  ok(evt.stylesheet, "evt.stylesheet is defined");
  ok(evt.stylesheet.toString().includes("CSSStyleSheet"), "evt.stylesheet is a stylesheet");
  ok(evt.rule, "evt.rule is defined");
}
