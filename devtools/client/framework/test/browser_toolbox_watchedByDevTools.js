/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that the "watchedByDevTools" flag is properly handled.
 */
const EXAMPLE_NET_URI =
  "http://example.net/document-builder.sjs?html=<div id=net>net";
const EXAMPLE_COM_URI =
  "https://example.com/document-builder.sjs?html=<div id=com>com";
const EXAMPLE_ORG_URI =
  "https://example.org/document-builder.sjs?headers=Cross-Origin-Opener-Policy:same-origin&html=<div id=org>org</div>";

add_task(async function() {
  const tab = await addTab(EXAMPLE_NET_URI);

  is(
    tab.linkedBrowser.browsingContext.watchedByDevTools,
    false,
    "watchedByDevTools isn't set when DevTools aren't opened"
  );

  info(
    "Open a toolbox for the opened tab and check that watchedByDevTools is set"
  );
  await gDevTools.showToolboxForTab(tab, { toolId: "options" });

  is(
    tab.linkedBrowser.browsingContext.watchedByDevTools,
    true,
    "watchedByDevTools is set after opening a toolbox"
  );

  info(
    "Check that watchedByDevTools persist when the tab navigates to a different origin"
  );
  await navigateTo(EXAMPLE_COM_URI);

  is(
    tab.linkedBrowser.browsingContext.watchedByDevTools,
    true,
    "watchedByDevTools is still set after navigating to a different origin"
  );

  info(
    "Check that watchedByDevTools persist when navigating to a page that creates a new browsing context"
  );
  const previousBrowsingContextId = tab.linkedBrowser.browsingContext.id;
  await navigateTo(EXAMPLE_ORG_URI);

  isnot(
    tab.linkedBrowser.browsingContext.id,
    previousBrowsingContextId,
    "A new browsing context was created"
  );

  is(
    tab.linkedBrowser.browsingContext.watchedByDevTools,
    true,
    "watchedByDevTools is still set after navigating to a new browsing context"
  );

  info("Check that the flag is reset when the toolbox is closed");
  await gDevTools.closeToolboxForTab(tab);
  is(
    tab.linkedBrowser.browsingContext.watchedByDevTools,
    false,
    "watchedByDevTools is reset after closing the toolbox"
  );
});
