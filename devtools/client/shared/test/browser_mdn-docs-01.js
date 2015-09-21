/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests the MdnDocsWidget object, and specifically its
 * loadCssDocs() function.
 *
 * The MdnDocsWidget is initialized with a document which has a specific
 * structure. You then call loadCssDocs(), passing in a CSS property name.
 * MdnDocsWidget then fetches docs for that property by making an XHR to
 * a docs page, and loads the results into the document. While the XHR is
 * still not resolved the document is put into an "initializing" state in
 * which the devtools throbber is displayed.
 *
 * In this file we test:
 * - the initial state of the document before the docs have loaded
 * - the state of the document after the docs have loaded
 */

"use strict";

const {CssDocsTooltip} = require("devtools/shared/widgets/Tooltip");
const {setBaseCssDocsUrl, MdnDocsWidget} = require("devtools/shared/widgets/MdnDocsWidget");
const promise = require("promise");

// frame to load the tooltip into
const MDN_DOCS_TOOLTIP_FRAME = "chrome://devtools/content/shared/widgets/mdn-docs-frame.xhtml";

/**
 * Test properties
 *
 * In the real tooltip, a CSS property name is used to look up an MDN page
 * for that property.
 * In the test code, the names defined here is used to look up a page
 * served by the test server.
 */
const BASIC_TESTING_PROPERTY = "html-mdn-css-basic-testing.html";

const BASIC_EXPECTED_SUMMARY = "A summary of the property.";
const BASIC_EXPECTED_SYNTAX = [{type: "comment",        text: "/* The part we want   */"},
                               {type: "text",           text: "\n"},
                               {type: "property-name",  text: "this"},
                               {type: "text",           text: ":"},
                               {type: "text",           text: " "},
                               {type: "property-value", text: "is-the-part-we-want"},
                               {type: "text",           text: ";"}];

const URI_PARAMS = "?utm_source=mozilla&utm_medium=firefox-inspector&utm_campaign=default";

add_task(function*() {
  setBaseCssDocsUrl(TEST_URI_ROOT);

  yield promiseTab("about:blank");
  let [host, win, doc] = yield createHost("bottom", MDN_DOCS_TOOLTIP_FRAME);
  let widget = new MdnDocsWidget(win.document);

  yield testTheBasics(widget);

  host.destroy();
  gBrowser.removeCurrentTab();
});

/**
 * Test all the basics
 * - initial content, before docs have loaded, is as expected
 * - throbber is set before docs have loaded
 * - contents are as expected after docs have loaded
 * - throbber is gone after docs have loaded
 * - mdn link text is correct and onclick behavior is correct
 */
function* testTheBasics(widget) {
  info("Test all the basic functionality in the widget");

  info("Get the widget state before docs have loaded");
  let promise = widget.loadCssDocs(BASIC_TESTING_PROPERTY);

  info("Check initial contents before docs have loaded");
  checkTooltipContents(widget.elements, {
    propertyName: BASIC_TESTING_PROPERTY,
    summary: "",
    syntax: ""
  });

  // throbber is set
  ok(widget.elements.info.classList.contains("devtools-throbber"),
     "Throbber is set");

  info("Now let the widget finish loading");
  yield promise;

  info("Check contents after docs have loaded");
  checkTooltipContents(widget.elements, {
    propertyName: BASIC_TESTING_PROPERTY,
    summary: BASIC_EXPECTED_SUMMARY,
    syntax: BASIC_EXPECTED_SYNTAX
  });

  // throbber is gone
  ok(!widget.elements.info.classList.contains("devtools-throbber"),
     "Throbber is not set");

  info("Check that MDN link text is correct and onclick behavior is correct");

  let mdnLink = widget.elements.linkToMdn;
  let expectedHref = TEST_URI_ROOT + BASIC_TESTING_PROPERTY + URI_PARAMS;
  is(mdnLink.href, expectedHref, "MDN link href is correct");

  let uri = yield checkLinkClick(mdnLink);
  is(uri, expectedHref, "New tab opened with the expected URI");
}

 /**
  * Clicking the "Visit MDN Page" in the tooltip panel
  * should open a new browser tab with the page loaded.
  *
  * To test this we'll listen for a new tab opening, and
  * when it does, add a listener to that new tab to tell
  * us when it has loaded.
  *
  * Then we click the link.
  *
  * In the tab's load listener, we'll resolve the promise
  * with the URI, which is expected to match the href
  * in the orginal link.
  *
  * One complexity is that when you open a new tab,
  * "about:blank" is first loaded into the tab before the
  * actual page. So we ignore that first load event, and keep
  * listening until "load" is triggered for a different URI.
  */
function checkLinkClick(link) {

  function loadListener(e) {
    let tab = e.target;
    var browser = getBrowser().getBrowserForTab(tab);
    var uri = browser.currentURI.spec;
    // this is horrible, and it's because when we open a new tab
    // "about:blank: is first loaded into it, before the actual
    // document we want to load.
    if (uri != "about:blank") {
      info("New browser tab has loaded");
      tab.removeEventListener("load", loadListener);
      gBrowser.removeTab(tab);
      info("Resolve promise with new tab URI");
      deferred.resolve(uri);
    }
  }

  function newTabListener(e) {
    gBrowser.tabContainer.removeEventListener("TabOpen", newTabListener);
    var tab = e.target;
    tab.addEventListener("load", loadListener, false);
  }

  let deferred = promise.defer();
  info("Check that clicking the link opens a new tab with the correct URI");
  gBrowser.tabContainer.addEventListener("TabOpen", newTabListener, false);
  info("Click the link to MDN");
  link.click();
  return deferred.promise;
}

/**
 * Utility function to check content of the tooltip.
 */
function checkTooltipContents(doc, expected) {

  is(doc.heading.textContent,
     expected.propertyName,
     "Property name is correct");

  is(doc.summary.textContent,
     expected.summary,
     "Summary is correct");

   checkCssSyntaxHighlighterOutput(expected.syntax, doc.syntax);
}
