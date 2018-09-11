/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.import("resource://gre/modules/Services.jsm");

const BROWSER_LANGUAGES_URL = "chrome://browser/content/preferences/browserLanguages.xul";

function assertLocaleOrder(list, locales) {
  is(list.itemCount, locales.split(",").length,
     "The right number of locales are requested");
  is(Array.from(list.children).map(child => child.value).join(","),
     locales, "The requested locales are in order");
}

function assertAvailableLocales(list, locales) {
  is(list.itemCount, locales.length, "The right number of locales are available");
  is(Array.from(list.firstElementChild.children).map(item => item.value).sort(),
     locales.sort().join(","), "The available locales match");
}

async function openDialog(doc) {
  let dialogLoaded = promiseLoadSubDialog(BROWSER_LANGUAGES_URL);
  doc.getElementById("manageBrowserLanguagesButton").doCommand();
  let dialogWin = await dialogLoaded;
  let dialogDoc = dialogWin.document;
  return {
    dialog: dialogDoc.getElementById("BrowserLanguagesDialog"),
    dialogDoc,
    available: dialogDoc.getElementById("availableLocales"),
    requested: dialogDoc.getElementById("requestedLocales"),
  };
}

add_task(async function testReorderingBrowserLanguages() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["intl.multilingual.enabled", true],
      ["intl.locale.requested", "pl,en-US"],
    ],
  });

  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});

  let doc = gBrowser.contentDocument;
  let messageBar = doc.getElementById("confirmBrowserLanguage");
  is(messageBar.hidden, true, "The message bar is hidden at first");

  // Open the dialog.
  let {dialog, dialogDoc, requested} = await openDialog(doc);

  // The initial order is set by the pref.
  assertLocaleOrder(requested, "pl,en-US");

  // Moving pl down changes the order.
  dialogDoc.getElementById("down").doCommand();
  assertLocaleOrder(requested, "en-US,pl");

  // Accepting the change shows the confirm message bar.
  let dialogClosed = BrowserTestUtils.waitForEvent(dialogDoc.documentElement, "dialogclosing");
  dialog.acceptDialog();
  await dialogClosed;
  is(messageBar.hidden, false, "The message bar is now visible");
  is(messageBar.querySelector("button").getAttribute("locales"), "en-US,pl",
     "The locales are set on the message bar button");

  // Open the dialog again.
  let newDialog = await openDialog(doc);
  dialog = newDialog.dialog;
  dialogDoc = newDialog.dialogDoc;
  requested = newDialog.requested;

  // The initial order comes from the previous settings.
  assertLocaleOrder(requested, "en-US,pl");

  // Select pl in the list.
  requested.selectedItem = requested.querySelector("[value='pl']");
  // Move pl back up.
  dialogDoc.getElementById("up").doCommand();
  assertLocaleOrder(requested, "pl,en-US");

  // Accepting the change hides the confirm message bar.
  dialogClosed = BrowserTestUtils.waitForEvent(dialogDoc.documentElement, "dialogclosing");
  dialog.acceptDialog();
  await dialogClosed;
  is(messageBar.hidden, true, "The message bar is hidden again");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function testAddAndRemoveRequestedLanguages() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["intl.multilingual.enabled", true],
      ["intl.locale.requested", "pl,it,en-US,he"],
    ],
  });

  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});

  let doc = gBrowser.contentDocument;
  let messageBar = doc.getElementById("confirmBrowserLanguage");
  is(messageBar.hidden, true, "The message bar is hidden at first");

  // Open the dialog.
  let {dialog, dialogDoc, available, requested} = await openDialog(doc);

  // The initial order is set by the pref.
  assertLocaleOrder(requested, "pl,it,en-US,he");
  assertAvailableLocales(available, []);

  // Remove pl and it from requested.
  dialogDoc.getElementById("remove").doCommand();
  dialogDoc.getElementById("remove").doCommand();
  assertLocaleOrder(requested, "en-US,he");
  assertAvailableLocales(available, ["it", "pl"]);

  // Find the item for "it".
  let [it] = [available.getItemAtIndex(0), available.getItemAtIndex(1)]
    .filter(item => item.value == "it");
  // Add "it" back to requested.
  available.selectedItem = it;
  dialogDoc.getElementById("add").doCommand();

  assertLocaleOrder(requested, "it,en-US,he");
  assertAvailableLocales(available, ["pl"]);

  // Accepting the change shows the confirm message bar.
  let dialogClosed = BrowserTestUtils.waitForEvent(dialogDoc.documentElement, "dialogclosing");
  dialog.acceptDialog();
  await dialogClosed;
  is(messageBar.hidden, false, "The message bar is now visible");
  is(messageBar.querySelector("button").getAttribute("locales"), "it,en-US,he",
     "The locales are set on the message bar button");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
