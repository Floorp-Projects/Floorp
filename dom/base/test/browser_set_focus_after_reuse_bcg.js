/* -*- Mode: JavaScript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const SITE_A_URL =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "file_set_focus_after_reuse_bcg_1.html";

const SITE_B_URL =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.org"
  ) + "file_set_focus_after_reuse_bcg_2.html";

async function test_set_focus_after_reuse_bcg() {
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, SITE_A_URL);

  async function clickButtonToNavigateSiteB() {
    // Navigate to site B
    const siteBLoaded = BrowserTestUtils.browserLoaded(
      tab.linkedBrowser,
      false,
      SITE_B_URL
    );
    await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
      var button = content.document.querySelector("button");
      button.click();
    });
    await siteBLoaded;
  }

  async function clickButtonToEmbedIframe() {
    // Make site B to embed the iframe that has the same origin as site A.
    const embeddedIframeLoaded = SpecialPowers.spawn(
      tab.linkedBrowser,
      [],
      async function () {
        await new Promise(r => {
          const iframe = content.document.querySelector("iframe");
          iframe.onload = r;
        });
      }
    );

    await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
      var button = content.document.querySelector("button");
      button.click();
    });
    await embeddedIframeLoaded;
  }

  await clickButtonToNavigateSiteB();

  await clickButtonToEmbedIframe();

  // Navigate back to site A.
  const pageNavigatedBackToSite1 = BrowserTestUtils.waitForContentEvent(
    tab.linkedBrowser,
    "pageshow"
  );
  tab.linkedBrowser.goBack();
  await pageNavigatedBackToSite1;

  // Reload site A.
  // This reloading is quite important to reproduce this bug as it'll
  // sync some BFCache status to the parent process for site A, which
  // allows the BCG to reuse the already-subscribed process of site A.
  await BrowserTestUtils.reloadTab(tab, true);

  // TODO (sefeng): If we use tab.linkedBrowser.goForward() for
  // the navigation, it'll trigger bug 1917343.
  await clickButtonToNavigateSiteB();

  await clickButtonToEmbedIframe();

  // Wait for the <input> to be focused within the embedded iframe.
  const activeBCInIframeProcess = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    async function () {
      let iframe = content.document.querySelector("iframe");

      // Get the active browsing context of the process that the iframe belongs to
      return SpecialPowers.spawn(iframe, [], () => {
        const FocusManager = SpecialPowers.Services.focus;
        return FocusManager.activeBrowsingContext;
      });
    }
  );

  Assert.ok(
    !!activeBCInIframeProcess,
    "activeBC should be set to the iframe process"
  );
  BrowserTestUtils.removeTab(tab);
}

add_task(async function () {
  await test_set_focus_after_reuse_bcg();
});
