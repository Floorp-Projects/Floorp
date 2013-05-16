const gTestRoot = getRootDirectory(gTestPath);
const gStyleSheet = "bug839103.css";

var gTab = null;
var needsInitialApplicableStateEvent = false;
var needsInitialApplicableStateEventFor = null;

function test() {
  waitForExplicitFinish();
  gBrowser.addEventListener("StyleSheetAdded", initialStylesheetAdded, true);
  gTab = gBrowser.selectedTab = gBrowser.addTab(gTestRoot + "test_bug839103.html");
  gTab.linkedBrowser.addEventListener("load", tabLoad, true);
}

function initialStylesheetAdded(evt) {
  gBrowser.removeEventListener("StyleSheetAdded", initialStylesheetAdded, true);
  ok(true, "received initial style sheet event");
  is(evt.type, "StyleSheetAdded", "evt.type has expected value");
  is(evt.target, gBrowser.contentDocument, "event targets correct document");
  ok(evt.stylesheet, "evt.stylesheet is defined");
  ok(evt.stylesheet.toString().contains("CSSStyleSheet"), "evt.stylesheet is a stylesheet");
  ok(evt.documentSheet, "style sheet is a document sheet");
}

function tabLoad(evt) {
  gTab.linkedBrowser.removeEventListener(evt.type, tabLoad, true);
  executeSoon(continueTest);
}

var gLinkElement = null;

function unexpectedContentEvent(evt) {
  ok(false, "Received a " + evt.type + " event on content");
}

// We've seen the original stylesheet in the document.
// Now add a stylesheet on the fly and make sure we see it.
function continueTest() {
  info("continuing test");

  let doc = gBrowser.contentDocument;
  doc.styleSheetChangeEventsEnabled = true;
  doc.addEventListener("StyleSheetAdded", unexpectedContentEvent, false);
  doc.addEventListener("StyleSheetRemoved", unexpectedContentEvent, false);
  doc.addEventListener("StyleSheetApplicableStateChanged", unexpectedContentEvent, false);
  doc.defaultView.addEventListener("StyleSheetAdded", unexpectedContentEvent, false);
  doc.defaultView.addEventListener("StyleSheetRemoved", unexpectedContentEvent, false);
  doc.defaultView.addEventListener("StyleSheetApplicableStateChanged", unexpectedContentEvent, false);
  let link = doc.createElement('link');
  link.setAttribute('rel', 'stylesheet');
  link.setAttribute('type', 'text/css');
  link.setAttribute('href', gTestRoot + gStyleSheet);
  gLinkElement = link;

  gBrowser.addEventListener("StyleSheetAdded", dynamicStylesheetAdded, true);
  gBrowser.addEventListener("StyleSheetApplicableStateChanged", dynamicStylesheetApplicableStateChanged, true);
  doc.body.appendChild(link);
}

function dynamicStylesheetAdded(evt) {
  gBrowser.removeEventListener("StyleSheetAdded", dynamicStylesheetAdded, true);
  ok(true, "received dynamic style sheet event");
  is(evt.type, "StyleSheetAdded", "evt.type has expected value");
  is(evt.target, gBrowser.contentDocument, "event targets correct document");
  ok(evt.stylesheet, "evt.stylesheet is defined");
  ok(evt.stylesheet.toString().contains("CSSStyleSheet"), "evt.stylesheet is a stylesheet");
  ok(evt.documentSheet, "style sheet is a document sheet");
}

function dynamicStylesheetApplicableStateChanged(evt) {
  gBrowser.removeEventListener("StyleSheetApplicableStateChanged", dynamicStylesheetApplicableStateChanged, true);
  ok(true, "received dynamic style sheet applicable state change event");
  is(evt.type, "StyleSheetApplicableStateChanged", "evt.type has expected value");
  is(evt.target, gBrowser.contentDocument, "event targets correct document");
  is(evt.stylesheet, gLinkElement.sheet, "evt.stylesheet has the right value");
  is(evt.applicable, true, "evt.applicable has the right value");

  gBrowser.addEventListener("StyleSheetApplicableStateChanged", dynamicStylesheetApplicableStateChangedToFalse, true);
  gLinkElement.disabled = true;
}

function dynamicStylesheetApplicableStateChangedToFalse(evt) {
  gBrowser.removeEventListener("StyleSheetApplicableStateChanged", dynamicStylesheetApplicableStateChangedToFalse, true);
  is(evt.type, "StyleSheetApplicableStateChanged", "evt.type has expected value");
  ok(true, "received dynamic style sheet applicable state change event after media=\"\" changed");
  is(evt.target, gBrowser.contentDocument, "event targets correct document");
  is(evt.stylesheet, gLinkElement.sheet, "evt.stylesheet has the right value");
  is(evt.applicable, false, "evt.applicable has the right value");

  gBrowser.addEventListener("StyleSheetRemoved", dynamicStylesheetRemoved, true);
  gBrowser.contentDocument.body.removeChild(gLinkElement);
}

function dynamicStylesheetRemoved(evt) {
  gBrowser.removeEventListener("StyleSheetRemoved", dynamicStylesheetRemoved, true);
  ok(true, "received dynamic style sheet removal");
  is(evt.type, "StyleSheetRemoved", "evt.type has expected value");
  is(evt.target, gBrowser.contentDocument, "event targets correct document");
  ok(evt.stylesheet, "evt.stylesheet is defined");
  ok(evt.stylesheet.toString().contains("CSSStyleSheet"), "evt.stylesheet is a stylesheet");
  ok(evt.stylesheet.href.contains(gStyleSheet), "evt.stylesheet is the removed stylesheet");

  gBrowser.addEventListener("StyleRuleAdded", styleRuleAdded, true);
  gBrowser.contentDocument.querySelector("style").sheet.insertRule("*{color:black}", 0);
}

function styleRuleAdded(evt) {
  gBrowser.removeEventListener("StyleRuleAdded", styleRuleAdded, true);
  ok(true, "received style rule added event");
  is(evt.type, "StyleRuleAdded", "evt.type has expected value");
  is(evt.target, gBrowser.contentDocument, "event targets correct document");
  ok(evt.stylesheet, "evt.stylesheet is defined");
  ok(evt.stylesheet.toString().contains("CSSStyleSheet"), "evt.stylesheet is a stylesheet");
  ok(evt.rule, "evt.rule is defined");
  is(evt.rule.cssText, "* { color: black; }", "evt.rule.cssText has expected value");

  gBrowser.addEventListener("StyleRuleChanged", styleRuleChanged, true);
  evt.rule.style.cssText = "color:green";
}

function styleRuleChanged(evt) {
  gBrowser.removeEventListener("StyleRuleChanged", styleRuleChanged, true);
  ok(true, "received style rule changed event");
  is(evt.type, "StyleRuleChanged", "evt.type has expected value");
  is(evt.target, gBrowser.contentDocument, "event targets correct document");
  ok(evt.stylesheet, "evt.stylesheet is defined");
  ok(evt.stylesheet.toString().contains("CSSStyleSheet"), "evt.stylesheet is a stylesheet");
  ok(evt.rule, "evt.rule is defined");
  is(evt.rule.cssText, "* { color: green; }", "evt.rule.cssText has expected value");

  gBrowser.addEventListener("StyleRuleRemoved", styleRuleRemoved, true);
  evt.stylesheet.deleteRule(0);
}

function styleRuleRemoved(evt) {
  gBrowser.removeEventListener("StyleRuleRemoved", styleRuleRemoved, true);
  ok(true, "received style rule removed event");
  is(evt.type, "StyleRuleRemoved", "evt.type has expected value");
  is(evt.target, gBrowser.contentDocument, "event targets correct document");
  ok(evt.stylesheet, "evt.stylesheet is defined");
  ok(evt.stylesheet.toString().contains("CSSStyleSheet"), "evt.stylesheet is a stylesheet");
  ok(evt.rule, "evt.rule is defined");

  executeSoon(concludeTest);
}

function concludeTest() {
  let doc = gBrowser.contentDocument;
  doc.removeEventListener("StyleSheetAdded", unexpectedContentEvent, false);
  doc.removeEventListener("StyleSheetRemoved", unexpectedContentEvent, false);
  doc.removeEventListener("StyleSheetApplicableStateChanged", unexpectedContentEvent, false);
  doc.defaultView.removeEventListener("StyleSheetAdded", unexpectedContentEvent, false);
  doc.defaultView.removeEventListener("StyleSheetRemoved", unexpectedContentEvent, false);
  doc.defaultView.removeEventListener("StyleSheetApplicableStateChanged", unexpectedContentEvent, false);
  gBrowser.removeCurrentTab();
  gLinkElement = null;
  gTab = null;
  finish();
}
