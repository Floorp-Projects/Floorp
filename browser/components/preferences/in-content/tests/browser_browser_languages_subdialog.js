/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm", this);
ChromeUtils.import("resource://gre/modules/Services.jsm");

AddonTestUtils.initMochitest(this);

const BROWSER_LANGUAGES_URL = "chrome://browser/content/preferences/browserLanguages.xul";

function getManifestData(locale) {
  return {
    langpack_id: locale,
    name: `${locale} Language Pack`,
    description: `${locale} Language pack`,
    languages: {
      [locale]: {
        chrome_resources: {},
        version: "1",
      },
    },
    applications: {
      gecko: {
        strict_min_version: AppConstants.MOZ_APP_VERSION,
        id: `langpack-${locale}@firefox.mozilla.org`,
        strict_max_version: AppConstants.MOZ_APP_VERSION,
      },
    },
    version: "1",
    manifest_version: 2,
    sources: {
      browser: {
        base_path: "browser/",
      },
    },
    author: "Mozilla",
  };
}

let testLocales = ["fr", "pl", "he"];
let testLangpacks;

function createTestLangpacks() {
  if (!testLangpacks) {
    testLangpacks = Promise.all(testLocales.map(locale =>
      AddonTestUtils.createTempXPIFile({
        "manifest.json": getManifestData(locale),
      })));
  }
  return testLangpacks;
}

function assertLocaleOrder(list, locales) {
  is(list.itemCount, locales.split(",").length,
     "The right number of locales are requested");
  is(Array.from(list.children).map(child => child.value).join(","),
     locales, "The requested locales are in order");
}

function assertAvailableLocales(list, locales) {
  let listLocales = Array.from(list.firstElementChild.children)
    .filter(item => item.value && item.value != "search");
  is(listLocales.length, locales.length, "The right number of locales are available");
  is(listLocales.map(item => item.value).sort(),
     locales.sort().join(","), "The available locales match");
}

function requestLocale(localeCode, available, dialogDoc) {
  let [locale] = Array.from(available.firstElementChild.children)
    .filter(item => item.value == localeCode);
  available.selectedItem = locale;
  dialogDoc.getElementById("add").doCommand();
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
      ["intl.locale.requested", "en-US"],
      ["extensions.langpacks.signatures.required", false],
    ],
  });

  let langpacks = await createTestLangpacks();
  let addons = await Promise.all(langpacks.map(async file => {
    let install = await AddonTestUtils.promiseInstallFile(file);
    return install.addon;
  }));

  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});

  let doc = gBrowser.contentDocument;
  let messageBar = doc.getElementById("confirmBrowserLanguage");
  is(messageBar.hidden, true, "The message bar is hidden at first");

  // Open the dialog.
  let {dialog, dialogDoc, available, requested} = await openDialog(doc);

  // The initial order is set by the pref.
  assertLocaleOrder(requested, "en-US");
  assertAvailableLocales(available, ["fr", "pl", "he"]);

  // Add pl and fr to requested.
  requestLocale("pl", available, dialogDoc);
  requestLocale("fr", available, dialogDoc);

  assertLocaleOrder(requested, "fr,pl,en-US");
  assertAvailableLocales(available, ["he"]);

  // Remove pl and fr from requested.
  dialogDoc.getElementById("remove").doCommand();
  dialogDoc.getElementById("remove").doCommand();
  assertLocaleOrder(requested, "en-US");
  assertAvailableLocales(available, ["fr", "pl", "he"]);

  // Add he to requested.
  requestLocale("he", available, dialogDoc);
  assertLocaleOrder(requested, "he,en-US");
  assertAvailableLocales(available, ["pl", "fr"]);

  // Accepting the change shows the confirm message bar.
  let dialogClosed = BrowserTestUtils.waitForEvent(dialogDoc.documentElement, "dialogclosing");
  dialog.acceptDialog();
  await dialogClosed;

  await waitForMutation(
    messageBar,
    {attributes: true, attributeFilter: ["hidden"]},
    target => !target.hidden);

  is(messageBar.hidden, false, "The message bar is now visible");
  is(messageBar.querySelector("button").getAttribute("locales"), "he,en-US",
    "The locales are set on the message bar button");

  await Promise.all(addons.map(addon => addon.uninstall()));

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
