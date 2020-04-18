/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var tmp = {};
ChromeUtils.import(
  "resource:///modules/translation/TranslationParent.jsm",
  tmp
);
var { Translation, TranslationTelemetry } = tmp;
const Telemetry = Services.telemetry;

var MetricsChecker = {
  HISTOGRAMS: {
    OPPORTUNITIES: Services.telemetry.getHistogramById(
      "TRANSLATION_OPPORTUNITIES"
    ),
    OPPORTUNITIES_BY_LANG: Services.telemetry.getKeyedHistogramById(
      "TRANSLATION_OPPORTUNITIES_BY_LANGUAGE"
    ),
    PAGES: Services.telemetry.getHistogramById("TRANSLATED_PAGES"),
    PAGES_BY_LANG: Services.telemetry.getKeyedHistogramById(
      "TRANSLATED_PAGES_BY_LANGUAGE"
    ),
    CHARACTERS: Services.telemetry.getHistogramById("TRANSLATED_CHARACTERS"),
    DENIED: Services.telemetry.getHistogramById("DENIED_TRANSLATION_OFFERS"),
    AUTO_REJECTED: Services.telemetry.getHistogramById(
      "AUTO_REJECTED_TRANSLATION_OFFERS"
    ),
    SHOW_ORIGINAL: Services.telemetry.getHistogramById(
      "REQUESTS_OF_ORIGINAL_CONTENT"
    ),
    TARGET_CHANGES: Services.telemetry.getHistogramById(
      "CHANGES_OF_TARGET_LANGUAGE"
    ),
    DETECTION_CHANGES: Services.telemetry.getHistogramById(
      "CHANGES_OF_DETECTED_LANGUAGE"
    ),
    SHOW_UI: Services.telemetry.getHistogramById(
      "SHOULD_TRANSLATION_UI_APPEAR"
    ),
    DETECT_LANG: Services.telemetry.getHistogramById(
      "SHOULD_AUTO_DETECT_LANGUAGE"
    ),
  },

  reset() {
    for (let i of Object.keys(this.HISTOGRAMS)) {
      this.HISTOGRAMS[i].clear();
    }
    this.updateMetrics();
  },

  updateMetrics() {
    this._metrics = {
      opportunitiesCount: this.HISTOGRAMS.OPPORTUNITIES.snapshot().sum || 0,
      pageCount: this.HISTOGRAMS.PAGES.snapshot().sum || 0,
      charCount: this.HISTOGRAMS.CHARACTERS.snapshot().sum || 0,
      deniedOffers: this.HISTOGRAMS.DENIED.snapshot().sum || 0,
      autoRejectedOffers: this.HISTOGRAMS.AUTO_REJECTED.snapshot().sum || 0,
      showOriginal: this.HISTOGRAMS.SHOW_ORIGINAL.snapshot().sum || 0,
      detectedLanguageChangedBefore:
        this.HISTOGRAMS.DETECTION_CHANGES.snapshot().values[1] || 0,
      detectedLanguageChangeAfter:
        this.HISTOGRAMS.DETECTION_CHANGES.snapshot().values[0] || 0,
      targetLanguageChanged: this.HISTOGRAMS.TARGET_CHANGES.snapshot().sum || 0,
      showUI: this.HISTOGRAMS.SHOW_UI.snapshot().sum || 0,
      detectLang: this.HISTOGRAMS.DETECT_LANG.snapshot().sum || 0,
      // Metrics for Keyed histograms are estimated below.
      opportunitiesCountByLang: {},
      pageCountByLang: {},
    };

    let opportunities = this.HISTOGRAMS.OPPORTUNITIES_BY_LANG.snapshot();
    let pages = this.HISTOGRAMS.PAGES_BY_LANG.snapshot();
    for (let source of Translation.supportedSourceLanguages) {
      this._metrics.opportunitiesCountByLang[source] = opportunities[source]
        ? opportunities[source].sum
        : 0;
      for (let target of Translation.supportedTargetLanguages) {
        if (source === target) {
          continue;
        }
        let key = source + " -> " + target;
        this._metrics.pageCountByLang[key] = pages[key] ? pages[key].sum : 0;
      }
    }
  },

  /**
   * A recurrent loop for making assertions about collected metrics.
   */
  _assertionLoop(prevMetrics, metrics, additions) {
    for (let metric of Object.keys(additions)) {
      let addition = additions[metric];
      // Allows nesting metrics. Useful for keyed histograms.
      if (typeof addition === "object") {
        this._assertionLoop(prevMetrics[metric], metrics[metric], addition);
        continue;
      }
      Assert.equal(prevMetrics[metric] + addition, metrics[metric]);
    }
  },

  checkAdditions(additions) {
    let prevMetrics = this._metrics;
    this.updateMetrics();
    this._assertionLoop(prevMetrics, this._metrics, additions);
  },
};

function getInfobarElement(browser, anonid) {
  let actor = browser.browsingContext.currentWindowGlobal.getActor(
    "Translation"
  );
  let notif = actor.notificationBox.getNotificationWithValue("translation");
  return notif._getAnonElt(anonid);
}

var offerTranslationFor = async function(text, from) {
  // Create some content to translate.
  const dataUrl = "data:text/html;charset=utf-8," + text;
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, dataUrl);

  let browser = gBrowser.getBrowserForTab(tab);
  let actor = browser.browsingContext.currentWindowGlobal.getActor(
    "Translation"
  );

  // Send a translation offer.
  actor.documentStateReceived({
    state: Translation.STATE_OFFER,
    originalShown: true,
    detectedLanguage: from,
  });

  return tab;
};

var acceptTranslationOffer = async function(tab) {
  let browser = tab.linkedBrowser;
  let translationPromise = waitForTranslationDone();

  getInfobarElement(browser, "translate").doCommand();
  await translationPromise;
};

var translate = async function(text, from, closeTab = true) {
  let tab = await offerTranslationFor(text, from);
  await acceptTranslationOffer(tab);
  if (closeTab) {
    gBrowser.removeTab(tab);
    return null;
  }
  return tab;
};

function waitForTranslationDone() {
  return new Promise(resolve => {
    Translation.setListenerForTests(() => {
      Translation.setListenerForTests(null);
      resolve();
    });
  });
}

function simulateUserSelectInMenulist(menulist, value) {
  menulist.value = value;
  menulist.doCommand();
}

add_task(async function setup() {
  const setupPrefs = prefs => {
    let prefsBackup = {};
    for (let p of prefs) {
      prefsBackup[p] = Services.prefs.setBoolPref;
      Services.prefs.setBoolPref(p, true);
    }
    return prefsBackup;
  };

  const restorePrefs = (prefs, backup) => {
    for (let p of prefs) {
      Services.prefs.setBoolPref(p, backup[p]);
    }
  };

  const prefs = [
    "browser.translation.detectLanguage",
    "browser.translation.ui.show",
  ];

  let prefsBackup = setupPrefs(prefs);

  let oldCanRecord = Telemetry.canRecordExtended;
  Telemetry.canRecordExtended = true;

  registerCleanupFunction(() => {
    restorePrefs(prefs, prefsBackup);
    Telemetry.canRecordExtended = oldCanRecord;
  });

  // Reset histogram metrics.
  MetricsChecker.reset();
});

add_task(async function test_telemetry() {
  // Translate a page.
  await translate("<h1>Привет, мир!</h1>", "ru");

  // Translate another page.
  await translate("<h1>Hallo Welt!</h1><h1>Bratwurst!</h1>", "de");
  await MetricsChecker.checkAdditions({
    opportunitiesCount: 2,
    opportunitiesCountByLang: { ru: 1, de: 1 },
    pageCount: 1,
    pageCountByLang: { "de -> en": 1 },
    charCount: 21,
    deniedOffers: 0,
  });
});

add_task(async function test_deny_translation_metric() {
  async function offerAndDeny(elementAnonid) {
    let tab = await offerTranslationFor("<h1>Hallo Welt!</h1>", "de", "en");
    getInfobarElement(tab.linkedBrowser, elementAnonid).doCommand();
    await MetricsChecker.checkAdditions({ deniedOffers: 1 });
    gBrowser.removeTab(tab);
  }

  await offerAndDeny("notNow");
  await offerAndDeny("neverForSite");
  await offerAndDeny("neverForLanguage");
  await offerAndDeny("closeButton");

  // Test that the close button doesn't record a denied translation if
  // the infobar is not in its "offer" state.
  let tab = await translate("<h1>Hallo Welt!</h1>", "de", false);
  await MetricsChecker.checkAdditions({ deniedOffers: 0 });
  gBrowser.removeTab(tab);
});

add_task(async function test_show_original() {
  let tab = await translate(
    "<h1>Hallo Welt!</h1><h1>Bratwurst!</h1>",
    "de",
    false
  );
  await MetricsChecker.checkAdditions({ pageCount: 1, showOriginal: 0 });
  getInfobarElement(tab.linkedBrowser, "showOriginal").doCommand();
  await MetricsChecker.checkAdditions({ pageCount: 0, showOriginal: 1 });
  gBrowser.removeTab(tab);
});

add_task(async function test_language_change() {
  // This is run 4 times, the total additions are checked afterwards.
  // eslint-disable-next-line no-unused-vars
  for (let i of Array(4)) {
    let tab = await offerTranslationFor("<h1>Hallo Welt!</h1>", "fr");
    let browser = tab.linkedBrowser;
    // In the offer state, translation is executed by the Translate button,
    // so we expect just a single recoding.
    let detectedLangMenulist = getInfobarElement(browser, "detectedLanguage");
    simulateUserSelectInMenulist(detectedLangMenulist, "de");
    simulateUserSelectInMenulist(detectedLangMenulist, "it");
    simulateUserSelectInMenulist(detectedLangMenulist, "de");
    await acceptTranslationOffer(tab);

    // In the translated state, a change in the form or to menulists
    // triggers re-translation right away.
    let fromLangMenulist = getInfobarElement(browser, "fromLanguage");
    simulateUserSelectInMenulist(fromLangMenulist, "it");
    simulateUserSelectInMenulist(fromLangMenulist, "de");

    // Selecting the same item shouldn't count.
    simulateUserSelectInMenulist(fromLangMenulist, "de");

    let toLangMenulist = getInfobarElement(browser, "toLanguage");
    simulateUserSelectInMenulist(toLangMenulist, "fr");
    simulateUserSelectInMenulist(toLangMenulist, "en");
    simulateUserSelectInMenulist(toLangMenulist, "it");

    // Selecting the same item shouldn't count.
    simulateUserSelectInMenulist(toLangMenulist, "it");

    // Setting the target language to the source language is a no-op,
    // so it shouldn't count.
    simulateUserSelectInMenulist(toLangMenulist, "de");

    gBrowser.removeTab(tab);
  }
  await MetricsChecker.checkAdditions({
    detectedLanguageChangedBefore: 4,
    detectedLanguageChangeAfter: 8,
    targetLanguageChanged: 12,
  });
});

add_task(async function test_never_offer_translation() {
  Services.prefs.setCharPref("browser.translation.neverForLanguages", "fr");

  let tab = await offerTranslationFor("<h1>Hallo Welt!</h1>", "fr");

  await MetricsChecker.checkAdditions({
    autoRejectedOffers: 1,
  });

  gBrowser.removeTab(tab);
  Services.prefs.clearUserPref("browser.translation.neverForLanguages");
});

add_task(async function test_translation_preferences() {
  let preferenceChecks = {
    "browser.translation.ui.show": [
      { value: false, expected: { showUI: 0 } },
      { value: true, expected: { showUI: 1 } },
    ],
    "browser.translation.detectLanguage": [
      { value: false, expected: { detectLang: 0 } },
      { value: true, expected: { detectLang: 1 } },
    ],
  };

  for (let preference of Object.keys(preferenceChecks)) {
    for (let check of preferenceChecks[preference]) {
      MetricsChecker.reset();
      Services.prefs.setBoolPref(preference, check.value);
      // Preference metrics are collected once when the provider is initialized.
      TranslationTelemetry.init();
      await MetricsChecker.checkAdditions(check.expected);
    }
    Services.prefs.clearUserPref(preference);
  }
});
