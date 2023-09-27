/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const RESOURCE_LINK =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "browser_contextmenu_loadblobinnewtab.html";

const blobDataAsString = `!"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ`;

// Helper method to right click on the provided link (selector as id),
// open in new tab and return the content of the first <pre> under the
// <body> of the new tab's document.
async function rightClickOpenInNewTabAndReturnContent(selector) {
  const loaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    RESOURCE_LINK
  );
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    RESOURCE_LINK
  );
  await loaded;

  const generatedBlobURL = await ContentTask.spawn(
    gBrowser.selectedBrowser,
    { selector },
    async args => {
      return content.document.getElementById(args.selector).href;
    }
  );

  const contextMenu = document.getElementById("contentAreaContextMenu");
  is(contextMenu.state, "closed", "checking if context menu is closed");

  let awaitPopupShown = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#" + selector,
    { type: "contextmenu", button: 2 },
    gBrowser.selectedBrowser
  );
  await awaitPopupShown;

  let awaitPopupHidden = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popuphidden"
  );

  const openPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    generatedBlobURL,
    false
  );

  document.getElementById("context-openlinkintab").doCommand();

  contextMenu.hidePopup();
  await awaitPopupHidden;

  let openTab = await openPromise;

  await BrowserTestUtils.switchTab(gBrowser, gBrowser.tabs[1]);

  let blobDataFromContent = await ContentTask.spawn(
    gBrowser.selectedBrowser,
    null,
    async function () {
      while (!content.document.querySelector("body pre")) {
        await new Promise(resolve =>
          content.setTimeout(() => {
            resolve();
          }, 100)
        );
      }
      return content.document.body.firstElementChild.innerText.trim();
    }
  );

  let tabClosed = BrowserTestUtils.waitForTabClosing(openTab);
  await BrowserTestUtils.removeTab(openTab);
  await tabClosed;

  return blobDataFromContent;
}

// Helper method to open selected link in new tab (selector as id),
// and return the content of the first <pre> under the <body> of
// the new tab's document.
async function openInNewTabAndReturnContent(selector) {
  const loaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    RESOURCE_LINK
  );
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    RESOURCE_LINK
  );
  await loaded;

  const generatedBlobURL = await ContentTask.spawn(
    gBrowser.selectedBrowser,
    { selector },
    async args => {
      return content.document.getElementById(args.selector).href;
    }
  );

  let openTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    generatedBlobURL
  );

  let blobDataFromContent = await ContentTask.spawn(
    gBrowser.selectedBrowser,
    null,
    async function () {
      while (!content.document.querySelector("body pre")) {
        await new Promise(resolve =>
          content.setTimeout(() => {
            resolve();
          }, 100)
        );
      }
      return content.document.body.firstElementChild.innerText.trim();
    }
  );

  let tabClosed = BrowserTestUtils.waitForTabClosing(openTab);
  await BrowserTestUtils.removeTab(openTab);
  await tabClosed;

  return blobDataFromContent;
}

add_task(async function test_rightclick_open_bloburl_in_new_tab() {
  let blobDataFromLoadedPage = await rightClickOpenInNewTabAndReturnContent(
    "blob-url-link"
  );
  is(
    blobDataFromLoadedPage,
    blobDataAsString,
    "The blobURL is correctly loaded"
  );
});

add_task(async function test_rightclick_open_bloburl_referrer_in_new_tab() {
  let blobDataFromLoadedPage = await rightClickOpenInNewTabAndReturnContent(
    "blob-url-referrer-link"
  );
  is(
    blobDataFromLoadedPage,
    blobDataAsString,
    "The blobURL is correctly loaded"
  );
});

add_task(async function test_open_bloburl_in_new_tab() {
  let blobDataFromLoadedPage = await openInNewTabAndReturnContent(
    "blob-url-link"
  );
  is(
    blobDataFromLoadedPage,
    blobDataAsString,
    "The blobURL is correctly loaded"
  );
});

add_task(async function test_open_bloburl_referrer_in_new_tab() {
  let blobDataFromLoadedPage = await openInNewTabAndReturnContent(
    "blob-url-referrer-link"
  );
  is(
    blobDataFromLoadedPage,
    blobDataAsString,
    "The blobURL is correctly loaded"
  );
});
