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
 * a docs page, and loads the results into the document.
 *
 * In this file we test that the tooltip can properly handle the different
 * structures that the docs page might have, including variant structures and
 * error conditions like parts of the document being missing.
 *
 * We also test that the tooltip properly handles the case where the page
 * doesn't exist at all.
 */

"use strict";

const {CssDocsTooltip} = require("devtools/client/shared/widgets/Tooltip");
const {setBaseCssDocsUrl, MdnDocsWidget} = require("devtools/client/shared/widgets/MdnDocsWidget");

// frame to load the tooltip into
const MDN_DOCS_TOOLTIP_FRAME = "chrome://devtools/content/shared/widgets/mdn-docs-frame.xhtml";

const BASIC_EXPECTED_SUMMARY = "A summary of the property.";
const BASIC_EXPECTED_SYNTAX = [{type: "comment",        text: "/* The part we want   */"},
                               {type: "text",           text: "\n"},
                               {type: "property-name",  text: "this"},
                               {type: "text",           text: ":"},
                               {type: "text",           text: " "},
                               {type: "property-value", text: "is-the-part-we-want"},
                               {type: "text",           text: ";"}];

const ERROR_MESSAGE = "Could not load docs page.";

/**
 * Test properties
 *
 * In the real tooltip, a CSS property name is used to look up an MDN page
 * for that property.
 * In the test code, the names defined here are used to look up a page
 * served by the test server. We have different properties to test
 * different ways that the docs pages might be constructed, including errors
 * like pages that don't include docs where we expect.
 */
const SYNTAX_OLD_STYLE = "html-mdn-css-syntax-old-style.html";
const NO_SUMMARY = "html-mdn-css-no-summary.html";
const NO_SYNTAX = "html-mdn-css-no-syntax.html";
const NO_SUMMARY_OR_SYNTAX = "html-mdn-css-no-summary-or-syntax.html";

const TEST_DATA = [{
  desc: "Test a property for which we don't have a page",
  docsPageUrl: "i-dont-exist.html",
  expectedContents: {
    propertyName: "i-dont-exist.html",
    summary: ERROR_MESSAGE,
    syntax: []
  }
}, {
  desc: "Test a property whose syntax section is specified using an old-style page",
  docsPageUrl: SYNTAX_OLD_STYLE,
  expectedContents: {
    propertyName: SYNTAX_OLD_STYLE,
    summary: BASIC_EXPECTED_SUMMARY,
    syntax: BASIC_EXPECTED_SYNTAX
  }
},  {
  desc: "Test a property whose page doesn't have a summary",
  docsPageUrl: NO_SUMMARY,
  expectedContents: {
    propertyName: NO_SUMMARY,
    summary: "",
    syntax: BASIC_EXPECTED_SYNTAX
  }
}, {
  desc: "Test a property whose page doesn't have a syntax",
  docsPageUrl: NO_SYNTAX,
  expectedContents: {
    propertyName: NO_SYNTAX,
    summary: BASIC_EXPECTED_SUMMARY,
    syntax: []
  }
}, {
  desc: "Test a property whose page doesn't have a summary or a syntax",
  docsPageUrl: NO_SUMMARY_OR_SYNTAX,
  expectedContents: {
    propertyName: NO_SUMMARY_OR_SYNTAX,
    summary: ERROR_MESSAGE,
    syntax: []
  }
}
];

add_task(function*() {
  setBaseCssDocsUrl(TEST_URI_ROOT);

  yield addTab("about:blank");
  let [host, win, doc] = yield createHost("bottom", MDN_DOCS_TOOLTIP_FRAME);
  let widget = new MdnDocsWidget(win.document);

  for (let {desc, docsPageUrl, expectedContents} of TEST_DATA) {
    info(desc);
    yield widget.loadCssDocs(docsPageUrl);
    checkTooltipContents(widget.elements, expectedContents);
  }
  host.destroy();
  gBrowser.removeCurrentTab();
});

function* testNonExistentPage(widget) {
  info("Test a property for which we don't have a page");
  yield widget.loadCssDocs("i-dont-exist.html");
  checkTooltipContents(widget.elements, {
    propertyName: "i-dont-exist.html",
    summary: ERROR_MESSAGE,
    syntax: ""
  });
}

/*
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
