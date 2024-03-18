/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const WebProtocolHandlerRegistrar = ChromeUtils.importESModule(
  "resource:///modules/WebProtocolHandlerRegistrar.sys.mjs"
).WebProtocolHandlerRegistrar.prototype;

add_setup(function add_setup() {
  Services.prefs.setBoolPref("browser.mailto.dualPrompt", true);
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
const sss_domain = subdomain + ".example.com";
const ss_setting = "system.under.test";

add_task(async function check_null_value() {
  Assert.ok(
    /[a-z0-9].+\.[a-z]+\.[a-z]+/.test(sss_domain),
    "test the validity of this random domain name before using it for tests: '" +
      sss_domain +
      "'"
  );
});

add_task(async function check_null_value() {
  Assert.equal(
    null,
    await WebProtocolHandlerRegistrar._getSiteSpecificSetting(
      sss_domain,
      ss_setting
    ),
    "site specific setting should initially not exist and return null."
  );
});

add_task(async function check_default_value() {
  Assert.equal(
    true,
    await WebProtocolHandlerRegistrar._getSiteSpecificSetting(
      sss_domain,
      ss_setting,
      null,
      true
    ),
    "site specific setting with a fallback/default value set to true."
  );
});

add_task(async function check_save_value() {
  WebProtocolHandlerRegistrar._saveSiteSpecificSetting(
    sss_domain,
    ss_setting,
    ss_setting
  );

  let fetchedSiteSpecificSetting;
  try {
    fetchedSiteSpecificSetting =
      await WebProtocolHandlerRegistrar._getSiteSpecificSetting(
        sss_domain,
        ss_setting
      );
  } finally {
    // make sure the cleanup happens, no matter what
    _deleteSiteSpecificSetting(sss_domain, ss_setting);
  }
  Assert.equal(
    ss_setting,
    fetchedSiteSpecificSetting,
    "site specific setting save and retrieve test."
  );

  Assert.equal(
    null,
    await WebProtocolHandlerRegistrar._getSiteSpecificSetting(
      sss_domain,
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

add_task(async function check_addLocal() {
  let currentHandler = WebProtocolHandlerRegistrar._addLocal(
    protocol,
    sss_domain,
    sss_domain
  );

  Assert.equal(
    sss_domain,
    currentHandler.name,
    "does the handler have the right name?"
  );
  Assert.equal(
    sss_domain,
    currentHandler.uriTemplate,
    "does the handler have the right uri?"
  );

  WebProtocolHandlerRegistrar._setLocalDefault(currentHandler);

  WebProtocolHandlerRegistrar.removeProtocolHandler(
    protocol,
    currentHandler.uriTemplate
  );
});

add_task(async function check_bar_is_shown() {
  let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
  browserWindow.windowUtils.disableNonTestMouseEvents(true);
  browserWindow.document
    .getElementById("main-window")
    .removeAttribute("remotecontrol");
  let browser = browserWindow.gBrowser.selectedBrowser;

  await WebProtocolHandlerRegistrar._askUserToSetMailtoHandler(
    browser,
    protocol,
    Services.io.newURI("https://" + sss_domain),
    sss_domain
  );

  let button_yes = browserWindow.document.querySelector(
    "[data-l10n-id='protocolhandler-mailto-os-handler-yes-button']"
  );
  Assert.notEqual(null, button_yes, "is the yes-button there?");

  let button_no = browserWindow.document.querySelector(
    "[data-l10n-id='protocolhandler-mailto-os-handler-no-button']"
  );
  Assert.notEqual(null, button_no, "is the no-button there?");
});
