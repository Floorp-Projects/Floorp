/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that the "watchedByDevTools" flag is properly handled.
 */

const EXAMPLE_NET_URI =
  "https://example.net/document-builder.sjs?html=<div id=net>net";
const EXAMPLE_COM_URI =
  "https://example.com/document-builder.sjs?html=<div id=com>com";
const EXAMPLE_ORG_URI =
  "https://example.org/document-builder.sjs?headers=Cross-Origin-Opener-Policy:same-origin&html=<div id=org>org</div>";

add_task(async function () {
  await pushPref("devtools.browsertoolbox.scope", "everything");

  const topWindow = Services.wm.getMostRecentWindow("navigator:browser");
  is(
    topWindow.browsingContext.watchedByDevTools,
    false,
    "watchedByDevTools isn't set on the parent process browsing context when DevTools aren't opened"
  );

  // Open 2 tabs that we can check the flag on
  const tabNet = await addTab(EXAMPLE_NET_URI);
  const tabCom = await addTab(EXAMPLE_COM_URI);

  is(
    tabNet.linkedBrowser.browsingContext.watchedByDevTools,
    false,
    "watchedByDevTools is not set on the .net tab"
  );
  is(
    tabCom.linkedBrowser.browsingContext.watchedByDevTools,
    false,
    "watchedByDevTools is not set on the .com tab"
  );

  info("Open the BrowserToolbox so the parent process will be watched");
  const ToolboxTask = await initBrowserToolboxTask();

  is(
    topWindow.browsingContext.watchedByDevTools,
    true,
    "watchedByDevTools is set when the browser toolbox is opened"
  );

  // Open a new tab when the browser toolbox is opened
  const newTab = await addTab(EXAMPLE_COM_URI);

  is(
    tabNet.linkedBrowser.browsingContext.watchedByDevTools,
    true,
    "watchedByDevTools is set on the .net tab browsing context after opening the browser toolbox"
  );
  is(
    tabCom.linkedBrowser.browsingContext.watchedByDevTools,
    true,
    "watchedByDevTools is set on the .com tab browsing context after opening the browser toolbox"
  );

  info(
    "Check that adding watchedByDevTools is set on a tab that was added when the browser toolbox was opened"
  );
  is(
    newTab.linkedBrowser.browsingContext.watchedByDevTools,
    true,
    "watchedByDevTools is set on the newly opened tab"
  );

  info(
    "Check that watchedByDevTools persist when navigating to a page that creates a new browsing context"
  );
  const previousBrowsingContextId = newTab.linkedBrowser.browsingContext.id;
  const onBrowserLoaded = BrowserTestUtils.browserLoaded(
    newTab.linkedBrowser,
    false,
    encodeURI(EXAMPLE_ORG_URI)
  );
  BrowserTestUtils.startLoadingURIString(newTab.linkedBrowser, EXAMPLE_ORG_URI);
  await onBrowserLoaded;

  isnot(
    newTab.linkedBrowser.browsingContext.id,
    previousBrowsingContextId,
    "A new browsing context was created"
  );

  is(
    newTab.linkedBrowser.browsingContext.watchedByDevTools,
    true,
    "watchedByDevTools is still set after navigating the tab to a page which forces a new browsing context"
  );

  info("Destroying browser toolbox");
  await ToolboxTask.destroy();

  is(
    topWindow.browsingContext.watchedByDevTools,
    false,
    "watchedByDevTools was reset when the browser toolbox was closed"
  );

  is(
    tabNet.linkedBrowser.browsingContext.watchedByDevTools,
    false,
    "watchedByDevTools was reset on the .net tab after closing the browser toolbox"
  );
  is(
    tabCom.linkedBrowser.browsingContext.watchedByDevTools,
    false,
    "watchedByDevTools was reset on the .com tab after closing the browser toolbox"
  );
  is(
    newTab.linkedBrowser.browsingContext.watchedByDevTools,
    false,
    "watchedByDevTools was reset on the tab opened while the browser toolbox was opened"
  );
});
