/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the disabled status of the autoconfig Reload button when the proxy type
// is autoconfig (network.proxy.type == 2).
add_task(async function testAutoconfigReloadButton() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["network.proxy.type", 2],
      ["network.proxy.autoconfig_url", "file:///nonexistent.pac"],
    ],
  });

  await openPreferencesViaOpenPreferencesAPI("general", { leaveOpen: true });
  const connectionURL =
    "chrome://browser/content/preferences/dialogs/connection.xhtml";
  const promiseDialogLoaded = promiseLoadSubDialog(connectionURL);
  gBrowser.contentDocument.getElementById("connectionSettings").click();
  const dialog = await promiseDialogLoaded;

  ok(
    !dialog.document.getElementById("autoReload").disabled,
    "Reload button is enabled when proxy type is autoconfig"
  );

  dialog.close();
  gBrowser.removeCurrentTab();
});
