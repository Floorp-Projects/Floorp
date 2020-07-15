/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI =
  "https://example.com/browser/devtools/client/webconsole/test/browser/test-storageaccess-errors.html";
const LEARN_MORE_URI =
  "https://developer.mozilla.org/docs/Web/API/Document/requestStorageAccess" +
  DOCS_GA_PARAMS;

const { UrlClassifierTestUtils } = ChromeUtils.import(
  "resource://testing-common/UrlClassifierTestUtils.jsm"
);

UrlClassifierTestUtils.addTestTrackers();
registerCleanupFunction(function() {
  UrlClassifierTestUtils.cleanupTestTrackers();
});

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  async function checkErrorMessage(text) {
    const message = await waitFor(
      () => findMessage(hud, text, ".message.error"),
      undefined,
      100
    );
    ok(true, "Error message is visible: " + text);

    const checkLink = ({ link, where, expectedLink, expectedTab }) => {
      is(link, expectedLink, `Clicking the provided link opens ${link}`);
      is(
        where,
        expectedTab,
        `Clicking the provided link opens in expected tab`
      );
    };

    info("Clicking on the Learn More link");
    const learnMoreLink = message.querySelector(".learn-more-link");
    const linkSimulation = await simulateLinkClick(learnMoreLink);
    checkLink({
      ...linkSimulation,
      expectedLink: LEARN_MORE_URI,
      expectedTab: "tab",
    });
  }

  const userGesture =
    "document.requestStorageAccess() may only be requested from inside a short running user-generated event handler";
  const nested =
    "document.requestStorageAccess() may not be called in a nested iframe.";
  const nullPrincipal =
    "document.requestStorageAccess() may not be called on a document with an opaque origin, such as a sandboxed iframe without allow-same-origin in its sandbox attribute.";
  const sandboxed =
    "document.requestStorageAccess() may not be called in a sandboxed iframe without allow-storage-access-by-user-activation in its sandbox attribute.";

  await checkErrorMessage(userGesture);
  await checkErrorMessage(nullPrincipal);
  await checkErrorMessage(nested);
  await checkErrorMessage(sandboxed);

  await closeConsole();
});
