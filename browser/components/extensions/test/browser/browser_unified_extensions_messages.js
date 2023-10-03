/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

loadTestSubscript("head_unified_extensions.js");

const verifyMessageBar = message => {
  Assert.equal(
    message.getAttribute("type"),
    "warning",
    "expected warning message"
  );
  Assert.ok(
    !message.hasAttribute("dismissable"),
    "expected message to not be dismissable"
  );

  const supportLink = message.querySelector("a");
  Assert.equal(
    supportLink.getAttribute("support-page"),
    "quarantined-domains",
    "expected the correct support page ID"
  );
  Assert.equal(
    supportLink.getAttribute("aria-label"),
    "Learn more: Some extensions are not allowed",
    "expected the correct aria-labelledby value"
  );
};

add_task(async function test_quarantined_domain_message_disabled() {
  const quarantinedDomain = "example.org";
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.quarantinedDomains.enabled", false],
      ["extensions.quarantinedDomains.list", quarantinedDomain],
    ],
  });

  // Load an extension that will have access to all domains, including the
  // quarantined domain.
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["activeTab"],
      browser_action: {},
    },
  });
  await extension.startup();

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: `https://${quarantinedDomain}/` },
    async () => {
      await openExtensionsPanel();
      Assert.equal(getMessageBars().length, 0, "expected no message");
      await closeExtensionsPanel();
    }
  );

  await extension.unload();
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_quarantined_domain_message() {
  const quarantinedDomain = "example.org";
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.quarantinedDomains.enabled", true],
      ["extensions.quarantinedDomains.list", quarantinedDomain],
    ],
  });

  // Load an extension that will have access to all domains, including the
  // quarantined domain.
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["activeTab"],
      browser_action: {},
    },
  });
  await extension.startup();

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: `https://${quarantinedDomain}/` },
    async () => {
      await openExtensionsPanel();

      const messages = getMessageBars();
      Assert.equal(messages.length, 1, "expected a message");

      const [message] = messages;
      verifyMessageBar(message);

      await closeExtensionsPanel();
    }
  );

  // Navigating to a different tab/domain shouldn't show any message.
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: `http://mochi.test:8888/` },
    async () => {
      await openExtensionsPanel();
      Assert.equal(getMessageBars().length, 0, "expected no message");
      await closeExtensionsPanel();
    }
  );

  // Back to a quarantined domain, if we update the list, we expect the message
  // to be gone when we re-open the panel (and not before because we don't
  // listen to the pref currently).
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: `https://${quarantinedDomain}/` },
    async () => {
      await openExtensionsPanel();

      const messages = getMessageBars();
      Assert.equal(messages.length, 1, "expected a message");

      const [message] = messages;
      verifyMessageBar(message);

      await closeExtensionsPanel();

      // Clear the list of quarantined domains.
      Services.prefs.setStringPref("extensions.quarantinedDomains.list", "");

      await openExtensionsPanel();
      Assert.equal(getMessageBars().length, 0, "expected no message");
      await closeExtensionsPanel();
    }
  );

  await extension.unload();
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_quarantined_domain_message_learn_more_link() {
  const quarantinedDomain = "example.org";
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.quarantinedDomains.enabled", true],
      ["extensions.quarantinedDomains.list", quarantinedDomain],
    ],
  });

  // Load an extension that will have access to all domains, including the
  // quarantined domain.
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["activeTab"],
      browser_action: {},
    },
  });
  await extension.startup();

  const expectedSupportURL =
    Services.urlFormatter.formatURLPref("app.support.baseURL") +
    "quarantined-domains";

  // We expect the SUMO page to be open in a new tab and the panel to be closed
  // when the user clicks on the "learn more" link.
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: `https://${quarantinedDomain}/` },
    async () => {
      await openExtensionsPanel();
      const messages = getMessageBars();
      Assert.equal(messages.length, 1, "expected a message");

      const [message] = messages;
      verifyMessageBar(message);

      const tabPromise = BrowserTestUtils.waitForNewTab(
        gBrowser,
        expectedSupportURL
      );
      const hidden = BrowserTestUtils.waitForEvent(
        gUnifiedExtensions.panel,
        "popuphidden",
        true
      );
      message.querySelector("a").click();
      const [tab] = await Promise.all([tabPromise, hidden]);
      BrowserTestUtils.removeTab(tab);
    }
  );

  // Same as above but with keyboard navigation.
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: `https://${quarantinedDomain}/` },
    async () => {
      await openExtensionsPanel();
      const messages = getMessageBars();
      Assert.equal(messages.length, 1, "expected a message");

      const [message] = messages;
      verifyMessageBar(message);

      const supportLink = message.querySelector("a");

      // Focus the "learn more" (support) link.
      const focused = BrowserTestUtils.waitForEvent(supportLink, "focus");
      EventUtils.synthesizeKey("VK_TAB", {});
      await focused;

      const tabPromise = BrowserTestUtils.waitForNewTab(
        gBrowser,
        expectedSupportURL
      );
      const hidden = BrowserTestUtils.waitForEvent(
        gUnifiedExtensions.panel,
        "popuphidden",
        true
      );
      EventUtils.synthesizeKey("KEY_Enter", {});
      const [tab] = await Promise.all([tabPromise, hidden]);
      BrowserTestUtils.removeTab(tab);
    }
  );

  await extension.unload();
  await SpecialPowers.popPrefEnv();
});
