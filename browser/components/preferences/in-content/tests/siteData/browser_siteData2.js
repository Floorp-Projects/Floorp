"use strict";

function assertAllSitesNotListed(win) {
  let frameDoc = win.gSubDialog._topDialog._frame.contentDocument;
  let removeBtn = frameDoc.getElementById("removeSelected");
  let removeAllBtn = frameDoc.getElementById("removeAll");
  let sitesList = frameDoc.getElementById("sitesList");
  let sites = sitesList.getElementsByTagName("richlistitem");
  is(sites.length, 0, "Should not list all sites");
  is(removeBtn.disabled, true, "Should disable the removeSelected button");
  is(removeAllBtn.disabled, true, "Should disable the removeAllBtn button");
}

// Test selecting and removing all sites one by one
add_task(async function test_selectRemove() {
  let hosts = await addTestData([
    {
      usage: 1024,
      origin: "https://account.xyz.com",
      persisted: true,
    },
    {
      usage: 1024,
      origin: "https://shopping.xyz.com",
    },
    {
      usage: 1024,
      origin: "http://cinema.bar.com",
      persisted: true,
    },
    {
      usage: 1024,
      origin: "http://email.bar.com",
    },
  ]);

  let updatePromise = promiseSiteDataManagerSitesUpdated();

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  await updatePromise;
  await openSiteDataSettingsDialog();

  let win = gBrowser.selectedBrowser.contentWindow;
  let doc = gBrowser.selectedBrowser.contentDocument;
  let frameDoc = null;
  let saveBtn = null;
  let cancelBtn = null;
  let settingsDialogClosePromise = null;

  // Test the initial state
  assertSitesListed(doc, hosts);

  // Test the "Cancel" button
  settingsDialogClosePromise = promiseSettingsDialogClose();
  frameDoc = win.gSubDialog._topDialog._frame.contentDocument;
  cancelBtn = frameDoc.getElementById("cancel");
  removeAllSitesOneByOne();
  assertAllSitesNotListed(win);
  cancelBtn.doCommand();
  await settingsDialogClosePromise;
  await openSiteDataSettingsDialog();
  assertSitesListed(doc, hosts);

  // Test the "Save Changes" button but cancelling save
  let cancelPromise = BrowserTestUtils.promiseAlertDialogOpen("cancel");
  settingsDialogClosePromise = promiseSettingsDialogClose();
  frameDoc = win.gSubDialog._topDialog._frame.contentDocument;
  saveBtn = frameDoc.getElementById("save");
  cancelBtn = frameDoc.getElementById("cancel");
  removeAllSitesOneByOne();
  assertAllSitesNotListed(win);
  saveBtn.doCommand();
  await cancelPromise;
  cancelBtn.doCommand();
  await settingsDialogClosePromise;
  await openSiteDataSettingsDialog();
  assertSitesListed(doc, hosts);

  // Test the "Save Changes" button and accepting save
  let acceptPromise = BrowserTestUtils.promiseAlertDialogOpen("accept");
  settingsDialogClosePromise = promiseSettingsDialogClose();
  updatePromise = promiseSiteDataManagerSitesUpdated();
  frameDoc = win.gSubDialog._topDialog._frame.contentDocument;
  saveBtn = frameDoc.getElementById("save");
  removeAllSitesOneByOne();
  assertAllSitesNotListed(win);
  saveBtn.doCommand();
  await acceptPromise;
  await settingsDialogClosePromise;
  await updatePromise;
  await openSiteDataSettingsDialog();
  assertAllSitesNotListed(win);

  await SiteDataTestUtils.clear();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  function removeAllSitesOneByOne() {
    frameDoc = win.gSubDialog._topDialog._frame.contentDocument;
    let removeBtn = frameDoc.getElementById("removeSelected");
    let sitesList = frameDoc.getElementById("sitesList");
    let sites = sitesList.getElementsByTagName("richlistitem");
    for (let i = sites.length - 1; i >= 0; --i) {
      sites[i].click();
      removeBtn.doCommand();
    }
  }
});

// Test selecting and removing partial sites
add_task(async function test_removePartialSites() {
  let hosts = await addTestData([
    {
      usage: 1024,
      origin: "https://account.xyz.com",
      persisted: true,
    },
    {
      usage: 1024,
      origin: "https://shopping.xyz.com",
      persisted: false,
    },
    {
      usage: 1024,
      origin: "http://cinema.bar.com",
      persisted: true,
    },
    {
      usage: 1024,
      origin: "http://email.bar.com",
      persisted: false,
    },
    {
      usage: 1024,
      origin: "https://s3-us-west-2.amazonaws.com",
      persisted: true,
    },
    {
      usage: 1024,
      origin: "https://127.0.0.1",
      persisted: false,
    },
  ]);

  let updatePromise = promiseSiteDataManagerSitesUpdated();

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  await updatePromise;
  await openSiteDataSettingsDialog();

  let win = gBrowser.selectedBrowser.contentWindow;
  let doc = gBrowser.selectedBrowser.contentDocument;
  let frameDoc = null;
  let saveBtn = null;
  let cancelBtn = null;
  let removeDialogOpenPromise = null;
  let settingsDialogClosePromise = null;

  // Test the initial state
  assertSitesListed(doc, hosts);

  // Test the "Cancel" button
  settingsDialogClosePromise = promiseSettingsDialogClose();
  frameDoc = win.gSubDialog._topDialog._frame.contentDocument;
  cancelBtn = frameDoc.getElementById("cancel");
  await removeSelectedSite(hosts.slice(0, 2));
  assertSitesListed(doc, hosts.slice(2));
  cancelBtn.doCommand();
  await settingsDialogClosePromise;
  await openSiteDataSettingsDialog();
  assertSitesListed(doc, hosts);

  // Test the "Save Changes" button but canceling save
  removeDialogOpenPromise = BrowserTestUtils.promiseAlertDialogOpen(
    "cancel",
    REMOVE_DIALOG_URL
  );
  settingsDialogClosePromise = promiseSettingsDialogClose();
  frameDoc = win.gSubDialog._topDialog._frame.contentDocument;
  saveBtn = frameDoc.getElementById("save");
  cancelBtn = frameDoc.getElementById("cancel");
  await removeSelectedSite(hosts.slice(0, 2));
  assertSitesListed(doc, hosts.slice(2));
  saveBtn.doCommand();
  await removeDialogOpenPromise;
  cancelBtn.doCommand();
  await settingsDialogClosePromise;
  await openSiteDataSettingsDialog();
  assertSitesListed(doc, hosts);

  // Test the "Save Changes" button and accepting save
  removeDialogOpenPromise = BrowserTestUtils.promiseAlertDialogOpen(
    "accept",
    REMOVE_DIALOG_URL
  );
  settingsDialogClosePromise = promiseSettingsDialogClose();
  frameDoc = win.gSubDialog._topDialog._frame.contentDocument;
  saveBtn = frameDoc.getElementById("save");
  await removeSelectedSite(hosts.slice(0, 2));
  assertSitesListed(doc, hosts.slice(2));
  saveBtn.doCommand();
  await removeDialogOpenPromise;
  await settingsDialogClosePromise;
  await openSiteDataSettingsDialog();
  assertSitesListed(doc, hosts.slice(2));

  await SiteDataTestUtils.clear();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  function removeSelectedSite(removeHosts) {
    frameDoc = win.gSubDialog._topDialog._frame.contentDocument;
    let removeBtn = frameDoc.getElementById("removeSelected");
    is(
      removeBtn.disabled,
      true,
      "Should start with disabled removeSelected button"
    );
    let sitesList = frameDoc.getElementById("sitesList");
    removeHosts.forEach(host => {
      let site = sitesList.querySelector(`richlistitem[host="${host}"]`);
      if (site) {
        site.click();
        let currentSelectedIndex = sitesList.selectedIndex;
        is(
          removeBtn.disabled,
          false,
          "Should enable the removeSelected button"
        );
        removeBtn.doCommand();
        let newSelectedIndex = sitesList.selectedIndex;
        if (currentSelectedIndex >= sitesList.itemCount) {
          is(newSelectedIndex, currentSelectedIndex - 1);
        } else {
          is(newSelectedIndex, currentSelectedIndex);
        }
      } else {
        ok(false, `Should not select and remove inexistent site of ${host}`);
      }
    });
  }
});

// Test searching and then removing only visible sites
add_task(async function() {
  let hosts = await addTestData([
    {
      usage: 1024,
      origin: "https://account.xyz.com",
      persisted: true,
    },
    {
      usage: 1024,
      origin: "https://shopping.xyz.com",
      persisted: false,
    },
    {
      usage: 1024,
      origin: "http://cinema.bar.com",
      persisted: true,
    },
    {
      usage: 1024,
      origin: "http://email.bar.com",
      persisted: false,
    },
  ]);

  let updatePromise = promiseSiteDataManagerSitesUpdated();

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  await updatePromise;
  await openSiteDataSettingsDialog();

  // Search "foo" to only list foo.com sites
  let win = gBrowser.selectedBrowser.contentWindow;
  let doc = gBrowser.selectedBrowser.contentDocument;
  let frameDoc = win.gSubDialog._topDialog._frame.contentDocument;
  let searchBox = frameDoc.getElementById("searchBox");
  searchBox.value = "xyz";
  searchBox.doCommand();
  assertSitesListed(
    doc,
    hosts.filter(host => host.includes("xyz"))
  );

  // Test only removing all visible sites listed
  updatePromise = promiseSiteDataManagerSitesUpdated();
  let acceptRemovePromise = BrowserTestUtils.promiseAlertDialogOpen(
    "accept",
    REMOVE_DIALOG_URL
  );
  let settingsDialogClosePromise = promiseSettingsDialogClose();
  let removeAllBtn = frameDoc.getElementById("removeAll");
  let saveBtn = frameDoc.getElementById("save");
  removeAllBtn.doCommand();
  saveBtn.doCommand();
  await acceptRemovePromise;
  await settingsDialogClosePromise;
  await updatePromise;
  await openSiteDataSettingsDialog();
  assertSitesListed(
    doc,
    hosts.filter(host => !host.includes("xyz"))
  );

  await SiteDataTestUtils.clear();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Test dynamically clearing all site data
add_task(async function() {
  let hosts = await addTestData([
    {
      usage: 1024,
      origin: "https://account.xyz.com",
      persisted: true,
    },
    {
      usage: 1024,
      origin: "https://shopping.xyz.com",
      persisted: false,
    },
  ]);

  let updatePromise = promiseSiteDataManagerSitesUpdated();

  // Test the initial state
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  await updatePromise;
  await openSiteDataSettingsDialog();
  let doc = gBrowser.selectedBrowser.contentDocument;
  assertSitesListed(doc, hosts);

  await addTestData([
    {
      usage: 1024,
      origin: "http://cinema.bar.com",
      persisted: true,
    },
    {
      usage: 1024,
      origin: "http://email.bar.com",
      persisted: false,
    },
  ]);

  // Test clearing all site data dynamically
  let win = gBrowser.selectedBrowser.contentWindow;
  let frameDoc = win.gSubDialog._topDialog._frame.contentDocument;
  updatePromise = promiseSiteDataManagerSitesUpdated();
  let acceptRemovePromise = BrowserTestUtils.promiseAlertDialogOpen("accept");
  let settingsDialogClosePromise = promiseSettingsDialogClose();
  let removeAllBtn = frameDoc.getElementById("removeAll");
  let saveBtn = frameDoc.getElementById("save");
  removeAllBtn.doCommand();
  saveBtn.doCommand();
  await acceptRemovePromise;
  await settingsDialogClosePromise;
  await updatePromise;
  await openSiteDataSettingsDialog();
  assertAllSitesNotListed(win);

  await SiteDataTestUtils.clear();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
