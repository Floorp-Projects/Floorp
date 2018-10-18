/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm", this);
ChromeUtils.import("resource://gre/modules/Services.jsm");


AddonTestUtils.initMochitest(this);

const BROWSER_LANGUAGES_URL = "chrome://browser/content/preferences/browserLanguages.xul";
const DICTIONARY_ID_PL = "pl@dictionaries.addons.mozilla.org";

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
    testLangpacks = Promise.all(testLocales.map(async locale => [
      locale,
      await AddonTestUtils.createTempXPIFile({
        "manifest.json": getManifestData(locale),
      }),
    ]));
  }
  return testLangpacks;
}

function createLocaleResult(target_locale, url) {
  return {
    type: "language",
    target_locale,
    current_compatible_version: {
      files: [{
        platform: "all",
        url,
      }],
    },
  };
}

async function createLanguageToolsFile() {
  let langpacks = await createTestLangpacks();
  let results = langpacks.map(([locale, file]) =>
    createLocaleResult(locale, Services.io.newFileURI(file).spec));

  let filename = "language-tools.json";
  let files = {[filename]: {results}};
  let tempdir = AddonTestUtils.tempDir.clone();
  let dir = await AddonTestUtils.promiseWriteFilesToDir(tempdir.path, files);
  dir.append(filename);

  return dir;
}

async function createDictionaryBrowseResults() {
  let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
  let dictionaryPath = testDir + "/addons/pl-dictionary.xpi";
  let filename = "dictionaries.json";
  let response = {
    page_size: 25,
    page_count: 1,
    count: 1,
    results: [{
      current_version: {
        id: 1823648,
        compatibility: {
          firefox: {max: "9999", min: "4.0"},
        },
        files: [{
          platform: "all",
          url: dictionaryPath,
        }],
        version: "1.0.20160228",
      },
      default_locale: "pl",
      description: "Polish spell-check",
      guid: DICTIONARY_ID_PL,
      name: "Polish Dictionary",
      slug: "polish-spellchecker-dictionary",
      status: "public",
      summary: "Polish dictionary",
      type: "dictionary",
    }],
  };

  let files = {[filename]: response};
  let dir = await AddonTestUtils.promiseWriteFilesToDir(
    AddonTestUtils.tempDir.path, files);
  dir.append(filename);

  return dir;
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

async function openDialog(doc, search = false) {
  let dialogLoaded = promiseLoadSubDialog(BROWSER_LANGUAGES_URL);
  if (search) {
    doc.getElementById("defaultBrowserLanguageSearch").doCommand();
  } else {
    doc.getElementById("manageBrowserLanguagesButton").doCommand();
  }
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
      ["intl.multilingual.downloadEnabled", true],
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
      ["intl.multilingual.downloadEnabled", true],
      ["intl.locale.requested", "en-US"],
      ["extensions.langpacks.signatures.required", false],
    ],
  });

  let langpacks = await createTestLangpacks();
  let addons = await Promise.all(langpacks.map(async ([locale, file]) => {
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

add_task(async function testInstallFromAMO() {
  let langpacks = await AddonManager.getAddonsByTypes(["locale"]);
  is(langpacks.length, 0, "There are no langpacks installed");

  let langpacksFile = await createLanguageToolsFile();
  let langpacksUrl = Services.io.newFileURI(langpacksFile).spec;
  let dictionaryBrowseFile = await createDictionaryBrowseResults();
  let browseApiEndpoint = Services.io.newFileURI(dictionaryBrowseFile).spec;

  await SpecialPowers.pushPrefEnv({
    set: [
      ["intl.multilingual.enabled", true],
      ["intl.multilingual.downloadEnabled", true],
      ["intl.locale.requested", "en-US"],
      ["extensions.getAddons.langpacks.url", langpacksUrl],
      ["extensions.langpacks.signatures.required", false],
      ["extensions.getAddons.get.url", browseApiEndpoint],
    ],
  });

  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});

  let doc = gBrowser.contentDocument;
  let messageBar = doc.getElementById("confirmBrowserLanguage");
  is(messageBar.hidden, true, "The message bar is hidden at first");

  // Open the dialog.
  let {dialogDoc, available, requested} = await openDialog(doc, true);

  let dropdown = dialogDoc.getElementById("availableLocales");
  if (dropdown.itemCount == 1) {
    await waitForMutation(
      dropdown.firstElementChild,
      {childList: true},
      target => dropdown.itemCount > 1);
  }

  // The initial order is set by the pref.
  assertLocaleOrder(requested, "en-US");
  assertAvailableLocales(available, ["fr", "he", "pl"]);
  is(Services.locale.availableLocales.join(","),
     "en-US", "There is only one installed locale");

  // Verify that there are no extra dictionaries.
  let dicts = await AddonManager.getAddonsByTypes(["dictionary"]);
  is(dicts.length, 0, "There are no installed dictionaries");

  // Add Polish, this will install the langpack.
  requestLocale("pl", available, dialogDoc);

  // Wait for the langpack to install and be added to the list.
  let requestedLocales = dialogDoc.getElementById("requestedLocales");
  await waitForMutation(
    requestedLocales,
    {childList: true},
    target => requestedLocales.itemCount == 2);

  // Verify the list is correct.
  assertLocaleOrder(requested, "pl,en-US");
  assertAvailableLocales(available, ["fr", "he"]);
  is(Services.locale.availableLocales.sort().join(","),
     "en-US,pl", "Polish is now installed");

  await BrowserTestUtils.waitForCondition(async () => {
    let newDicts = await AddonManager.getAddonsByTypes(["dictionary"]);
    let done = newDicts.length != 0;

    if (done) {
      is(newDicts[0].id, DICTIONARY_ID_PL, "The polish dictionary was installed");
    }

    return done;
  });

  // Uninstall the langpack and dictionary.
  let installs = await AddonManager.getAddonsByTypes(["locale", "dictionary"]);
  is(installs.length, 2, "There is one langpack and one dictionary installed");
  await Promise.all(installs.map(item => item.uninstall()));

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

let hasSearchOption = popup => Array.from(popup.children).some(el => el.value == "search");

add_task(async function testDownloadEnabled() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["intl.multilingual.enabled", true],
      ["intl.multilingual.downloadEnabled", true],
    ],
  });

  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  let doc = gBrowser.contentDocument;

  let defaultMenulist = doc.getElementById("defaultBrowserLanguage");
  ok(hasSearchOption(defaultMenulist.firstChild), "There's a search option in the General pane");

  let { available } = await openDialog(doc, false);
  ok(hasSearchOption(available.firstChild), "There's a search option in the dialog");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});


add_task(async function testDownloadDisabled() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["intl.multilingual.enabled", true],
      ["intl.multilingual.downloadEnabled", false],
    ],
  });

  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  let doc = gBrowser.contentDocument;

  let defaultMenulist = doc.getElementById("defaultBrowserLanguage");
  ok(!hasSearchOption(defaultMenulist.firstChild), "There's no search option in the General pane");

  let { available } = await openDialog(doc, false);
  ok(!hasSearchOption(available.firstChild), "There's no search option in the dialog");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
