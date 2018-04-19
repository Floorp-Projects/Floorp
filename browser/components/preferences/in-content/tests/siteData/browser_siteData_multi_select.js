"use strict";

// Test selecting and removing partial sites
add_task(async function() {
  mockSiteDataManager.register(SiteDataManager, [
    {
      usage: 1024,
      origin: "https://account.xyz.com",
      persisted: true
    },
    {
      usage: 1024,
      origin: "https://shopping.xyz.com",
      persisted: false
    },
    {
      usage: 1024,
      origin: "http://cinema.bar.com",
      persisted: true
    },
    {
      usage: 1024,
      origin: "http://email.bar.com",
      persisted: false
    },
    {
      usage: 1024,
      origin: "https://s3-us-west-2.amazonaws.com",
      persisted: true
    },
    {
      usage: 1024,
      origin: "https://127.0.0.1",
      persisted: false
    },
    {
      usage: 1024,
      origin: "https://[0:0:0:0:0:0:0:1]",
      persisted: true
    },
  ]);
  let fakeHosts = mockSiteDataManager.fakeSites.map(site => site.principal.URI.host);

  let updatePromise = promiseSiteDataManagerSitesUpdated();
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  await updatePromise;
  await openSiteDataSettingsDialog();

  let doc = gBrowser.selectedBrowser.contentDocument;

  // Test the initial state
  assertSitesListed(doc, fakeHosts);
  let win = gBrowser.selectedBrowser.contentWindow;
  let frameDoc = win.gSubDialog._topDialog._frame.contentDocument;
  let removeBtn = frameDoc.getElementById("removeSelected");
  is(removeBtn.disabled, true, "Should start with disabled removeSelected button");

  let removeDialogOpenPromise = BrowserTestUtils.promiseAlertDialogOpen("accept", REMOVE_DIALOG_URL);
  let settingsDialogClosePromise = promiseSettingsDialogClose();

  // Select some sites to remove.
  let sitesList = frameDoc.getElementById("sitesList");
  fakeHosts.slice(0, 2).forEach(host => {
    let site = sitesList.querySelector(`richlistitem[host="${host}"]`);
    sitesList.addItemToSelection(site);
  });

  is(removeBtn.disabled, false, "Should enable the removeSelected button");
  removeBtn.doCommand();
  is(sitesList.selectedIndex, 0, "Should select next item");

  let saveBtn = frameDoc.getElementById("save");
  assertSitesListed(doc, fakeHosts.slice(2));
  saveBtn.doCommand();

  await removeDialogOpenPromise;
  await settingsDialogClosePromise;
  await openSiteDataSettingsDialog();

  assertSitesListed(doc, fakeHosts.slice(2));

  await mockSiteDataManager.unregister();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
