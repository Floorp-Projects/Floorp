/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function busyChecker(isBusy) {
  return function (event) {
    let scEvent;
    try {
      scEvent = event.QueryInterface(nsIAccessibleStateChangeEvent);
    } catch (e) {
      return false;
    }

    return scEvent.state == STATE_BUSY && scEvent.isEnabled == isBusy;
  };
}

function inIframeChecker(iframeId) {
  return function (event) {
    return getAccessibleDOMNodeID(event.accessibleDocument.parent) == iframeId;
  };
}

function urlChecker(url) {
  return function (event) {
    info(`${event.accessibleDocument.URL} == ${url}`);
    return event.accessibleDocument.URL == url;
  };
}

async function runTests(browser, accDoc) {
  let onLoadEvents = waitForEvents({
    expected: [
      [EVENT_REORDER, getAccessible(browser)],
      [EVENT_DOCUMENT_LOAD_COMPLETE, "body2"],
      [EVENT_STATE_CHANGE, busyChecker(false)],
    ],
    unexpected: [
      [EVENT_DOCUMENT_LOAD_COMPLETE, inIframeChecker("iframe1")],
      [EVENT_STATE_CHANGE, inIframeChecker("iframe1")],
    ],
  });

  BrowserTestUtils.startLoadingURIString(
    browser,
    `data:text/html;charset=utf-8,
    <html><body id="body2">
      <iframe id="iframe1" src="http://example.com"></iframe>
    </body></html>`
  );

  await onLoadEvents;

  onLoadEvents = waitForEvents([
    [EVENT_DOCUMENT_LOAD_COMPLETE, urlChecker("about:about")],
    [EVENT_STATE_CHANGE, busyChecker(false)],
    [EVENT_REORDER, getAccessible(browser)],
  ]);

  BrowserTestUtils.startLoadingURIString(browser, "about:about");

  await onLoadEvents;

  onLoadEvents = waitForEvents([
    [EVENT_DOCUMENT_RELOAD, evt => evt.isFromUserInput],
    [EVENT_REORDER, getAccessible(browser)],
    [EVENT_STATE_CHANGE, busyChecker(false)],
  ]);

  EventUtils.synthesizeKey("VK_F5", {}, browser.ownerGlobal);

  await onLoadEvents;

  onLoadEvents = waitForEvents([
    [EVENT_DOCUMENT_LOAD_COMPLETE, urlChecker("about:mozilla")],
    [EVENT_STATE_CHANGE, busyChecker(false)],
    [EVENT_REORDER, getAccessible(browser)],
  ]);

  BrowserTestUtils.startLoadingURIString(browser, "about:mozilla");

  await onLoadEvents;

  onLoadEvents = waitForEvents([
    [EVENT_DOCUMENT_RELOAD, evt => !evt.isFromUserInput],
    [EVENT_REORDER, getAccessible(browser)],
    [EVENT_STATE_CHANGE, busyChecker(false)],
  ]);

  browser.reload();

  await onLoadEvents;

  onLoadEvents = waitForEvents([
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    [EVENT_DOCUMENT_LOAD_COMPLETE, urlChecker("http://www.wronguri.wronguri/")],
    [EVENT_STATE_CHANGE, busyChecker(false)],
    [EVENT_REORDER, getAccessible(browser)],
  ]);

  BrowserTestUtils.startLoadingURIString(
    browser,
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    "http://www.wronguri.wronguri/"
  );

  await onLoadEvents;

  onLoadEvents = waitForEvents([
    [EVENT_DOCUMENT_LOAD_COMPLETE, urlChecker("https://nocert.example.com/")],
    [EVENT_STATE_CHANGE, busyChecker(false)],
    [EVENT_REORDER, getAccessible(browser)],
  ]);

  BrowserTestUtils.startLoadingURIString(
    browser,
    "https://nocert.example.com:443/"
  );

  await onLoadEvents;
}

/**
 * Test caching of accessible object states
 */
addAccessibleTask("", runTests);
