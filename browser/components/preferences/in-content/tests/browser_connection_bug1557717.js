/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

 "use strict";

 add_task(async function testLockedPreferences() {
   Services.prefs.lockPref("network.proxy.no_proxies_on");
   Services.prefs.lockPref("signon.autologin.proxy");
   Services.prefs.lockPref("network.proxy.autoconfig_url");
   Services.prefs.lockPref("network.proxy.share_proxy_settings");

   registerCleanupFunction(async function() {
      Services.prefs.unlockPref("network.proxy.no_proxies_on");
      Services.prefs.unlockPref("signon.autologin.proxy");
      Services.prefs.unlockPref("network.proxy.autoconfig_url");
      Services.prefs.unlockPref("network.proxy.share_proxy_settings");
   });

   await openPreferencesViaOpenPreferencesAPI("general", { leaveOpen: true });
   const connectionURL = "chrome://browser/content/preferences/connection.xul";
   const promiseDialogLoaded = promiseLoadSubDialog(connectionURL);
   gBrowser.contentDocument.getElementById("connectionSettings").click();
   const dialog = await promiseDialogLoaded;

   Services.prefs.setIntPref("network.proxy.type", 2);
   registerCleanupFunction(async function() {
      // I'm not using SpecialPowers because we need
      // to set the value twice.
     Services.prefs.clearUserPref("network.proxy.type");
   });

   ok(dialog.document.getElementById("networkProxyAutoconfigURL").disabled,
      "Proxy autoconfig URL on should be disabled");

   Services.prefs.setIntPref("network.proxy.type", 1);

   ok(dialog.document.getElementById("networkProxyNone").disabled,
      "No proxies on should be disabled");
   ok(dialog.document.getElementById("autologinProxy").disabled,
      "Proxy autologin should be disabled");

   ok(dialog.document.getElementById("shareAllProxies").disabled,
     "Share all proxies should be disabled");

   dialog.close();
   gBrowser.removeCurrentTab();
 });
