"use strict";

// Test selecting and removing partial sites
add_task(async function() {
  await SiteDataTestUtils.clear();

  let hosts = await addTestData([
    {
      usage: 1024,
      origin: "https://127.0.0.1",
      persisted: false,
    },
    {
      usage: 1024 * 4,
      origin: "http://cinema.bar.com",
      persisted: true,
    },
    {
      usage: 1024 * 3,
      origin: "http://email.bar.com",
      persisted: false,
    },
    {
      usage: 1024 * 2,
      origin: "https://s3-us-west-2.amazonaws.com",
      persisted: true,
    },
    {
      usage: 1024 * 6,
      origin: "https://account.xyz.com",
      persisted: true,
    },
    {
      usage: 1024 * 5,
      origin: "https://shopping.xyz.com",
      persisted: false,
    },
  ]);

  let updatePromise = promiseSiteDataManagerSitesUpdated();
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  await updatePromise;
  await openSiteDataSettingsDialog();

  let doc = gBrowser.selectedBrowser.contentDocument;

  // Test the initial state
  assertSitesListed(doc, hosts);
  let win = gBrowser.selectedBrowser.contentWindow;
  let frameDoc = win.gSubDialog._topDialog._frame.contentDocument;
  let removeBtn = frameDoc.getElementById("removeSelected");
  is(
    removeBtn.disabled,
    true,
    "Should start with disabled removeSelected button"
  );

  let hostCol = frameDoc.getElementById("hostCol");
  hostCol.click();

  let removeDialogOpenPromise = BrowserTestUtils.promiseAlertDialogOpen(
    "accept",
    REMOVE_DIALOG_URL
  );
  let settingsDialogClosePromise = promiseSettingsDialogClose();

  // Select some sites to remove.
  let sitesList = frameDoc.getElementById("sitesList");
  hosts.slice(0, 2).forEach(host => {
    let site = sitesList.querySelector(`richlistitem[host="${host}"]`);
    sitesList.addItemToSelection(site);
  });

  is(removeBtn.disabled, false, "Should enable the removeSelected button");
  removeBtn.doCommand();
  is(sitesList.selectedIndex, 0, "Should select next item");
  assertSitesListed(doc, hosts.slice(2));

  // Select some other sites to remove with Delete.
  hosts.slice(2, 4).forEach(host => {
    let site = sitesList.querySelector(`richlistitem[host="${host}"]`);
    sitesList.addItemToSelection(site);
  });

  is(removeBtn.disabled, false, "Should enable the removeSelected button");
  EventUtils.synthesizeKey("VK_DELETE");
  is(sitesList.selectedIndex, 0, "Should select next item");
  assertSitesListed(doc, hosts.slice(4));

  updatePromise = promiseSiteDataManagerSitesUpdated();
  let saveBtn = frameDoc.getElementById("save");
  saveBtn.doCommand();

  await removeDialogOpenPromise;
  await settingsDialogClosePromise;

  await updatePromise;
  await openSiteDataSettingsDialog();

  assertSitesListed(doc, hosts.slice(4));

  await SiteDataTestUtils.clear();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
