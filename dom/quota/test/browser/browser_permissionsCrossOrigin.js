/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const emptyURL =
  "https://example.com/browser/dom/quota/test/browser/empty.html";

addTest(async function testNoPermissionPrompt() {
  registerPopupEventHandler("popupshowing", function() {
    ok(false, "Shouldn't show a popup this time");
  });
  registerPopupEventHandler("popupshown", function() {
    ok(false, "Shouldn't show a popup this time");
  });
  registerPopupEventHandler("popuphidden", function() {
    ok(false, "Shouldn't show a popup this time");
  });

  info("Creating tab");

  await BrowserTestUtils.withNewTab(emptyURL, async function(browser) {
    await new Promise(r => {
      SpecialPowers.pushPrefEnv(
        {
          set: [
            ["dom.security.featurePolicy.enabled", true],
            ["permissions.delegation.enabled", true],
            ["dom.security.featurePolicy.header.enabled", true],
            ["dom.security.featurePolicy.webidl.enabled", true],
          ],
        },
        r
      );
    });

    await SpecialPowers.spawn(browser, [], async function(host0) {
      let frame = content.document.createElement("iframe");
      // Cross origin src
      frame.src = "https://example.org/browser/dom/quota/test/empty.html";
      content.document.body.appendChild(frame);
      await ContentTaskUtils.waitForEvent(frame, "load");

      await content.SpecialPowers.spawn(frame, [], async function() {
        // Request a permission.
        const persistAllowed = await this.content.navigator.storage.persist();
        Assert.ok(
          !persistAllowed,
          "navigator.storage.persist() has been denied"
        );
      });
      content.document.body.removeChild(frame);
    });
  });

  unregisterAllPopupEventHandlers();
});
