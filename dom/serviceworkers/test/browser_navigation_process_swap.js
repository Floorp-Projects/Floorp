/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test tests a navigation request to a Service Worker-controlled origin &
 * scope that results in a cross-origin redirect to a
 * non-Service Worker-controlled scope which additionally participates in
 * cross-process redirect.
 *
 * On success, the test will not crash.
 */

const ORIGIN = 'http://mochi.test:8888';
const TEST_ROOT = getRootDirectory(gTestPath)
  .replace('chrome://mochitests/content', ORIGIN);

const SW_REGISTER_PAGE_URL = `${TEST_ROOT}empty_with_utils.html`;
const SW_SCRIPT_URL = `${TEST_ROOT}empty.js`;

const FILE_URL = (() => {
  // Get the file as an nsIFile.
  const file = getChromeDir(getResolvedURI(gTestPath));
  file.append('empty.html');

  // Convert the nsIFile to an nsIURI to access the path.
  return Services.io.newFileURI(file).spec;
})();

const CROSS_ORIGIN = 'http://example.com';
const CROSS_ORIGIN_URL = SW_REGISTER_PAGE_URL.replace(ORIGIN, CROSS_ORIGIN);
const CROSS_ORIGIN_REDIRECT_URL =
  `${TEST_ROOT}redirect.sjs?${CROSS_ORIGIN_URL}`;

async function loadURI(aXULBrowser, aURI) {
  const browserLoadedPromise = BrowserTestUtils.browserLoaded(aXULBrowser);
  await BrowserTestUtils.loadURI(aXULBrowser, aURI);

  return browserLoadedPromise;
}

async function runTest() {
  // Step 1: register a Service Worker under `ORIGIN` so that all subsequent
  // requests to `ORIGIN` will be marked as controlled.
  await SpecialPowers.pushPrefEnv({
    'set': [
      ['browser.tabs.remote.useHTTPResponseProcessSelection', true],
      ['dom.serviceWorkers.enabled', true],
      ['dom.serviceWorkers.exemptFromPerDomainMax', true],
      ['dom.serviceWorkers.testing.enabled', true],
      ['devtools.console.stdout.content', true],
    ],
  });

  info(`Loading tab with page ${SW_REGISTER_PAGE_URL}`);
  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: SW_REGISTER_PAGE_URL,
  });
  info(`Loaded page ${SW_REGISTER_PAGE_URL}`);

  info(`Registering Service Worker ${SW_SCRIPT_URL}`);
  await ContentTask.spawn(
    tab.linkedBrowser,
    { scriptURL: SW_SCRIPT_URL },
    async ({ scriptURL }) =>
      await content.wrappedJSObject.registerAndWaitForActive(scriptURL),
  );
  info(`Registered and activated Service Worker ${SW_SCRIPT_URL}`);

  // Step 2: open a page over file:// and navigate to trigger a process swap
  // for the response.
  info(`Loading ${FILE_URL}`)
  await loadURI(tab.linkedBrowser, FILE_URL);

  Assert.equal(tab.linkedBrowser.remoteType, E10SUtils.FILE_REMOTE_TYPE,
               `${FILE_URL} should load in a file process`);

  info(`Dynamically creating ${FILE_URL}'s link`);
  await ContentTask.spawn(
    tab.linkedBrowser,
    { href: CROSS_ORIGIN_REDIRECT_URL },
    ({ href }) => {
      const { document } = content;
      const link = document.createElement('a');
      link.href = href;
      link.id = 'link';
      link.appendChild(document.createTextNode(href));
      document.body.appendChild(link);
    },
  );

  const redirectPromise = BrowserTestUtils.waitForLocationChange(
    gBrowser, CROSS_ORIGIN_URL);

  info('Starting navigation')
  await BrowserTestUtils.synthesizeMouseAtCenter('#link', {},
                                                 tab.linkedBrowser);

  info(`Waiting for location to change to ${CROSS_ORIGIN_URL}`);
  await redirectPromise;

  info('Waiting for the browser to stop')
  await BrowserTestUtils.browserStopped(tab.linkedBrowser);

  Assert.equal(tab.linkedBrowser.remoteType, E10SUtils.WEB_REMOTE_TYPE,
               `${CROSS_ORIGIN_URL} should load in a web-content process`);

  // Step 3: cleanup.
  info('Loading initial page to unregister all Service Workers');
  await loadURI(tab.linkedBrowser, SW_REGISTER_PAGE_URL);

  info('Unregistering all Service Workers');
  await ContentTask.spawn(
    tab.linkedBrowser,
    null,
    async () => await content.wrappedJSObject.unregisterAll(),
  )

  info('Closing tab');
  BrowserTestUtils.removeTab(tab);
}

add_task(runTest);
