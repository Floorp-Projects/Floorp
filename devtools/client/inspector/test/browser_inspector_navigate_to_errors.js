/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Test that inspector works when navigating to error pages.

const TEST_URL_1 = "data:text/html,<html><body id=\"test-doc-1\">page</body></html>";
const TEST_URL_2 = "http://127.0.0.1:36325/";
const TEST_URL_3 = "http://www.wronguri.wronguri/";
const TEST_URL_4 = "data:text/html,<html><body>test-doc-4</body></html>";

add_task(function* () {
  // Open the inspector on a valid URL
  let { inspector, testActor } = yield openInspectorForURL(TEST_URL_1);

  info("Navigate to closed port");
  yield navigateTo(inspector, TEST_URL_2);

  let documentURI = yield testActor.eval("document.documentURI;");
  ok(documentURI.startsWith("about:neterror"), "content is correct.");

  let hasPage = yield getNodeFront("#test-doc-1", inspector);
  ok(!hasPage, "Inspector actor is no longer able to reach previous page DOM node");

  let hasNetErrorNode = yield getNodeFront("#errorShortDesc", inspector);
  ok(hasNetErrorNode, "Inspector actor is able to reach error page DOM node");

  let bundle = Services.strings.createBundle("chrome://global/locale/appstrings.properties");
  let domain = TEST_URL_2.match(/^http:\/\/(.*)\/$/)[1];
  let errorMsg = bundle.formatStringFromName("connectionFailure",
                                             [domain], 1);
  is(yield getDisplayedNodeTextContent("#errorShortDescText", inspector), errorMsg,
     "Inpector really inspects the error page");

  info("Navigate to unknown domain");
  yield navigateTo(inspector, TEST_URL_3);

  domain = TEST_URL_3.match(/^http:\/\/(.*)\/$/)[1];
  errorMsg = bundle.formatStringFromName("dnsNotFound",
                                         [domain], 1);
  is(yield getDisplayedNodeTextContent("#errorShortDescText", inspector), errorMsg,
     "Inspector really inspects the new error page");

  info("Navigate to a valid url");
  yield navigateTo(inspector, TEST_URL_4);

  is(yield getDisplayedNodeTextContent("body", inspector), "test-doc-4",
     "Inspector really inspects the valid url");
});
