/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const ORIGIN = "https://example.com";
const CROSS_SUBFRAME_PAGE =
  getRootDirectory(gTestPath).replace("chrome://mochitests/content", ORIGIN) +
  "temporary_permissions_subframe.html";

const PromptResult = {
  ALLOW: "allow",
  DENY: "deny",
  PROMPT: "prompt",
};

var Perms = Services.perms;
var uri = NetUtil.newURI(ORIGIN);
var principal = Services.scriptSecurityManager.createContentPrincipal(uri, {});

add_task(async function setup() {
  await new Promise(r => {
    SpecialPowers.pushPrefEnv(
      {
        set: [
          ["dom.security.featurePolicy.enabled", true],
          ["dom.security.featurePolicy.header.enabled", true],
          ["dom.security.featurePolicy.webidl.enabled", true],
          ["permissions.delegation.enabled", true],
        ],
      },
      r
    );
  });
});

// Test that temp blocked permissions in first party affect the third party
// iframe.
add_task(async function testUseTempPermissionsFirstParty() {
  await BrowserTestUtils.withNewTab(CROSS_SUBFRAME_PAGE, async function(
    browser
  ) {
    SitePermissions.setForPrincipal(
      principal,
      "geo",
      SitePermissions.BLOCK,
      SitePermissions.SCOPE_TEMPORARY,
      browser
    );

    // Request a permission.
    await ContentTask.spawn(browser, uri.host, async function(host0) {
      let frame = content.document.getElementById("frame");
      function onMessage(event) {
        // Check the result right here because there's no notification
        is(event.data, "deny", "Expected deny for third party");
        content.window.removeEventListener("message", onMessage);
      }

      content.window.addEventListener("message", onMessage);

      await content.SpecialPowers.spawn(frame, [host0], async function(host) {
        const { E10SUtils } = ChromeUtils.import(
          "resource://gre/modules/E10SUtils.jsm"
        );

        E10SUtils.wrapHandlingUserInput(this.content, true, function() {
          let frameDoc = this.content.document;
          frameDoc.getElementById("geo").click();
        });
      });
    });

    SitePermissions.removeFromPrincipal(principal, "geo", browser);
  });
});

// Test that persistent permissions in first party affect the third party
// iframe.
add_task(async function testUsePersistentPermissionsFirstParty() {
  await BrowserTestUtils.withNewTab(CROSS_SUBFRAME_PAGE, async function(
    browser
  ) {
    async function checkPermission(aPermission, aExpect) {
      PermissionTestUtils.add(uri, "geo", aPermission);

      let waitForPrompt = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshown"
      );

      // Request a permission.
      await ContentTask.spawn(
        browser,
        { host: uri.host, expect: aExpect },
        async function(args) {
          let frame = content.document.getElementById("frame");
          if (args.expect != "prompt") {
            function onMessage(event) {
              // Check the result right here because there's no notification
              is(
                event.data,
                args.expect,
                "Expected correct permission for third party"
              );
              content.window.removeEventListener("message", onMessage);
            }
            content.window.addEventListener("message", onMessage);
          }

          await content.SpecialPowers.spawn(frame, [args.host], async function(
            host
          ) {
            const { E10SUtils } = ChromeUtils.import(
              "resource://gre/modules/E10SUtils.jsm"
            );

            E10SUtils.wrapHandlingUserInput(this.content, true, function() {
              let frameDoc = this.content.document;
              frameDoc.getElementById("geo").click();
            });
          });
        }
      );

      if (aExpect == PromptResult.PROMPT) {
        await waitForPrompt;
        // Notification is shown, check label and deny to clean
        let popuphidden = BrowserTestUtils.waitForEvent(
          PopupNotifications.panel,
          "popuphidden"
        );

        let notification = PopupNotifications.panel.firstElementChild;
        // Check the label of the notificaiton should be the first party
        is(
          PopupNotifications.getNotification("geolocation").options.name,
          uri.host,
          "Use first party's origin"
        );

        EventUtils.synthesizeMouseAtCenter(notification.secondaryButton, {});

        await popuphidden;
        SitePermissions.removeFromPrincipal(null, "geo", browser);
      }

      PermissionTestUtils.remove(uri, "geo");
    }

    await checkPermission(Perms.PROMPT_ACTION, PromptResult.PROMPT);
    await checkPermission(Perms.DENY_ACTION, PromptResult.DENY);
    await checkPermission(Perms.ALLOW_ACTION, PromptResult.ALLOW);
  });
});
