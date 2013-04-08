const gTestRoot = getRootDirectory(gTestPath);
const gStyleSheet = "bug839103.css";

var gTab = null;

function test() {
  waitForExplicitFinish();
  gBrowser.addEventListener("StyleSheetAdded", initialStylesheetAdded, true);
  gTab = gBrowser.selectedTab = gBrowser.addTab(gTestRoot + "test_bug839103.html");
  gTab.linkedBrowser.addEventListener("load", tabLoad, true);
}

function initialStylesheetAdded(evt) {
  gBrowser.removeEventListener("StyleSheetAdded", initialStylesheetAdded, true);
  ok(true, "received initial style sheet event");
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
  doc.addEventListener("StyleSheetAdded", unexpectedContentEvent, false);
  doc.addEventListener("StyleSheetRemoved", unexpectedContentEvent, false);
  doc.defaultView.addEventListener("StyleSheetAdded", unexpectedContentEvent, false);
  doc.defaultView.addEventListener("StyleSheetRemoved", unexpectedContentEvent, false);
  let link = doc.createElement('link');
  link.setAttribute('rel', 'stylesheet');
  link.setAttribute('type', 'text/css');
  link.setAttribute('href', gTestRoot + gStyleSheet);
  gLinkElement = link;

  gBrowser.addEventListener("StyleSheetAdded", dynamicStylesheetAdded, true);
  doc.body.appendChild(link);
}

function dynamicStylesheetAdded(evt) {
  gBrowser.removeEventListener("StyleSheetAdded", dynamicStylesheetAdded, true);
  ok(true, "received dynamic style sheet event");
  is(evt.target, gBrowser.contentDocument, "event targets correct document");
  ok(evt.stylesheet, "evt.stylesheet is defined");
  ok(evt.stylesheet.toString().contains("CSSStyleSheet"), "evt.stylesheet is a stylesheet");
  ok(evt.documentSheet, "style sheet is a document sheet");

  gBrowser.addEventListener("StyleSheetRemoved", dynamicStylesheetRemoved, true);
  gBrowser.contentDocument.body.removeChild(gLinkElement);
}

function dynamicStylesheetRemoved(evt) {
  gBrowser.removeEventListener("StyleSheetRemoved", dynamicStylesheetRemoved, true);
  ok(true, "received dynamic style sheet removal");
  is(evt.target, gBrowser.contentDocument, "event targets correct document");
  ok(evt.stylesheet, "evt.stylesheet is defined");
  ok(evt.stylesheet.toString().contains("CSSStyleSheet"), "evt.stylesheet is a stylesheet");
  ok(evt.stylesheet.href.contains(gStyleSheet), "evt.stylesheet is the removed stylesheet");
  executeSoon(concludeTest);
}

function concludeTest() {
  let doc = gBrowser.contentDocument;
  doc.removeEventListener("StyleSheetAdded", unexpectedContentEvent, false);
  doc.removeEventListener("StyleSheetRemoved", unexpectedContentEvent, false);
  doc.defaultView.removeEventListener("StyleSheetAdded", unexpectedContentEvent, false);
  doc.defaultView.removeEventListener("StyleSheetRemoved", unexpectedContentEvent, false);
  gBrowser.removeCurrentTab();
  gLinkElement = null;
  gTab = null;
  finish();
}