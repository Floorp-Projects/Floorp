/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that a result has the various elements displayed in the URL bar as
 * we expect them to be.
 */

add_task(async function setup() {
  await PlacesUtils.history.clear();

  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
    Services.prefs.clearUserPref("browser.urlbar.trimURLs");
  });
});

async function testResult(input, expected) {
  const ESCAPED_URL = encodeURI(input.url);

  await PlacesTestUtils.addVisits({
    uri: input.url,
    title: input.title,
  });

  await promiseAutocompleteResultPopup("\u6e2C\u8a66");

  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, ESCAPED_URL,
    "Should have the correct url to load");
  Assert.equal(result.displayed.url, expected.displayedUrl,
    "Should have the correct displayed url");
  Assert.equal(result.displayed.title, input.title,
    "Should have the expected title");
  Assert.equal(result.displayed.typeIcon, "none",
    "Should not have a type icon");
  Assert.equal(result.image, `page-icon:${ESCAPED_URL}`,
    "Should have the correct favicon");

  assertDisplayedHighlights("title", result.element.title, expected.highlightedTitle);

  assertDisplayedHighlights("url", result.element.url, expected.highlightedUrl);
}

function assertDisplayedHighlights(elementName, element, expectedResults) {
  Assert.equal(element.childNodes.length, expectedResults.length,
    `Should have the correct number of child nodes for ${elementName}`);

  for (let i = 0; i < element.childNodes.length; i++) {
    let child = element.childNodes[i];
    Assert.equal(child.textContent, expectedResults[i][0],
      `Should have the correct text for the ${i} part of the ${elementName}`);
    Assert.equal(child.nodeName, expectedResults[i][1] ? "strong" : "#text",
      `Should have the correct text/strong status for the ${i} part of the ${elementName}`);
  }
}

add_task(async function test_url_result() {
  await testResult({
    query: "\u6e2C\u8a66",
    title: "The \u6e2C\u8a66 URL",
    url: "http://example.com/\u6e2C\u8a66test",
  }, {
    displayedUrl: "example.com/\u6e2C\u8a66test",
    highlightedTitle: [
      ["The ", false],
      ["\u6e2C\u8a66", true],
      [" URL", false],
    ],
    highlightedUrl: [
      ["example.com/", false],
      ["\u6e2C\u8a66", true],
      ["test", false],
    ],
  });
});

add_task(async function test_url_result_no_trimming() {
  Services.prefs.setBoolPref("browser.urlbar.trimURLs", false);

  await testResult({
    query: "\u6e2C\u8a66",
    title: "The \u6e2C\u8a66 URL",
    url: "http://example.com/\u6e2C\u8a66test",
  }, {
    displayedUrl: "http://example.com/\u6e2C\u8a66test",
    highlightedTitle: [
      ["The ", false],
      ["\u6e2C\u8a66", true],
      [" URL", false],
    ],
    highlightedUrl: [
      ["http://example.com/", false],
      ["\u6e2C\u8a66", true],
      ["test", false],
    ],
  });

  Services.prefs.clearUserPref("browser.urlbar.trimURLs");
});
