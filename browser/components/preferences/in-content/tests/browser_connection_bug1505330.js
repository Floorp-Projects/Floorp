/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

 "use strict";

 // Test the disabled status of the autoconfig Reload button when the proxy type
 // is autoconfig (network.proxy.type == 2).
 add_task(async function testAutoconfigReloadButton() {
   Services.prefs.lockPref("signon.autologin.proxy");

   await openPreferencesViaOpenPreferencesAPI("general", { leaveOpen: true });
   const connectionURL = "chrome://browser/content/preferences/connection.xul";
   const promiseDialogLoaded = promiseLoadSubDialog(connectionURL);
   gBrowser.contentDocument.getElementById("connectionSettings").click();
   const dialog = await promiseDialogLoaded;

   ok(!dialog.document.getElementById("networkProxyType").firstChild.disabled,
      "Connection options should not be disabled");
   ok(dialog.document.getElementById("autologinProxy").disabled,
      "Proxy autologin should be disabled");

   dialog.close();
   Services.prefs.unlockPref("signon.autologin.proxy");
   gBrowser.removeCurrentTab();
 });
