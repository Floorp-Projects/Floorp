/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "Translation",
  "TranslationParent",
  "TranslationTelemetry",
];

const TRANSLATION_PREF_SHOWUI = "browser.translation.ui.show";
const TRANSLATION_PREF_DETECT_LANG = "browser.translation.detectLanguage";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var Translation = {
  STATE_OFFER: 0,
  STATE_TRANSLATING: 1,
  STATE_TRANSLATED: 2,
  STATE_ERROR: 3,
  STATE_UNAVAILABLE: 4,

  translationListener: null,

  serviceUnavailable: false,

  supportedSourceLanguages: [
    "bg",
    "cs",
    "de",
    "en",
    "es",
    "fr",
    "ja",
    "ko",
    "nl",
    "no",
    "pl",
    "pt",
    "ru",
    "tr",
    "vi",
    "zh",
  ],
  supportedTargetLanguages: [
    "bg",
    "cs",
    "de",
    "en",
    "es",
    "fr",
    "ja",
    "ko",
    "nl",
    "no",
    "pl",
    "pt",
    "ru",
    "tr",
    "vi",
    "zh",
  ],

  setListenerForTests(listener) {
    this.translationListener = listener;
  },

  _defaultTargetLanguage: "",
  get defaultTargetLanguage() {
    if (!this._defaultTargetLanguage) {
      this._defaultTargetLanguage = Services.locale.appLocaleAsBCP47.split(
        "-"
      )[0];
    }
    return this._defaultTargetLanguage;
  },

  openProviderAttribution() {
    let attribution = this.supportedEngines[this.translationEngine];
    const { BrowserWindowTracker } = ChromeUtils.import(
      "resource:///modules/BrowserWindowTracker.jsm"
    );
    BrowserWindowTracker.getTopWindow().openWebLinkIn(attribution, "tab");
  },

  /**
   * The list of translation engines and their attributions.
   */
  supportedEngines: {
    Google: "",
    Bing: "http://aka.ms/MicrosoftTranslatorAttribution",
    Yandex: "http://translate.yandex.com/",
  },

  /**
   * Fallback engine (currently Google) if the preferences seem confusing.
   */
  get defaultEngine() {
    return Object.keys(this.supportedEngines)[0];
  },

  /**
   * Returns the name of the preferred translation engine.
   */
  get translationEngine() {
    let engine = Services.prefs.getCharPref("browser.translation.engine");
    return !Object.keys(this.supportedEngines).includes(engine)
      ? this.defaultEngine
      : engine;
  },
};

/* Translation objects keep the information related to translation for
 * a specific browser.  The properties exposed to the infobar are:
 * - detectedLanguage, code of the language detected on the web page.
 * - state, the state in which the infobar should be displayed
 * - translatedFrom, if already translated, source language code.
 * - translatedTo, if already translated, target language code.
 * - translate, method starting the translation of the current page.
 * - showOriginalContent, method showing the original page content.
 * - showTranslatedContent, method showing the translation for an
 *   already translated page whose original content is shown.
 * - originalShown, boolean indicating if the original or translated
 *   version of the page is shown.
 */
class TranslationParent extends JSWindowActorParent {
  actorCreated() {
    this._state = 0;
    this.originalShown = true;
  }

  get browser() {
    let browser = this.browsingContext.top.embedderElement;
    return browser.outerBrowser ? browser.outerBrowser : browser;
  }

  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "Translation:DocumentState":
        this.documentStateReceived(aMessage.data);
        break;
    }
  }

  documentStateReceived(aData) {
    if (aData.state == Translation.STATE_OFFER) {
      if (aData.detectedLanguage == Translation.defaultTargetLanguage) {
        // Detected language is the same as the user's locale.
        return;
      }

      if (
        !Translation.supportedTargetLanguages.includes(aData.detectedLanguage)
      ) {
        // Detected language is not part of the supported languages.
        TranslationTelemetry.recordMissedTranslationOpportunity(
          aData.detectedLanguage
        );
        return;
      }

      TranslationTelemetry.recordTranslationOpportunity(aData.detectedLanguage);
    }

    if (!Services.prefs.getBoolPref(TRANSLATION_PREF_SHOWUI)) {
      return;
    }

    // Set all values before showing a new translation infobar.
    this._state = Translation.serviceUnavailable
      ? Translation.STATE_UNAVAILABLE
      : aData.state;
    this.detectedLanguage = aData.detectedLanguage;
    this.translatedFrom = aData.translatedFrom;
    this.translatedTo = aData.translatedTo;
    this.originalShown = aData.originalShown;

    this.showURLBarIcon();

    if (this.shouldShowInfoBar(this.browser.contentPrincipal)) {
      this.showTranslationInfoBar();
    }
  }

  translate(aFrom, aTo) {
    if (
      aFrom == aTo ||
      (this.state == Translation.STATE_TRANSLATED &&
        this.translatedFrom == aFrom &&
        this.translatedTo == aTo)
    ) {
      // Nothing to do.
      return;
    }

    if (this.state == Translation.STATE_OFFER) {
      if (this.detectedLanguage != aFrom) {
        TranslationTelemetry.recordDetectedLanguageChange(true);
      }
    } else {
      if (this.translatedFrom != aFrom) {
        TranslationTelemetry.recordDetectedLanguageChange(false);
      }
      if (this.translatedTo != aTo) {
        TranslationTelemetry.recordTargetLanguageChange();
      }
    }

    this.state = Translation.STATE_TRANSLATING;
    this.translatedFrom = aFrom;
    this.translatedTo = aTo;

    this.sendQuery("Translation:TranslateDocument", {
      from: aFrom,
      to: aTo,
    }).then(
      result => {
        this.translationFinished(result);
      },
      () => {}
    );
  }

  showURLBarIcon() {
    let chromeWin = this.browser.ownerGlobal;
    let PopupNotifications = chromeWin.PopupNotifications;
    let removeId = this.originalShown ? "translated" : "translate";
    let notification = PopupNotifications.getNotification(
      removeId,
      this.browser
    );
    if (notification) {
      PopupNotifications.remove(notification);
    }

    let callback = (aTopic, aNewBrowser) => {
      if (aTopic == "swapping") {
        let infoBarVisible = this.notificationBox.getNotificationWithValue(
          "translation"
        );
        if (infoBarVisible) {
          this.showTranslationInfoBar();
        }
        return true;
      }

      if (aTopic != "showing") {
        return false;
      }
      let translationNotification = this.notificationBox.getNotificationWithValue(
        "translation"
      );
      if (translationNotification) {
        translationNotification.close();
      } else {
        this.showTranslationInfoBar();
      }
      return true;
    };

    let addId = this.originalShown ? "translate" : "translated";
    PopupNotifications.show(
      this.browser,
      addId,
      null,
      addId + "-notification-icon",
      null,
      null,
      { dismissed: true, eventCallback: callback }
    );
  }

  get state() {
    return this._state;
  }

  set state(val) {
    let notif = this.notificationBox.getNotificationWithValue("translation");
    if (notif) {
      notif.state = val;
    }
    this._state = val;
  }

  showOriginalContent() {
    this.originalShown = true;
    this.showURLBarIcon();
    this.sendAsyncMessage("Translation:ShowOriginal");
    TranslationTelemetry.recordShowOriginalContent();
  }

  showTranslatedContent() {
    this.originalShown = false;
    this.showURLBarIcon();
    this.sendAsyncMessage("Translation:ShowTranslation");
  }

  get notificationBox() {
    return this.browser.ownerGlobal.gBrowser.getNotificationBox(this.browser);
  }

  showTranslationInfoBar() {
    let notificationBox = this.notificationBox;
    let notif = notificationBox.appendNotification(
      "",
      "translation",
      null,
      notificationBox.PRIORITY_INFO_HIGH,
      null,
      null,
      "translation-notification"
    );
    notif.init(this);
    return notif;
  }

  shouldShowInfoBar(aPrincipal) {
    // Never show the infobar automatically while the translation
    // service is temporarily unavailable.
    if (Translation.serviceUnavailable) {
      return false;
    }

    // Check if we should never show the infobar for this language.
    let neverForLangs = Services.prefs.getCharPref(
      "browser.translation.neverForLanguages"
    );
    if (neverForLangs.split(",").includes(this.detectedLanguage)) {
      TranslationTelemetry.recordAutoRejectedTranslationOffer();
      return false;
    }

    // or if we should never show the infobar for this domain.
    let perms = Services.perms;
    if (
      perms.testExactPermissionFromPrincipal(aPrincipal, "translate") ==
      perms.DENY_ACTION
    ) {
      TranslationTelemetry.recordAutoRejectedTranslationOffer();
      return false;
    }

    return true;
  }

  translationFinished(result) {
    if (result.success) {
      this.originalShown = false;
      this.state = Translation.STATE_TRANSLATED;
      this.showURLBarIcon();

      // Record the number of characters translated.
      TranslationTelemetry.recordTranslation(
        result.from,
        result.to,
        result.characterCount
      );
    } else if (result.unavailable) {
      Translation.serviceUnavailable = true;
      this.state = Translation.STATE_UNAVAILABLE;
    } else {
      this.state = Translation.STATE_ERROR;
    }

    if (Translation.translationListener) {
      Translation.translationListener();
    }
  }

  infobarClosed() {
    if (this.state == Translation.STATE_OFFER) {
      TranslationTelemetry.recordDeniedTranslationOffer();
    }
  }
}

/**
 * Uses telemetry histograms for collecting statistics on the usage of the
 * translation component.
 *
 * NOTE: Metrics are only recorded if the user enabled the telemetry option.
 */
var TranslationTelemetry = {
  init() {
    // Constructing histograms.
    const plain = id => Services.telemetry.getHistogramById(id);
    const keyed = id => Services.telemetry.getKeyedHistogramById(id);
    this.HISTOGRAMS = {
      OPPORTUNITIES: () => plain("TRANSLATION_OPPORTUNITIES"),
      OPPORTUNITIES_BY_LANG: () =>
        keyed("TRANSLATION_OPPORTUNITIES_BY_LANGUAGE"),
      PAGES: () => plain("TRANSLATED_PAGES"),
      PAGES_BY_LANG: () => keyed("TRANSLATED_PAGES_BY_LANGUAGE"),
      CHARACTERS: () => plain("TRANSLATED_CHARACTERS"),
      DENIED: () => plain("DENIED_TRANSLATION_OFFERS"),
      AUTO_REJECTED: () => plain("AUTO_REJECTED_TRANSLATION_OFFERS"),
      SHOW_ORIGINAL: () => plain("REQUESTS_OF_ORIGINAL_CONTENT"),
      TARGET_CHANGES: () => plain("CHANGES_OF_TARGET_LANGUAGE"),
      DETECTION_CHANGES: () => plain("CHANGES_OF_DETECTED_LANGUAGE"),
      SHOW_UI: () => plain("SHOULD_TRANSLATION_UI_APPEAR"),
      DETECT_LANG: () => plain("SHOULD_AUTO_DETECT_LANGUAGE"),
    };

    // Capturing the values of flags at the startup.
    this.recordPreferences();
  },

  /**
   * Record a translation opportunity in the health report.
   * @param language
   *        The language of the page.
   */
  recordTranslationOpportunity(language) {
    return this._recordOpportunity(language, true);
  },

  /**
   * Record a missed translation opportunity in the health report.
   * A missed opportunity is when the language detected is not part
   * of the supported languages.
   * @param language
   *        The language of the page.
   */
  recordMissedTranslationOpportunity(language) {
    return this._recordOpportunity(language, false);
  },

  /**
   * Record an automatically rejected translation offer in the health
   * report. A translation offer is automatically rejected when a user
   * has previously clicked "Never translate this language" or "Never
   * translate this site", which results in the infobar not being shown for
   * the translation opportunity.
   *
   * These translation opportunities should still be recorded in addition to
   * recording the automatic rejection of the offer.
   */
  recordAutoRejectedTranslationOffer() {
    this.HISTOGRAMS.AUTO_REJECTED().add();
  },

  /**
   * Record a translation in the health report.
   * @param langFrom
   *        The language of the page.
   * @param langTo
   *        The language translated to
   * @param numCharacters
   *        The number of characters that were translated
   */
  recordTranslation(langFrom, langTo, numCharacters) {
    this.HISTOGRAMS.PAGES().add();
    this.HISTOGRAMS.PAGES_BY_LANG().add(langFrom + " -> " + langTo);
    this.HISTOGRAMS.CHARACTERS().add(numCharacters);
  },

  /**
   * Record a change of the detected language in the health report. This should
   * only be called when actually executing a translation, not every time the
   * user changes in the language in the UI.
   *
   * @param beforeFirstTranslation
   *        A boolean indicating if we are recording a change of detected
   *        language before translating the page for the first time. If we
   *        have already translated the page from the detected language and
   *        the user has manually adjusted the detected language false should
   *        be passed.
   */
  recordDetectedLanguageChange(beforeFirstTranslation) {
    this.HISTOGRAMS.DETECTION_CHANGES().add(beforeFirstTranslation);
  },

  /**
   * Record a change of the target language in the health report. This should
   * only be called when actually executing a translation, not every time the
   * user changes in the language in the UI.
   */
  recordTargetLanguageChange() {
    this.HISTOGRAMS.TARGET_CHANGES().add();
  },

  /**
   * Record a denied translation offer.
   */
  recordDeniedTranslationOffer() {
    this.HISTOGRAMS.DENIED().add();
  },

  /**
   * Record a "Show Original" command use.
   */
  recordShowOriginalContent() {
    this.HISTOGRAMS.SHOW_ORIGINAL().add();
  },

  /**
   * Record the state of translation preferences.
   */
  recordPreferences() {
    if (Services.prefs.getBoolPref(TRANSLATION_PREF_SHOWUI)) {
      this.HISTOGRAMS.SHOW_UI().add(1);
    }
    if (Services.prefs.getBoolPref(TRANSLATION_PREF_DETECT_LANG)) {
      this.HISTOGRAMS.DETECT_LANG().add(1);
    }
  },

  _recordOpportunity(language, success) {
    this.HISTOGRAMS.OPPORTUNITIES().add(success);
    this.HISTOGRAMS.OPPORTUNITIES_BY_LANG().add(language, success);
  },
};

TranslationTelemetry.init();
