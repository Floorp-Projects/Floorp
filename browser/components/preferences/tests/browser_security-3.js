/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async function () {
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  registerCleanupFunction(async function () {
    Services.prefs.unlockPref("browser.safebrowsing.phishing.enabled");
    Services.prefs.unlockPref("browser.safebrowsing.malware.enabled");
    Services.prefs.unlockPref("browser.safebrowsing.downloads.enabled");
    Services.prefs.unlockPref(
      "browser.safebrowsing.downloads.remote.block_potentially_unwanted"
    );
    Services.prefs.unlockPref(
      "browser.safebrowsing.downloads.remote.block_uncommon"
    );
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  });
});

// This test just reloads the preferences page for the various tests.
add_task(async function () {
  Services.prefs.lockPref("browser.safebrowsing.phishing.enabled");
  Services.prefs.lockPref("browser.safebrowsing.malware.enabled");
  Services.prefs.lockPref("browser.safebrowsing.downloads.enabled");
  Services.prefs.lockPref(
    "browser.safebrowsing.downloads.remote.block_potentially_unwanted"
  );
  Services.prefs.lockPref(
    "browser.safebrowsing.downloads.remote.block_uncommon"
  );

  gBrowser.reload();
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  let doc = gBrowser.selectedBrowser.contentDocument;
  is(
    doc.getElementById("enableSafeBrowsing").disabled,
    true,
    "Safe browsing should be disabled"
  );
  is(
    doc.getElementById("blockDownloads").disabled,
    true,
    "Block downloads should be disabled"
  );
  is(
    doc.getElementById("blockUncommonUnwanted").disabled,
    true,
    "Block common unwanted should be disabled"
  );

  Services.prefs.unlockPref("browser.safebrowsing.phishing.enabled");
  Services.prefs.unlockPref("browser.safebrowsing.malware.enabled");

  gBrowser.reload();
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  doc = gBrowser.selectedBrowser.contentDocument;

  let checkbox = doc.getElementById("enableSafeBrowsing");
  checkbox.scrollIntoView();
  EventUtils.synthesizeMouseAtCenter(
    checkbox,
    {},
    gBrowser.selectedBrowser.contentWindow
  );

  is(
    doc.getElementById("blockDownloads").disabled,
    true,
    "Block downloads should be disabled"
  );
  is(
    doc.getElementById("blockUncommonUnwanted").disabled,
    true,
    "Block common unwanted should be disabled"
  );

  EventUtils.synthesizeMouseAtCenter(
    checkbox,
    {},
    gBrowser.selectedBrowser.contentWindow
  );

  is(
    doc.getElementById("blockDownloads").disabled,
    true,
    "Block downloads should be disabled"
  );
  is(
    doc.getElementById("blockUncommonUnwanted").disabled,
    true,
    "Block common unwanted should be disabled"
  );

  Services.prefs.unlockPref("browser.safebrowsing.downloads.enabled");

  gBrowser.reload();
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  doc = gBrowser.selectedBrowser.contentDocument;

  checkbox = doc.getElementById("blockDownloads");
  checkbox.scrollIntoView();
  EventUtils.synthesizeMouseAtCenter(
    checkbox,
    {},
    gBrowser.selectedBrowser.contentWindow
  );

  is(
    doc.getElementById("blockUncommonUnwanted").disabled,
    true,
    "Block common unwanted should be disabled"
  );

  EventUtils.synthesizeMouseAtCenter(
    checkbox,
    {},
    gBrowser.selectedBrowser.contentWindow
  );

  is(
    doc.getElementById("blockUncommonUnwanted").disabled,
    true,
    "Block common unwanted should be disabled"
  );
});
