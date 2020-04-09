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

  await PlacesUtils.history.clear();
  await PlacesTestUtils.addVisits({
    uri: input.url,
    title: input.title,
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus: SimpleTest.waitForFocus,
    value: input.query,
  });

  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, ESCAPED_URL, "Should have the correct url to load");
  Assert.equal(
    result.displayed.url,
    expected.displayedUrl,
    "Should have the correct displayed url"
  );
  Assert.equal(
    result.displayed.title,
    input.title,
    "Should have the expected title"
  );
  Assert.equal(
    result.displayed.typeIcon,
    "none",
    "Should not have a type icon"
  );
  Assert.equal(
    result.image,
    `page-icon:${ESCAPED_URL}`,
    "Should have the correct favicon"
  );

  assertDisplayedHighlights(
    "title",
    result.element.title,
    expected.highlightedTitle
  );

  assertDisplayedHighlights("url", result.element.url, expected.highlightedUrl);
}

function assertDisplayedHighlights(elementName, element, expectedResults) {
  Assert.equal(
    element.childNodes.length,
    expectedResults.length,
    `Should have the correct number of child nodes for ${elementName}`
  );

  for (let i = 0; i < element.childNodes.length; i++) {
    let child = element.childNodes[i];
    Assert.equal(
      child.textContent,
      expectedResults[i][0],
      `Should have the correct text for the ${i} part of the ${elementName}`
    );
    Assert.equal(
      child.nodeName,
      expectedResults[i][1] ? "strong" : "#text",
      `Should have the correct text/strong status for the ${i} part of the ${elementName}`
    );
  }
}

add_task(async function test_url_result() {
  await testResult(
    {
      query: "\u6e2C\u8a66",
      title: "The \u6e2C\u8a66 URL",
      url: "https://example.com/\u6e2C\u8a66test",
    },
    {
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
    }
  );
});

add_task(async function test_url_result_no_path() {
  await testResult(
    {
      query: "ample",
      title: "The Title",
      url: "https://example.com/",
    },
    {
      displayedUrl: "example.com",
      highlightedTitle: [["The Title", false]],
      highlightedUrl: [
        ["ex", false],
        ["ample", true],
        [".com", false],
      ],
    }
  );
});

add_task(async function test_url_result_www() {
  await testResult(
    {
      query: "ample",
      title: "The Title",
      url: "https://www.example.com/",
    },
    {
      displayedUrl: "example.com",
      highlightedTitle: [["The Title", false]],
      highlightedUrl: [
        ["ex", false],
        ["ample", true],
        [".com", false],
      ],
    }
  );
});

add_task(async function test_url_result_no_trimming() {
  Services.prefs.setBoolPref("browser.urlbar.trimURLs", false);

  await testResult(
    {
      query: "\u6e2C\u8a66",
      title: "The \u6e2C\u8a66 URL",
      url: "http://example.com/\u6e2C\u8a66test",
    },
    {
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
    }
  );

  Services.prefs.clearUserPref("browser.urlbar.trimURLs");
});

add_task(async function test_case_insensitive_highlights_1() {
  await testResult(
    {
      query: "exam",
      title: "The examPLE URL EXAMple",
      url: "https://example.com/ExAm",
    },
    {
      displayedUrl: "example.com/ExAm",
      highlightedTitle: [
        ["The ", false],
        ["exam", true],
        ["PLE URL ", false],
        ["EXAM", true],
        ["ple", false],
      ],
      highlightedUrl: [
        ["exam", true],
        ["ple.com/", false],
        ["ExAm", true],
      ],
    }
  );
});

add_task(async function test_case_insensitive_highlights_2() {
  await testResult(
    {
      query: "EXAM",
      title: "The examPLE URL EXAMple",
      url: "https://example.com/ExAm",
    },
    {
      displayedUrl: "example.com/ExAm",
      highlightedTitle: [
        ["The ", false],
        ["exam", true],
        ["PLE URL ", false],
        ["EXAM", true],
        ["ple", false],
      ],
      highlightedUrl: [
        ["exam", true],
        ["ple.com/", false],
        ["ExAm", true],
      ],
    }
  );
});

add_task(async function test_case_insensitive_highlights_3() {
  await testResult(
    {
      query: "eXaM",
      title: "The examPLE URL EXAMple",
      url: "https://example.com/ExAm",
    },
    {
      displayedUrl: "example.com/ExAm",
      highlightedTitle: [
        ["The ", false],
        ["exam", true],
        ["PLE URL ", false],
        ["EXAM", true],
        ["ple", false],
      ],
      highlightedUrl: [
        ["exam", true],
        ["ple.com/", false],
        ["ExAm", true],
      ],
    }
  );
});

add_task(async function test_case_insensitive_highlights_4() {
  await testResult(
    {
      query: "ExAm",
      title: "The examPLE URL EXAMple",
      url: "https://example.com/ExAm",
    },
    {
      displayedUrl: "example.com/ExAm",
      highlightedTitle: [
        ["The ", false],
        ["exam", true],
        ["PLE URL ", false],
        ["EXAM", true],
        ["ple", false],
      ],
      highlightedUrl: [
        ["exam", true],
        ["ple.com/", false],
        ["ExAm", true],
      ],
    }
  );
});

add_task(async function test_case_insensitive_highlights_5() {
  await testResult(
    {
      query: "exam foo",
      title: "The examPLE URL foo EXAMple FOO",
      url: "https://example.com/ExAm/fOo",
    },
    {
      displayedUrl: "example.com/ExAm/fOo",
      highlightedTitle: [
        ["The ", false],
        ["exam", true],
        ["PLE URL ", false],
        ["foo", true],
        [" ", false],
        ["EXAM", true],
        ["ple ", false],
        ["FOO", true],
      ],
      highlightedUrl: [
        ["exam", true],
        ["ple.com/", false],
        ["ExAm", true],
        ["/", false],
        ["fOo", true],
      ],
    }
  );
});

add_task(async function test_case_insensitive_highlights_6() {
  await testResult(
    {
      query: "EXAM FOO",
      title: "The examPLE URL foo EXAMple FOO",
      url: "https://example.com/ExAm/fOo",
    },
    {
      displayedUrl: "example.com/ExAm/fOo",
      highlightedTitle: [
        ["The ", false],
        ["exam", true],
        ["PLE URL ", false],
        ["foo", true],
        [" ", false],
        ["EXAM", true],
        ["ple ", false],
        ["FOO", true],
      ],
      highlightedUrl: [
        ["exam", true],
        ["ple.com/", false],
        ["ExAm", true],
        ["/", false],
        ["fOo", true],
      ],
    }
  );
});
