/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function promiseEventDocumentLoadComplete(expectedURL) {
  return new Promise(resolve => {
    waitForEvent(EVENT_DOCUMENT_LOAD_COMPLETE, event => {
      try {
        if (
          event.accessible.QueryInterface(nsIAccessibleDocument).URL ==
          expectedURL
        ) {
          resolve(event.accessible.QueryInterface(nsIAccessibleDocument));
          return true;
        }
        return false;
      } catch (e) {
        return false;
      }
    });
  });
}

add_task(async function testInDataURI() {
  const kURL = "data:text/html,Some text";
  const waitForDocumentLoadComplete = promiseEventDocumentLoadComplete("");
  await BrowserTestUtils.withNewTab(kURL, async () => {
    is(
      (await waitForDocumentLoadComplete).URL,
      "",
      "nsIAccessibleDocument.URL shouldn't return data URI"
    );
  });
});

add_task(async function testInHTTPSURIContainingPrivateThings() {
  await SpecialPowers.pushPrefEnv({
    set: [["network.auth.confirmAuth.enabled", false]],
  });
  const kURL =
    "https://username:password@example.com/browser/toolkit/content/tests/browser/file_empty.html?query=some#ref";
  const kURLWithoutUserPass =
    "https://example.com/browser/toolkit/content/tests/browser/file_empty.html?query=some#ref";
  const waitForDocumentLoadComplete =
    promiseEventDocumentLoadComplete(kURLWithoutUserPass);
  await BrowserTestUtils.withNewTab(kURL, async () => {
    is(
      (await waitForDocumentLoadComplete).URL,
      kURLWithoutUserPass,
      "nsIAccessibleDocument.URL shouldn't contain user/pass section"
    );
  });
});
