/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI_FIRST_PARTY = "https://example.com";
const TEST_URI_THIRD_PARTY = "https://itisatracker.org";
const LEARN_MORE_URI =
  "https://developer.mozilla.org/docs/Web/API/Document/requestStorageAccess" +
  DOCS_GA_PARAMS;

const { UrlClassifierTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/UrlClassifierTestUtils.sys.mjs"
);

UrlClassifierTestUtils.addTestTrackers();
registerCleanupFunction(function () {
  UrlClassifierTestUtils.cleanupTestTrackers();
});

/**
 * Run document.requestStorageAccess in an iframe.
 * @param {Object} options - Request / iframe options.
 * @param {boolean} [options.withUserActivation] - Whether the requesting iframe
 * should have user activation prior to calling rsA.
 * @param {string} [options.sandboxAttr] - Iframe sandbox attributes.
 * @param {boolean} [options.nested] - If the iframe calling rsA should be
 * nested in another same-origin iframe.
 */
async function runRequestStorageAccess({
  withUserActivation = false,
  sandboxAttr = "",
  nested = false,
}) {
  let parentBC = gBrowser.selectedBrowser.browsingContext;

  // Spawn the rsA iframe in an iframe.
  if (nested) {
    parentBC = await SpecialPowers.spawn(
      parentBC,
      [TEST_URI_THIRD_PARTY],
      async uri => {
        const frame = content.document.createElement("iframe");
        frame.setAttribute("src", uri);
        const loadPromise = ContentTaskUtils.waitForEvent(frame, "load");
        content.document.body.appendChild(frame);
        await loadPromise;
        return frame.browsingContext;
      }
    );
  }

  // Create an iframe which is a third party to the top level.
  const frameBC = await SpecialPowers.spawn(
    parentBC,
    [TEST_URI_THIRD_PARTY, sandboxAttr],
    async (uri, sandbox) => {
      const frame = content.document.createElement("iframe");
      frame.setAttribute("src", uri);
      if (sandbox) {
        frame.setAttribute("sandbox", sandbox);
      }
      const loadPromise = ContentTaskUtils.waitForEvent(frame, "load");
      content.document.body.appendChild(frame);
      await loadPromise;
      return frame.browsingContext;
    }
  );

  // Call requestStorageAccess in the iframe.
  await SpecialPowers.spawn(frameBC, [withUserActivation], userActivation => {
    if (userActivation) {
      content.document.notifyUserGestureActivation();
    }
    content.document.requestStorageAccess();
  });
}

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI_FIRST_PARTY);

  async function checkErrorMessage(text) {
    const message = await waitFor(
      () => findErrorMessage(hud, text),
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
  const nullPrincipal =
    "document.requestStorageAccess() may not be called on a document with an opaque origin, such as a sandboxed iframe without allow-same-origin in its sandbox attribute.";
  const sandboxed =
    "document.requestStorageAccess() may not be called in a sandboxed iframe without allow-storage-access-by-user-activation in its sandbox attribute.";

  await runRequestStorageAccess({ withUserActivation: false });
  await checkErrorMessage(userGesture);

  await runRequestStorageAccess({
    withUserActivation: true,
    sandboxAttr: "allow-scripts",
  });
  await checkErrorMessage(nullPrincipal);

  await runRequestStorageAccess({
    withUserActivation: true,
    sandboxAttr: "allow-same-origin allow-scripts",
  });
  await checkErrorMessage(sandboxed);

  await closeConsole();
});
