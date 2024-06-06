/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentAPI } = ChromeUtils.importESModule(
  "resource://nimbus/ExperimentAPI.sys.mjs"
);
const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);
const WebProtocolHandlerRegistrar = ChromeUtils.importESModule(
  "resource:///modules/WebProtocolHandlerRegistrar.sys.mjs"
).WebProtocolHandlerRegistrar.prototype;

add_setup(async () => {
  Services.prefs.setCharPref("browser.protocolhandler.loglevel", "debug");
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.protocolhandler.loglevel");
  });

  Services.prefs.clearUserPref("browser.mailto.dualPrompt");
  Services.prefs.clearUserPref("browser.mailto.dualPrompt.onLocationChange");

  await ExperimentAPI.ready();

  WebProtocolHandlerRegistrar._addWebProtocolHandler(
    protocol,
    "example.com",
    "example.com"
  );
});

/* helper function to delete site specific settings needed to clean up
 * the testing setup after some of these tests.
 *
 * @see: nsIContentPrefService2.idl
 */
function _deleteSiteSpecificSetting(domain, setting, context = null) {
  const contentPrefs = Cc["@mozilla.org/content-pref/service;1"].getService(
    Ci.nsIContentPrefService2
  );

  return contentPrefs.removeByDomainAndName(domain, setting, context, {
    handleResult(_) {},
    handleCompletion() {
      Assert.ok(true, "setting successfully deleted.");
    },
    handleError(_) {
      Assert.ok(false, "could not delete site specific setting.");
    },
  });
}

// site specific settings
const protocol = "mailto";
const subdomain = (Math.random() + 1).toString(36).substring(7);
const sss_domain = "example.com";
const ss_setting = "system.under.test";

const selector_mailto_prompt =
  'notification-message[message-bar-type="infobar"]' +
  '[value="OS Protocol Registration: mailto"]';

add_task(async function check_null_value() {
  Assert.equal(
    null,
    await WebProtocolHandlerRegistrar._getSiteSpecificSetting(
      `${subdomain}.${sss_domain}`,
      ss_setting
    ),
    "site specific setting should initially not exist and return null."
  );
});

add_task(async function check_default_value() {
  Assert.equal(
    true,
    await WebProtocolHandlerRegistrar._getSiteSpecificSetting(
      `${subdomain}.${sss_domain}`,
      ss_setting,
      null,
      true
    ),
    "site specific setting with a fallback/default value set to true."
  );
});

add_task(async function check_save_value() {
  WebProtocolHandlerRegistrar._saveSiteSpecificSetting(
    `${subdomain}.${sss_domain}`,
    ss_setting,
    ss_setting
  );

  let fetchedSiteSpecificSetting;
  try {
    fetchedSiteSpecificSetting =
      await WebProtocolHandlerRegistrar._getSiteSpecificSetting(
        `${subdomain}.${sss_domain}`,
        ss_setting
      );
  } finally {
    // make sure the cleanup happens, no matter what
    _deleteSiteSpecificSetting(`${subdomain}.${sss_domain}`, ss_setting);
  }
  Assert.equal(
    ss_setting,
    fetchedSiteSpecificSetting,
    "site specific setting save and retrieve test."
  );

  Assert.equal(
    null,
    await WebProtocolHandlerRegistrar._getSiteSpecificSetting(
      `${subdomain}.${sss_domain}`,
      ss_setting
    ),
    "site specific setting should not exist after delete."
  );
});

add_task(async function check_installHash() {
  Assert.notEqual(
    null,
    WebProtocolHandlerRegistrar._getInstallHash(),
    "test to check the installHash"
  );
});

add_task(async function check_addWebProtocolHandler() {
  let currentHandler = WebProtocolHandlerRegistrar._addWebProtocolHandler(
    protocol,
    sss_domain,
    "https://" + sss_domain
  );

  Assert.equal(
    sss_domain,
    currentHandler.name,
    "does the handler have the right name?"
  );
  Assert.equal(
    "https://" + sss_domain,
    currentHandler.uriTemplate,
    "does the handler have the right uri?"
  );

  WebProtocolHandlerRegistrar._setProtocolHandlerDefault(
    protocol,
    currentHandler
  );
  WebProtocolHandlerRegistrar.removeProtocolHandler(
    protocol,
    currentHandler.uriTemplate
  );
});

/* The next test is to ensure that we offer users to configure a webmailer
 * instead of a custom executable. They will not get a prompt if they have
 * configured their OS as default handler.
 */
add_task(async function promptShownForLocalHandler() {
  let handlerApp = Cc[
    "@mozilla.org/uriloader/local-handler-app;1"
  ].createInstance(Ci.nsILocalHandlerApp);
  handlerApp.executable = Services.dirsvc.get("XREExeF", Ci.nsIFile);
  WebProtocolHandlerRegistrar._setProtocolHandlerDefault(protocol, handlerApp);

  await BrowserTestUtils.withNewTab("https://example.com/", async browser => {
    await WebProtocolHandlerRegistrar._askUserToSetMailtoHandler(
      browser,
      protocol,
      Services.io.newURI("https://example.com"),
      "https://example.com"
    );

    Assert.notEqual(
      null,
      document.querySelector(selector_mailto_prompt),
      "The prompt is shown when an executable is configured as handler."
    );
  });
});

function test_rollout(
  dualPrompt = false,
  onLocationChange = false,
  dismissNotNowMinutes = 15,
  dismissXClickMinutes = 15
) {
  return ExperimentFakes.enrollWithFeatureConfig(
    {
      featureId: NimbusFeatures.mailto.featureId,
      value: {
        dualPrompt,
        "dualPrompt.onLocationChange": onLocationChange,
        "dualPrompt.dismissXClickMinutes": dismissXClickMinutes,
        "dualPrompt.dismissNotNowMinutes": dismissNotNowMinutes,
      },
    },
    { isRollout: true }
  );
}

add_task(async function check_no_button() {
  let cleanup = await test_rollout(true, true);

  const url = "https://" + sss_domain;
  WebProtocolHandlerRegistrar._addWebProtocolHandler(protocol, sss_domain, url);

  await BrowserTestUtils.withNewTab("https://example.com/", async () => {
    Assert.notEqual(
      null,
      document.querySelector(selector_mailto_prompt),
      "The prompt is shown with dualPrompt.onLocationChange toggled on."
    );
  });

  await BrowserTestUtils.withNewTab("https://example.com/", async browser => {
    let button_no = document.querySelector(
      "[data-l10n-id='protocolhandler-mailto-os-handler-no-button']"
    );
    Assert.notEqual(null, button_no, "is the no-button there?");

    await button_no.click();
    Assert.equal(
      null,
      document.querySelector(selector_mailto_prompt),
      "prompt hidden after button_no clicked."
    );

    await WebProtocolHandlerRegistrar._askUserToSetMailtoHandler(
      browser,
      protocol,
      Services.io.newURI("https://example.com"),
      "https://example.com"
    );

    Assert.equal(
      null,
      document.querySelector(selector_mailto_prompt),
      "prompt stays hidden even when called after the no button was clicked."
    );
  });

  cleanup();
});

add_task(async function check_x_button() {
  let cleanup = await test_rollout(true, true, 0, 15);

  await BrowserTestUtils.withNewTab("https://example.com/", async browser => {
    Assert.notEqual(
      null,
      document.querySelector(selector_mailto_prompt),
      "prompt gets shown again- the timeout for the no_button was set to zero"
    );

    let button_x = document
      .querySelector(".infobar")
      .shadowRoot.querySelector(".close");
    Assert.notEqual(null, button_x, "is the x-button there?");

    await button_x.click();
    Assert.equal(
      null,
      document.querySelector(selector_mailto_prompt),
      "prompt hidden after 'X' button clicked."
    );

    await WebProtocolHandlerRegistrar._askUserToSetMailtoHandler(
      browser,
      protocol,
      Services.io.newURI("https://example.com"),
      "https://example.com"
    );

    Assert.equal(
      null,
      document.querySelector(selector_mailto_prompt),
      "prompt stays hidden even when called after the no button was clicked."
    );
  });

  cleanup();
});

add_task(async function check_x_button() {
  let cleanup = await test_rollout(true, true, 0, 0);

  await BrowserTestUtils.withNewTab("https://example.com/", async () => {
    Assert.notEqual(
      null,
      document.querySelector(selector_mailto_prompt),
      "prompt gets shown again- the timeout for the no_button was set to zero"
    );
  });

  cleanup();
});

add_task(async function check_bar_is_not_shown() {
  let cleanup = await test_rollout(true, false);

  await BrowserTestUtils.withNewTab("https://example.com/", async () => {
    Assert.equal(
      null,
      document.querySelector(selector_mailto_prompt),
      "Prompt is not shown, because the dualPrompt.onLocationChange is off"
    );
  });

  cleanup();
});
