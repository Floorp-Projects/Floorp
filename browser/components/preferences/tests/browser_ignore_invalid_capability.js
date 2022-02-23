/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function testInvalidCapabilityIgnored() {
  info(
    "Test to make sure that invalid combinations of type and capability are ignored \
     so the cookieExceptions management popup does not crash"
  );
  PermissionTestUtils.add(
    "https://mozilla.org",
    "cookie",
    Ci.nsICookiePermission.ACCESS_ALLOW
  );

  // This is an invalid combination of type & capability and should be ignored
  PermissionTestUtils.add(
    "https://foobar.org",
    "cookie",
    Ci.nsIHttpsOnlyModePermission.LOAD_INSECURE_ALLOW_SESSION
  );

  await openPreferencesViaOpenPreferencesAPI("panePrivacy", {
    leaveOpen: true,
  });
  let doc = gBrowser.contentDocument;
  let promiseSubDialogLoaded = promiseLoadSubDialog(
    "chrome://browser/content/preferences/dialogs/permissions.xhtml"
  );
  doc.getElementById("cookieExceptions").doCommand();

  let win = await promiseSubDialogLoaded;
  doc = win.document;

  is(
    doc.getElementById("permissionsBox").itemCount,
    1,
    "We only display the permission that is valid for the type cookie"
  );
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
