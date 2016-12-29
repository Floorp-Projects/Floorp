/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var tmp = {};
Cu.import("resource:///modules/translation/Translation.jsm", tmp);
var {Translation, TranslationTelemetry} = tmp;
const Telemetry = Services.telemetry;

var MetricsChecker = {
  HISTOGRAMS: {
    OPPORTUNITIES         : Services.telemetry.getHistogramById("TRANSLATION_OPPORTUNITIES"),
    OPPORTUNITIES_BY_LANG : Services.telemetry.getKeyedHistogramById("TRANSLATION_OPPORTUNITIES_BY_LANGUAGE"),
    PAGES                 : Services.telemetry.getHistogramById("TRANSLATED_PAGES"),
    PAGES_BY_LANG         : Services.telemetry.getKeyedHistogramById("TRANSLATED_PAGES_BY_LANGUAGE"),
    CHARACTERS            : Services.telemetry.getHistogramById("TRANSLATED_CHARACTERS"),
    DENIED                : Services.telemetry.getHistogramById("DENIED_TRANSLATION_OFFERS"),
    AUTO_REJECTED         : Services.telemetry.getHistogramById("AUTO_REJECTED_TRANSLATION_OFFERS"),
    SHOW_ORIGINAL         : Services.telemetry.getHistogramById("REQUESTS_OF_ORIGINAL_CONTENT"),
    TARGET_CHANGES        : Services.telemetry.getHistogramById("CHANGES_OF_TARGET_LANGUAGE"),
    DETECTION_CHANGES     : Services.telemetry.getHistogramById("CHANGES_OF_DETECTED_LANGUAGE"),
    SHOW_UI               : Services.telemetry.getHistogramById("SHOULD_TRANSLATION_UI_APPEAR"),
    DETECT_LANG           : Services.telemetry.getHistogramById("SHOULD_AUTO_DETECT_LANGUAGE"),
  },

  reset: function() {
    for (let i of Object.keys(this.HISTOGRAMS)) {
      this.HISTOGRAMS[i].clear();
    }
    this.updateMetrics();
  },

  updateMetrics: function() {
    this._metrics = {
      opportunitiesCount: this.HISTOGRAMS.OPPORTUNITIES.snapshot().sum || 0,
      pageCount: this.HISTOGRAMS.PAGES.snapshot().sum || 0,
      charCount: this.HISTOGRAMS.CHARACTERS.snapshot().sum || 0,
      deniedOffers: this.HISTOGRAMS.DENIED.snapshot().sum || 0,
      autoRejectedOffers: this.HISTOGRAMS.AUTO_REJECTED.snapshot().sum || 0,
      showOriginal: this.HISTOGRAMS.SHOW_ORIGINAL.snapshot().sum || 0,
      detectedLanguageChangedBefore: this.HISTOGRAMS.DETECTION_CHANGES.snapshot().counts[1] || 0,
      detectedLanguageChangeAfter: this.HISTOGRAMS.DETECTION_CHANGES.snapshot().counts[0] || 0,
      targetLanguageChanged: this.HISTOGRAMS.TARGET_CHANGES.snapshot().sum || 0,
      showUI: this.HISTOGRAMS.SHOW_UI.snapshot().sum || 0,
      detectLang: this.HISTOGRAMS.DETECT_LANG.snapshot().sum || 0,
      // Metrics for Keyed histograms are estimated below.
      opportunitiesCountByLang: {},
      pageCountByLang: {}
    };

    let opportunities = this.HISTOGRAMS.OPPORTUNITIES_BY_LANG.snapshot();
    let pages = this.HISTOGRAMS.PAGES_BY_LANG.snapshot();
    for (let source of Translation.supportedSourceLanguages) {
      this._metrics.opportunitiesCountByLang[source] = opportunities[source] ?
        opportunities[source].sum : 0;
      for (let target of Translation.supportedTargetLanguages) {
        if (source === target) continue;
        let key = source + " -> " + target;
        this._metrics.pageCountByLang[key] = pages[key] ? pages[key].sum : 0;
      }
    }
  },

  /**
   * A recurrent loop for making assertions about collected metrics.
   */
  _assertionLoop: function(prevMetrics, metrics, additions) {
    for (let metric of Object.keys(additions)) {
      let addition = additions[metric];
      // Allows nesting metrics. Useful for keyed histograms.
      if (typeof addition === 'object') {
        this._assertionLoop(prevMetrics[metric], metrics[metric], addition);
        continue;
      }
      Assert.equal(prevMetrics[metric] + addition, metrics[metric]);
    }
  },

  checkAdditions: function(additions) {
    let prevMetrics = this._metrics;
    this.updateMetrics();
    this._assertionLoop(prevMetrics, this._metrics, additions);
  }

};

function getInfobarElement(browser, anonid) {
  let notif = browser.translationUI
                     .notificationBox.getNotificationWithValue("translation");
  return notif._getAnonElt(anonid);
}

var offerTranslationFor = Task.async(function*(text, from) {
  // Create some content to translate.
  const dataUrl = "data:text/html;charset=utf-8," + text;
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, dataUrl);

  let browser = gBrowser.getBrowserForTab(tab);

  // Send a translation offer.
  Translation.documentStateReceived(browser, {state: Translation.STATE_OFFER,
                                              originalShown: true,
                                              detectedLanguage: from});

  return tab;
});

var acceptTranslationOffer = Task.async(function*(tab) {
  let browser = tab.linkedBrowser;
  getInfobarElement(browser, "translate").doCommand();
  yield waitForMessage(browser, "Translation:Finished");
});

var translate = Task.async(function*(text, from, closeTab = true) {
  let tab = yield offerTranslationFor(text, from);
  yield acceptTranslationOffer(tab);
  if (closeTab) {
    gBrowser.removeTab(tab);
    return null;
  }
  return tab;
});

function waitForMessage({messageManager}, name) {
  return new Promise(resolve => {
    messageManager.addMessageListener(name, function onMessage() {
      messageManager.removeMessageListener(name, onMessage);
      resolve();
    });
  });
}

function simulateUserSelectInMenulist(menulist, value) {
  menulist.value = value;
  menulist.doCommand();
}

add_task(function* setup() {
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
    "toolkit.telemetry.enabled",
    "browser.translation.detectLanguage",
    "browser.translation.ui.show"
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

add_task(function* test_telemetry() {
  // Translate a page.
  yield translate("<h1>Привет, мир!</h1>", "ru");

  // Translate another page.
  yield translate("<h1>Hallo Welt!</h1><h1>Bratwurst!</h1>", "de");
  yield MetricsChecker.checkAdditions({
    opportunitiesCount: 2,
    opportunitiesCountByLang: { "ru" : 1, "de" : 1 },
    pageCount: 1,
    pageCountByLang: { "de -> en" : 1 },
    charCount: 21,
    deniedOffers: 0
  });
});

add_task(function* test_deny_translation_metric() {
  function* offerAndDeny(elementAnonid) {
    let tab = yield offerTranslationFor("<h1>Hallo Welt!</h1>", "de", "en");
    getInfobarElement(tab.linkedBrowser, elementAnonid).doCommand();
    yield MetricsChecker.checkAdditions({ deniedOffers: 1 });
    gBrowser.removeTab(tab);
  }

  yield offerAndDeny("notNow");
  yield offerAndDeny("neverForSite");
  yield offerAndDeny("neverForLanguage");
  yield offerAndDeny("closeButton");

  // Test that the close button doesn't record a denied translation if
  // the infobar is not in its "offer" state.
  let tab = yield translate("<h1>Hallo Welt!</h1>", "de", false);
  yield MetricsChecker.checkAdditions({ deniedOffers: 0 });
  gBrowser.removeTab(tab);
});

add_task(function* test_show_original() {
  let tab =
    yield translate("<h1>Hallo Welt!</h1><h1>Bratwurst!</h1>", "de", false);
  yield MetricsChecker.checkAdditions({ pageCount: 1, showOriginal: 0 });
  getInfobarElement(tab.linkedBrowser, "showOriginal").doCommand();
  yield MetricsChecker.checkAdditions({ pageCount: 0, showOriginal: 1 });
  gBrowser.removeTab(tab);
});

add_task(function* test_language_change() {
  // This is run 4 times, the total additions are checked afterwards.
  for (let i of Array(4)) { // eslint-disable-line no-unused-vars
    let tab = yield offerTranslationFor("<h1>Hallo Welt!</h1>", "fr");
    let browser = tab.linkedBrowser;
    // In the offer state, translation is executed by the Translate button,
    // so we expect just a single recoding.
    let detectedLangMenulist = getInfobarElement(browser, "detectedLanguage");
    simulateUserSelectInMenulist(detectedLangMenulist, "de");
    simulateUserSelectInMenulist(detectedLangMenulist, "it");
    simulateUserSelectInMenulist(detectedLangMenulist, "de");
    yield acceptTranslationOffer(tab);

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
  yield MetricsChecker.checkAdditions({
    detectedLanguageChangedBefore: 4,
    detectedLanguageChangeAfter: 8,
    targetLanguageChanged: 12
  });
});

add_task(function* test_never_offer_translation() {
  Services.prefs.setCharPref("browser.translation.neverForLanguages", "fr");

  let tab = yield offerTranslationFor("<h1>Hallo Welt!</h1>", "fr");

  yield MetricsChecker.checkAdditions({
    autoRejectedOffers: 1,
  });

  gBrowser.removeTab(tab);
  Services.prefs.clearUserPref("browser.translation.neverForLanguages");
});

add_task(function* test_translation_preferences() {

  let preferenceChecks = {
    "browser.translation.ui.show" : [
      {value: false, expected: {showUI: 0}},
      {value: true, expected: {showUI: 1}}
    ],
    "browser.translation.detectLanguage" : [
      {value: false, expected: {detectLang: 0}},
      {value: true, expected: {detectLang: 1}}
    ],
  };

  for (let preference of Object.keys(preferenceChecks)) {
    for (let check of preferenceChecks[preference]) {
      MetricsChecker.reset();
      Services.prefs.setBoolPref(preference, check.value);
      // Preference metrics are collected once when the provider is initialized.
      TranslationTelemetry.init();
      yield MetricsChecker.checkAdditions(check.expected);
    }
    Services.prefs.clearUserPref(preference);
  }

});
