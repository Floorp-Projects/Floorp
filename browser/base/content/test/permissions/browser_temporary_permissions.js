/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const ORIGIN = "https://example.com";
const PERMISSIONS_PAGE =
  getRootDirectory(gTestPath).replace("chrome://mochitests/content", ORIGIN) +
  "permissions.html";
const SUBFRAME_PAGE =
  getRootDirectory(gTestPath).replace("chrome://mochitests/content", ORIGIN) +
  "temporary_permissions_subframe.html";

// Test that setting temp permissions triggers a change in the identity block.
add_task(async function testTempPermissionChangeEvents() {
  let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    ORIGIN
  );
  let id = "geo";

  await BrowserTestUtils.withNewTab(ORIGIN, function(browser) {
    SitePermissions.setForPrincipal(
      principal,
      id,
      SitePermissions.BLOCK,
      SitePermissions.SCOPE_TEMPORARY,
      browser
    );

    Assert.deepEqual(SitePermissions.getForPrincipal(principal, id, browser), {
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_TEMPORARY,
    });

    let geoIcon = document.querySelector(
      ".blocked-permission-icon[data-permission-id=geo]"
    );

    Assert.notEqual(
      geoIcon.getBoundingClientRect().width,
      0,
      "geo anchor should be visible"
    );

    SitePermissions.removeFromPrincipal(principal, id, browser);

    Assert.equal(
      geoIcon.getBoundingClientRect().width,
      0,
      "geo anchor should not be visible"
    );
  });
});

// Test that temp blocked permissions requested by subframes (with a different URI) affect the whole page.
add_task(async function testTempPermissionSubframes() {
  let uri = NetUtil.newURI(ORIGIN);
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );
  let id = "geo";

  await BrowserTestUtils.withNewTab(SUBFRAME_PAGE, async function(browser) {
    let popupshown = BrowserTestUtils.waitForEvent(
      PopupNotifications.panel,
      "popupshown"
    );

    await new Promise(r => {
      SpecialPowers.pushPrefEnv(
        {
          set: [
            ["dom.security.featurePolicy.enabled", true],
            ["dom.security.featurePolicy.header.enabled", true],
            ["dom.security.featurePolicy.webidl.enabled", true],
          ],
        },
        r
      );
    });

    // Request a permission.
    await ContentTask.spawn(browser, uri.host, async function(host0) {
      // FIXME(Fission): The load event fires before cross-origin iframes have
      // loaded (bug 1559841).
      if (content.SpecialPowers.useRemoteSubframes) {
        for (let i = 0; i < 800; i++) {
          await new Promise(resolve => content.setTimeout(resolve, 0));
        }
      }

      let frame = content.document.getElementById("frame");

      await content.SpecialPowers.spawn(frame, [host0], async function(host) {
        const { E10SUtils } = ChromeUtils.import(
          "resource://gre/modules/E10SUtils.jsm"
        );

        E10SUtils.wrapHandlingUserInput(this.content, true, function() {
          let frameDoc = this.content.document;

          // Make sure that the origin of our test page is different.
          Assert.notEqual(frameDoc.location.host, host);

          frameDoc.getElementById("geo").click();
        });
      });
    });

    await popupshown;

    let popuphidden = BrowserTestUtils.waitForEvent(
      PopupNotifications.panel,
      "popuphidden"
    );

    let notification = PopupNotifications.panel.firstElementChild;
    EventUtils.synthesizeMouseAtCenter(notification.secondaryButton, {});

    await popuphidden;

    Assert.deepEqual(SitePermissions.getForPrincipal(principal, id, browser), {
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_TEMPORARY,
    });
  });
});
