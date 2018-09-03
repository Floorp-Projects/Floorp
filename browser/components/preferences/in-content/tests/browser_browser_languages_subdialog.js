/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.import("resource://gre/modules/Services.jsm");

const BROWSER_LANGUAGES_URL = "chrome://browser/content/preferences/browserLanguages.xul";

function assertLocaleOrder(list, locales) {
  is(list.children.length, 2, "There are two requested locales");
  is(Array.from(list.children).map(child => child.value).join(","),
     locales, "The requested locales are in order");
}

async function openDialog(doc) {
  let dialogLoaded = promiseLoadSubDialog(BROWSER_LANGUAGES_URL);
  doc.getElementById("manageBrowserLanguagesButton").doCommand();
  let dialogWin = await dialogLoaded;
  let dialogDoc = dialogWin.document;
  let list = dialogDoc.getElementById("activeLocales");
  let dialog = dialogDoc.getElementById("BrowserLanguagesDialog");
  return {dialog, dialogDoc, list};
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
  let {dialog, dialogDoc, list} = await openDialog(doc);

  // The initial order is set by the pref.
  assertLocaleOrder(list, "pl,en-US");

  // Moving pl down changes the order.
  dialogDoc.getElementById("down").doCommand();
  assertLocaleOrder(list, "en-US,pl");

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
  list = newDialog.list;

  // The initial order comes from the previous settings.
  assertLocaleOrder(list, "en-US,pl");

  // Select pl in the list.
  list.selectedItem = list.querySelector("[value='pl']");
  // Move pl back up.
  dialogDoc.getElementById("up").doCommand();
  assertLocaleOrder(list, "pl,en-US");

  // Accepting the change hides the confirm message bar.
  dialogClosed = BrowserTestUtils.waitForEvent(dialogDoc.documentElement, "dialogclosing");
  dialog.acceptDialog();
  await dialogClosed;
  is(messageBar.hidden, true, "The message bar is hidden again");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
