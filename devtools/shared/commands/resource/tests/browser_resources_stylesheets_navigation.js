/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceCommand API around STYLESHEET and navigation (reloading, creation of new browsing context, …)

const ORG_DOC_BUILDER = "https://example.org/document-builder.sjs";
const COM_DOC_BUILDER = "https://example.com/document-builder.sjs";

// Since the order of resources is not guaranteed, we put a number in the title attribute
// of the <style> elements so we can sort them in a way that makes it easier for us to assert.
let currentStyleTitle = 0;

const TEST_URI =
  `${ORG_DOC_BUILDER}?html=1<h1>top-level example.org</h1>` +
  `<style title="${currentStyleTitle++}">.top-level-org{}</style>` +
  `<iframe id="same-origin-1" src="${ORG_DOC_BUILDER}?html=<h2>example.org 1</h2><style title=${currentStyleTitle++}>.frame-org-1{}</style>"></iframe>` +
  `<iframe id="same-origin-2" src="${ORG_DOC_BUILDER}?html=<h2>example.org 2</h2><style title=${currentStyleTitle++}>.frame-org-2{}</style>"></iframe>` +
  `<iframe id="remote-origin-1" src="${COM_DOC_BUILDER}?html=<h2>example.com 1</h2><style title=${currentStyleTitle++}>.frame-com-1{}</style>"></iframe>` +
  `<iframe id="remote-origin-2" src="${COM_DOC_BUILDER}?html=<h2>example.com 2</h2><style title=${currentStyleTitle++}>.frame-com-2{}</style>"></iframe>`;

const COOP_HEADERS = "Cross-Origin-Opener-Policy:same-origin";
const TEST_URI_NEW_BROWSING_CONTEXT =
  `${ORG_DOC_BUILDER}?headers=${COOP_HEADERS}` +
  `&html=<h1>top-level example.org</div>` +
  `<style>.top-level-org-new-bc{}</style>`;

add_task(async function() {
  info(
    "Open a new tab and check that styleSheetChangeEventsEnabled is false by default"
  );
  const tab = await addTab(TEST_URI);

  is(
    await getDocumentStyleSheetChangeEventsEnabled(tab.linkedBrowser),
    false,
    `styleSheetChangeEventsEnabled is false at the beginning`
  );

  const { client, resourceCommand, targetCommand } = await initResourceCommand(
    tab
  );

  let availableResources = [];
  await resourceCommand.watchResources([resourceCommand.TYPES.STYLESHEET], {
    onAvailable: resources => {
      availableResources.push(...resources);
    },
  });

  info("Wait for all the stylesheets resources of main document and iframes");
  await waitFor(() => availableResources.length === 5);
  is(availableResources.length, 5, "Retrieved the expected stylesheets");

  // the order of the resources is not guaranteed.
  sortResourcesByExpectedOrder(availableResources);
  await assertResource(availableResources[0], {
    styleText: `.top-level-org{}`,
  });
  await assertResource(availableResources[1], {
    styleText: `.frame-org-1{}`,
  });
  await assertResource(availableResources[2], {
    styleText: `.frame-org-2{}`,
  });
  await assertResource(availableResources[3], {
    styleText: `.frame-com-1{}`,
  });
  await assertResource(availableResources[4], {
    styleText: `.frame-com-2{}`,
  });

  // clear availableResources so it's easier to test
  availableResources = [];

  is(
    await getDocumentStyleSheetChangeEventsEnabled(tab.linkedBrowser),
    true,
    `styleSheetChangeEventsEnabled is true after watching stylesheets`
  );

  info("Navigate a remote frame to a different page");
  const iframeNewUrl =
    `https://example.com/document-builder.sjs?` +
    `html=<h2>example.com new bc</h2><style title=6>.frame-com-new-bc{}</style>`;
  await SpecialPowers.spawn(tab.linkedBrowser, [iframeNewUrl], url => {
    const { browsingContext } = content.document.querySelector(
      "#remote-origin-2"
    );
    return SpecialPowers.spawn(browsingContext, [url], innerUrl => {
      content.document.location = innerUrl;
    });
  });
  await waitFor(() => availableResources.length == 1);
  ok(true, "We're notified about the iframe new document stylesheet");
  await assertResource(availableResources[0], {
    styleText: `.frame-com-new-bc{}`,
  });
  const iframeNewBrowsingContext = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    () => content.document.querySelector("#remote-origin-2").browsingContext
  );

  is(
    await getDocumentStyleSheetChangeEventsEnabled(iframeNewBrowsingContext),
    true,
    `styleSheetChangeEventsEnabled is still true after navigating the iframe`
  );

  // clear availableResources so it's easier to test
  availableResources = [];

  info("Check that styleSheetChangeEventsEnabled persist after reloading");
  await refreshTab();

  // ⚠️ We're only getting the stylesheets for the top-level document and the remote frames;
  // the same-origin iframes stylesheets are missing.
  // This should be fixed in Bug 1725547.
  info(
    "Wait until we're notified about all the stylesheets (top-level document + iframe)"
  );
  await waitFor(() => availableResources.length === 3);
  is(
    availableResources.length,
    3,
    "Retrieved the expected stylesheets after the page was reloaded"
  );

  // the order of the resources is not guaranteed.
  sortResourcesByExpectedOrder(availableResources);
  await assertResource(availableResources[0], {
    styleText: `.top-level-org{}`,
  });
  // This should be uncommented as part of Bug 1725547.
  // await assertResource(availableResources[1], {
  //   styleText: `.frame-org-1{}`,
  // });
  // await assertResource(availableResources[2], {
  //   styleText: `.frame-org-2{}`,
  // });
  await assertResource(availableResources[1], {
    styleText: `.frame-com-1{}`,
  });
  await assertResource(availableResources[2], {
    styleText: `.frame-com-new-bc{}`,
  });

  is(
    await getDocumentStyleSheetChangeEventsEnabled(tab.linkedBrowser),
    true,
    `styleSheetChangeEventsEnabled is still true on the top level document after reloading`
  );

  // This should be uncommented as part of Bug 1725547.
  // const bc = await SpecialPowers.spawn(
  //   tab.linkedBrowser,
  //   [],
  //   () => content.document.querySelector("#same-origin-1").browsingContext
  // );
  // is(
  //   await getDocumentStyleSheetChangeEventsEnabled(bc),
  //   true,
  //   `styleSheetChangeEventsEnabled is still true on the iframe after reloading`
  // );

  // clear availableResources so it's easier to test
  availableResources = [];

  info(
    "Check that styleSheetChangeEventsEnabled persist when navigating to a page that creates a new browsing context"
  );
  const previousBrowsingContextId = tab.linkedBrowser.browsingContext.id;
  const onLoaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await BrowserTestUtils.loadURI(
    tab.linkedBrowser,
    TEST_URI_NEW_BROWSING_CONTEXT
  );
  await onLoaded;

  isnot(
    tab.linkedBrowser.browsingContext.id,
    previousBrowsingContextId,
    "A new browsing context was created"
  );

  info("Wait to get the stylesheet for the new document");
  await waitFor(() => availableResources.length === 1);
  ok(true, "We received the stylesheet for the new document");
  await assertResource(availableResources[0], {
    styleText: `.top-level-org-new-bc{}`,
  });
  is(
    await getDocumentStyleSheetChangeEventsEnabled(tab.linkedBrowser),
    true,
    `styleSheetChangeEventsEnabled is still true after navigating to a new browsing context`
  );

  targetCommand.destroy();
  await client.close();
});

/**
 * Returns the value of the browser/browsingContext document `styleSheetChangeEventsEnabled`
 * property.
 *
 * @param {Browser|BrowsingContext} browserOrBrowsingContext: The browser element or a
 *        browsing context.
 * @returns {Promise<Boolean>}
 */
function getDocumentStyleSheetChangeEventsEnabled(browserOrBrowsingContext) {
  return SpecialPowers.spawn(browserOrBrowsingContext, [], () => {
    return content.document.styleSheetChangeEventsEnabled;
  });
}

/**
 * Sort the passed array of stylesheet resources.
 *
 * Since the order of resources are not guaranteed, the <style> elements we use in this test
 * have a "title" attribute that represent their expected order so we can sort them in
 * a way that makes it easier for us to assert.
 *
 * @param {Array<Object>} resources: Array of stylesheet resources
 */
function sortResourcesByExpectedOrder(resources) {
  resources.sort((a, b) => {
    return Number(a.title) > Number(b.title);
  });
}

/**
 * Check that the resources have the expected text
 *
 * @param {Array<Object>} resources: Array of stylesheet resources
 * @param {Array<Object>} expected: Array of object of the following shape:
 * @param {Object} expected[]
 * @param {Object} expected[].styleText: Expected text content of the stylesheet
 */
async function assertResource(resource, expected) {
  const styleSheetsFront = await resource.targetFront.getFront("stylesheets");
  const styleText = (
    await styleSheetsFront.getText(resource.resourceId)
  ).str.trim();
  is(styleText, expected.styleText, "Style text is correct");
}
