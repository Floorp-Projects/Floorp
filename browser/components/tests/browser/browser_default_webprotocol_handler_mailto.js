/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const WebProtocolHandlerRegistrar = ChromeUtils.importESModule(
  "resource:///modules/WebProtocolHandlerRegistrar.sys.mjs"
).WebProtocolHandlerRegistrar.prototype;

add_setup(function add_setup() {
  Services.prefs.setBoolPref("browser.mailto.dualPrompt", true);
});

// site specific settings
const protocol = "mailto";
const sss_domain = "test.example.com";
const ss_setting = "system.under.test";

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
  Assert.equal(
    ss_setting,
    await WebProtocolHandlerRegistrar._getSiteSpecificSetting(
      sss_domain,
      ss_setting
    ),
    "site specific setting save and retrieve test."
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
